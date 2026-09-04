#include <Arduino.h>
#include <Arduino_GFX.h>
#include <Arduino_TFT.h>
#include <TJpg_Decoder.h>
#include <WebServer.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "web_page.h"
#include <databus/Arduino_ESP32SPIDMA.h>
#include <display/Arduino_ST7789.h>
#include <driver/i2s.h>
#include <esp32-hal-gpio.h>
#include <math.h>
#include <soc/gpio_sig_map.h>

#if __has_include("wifi_config.h")
#include "wifi_config.h"
#else
static const char *WIFI_SSID = "CHANGE_ME_WIFI_SSID";
static const char *WIFI_PASSWORD = "CHANGE_ME_WIFI_PASSWORD";
#endif

static constexpr uint8_t PIN_LCD_BL = 33;
static constexpr uint8_t PIN_LCD_CS = 34;
static constexpr uint8_t PIN_LCD_RS = 35;
static constexpr uint8_t PIN_LCD_SCK = 36;
static constexpr uint8_t PIN_LCD_SDA = 37;

static constexpr uint8_t PIN_I2C_SDA = 44;
static constexpr uint8_t PIN_I2C_SCL = 43;

static constexpr uint8_t PIN_AUDIO_DOUT = 38;
static constexpr uint8_t PIN_AUDIO_MCLK = 39;
static constexpr uint8_t PIN_AUDIO_DIN = 40;
static constexpr uint8_t PIN_AUDIO_BCK = 41;
static constexpr uint8_t PIN_AUDIO_LRCK = 42;
static constexpr uint8_t PIN_BUZZER = 21;
static constexpr uint8_t BUZZER_AUDIO_CHANNEL = 7;
static constexpr uint32_t BUZZER_AUDIO_PWM_HZ = 31250;
static constexpr uint32_t BUZZER_AUDIO_SAMPLE_RATE = 8000;
static constexpr size_t BUZZER_AUDIO_MAX_BYTES = 1200;
static constexpr uint16_t BUZZER_AUDIO_UDP_PORT = 4210;
static constexpr uint32_t BUZZER_UDP_TIMEOUT_MS = 180;

static constexpr int16_t LCD_PANEL_WIDTH = 172;
static constexpr int16_t LCD_PANEL_HEIGHT = 320;
static constexpr uint8_t LCD_ROTATION = 5;
static constexpr int16_t LCD_OFFSET_X = 34;
static constexpr int16_t LCD_OFFSET_Y = 0;

static constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 10000;
static constexpr size_t MAX_JPEG_BODY_BYTES = 65536;

static constexpr uint8_t PIN_BTN_B = 46;
static constexpr uint32_t BTN_DEBOUNCE_MS = 200;

// 1-bit black-and-white stream parameters
// Source frame is 160x86; it is scaled 2x to fill the 320x172 TFT.
static constexpr int16_t BW_SRC_WIDTH = 160;
static constexpr int16_t BW_SRC_HEIGHT = 86;
static constexpr int16_t BW_DST_WIDTH = 320;
static constexpr int16_t BW_DST_HEIGHT = 172;
static constexpr int16_t BW_CHUNK_ROWS = 8;
static constexpr uint8_t ST7789_MADCTL_BGR = 0x08;
static constexpr uint8_t JPEG_BRIGHTNESS_NUMERATOR = 6;
static constexpr uint8_t JPEG_BRIGHTNESS_DENOMINATOR = 5;
static constexpr uint8_t ES8388_ADDR = 0x10;
static constexpr uint32_t AUDIO_SAMPLE_RATE = 44100;
static constexpr uint16_t AUDIO_TONE_MS = 900;
static constexpr uint16_t AUDIO_GAP_MS = 180;
static constexpr float AUDIO_AMPLITUDE = 28000.0f;
static constexpr bool AUDIO_DIFFERENTIAL_OUTPUT = true;

class Mpython_ST7789 : public Arduino_ST7789 {
public:
  using Arduino_ST7789::Arduino_ST7789;

  void setRotation(uint8_t r) override {
    Arduino_TFT::setRotation(r);
    switch (_rotation) {
      case 1:
        r = ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_BGR;
        break;
      case 2:
        r = ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_BGR;
        break;
      case 3:
        r = ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_BGR;
        break;
      case 4:
        r = ST7789_MADCTL_MX | ST7789_MADCTL_BGR;
        break;
      case 5:
        r = ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_BGR;
        break;
      case 6:
        r = ST7789_MADCTL_MY | ST7789_MADCTL_BGR;
        break;
      case 7:
        r = ST7789_MADCTL_MV | ST7789_MADCTL_BGR;
        break;
      default:
        r = ST7789_MADCTL_BGR;
        break;
    }
    _bus->beginWrite();
    _bus->writeC8D8(ST7789_MADCTL, r);
    _bus->endWrite();
  }
};

Arduino_DataBus *lcdBus = new Arduino_ESP32SPIDMA(
    PIN_LCD_RS, PIN_LCD_CS, PIN_LCD_SCK, PIN_LCD_SDA, GFX_NOT_DEFINED);

Arduino_GFX *lcd = new Mpython_ST7789(
    lcdBus, GFX_NOT_DEFINED, LCD_ROTATION, false, LCD_PANEL_WIDTH,
    LCD_PANEL_HEIGHT, LCD_OFFSET_X, LCD_OFFSET_Y);

WebServer server(80);
WiFiUDP buzzerAudioUdp;
bool tftOk = false;
bool tftImageActive = false;
uint32_t streamFrameCount = 0;
uint32_t lastWifiAttemptMs = 0;
bool wifiIpPrinted = false;
bool standbyDrawn = false;

uint32_t lastBtnBMs = 0;

uint8_t *uploadBuffer = nullptr;
size_t uploadLen = 0;
bool uploadDecodeOk = false;

uint8_t *frameBwBuffer = nullptr;
size_t frameBwLen = 0;
bool frameBwOk = false;
uint16_t *frameBwPixelBuffer = nullptr;
bool audioOk = false;
bool i2sPlaybackInstalled = false;
String lastAudioError;
uint8_t audioSerialFormat = 0;
bool buzzerPwmActive = false;
bool buzzerUdpToneActive = false;
uint32_t lastBuzzerUdpMs = 0;
uint8_t buzzerUdpBuffer[512];

bool es8388Write(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ES8388_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool es8388Read(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(ES8388_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(static_cast<uint8_t>(ES8388_ADDR), static_cast<uint8_t>(1)) != 1) {
    return false;
  }
  value = Wire.read();
  return true;
}

bool initEs8388Playback(uint8_t serialFormat) {
  uint8_t probe = 0;
  if (!es8388Read(0x00, probe)) {
    lastAudioError = "ES8388 not found at I2C 0x10";
    return false;
  }
  Serial.print("ES8388 probe reg 0x00: 0x");
  Serial.println(probe, HEX);

  bool ok = true;
  ok &= es8388Write(0x00, 0x80);
  delay(10);
  ok &= es8388Write(0x00, 0x00);
  ok &= es8388Write(0x19, 0x04);
  ok &= es8388Write(0x01, 0x50);
  ok &= es8388Write(0x02, 0x00);
  ok &= es8388Write(0x08, 0x00);
  ok &= es8388Write(0x04, 0xC0);
  ok &= es8388Write(0x00, 0x12);
  ok &= es8388Write(0x17, serialFormat == 0 ? 0x18 : 0x1A);
  ok &= es8388Write(0x18, 0x02);
  ok &= es8388Write(0x26, 0x00);
  ok &= es8388Write(0x27, 0x90);
  ok &= es8388Write(0x2A, 0x90);
  ok &= es8388Write(0x2B, 0x80);
  ok &= es8388Write(0x2D, 0x00);
  ok &= es8388Write(0x1A, 0x00);
  ok &= es8388Write(0x1B, 0x00);
  ok &= es8388Write(0x1D, 0x01);
  ok &= es8388Write(0x03, 0xFF);
  ok &= es8388Write(0x02, 0xF0);
  delay(1);
  ok &= es8388Write(0x02, 0x00);
  ok &= es8388Write(0x04, 0x3C);
  ok &= es8388Write(0x03, 0x00);
  ok &= es8388Write(0x2E, 0x21);
  ok &= es8388Write(0x2F, 0x21);
  ok &= es8388Write(0x30, 0x21);
  ok &= es8388Write(0x31, 0x21);
  ok &= es8388Write(0x19, 0x00);

  if (!ok) {
    lastAudioError = "ES8388 register init failed";
  } else {
    Serial.println("ES8388 playback init OK");
  }
  return ok;
}

bool initI2sPlayback(uint8_t serialFormat) {
  if (i2sPlaybackInstalled) {
    i2s_driver_uninstall(I2S_NUM_0);
    i2sPlaybackInstalled = false;
  }

  i2s_config_t config = {};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = AUDIO_SAMPLE_RATE;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = serialFormat == 0 ? I2S_COMM_FORMAT_STAND_I2S : I2S_COMM_FORMAT_STAND_MSB;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 6;
  config.dma_buf_len = 256;
  config.use_apll = true;
  config.tx_desc_auto_clear = true;
  config.fixed_mclk = AUDIO_SAMPLE_RATE * 256;
  config.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  config.bits_per_chan = I2S_BITS_PER_CHAN_16BIT;

  esp_err_t err = i2s_driver_install(I2S_NUM_0, &config, 0, nullptr);
  if (err != ESP_OK) {
    lastAudioError = "i2s_driver_install failed: " + String(static_cast<int>(err));
    return false;
  }

  i2s_pin_config_t pins = {};
  pins.mck_io_num = PIN_AUDIO_MCLK;
  pins.bck_io_num = PIN_AUDIO_BCK;
  pins.ws_io_num = PIN_AUDIO_LRCK;
  pins.data_out_num = PIN_AUDIO_DIN;
  pins.data_in_num = PIN_AUDIO_DOUT;

  err = i2s_set_pin(I2S_NUM_0, &pins);
  if (err != ESP_OK) {
    lastAudioError = "i2s_set_pin failed: " + String(static_cast<int>(err));
    i2s_driver_uninstall(I2S_NUM_0);
    i2sPlaybackInstalled = false;
    return false;
  }

  pinMode(PIN_AUDIO_MCLK, OUTPUT);
  pinMatrixOutAttach(PIN_AUDIO_MCLK, I2S0_MCLK_OUT_IDX, false, false);
  i2s_set_clk(I2S_NUM_0, AUDIO_SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);

  i2s_zero_dma_buffer(I2S_NUM_0);
  i2sPlaybackInstalled = true;
  Serial.println("I2S playback init OK");
  return true;
}

bool initAudioPlayback(uint8_t serialFormat = 0) {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  if (!initEs8388Playback(serialFormat)) {
    audioOk = false;
    Serial.println(lastAudioError);
    return false;
  }
  if (!initI2sPlayback(serialFormat)) {
    audioOk = false;
    Serial.println(lastAudioError);
    return false;
  }
  audioSerialFormat = serialFormat;
  audioOk = true;
  lastAudioError = "";
  return true;
}

void playSilenceMs(uint16_t durationMs);

void startAudioOutput() {
  es8388Write(0x19, 0x00);
  delay(30);
}

void stopAudioOutput() {
  playSilenceMs(120);
  es8388Write(0x19, 0x04);
}

void playToneHz(float frequency, uint16_t durationMs) {
  static int16_t samples[256 * 2];
  const uint32_t totalFrames = (AUDIO_SAMPLE_RATE * durationMs) / 1000;
  uint32_t framesWritten = 0;
  float phase = 0.0f;
  const float phaseStep = 2.0f * PI * frequency / AUDIO_SAMPLE_RATE;

  while (framesWritten < totalFrames) {
    const uint32_t frames = min<uint32_t>(256, totalFrames - framesWritten);
    for (uint32_t i = 0; i < frames; i++) {
      const int16_t sample = static_cast<int16_t>(sinf(phase) * AUDIO_AMPLITUDE);
      phase += phaseStep;
      if (phase >= 2.0f * PI) {
        phase -= 2.0f * PI;
      }
      samples[i * 2] = sample;
      samples[i * 2 + 1] = AUDIO_DIFFERENTIAL_OUTPUT ? -sample : sample;
    }
    size_t bytesWritten = 0;
    i2s_write(I2S_NUM_0, samples, frames * 2 * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
    framesWritten += frames;
  }
}

void playSilenceMs(uint16_t durationMs) {
  static int16_t silence[256 * 2] = {};
  const uint32_t totalFrames = (AUDIO_SAMPLE_RATE * durationMs) / 1000;
  uint32_t framesWritten = 0;
  while (framesWritten < totalFrames) {
    const uint32_t frames = min<uint32_t>(256, totalFrames - framesWritten);
    size_t bytesWritten = 0;
    i2s_write(I2S_NUM_0, silence, frames * 2 * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
    framesWritten += frames;
  }
}

bool playDoReMi() {
  if ((!audioOk || audioSerialFormat != 0) && !initAudioPlayback(0)) {
    return false;
  }
  startAudioOutput();
  playToneHz(261.63f, AUDIO_TONE_MS);
  playSilenceMs(AUDIO_GAP_MS);
  playToneHz(329.63f, AUDIO_TONE_MS);
  playSilenceMs(AUDIO_GAP_MS);
  playToneHz(392.00f, AUDIO_TONE_MS);
  stopAudioOutput();
  return true;
}

bool playLongTone() {
  if ((!audioOk || audioSerialFormat != 0) && !initAudioPlayback(0)) {
    return false;
  }
  startAudioOutput();
  playToneHz(440.0f, 2500);
  stopAudioOutput();
  return true;
}

bool playLongToneLeftJustified() {
  if ((!audioOk || audioSerialFormat != 1) && !initAudioPlayback(1)) {
    return false;
  }
  startAudioOutput();
  playToneHz(440.0f, 2500);
  stopAudioOutput();
  return true;
}

bool playSquareTone() {
  if ((!audioOk || audioSerialFormat != 0) && !initAudioPlayback(0)) {
    return false;
  }
  startAudioOutput();
  static int16_t samples[256 * 2];
  const uint32_t totalFrames = AUDIO_SAMPLE_RATE * 2;
  uint32_t framesWritten = 0;
  uint32_t phase = 0;
  const uint32_t halfPeriod = AUDIO_SAMPLE_RATE / (440 * 2);
  while (framesWritten < totalFrames) {
    const uint32_t frames = min<uint32_t>(256, totalFrames - framesWritten);
    for (uint32_t i = 0; i < frames; i++) {
      const int16_t sample = ((phase / halfPeriod) & 1) ? 28000 : -28000;
      phase++;
      samples[i * 2] = sample;
      samples[i * 2 + 1] = AUDIO_DIFFERENTIAL_OUTPUT ? -sample : sample;
    }
    size_t bytesWritten = 0;
    i2s_write(I2S_NUM_0, samples, frames * 2 * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
    framesWritten += frames;
  }
  stopAudioOutput();
  return true;
}

void playBuzzerDoReMi() {
  tone(PIN_BUZZER, 262, 700);
  delay(820);
  tone(PIN_BUZZER, 330, 700);
  delay(820);
  tone(PIN_BUZZER, 392, 900);
  delay(1020);
  noTone(PIN_BUZZER);
}

void playBuzzerTone() {
  tone(PIN_BUZZER, 440, 2500);
  delay(2700);
  noTone(PIN_BUZZER);
}

void beginBuzzerPwm() {
  noTone(PIN_BUZZER);
  ledcSetup(BUZZER_AUDIO_CHANNEL, BUZZER_AUDIO_PWM_HZ, 8);
  ledcAttachPin(PIN_BUZZER, BUZZER_AUDIO_CHANNEL);
  buzzerPwmActive = true;
}

void stopBuzzerPwm() {
  if (buzzerPwmActive) {
    ledcWrite(BUZZER_AUDIO_CHANNEL, 0);
    ledcDetachPin(PIN_BUZZER);
    buzzerPwmActive = false;
  }
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
}

void playBuzzerPcm8(const uint8_t *data, size_t len) {
  if (!data || len == 0) {
    return;
  }
  buzzerUdpToneActive = false;
  beginBuzzerPwm();
  const uint32_t intervalUs = 1000000UL / BUZZER_AUDIO_SAMPLE_RATE;
  uint32_t nextUs = micros();
  for (size_t i = 0; i < len; i++) {
    ledcWrite(BUZZER_AUDIO_CHANNEL, data[i]);
    nextUs += intervalUs;
    while (static_cast<int32_t>(micros() - nextUs) < 0) {
      delayMicroseconds(8);
    }
    if ((i & 0x3F) == 0) {
      yield();
    }
  }
  ledcWrite(BUZZER_AUDIO_CHANNEL, 128);
}

void playBuzzerUdpTone(uint16_t freq) {
  stopBuzzerPwm();
  freq = constrain(freq, 80, 3000);
  tone(PIN_BUZZER, freq);
  buzzerUdpToneActive = true;
}

void stopBuzzerAll() {
  noTone(PIN_BUZZER);
  stopBuzzerPwm();
  buzzerUdpToneActive = false;
}

String audioRegisterDumpJson() {
  String json = "{\"ok\":true,\"registers\":{";
  bool first = true;
  for (uint8_t reg = 0; reg <= 0x34; reg++) {
    uint8_t value = 0;
    if (!es8388Read(reg, value)) {
      continue;
    }
    if (!first) {
      json += ",";
    }
    first = false;
    char key[8];
    char val[8];
    snprintf(key, sizeof(key), "\"%02X\"", reg);
    snprintf(val, sizeof(val), "\"%02X\"", value);
    json += key;
    json += ":";
    json += val;
  }
  json += "}}";
  return json;
}

uint8_t scaleColorChannel(uint8_t value, uint8_t maxValue) {
  uint16_t scaled = (static_cast<uint16_t>(value) * JPEG_BRIGHTNESS_NUMERATOR +
                     JPEG_BRIGHTNESS_DENOMINATOR / 2) /
                    JPEG_BRIGHTNESS_DENOMINATOR;
  return scaled > maxValue ? maxValue : static_cast<uint8_t>(scaled);
}

void brightenRgb565Tile(uint16_t *bitmap, uint32_t pixelCount) {
  for (uint32_t i = 0; i < pixelCount; i++) {
    const uint16_t color = bitmap[i];
    const uint8_t r = scaleColorChannel((color >> 11) & 0x1F, 0x1F);
    const uint8_t g = scaleColorChannel((color >> 5) & 0x3F, 0x3F);
    const uint8_t b = scaleColorChannel(color & 0x1F, 0x1F);
    bitmap[i] = (static_cast<uint16_t>(r) << 11) |
                (static_cast<uint16_t>(g) << 5) |
                b;
  }
}

void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  const uint32_t nowMs = millis();
  if (lastWifiAttemptMs != 0 && nowMs - lastWifiAttemptMs < WIFI_RECONNECT_INTERVAL_MS) {
    return;
  }
  lastWifiAttemptMs = nowMs;

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

bool jpgOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  if (x < 0 || y < 0 || w == 0 || h == 0) {
    return 0;
  }
  brightenRgb565Tile(bitmap, static_cast<uint32_t>(w) * h);
  lcd->draw16bitRGBBitmap(x, y, bitmap, w, h);
  return 1;
}

bool drawJpegToTft(const uint8_t *data, size_t len) {
  if (len == 0 || len > MAX_JPEG_BODY_BYTES) {
    return false;
  }
  TJpgDec.setJpgScale(0);
  TJpgDec.setSwapBytes(false);
  bool ok = TJpgDec.drawJpg(0, 0, data, static_cast<uint32_t>(len)) == 0;
  if (ok) {
    tftImageActive = true;
    streamFrameCount++;
  }
  return ok;
}

void handleRoot() {
  server.send_P(200, "text/html", WEB_PAGE);
}

void handleUploadData() {
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    uploadLen = 0;
    uploadDecodeOk = false;
    if (uploadBuffer) {
      free(uploadBuffer);
      uploadBuffer = nullptr;
    }
    uploadBuffer = static_cast<uint8_t *>(malloc(MAX_JPEG_BODY_BYTES));
    if (!uploadBuffer) {
      Serial.println("Upload: cannot allocate buffer");
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadBuffer && uploadLen + upload.currentSize <= MAX_JPEG_BODY_BYTES) {
      memcpy(uploadBuffer + uploadLen, upload.buf, upload.currentSize);
      uploadLen += upload.currentSize;
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    Serial.print("Upload received bytes: ");
    Serial.println(uploadLen);
    if (uploadLen >= 2 && uploadBuffer[0] == 0xFF && uploadBuffer[1] == 0xD8) {
      uploadDecodeOk = drawJpegToTft(uploadBuffer, uploadLen);
      Serial.println(uploadDecodeOk ? "Upload decoded OK" : "Upload decode failed");
    } else {
      Serial.println("Upload rejected: not a JPEG");
    }
    if (uploadBuffer) {
      free(uploadBuffer);
      uploadBuffer = nullptr;
    }
  }
}

void handleUploadResult() {
  server.send(200, "application/json", uploadDecodeOk ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"decode\"}");
}

void handleFrame() {
  String body = server.arg("plain");
  if (body.length() == 0) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"empty\"}");
    return;
  }
  drawJpegToTft(reinterpret_cast<const uint8_t *>(body.c_str()), body.length());
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleFrameBw() {
  server.send(400, "application/json", "{\"ok\":false,\"error\":\"use_multipart\"}");
}

void drawBwFrameToTft(const uint8_t *data, size_t len) {
  const size_t expectedBytes = (static_cast<size_t>(BW_SRC_WIDTH) * BW_SRC_HEIGHT) / 8;
  if (len != expectedBytes || !frameBwPixelBuffer) {
    return;
  }

  // Expand the 1-bit source into a 16-bit RGB565 buffer and scale 2x to fill the screen.
  uint16_t *buf = frameBwPixelBuffer;
  for (int16_t sy = 0; sy < BW_SRC_HEIGHT; sy++) {
    const uint8_t *rowData = data + (static_cast<size_t>(sy) * BW_SRC_WIDTH) / 8;
    for (int16_t sx = 0; sx < BW_SRC_WIDTH; sx++) {
      uint8_t b = rowData[sx / 8];
      bool white = (b >> (7 - (sx & 7))) & 1;
      uint16_t color = white ? 0xFFFF : 0x0000;
      int16_t dx = sx * 2;
      int16_t dy = sy * 2;
      size_t idx0 = static_cast<size_t>(dy) * BW_DST_WIDTH + dx;
      size_t idx1 = idx0 + BW_DST_WIDTH;
      buf[idx0] = color;
      buf[idx0 + 1] = color;
      buf[idx1] = color;
      buf[idx1 + 1] = color;
    }
  }

  // Explicitly use the TFT subclass fast path that streams pixels through a
  // single address window instead of the GFX base-class per-pixel fallback.
  static_cast<Arduino_TFT *>(lcd)->draw16bitRGBBitmap(0, 0, buf, BW_DST_WIDTH, BW_DST_HEIGHT);
}

void handleFrameBwData() {
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    frameBwLen = 0;
    frameBwOk = false;
    if (frameBwBuffer) {
      free(frameBwBuffer);
      frameBwBuffer = nullptr;
    }
    frameBwBuffer = static_cast<uint8_t *>(malloc((BW_SRC_WIDTH * BW_SRC_HEIGHT) / 8));
    if (!frameBwBuffer) {
      Serial.println("BW frame: cannot allocate buffer");
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    const size_t expectedBytes = (static_cast<size_t>(BW_SRC_WIDTH) * BW_SRC_HEIGHT) / 8;
    if (frameBwBuffer && frameBwLen + upload.currentSize <= expectedBytes) {
      memcpy(frameBwBuffer + frameBwLen, upload.buf, upload.currentSize);
      frameBwLen += upload.currentSize;
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    const size_t expectedBytes = (static_cast<size_t>(BW_SRC_WIDTH) * BW_SRC_HEIGHT) / 8;
    Serial.print("BW frame received bytes: ");
    Serial.println(frameBwLen);
    if (frameBwLen == expectedBytes) {
      drawBwFrameToTft(frameBwBuffer, frameBwLen);
      tftImageActive = true;
      streamFrameCount++;
      frameBwOk = true;
      Serial.println("BW frame drawn OK");
    } else {
      frameBwOk = false;
      Serial.println("BW frame rejected: wrong size");
    }
    if (frameBwBuffer) {
      free(frameBwBuffer);
      frameBwBuffer = nullptr;
    }
  }
}

void handleFrameBwResult() {
  server.send(200, "application/json", frameBwOk ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"size\"}");
}

void handleStatus() {
  String json = "{\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"frames\":" + String(streamFrameCount) + ",";
  json += "\"image_active\":" + String(tftImageActive ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

void drawColorTestPattern() {
  static constexpr uint16_t colors[] = {
      WHITE,
      RED,
      GREEN,
      BLUE,
      BLACK,
      RGB565(128, 128, 128),
  };
  static constexpr int16_t barCount = static_cast<int16_t>(sizeof(colors) / sizeof(colors[0]));
  const int16_t barWidth = BW_DST_WIDTH / barCount;

  for (int16_t i = 0; i < barCount; i++) {
    const int16_t x = i * barWidth;
    const int16_t w = (i == barCount - 1) ? (BW_DST_WIDTH - x) : barWidth;
    lcd->fillRect(x, 0, w, BW_DST_HEIGHT, colors[i]);
  }
  tftImageActive = true;
  standbyDrawn = false;
}

void handleColorTest() {
  drawColorTestPattern();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleDoReMiTest() {
  const bool ok = playDoReMi();
  if (ok) {
    server.send(200, "application/json", "{\"ok\":true,\"test\":\"doremi\"}");
  } else {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"" + lastAudioError + "\"}");
  }
}

void handleToneTest() {
  const bool ok = playLongTone();
  if (ok) {
    server.send(200, "application/json", "{\"ok\":true,\"test\":\"tone\",\"frequency\":440}");
  } else {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"" + lastAudioError + "\"}");
  }
}

void handleToneLeftJustifiedTest() {
  const bool ok = playLongToneLeftJustified();
  if (ok) {
    server.send(200, "application/json", "{\"ok\":true,\"test\":\"tone_lj\",\"frequency\":440}");
  } else {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"" + lastAudioError + "\"}");
  }
}

void handleSquareToneTest() {
  const bool ok = playSquareTone();
  if (ok) {
    server.send(200, "application/json", "{\"ok\":true,\"test\":\"square\",\"frequency\":440}");
  } else {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"" + lastAudioError + "\"}");
  }
}

void handleAudioDebug() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  server.send(200, "application/json", audioRegisterDumpJson());
}

void handleBuzzerToneTest() {
  playBuzzerTone();
  server.send(200, "application/json", "{\"ok\":true,\"test\":\"buzzer_tone\",\"frequency\":440}");
}

void handleBuzzerDoReMiTest() {
  playBuzzerDoReMi();
  server.send(200, "application/json", "{\"ok\":true,\"test\":\"buzzer_doremi\"}");
}

void handleBuzzerTone() {
  int freq = server.hasArg("freq") ? server.arg("freq").toInt() : 440;
  int duration = server.hasArg("duration") ? server.arg("duration").toInt() : 350;
  freq = constrain(freq, 80, 3000);
  duration = constrain(duration, 40, 3000);
  stopBuzzerAll();
  tone(PIN_BUZZER, freq, duration);
  String json = "{\"ok\":true,\"freq\":";
  json += String(freq);
  json += ",\"duration\":";
  json += String(duration);
  json += "}";
  server.send(200, "application/json", json);
}

void handleBuzzerStop() {
  stopBuzzerAll();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleBuzzerAudio() {
  String body = server.arg("plain");
  if (body.length() == 0) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"empty\"}");
    return;
  }
  const size_t len = min(static_cast<size_t>(body.length()), BUZZER_AUDIO_MAX_BYTES);
  playBuzzerPcm8(reinterpret_cast<const uint8_t *>(body.c_str()), len);
  String json = "{\"ok\":true,\"bytes\":";
  json += String(len);
  json += ",\"sample_rate\":";
  json += String(BUZZER_AUDIO_SAMPLE_RATE);
  json += "}";
  server.send(200, "application/json", json);
}

void handleBuzzerAudioInfo() {
  String json = "{\"ok\":true,\"udp_port\":";
  json += String(BUZZER_AUDIO_UDP_PORT);
  json += ",\"sample_rate\":";
  json += String(BUZZER_AUDIO_SAMPLE_RATE);
  json += ",\"format\":\"unsigned_pcm8_or_pitch_tone\"}";
  server.send(200, "application/json", json);
}

void handleBuzzerUdpAudio() {
  const int packetSize = buzzerAudioUdp.parsePacket();
  if (packetSize <= 0) {
    return;
  }
  const size_t len = min(static_cast<size_t>(packetSize), sizeof(buzzerUdpBuffer));
  const int readLen = buzzerAudioUdp.read(buzzerUdpBuffer, len);
  if (readLen > 0) {
    lastBuzzerUdpMs = millis();
    if (readLen >= 3 && buzzerUdpBuffer[0] == 'T') {
      const uint16_t freq = (static_cast<uint16_t>(buzzerUdpBuffer[1]) << 8) | buzzerUdpBuffer[2];
      playBuzzerUdpTone(freq);
      return;
    }
    if (readLen >= 1 && buzzerUdpBuffer[0] == 'S') {
      stopBuzzerAll();
      return;
    }
    playBuzzerPcm8(buzzerUdpBuffer, static_cast<size_t>(readLen));
  }
}

void drawStandbyScreen() {
  if (!tftOk || standbyDrawn) {
    return;
  }
  standbyDrawn = true;

  lcd->fillScreen(BLACK);
  lcd->setTextSize(2);
  lcd->setTextColor(WHITE, BLACK);
  lcd->setCursor(20, 70);
  lcd->print("TFT Stream Ready");
  lcd->setTextSize(1);
  lcd->setCursor(20, 110);
  lcd->print("Open in browser:");
  lcd->setCursor(20, 130);
  lcd->print("http://");
  lcd->print(WiFi.localIP().toString());
}

void setup() {
  Serial.begin(115200);
  delay(800);

  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  pinMode(PIN_BTN_B, INPUT_PULLUP);

  if (lcd->begin()) {
    tftOk = true;
    lcd->setRotation(LCD_ROTATION);
    lcd->fillScreen(BLACK);
    frameBwPixelBuffer = static_cast<uint16_t *>(malloc(static_cast<size_t>(BW_DST_WIDTH) * BW_DST_HEIGHT * sizeof(uint16_t)));
    if (!frameBwPixelBuffer) {
      Serial.println("Failed to allocate BW pixel buffer");
    }
  }

  TJpgDec.setCallback(jpgOutput);

  connectWifi();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/upload", HTTP_POST, handleUploadResult, handleUploadData);
  server.on("/api/frame", HTTP_POST, handleFrame);
  server.on("/api/frame/bw", HTTP_POST, handleFrameBwResult, handleFrameBwData);
  server.on("/test/colors", HTTP_GET, handleColorTest);
  server.on("/test/doremi", HTTP_GET, handleDoReMiTest);
  server.on("/test/tone", HTTP_GET, handleToneTest);
  server.on("/test/tone/lj", HTTP_GET, handleToneLeftJustifiedTest);
  server.on("/test/tone/square", HTTP_GET, handleSquareToneTest);
  server.on("/test/audio/debug", HTTP_GET, handleAudioDebug);
  server.on("/test/buzzer/tone", HTTP_GET, handleBuzzerToneTest);
  server.on("/test/buzzer/doremi", HTTP_GET, handleBuzzerDoReMiTest);
  server.on("/api/buzzer/tone", HTTP_GET, handleBuzzerTone);
  server.on("/api/buzzer/stop", HTTP_GET, handleBuzzerStop);
  server.on("/api/buzzer/audio", HTTP_POST, handleBuzzerAudio);
  server.on("/api/buzzer/audio", HTTP_GET, handleBuzzerAudioInfo);
  server.on("/status", HTTP_GET, handleStatus);
  server.begin();
  buzzerAudioUdp.begin(BUZZER_AUDIO_UDP_PORT);
  Serial.println("HTTP server started on port 80");
  Serial.print("Buzzer UDP audio port: ");
  Serial.println(BUZZER_AUDIO_UDP_PORT);
}

void loop() {
  connectWifi();
  server.handleClient();
  handleBuzzerUdpAudio();
  if ((buzzerPwmActive || buzzerUdpToneActive) &&
      lastBuzzerUdpMs != 0 &&
      millis() - lastBuzzerUdpMs > BUZZER_UDP_TIMEOUT_MS) {
    stopBuzzerAll();
    lastBuzzerUdpMs = 0;
  }

  if (WiFi.status() == WL_CONNECTED && !wifiIpPrinted) {
    wifiIpPrinted = true;
    Serial.print("WiFi connected, open http://");
    Serial.print(WiFi.localIP());
    Serial.println("/ in your browser");
    drawStandbyScreen();
  }

  if (digitalRead(PIN_BTN_B) == LOW) {
    const uint32_t nowMs = millis();
    if (nowMs - lastBtnBMs >= BTN_DEBOUNCE_MS) {
      lastBtnBMs = nowMs;
      if (tftImageActive) {
        Serial.println("Button B pressed: clearing image");
        tftImageActive = false;
        standbyDrawn = false;
        drawStandbyScreen();
      }
    }
  }
}
