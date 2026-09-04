#ifndef WEB_PAGE_H
#define WEB_PAGE_H

const char WEB_PAGE[] PROGMEM = R"webpage(<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>mPython TFT + Buzzer</title>
  <style>
    :root {
      color-scheme: dark;
      --bg: #101214;
      --panel: #191d20;
      --line: #30363b;
      --text: #edf2f4;
      --muted: #9aa6ac;
      --accent: #00a2c7;
      --accent-2: #f5c542;
      --danger: #e2574c;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0 auto;
      max-width: 760px;
      padding: 18px;
      background: var(--bg);
      color: var(--text);
      font-family: ui-monospace, Menlo, Consolas, monospace;
    }
    h1 { margin: 0 0 14px; font-size: 1.35rem; font-weight: 700; letter-spacing: 0; }
    h2 { margin: 0 0 10px; font-size: 1rem; color: var(--accent-2); letter-spacing: 0; }
    p { margin: 8px 0; color: var(--muted); line-height: 1.45; }
    code { color: var(--text); background: #252b2f; padding: 2px 5px; border-radius: 4px; word-break: break-all; }
    .panel {
      margin: 12px 0;
      padding: 14px;
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 8px;
    }
    .row { display: flex; flex-wrap: wrap; gap: 8px; align-items: center; }
    button, input[type=file] {
      min-height: 42px;
      padding: 10px 14px;
      border: 1px solid var(--line);
      border-radius: 6px;
      font: inherit;
    }
    button {
      background: var(--accent);
      color: #061014;
      cursor: pointer;
      font-weight: 700;
    }
    button.secondary { background: #252b2f; color: var(--text); }
    button.stop { background: var(--danger); color: #fff; }
    button:disabled { opacity: .55; cursor: not-allowed; }
    input[type=file] { width: 100%; background: #252b2f; color: var(--text); }
    #status {
      min-height: 42px;
      margin-top: 10px;
      padding: 10px;
      background: #0b0d0e;
      border: 1px solid var(--line);
      border-radius: 6px;
      color: var(--muted);
      white-space: pre-wrap;
    }
    video { display: none; }
    canvas {
      display: block;
      width: 100%;
      max-width: 320px;
      margin-top: 10px;
      background: #000;
      border: 1px solid var(--line);
      border-radius: 6px;
    }
    .piano {
      position: relative;
      display: grid;
      grid-template-columns: repeat(8, minmax(0, 1fr));
      height: 172px;
      margin-top: 8px;
      touch-action: none;
    }
    .key {
      height: 172px;
      border-radius: 0 0 7px 7px;
      border: 1px solid #8b9398;
      background: #f5f1e8;
      color: #111;
      align-self: stretch;
      padding-top: 122px;
      box-shadow: inset 0 -18px 0 rgba(0,0,0,.08);
    }
    .key.active { background: #ffe28a; }
    .black {
      position: absolute;
      top: 0;
      width: 8.5%;
      height: 104px;
      z-index: 2;
      padding: 70px 0 0;
      border-color: #000;
      background: #050607;
      color: #fff;
      box-shadow: 0 5px 10px rgba(0,0,0,.5);
    }
    .black.active { background: #254450; }
    .b-cs { left: 8.5%; }
    .b-ds { left: 21%; }
    .b-fs { left: 46%; }
    .b-gs { left: 58.5%; }
    .b-as { left: 71%; }
    .meter {
      width: 100%;
      height: 10px;
      margin-top: 10px;
      overflow: hidden;
      background: #0b0d0e;
      border: 1px solid var(--line);
      border-radius: 99px;
    }
    #micLevel {
      width: 0%;
      height: 100%;
      background: var(--accent-2);
      transition: width 80ms linear;
    }
    @media (max-width: 520px) {
      body { padding: 12px; }
      .piano, .key { height: 140px; }
      .key { padding: 96px 2px 0; font-size: .78rem; }
      .black { height: 86px; padding-top: 57px; font-size: .68rem; }
    }
  </style>
</head>
<body>
  <h1>mPython TFT + Buzzer Lab</h1>
  <p>TFT: 320x172. Buzzer: GPIO21. Host: <code id="host"></code></p>

  <section class="panel">
    <h2>Static Upload</h2>
    <input type="file" id="f" accept="image/jpeg">
    <button id="u">Upload to TFT</button>
  </section>

  <section class="panel">
    <h2>Webcam Stream</h2>
    <div class="row">
      <button id="c">Start Camera</button>
      <button id="colorTest" class="secondary">TFT Color Test</button>
    </div>
    <video id="v" autoplay playsinline></video>
    <canvas id="a" width="320" height="172"></canvas>
  </section>

  <section class="panel">
    <h2>Buzzer Piano</h2>
    <div class="piano" id="piano">
      <button class="key" data-freq="262">C4</button>
      <button class="key" data-freq="294">D4</button>
      <button class="key" data-freq="330">E4</button>
      <button class="key" data-freq="349">F4</button>
      <button class="key" data-freq="392">G4</button>
      <button class="key" data-freq="440">A4</button>
      <button class="key" data-freq="494">B4</button>
      <button class="key" data-freq="523">C5</button>
      <button class="key black b-cs" data-freq="277">C#</button>
      <button class="key black b-ds" data-freq="311">D#</button>
      <button class="key black b-fs" data-freq="370">F#</button>
      <button class="key black b-gs" data-freq="415">G#</button>
      <button class="key black b-as" data-freq="466">A#</button>
    </div>
    <div class="row">
      <button id="doremi" class="secondary">Do Re Mi</button>
      <button id="stopTone" class="stop">Stop</button>
    </div>
  </section>

  <section class="panel">
    <h2>Voice to Buzzer</h2>
    <div class="row">
      <button id="mic">Start Voice</button>
      <button id="micStop" class="stop">Stop Voice</button>
    </div>
    <div class="meter"><div id="micLevel"></div></div>
    <p>如果瀏覽器唔畀用咪，開 terminal 行：<code>python3 tools/stream_voice_buzzer.py <span id="host2"></span></code></p>
  </section>

  <div id="status">Idle</div>

  <script>
    const v = document.getElementById('v');
    const a = document.getElementById('a');
    const x = a.getContext('2d', { colorSpace: 'srgb' });
    const f = document.getElementById('f');
    const u = document.getElementById('u');
    const c = document.getElementById('c');
    const s = document.getElementById('status');
    const micBtn = document.getElementById('mic');
    const micStopBtn = document.getElementById('micStop');
    const micLevel = document.getElementById('micLevel');
    document.getElementById('host').textContent = location.hostname;
    document.getElementById('host2').textContent = location.hostname;

    let streaming = false;
    let sending = false;
    let fpsCount = 0;
    let lastFps = performance.now();
    let micStream = null;
    let micCtx = null;
    let micNode = null;
    let micSource = null;
    let micQueue = [];
    let micSending = false;
    let micRunning = false;

    function setStatus(msg) { s.textContent = msg; }

    async function postImage(blob, endpoint) {
      const res = await fetch(endpoint, {
        method: 'POST',
        headers: { 'Content-Type': 'image/jpeg' },
        body: blob
      });
      return res.json();
    }

    function drawCoverCanvas(imgOrVideo) {
      const tw = a.width, th = a.height;
      const iw = imgOrVideo.videoWidth || imgOrVideo.width;
      const ih = imgOrVideo.videoHeight || imgOrVideo.height;
      const scale = Math.max(tw / iw, th / ih);
      const dw = iw * scale;
      const dh = ih * scale;
      const dx = (tw - dw) / 2;
      const dy = (th - dh) / 2;
      x.imageSmoothingEnabled = true;
      x.imageSmoothingQuality = 'high';
      x.fillStyle = '#000';
      x.fillRect(0, 0, tw, th);
      x.drawImage(imgOrVideo, dx, dy, dw, dh);
    }

    u.onclick = async () => {
      if (!f.files[0]) return setStatus('Choose a file first');
      if (f.files[0].type !== 'image/jpeg') return setStatus('Only JPEG files are supported');
      setStatus('Resizing...');
      const img = new Image();
      img.onload = () => {
        drawCoverCanvas(img);
        a.toBlob(async blob => {
          if (!blob || blob.size === 0) return setStatus('Resize failed: empty image');
          setStatus('Uploading ' + blob.size + ' bytes...');
          const form = new FormData();
          form.append('file', blob, 'image.jpg');
          try {
            const res = await fetch('/upload', { method: 'POST', body: form });
            const data = await res.json();
            setStatus(data.ok ? 'Upload OK' : 'Upload failed: ' + (data.error || ''));
          } catch (e) {
            setStatus('Upload error: ' + e.message);
          }
        }, 'image/jpeg', 0.95);
      };
      img.onerror = () => setStatus('Failed to load image');
      img.src = URL.createObjectURL(f.files[0]);
    };

    c.onclick = async () => {
      if (streaming) {
        streaming = false;
        c.textContent = 'Start Camera';
        if (v.srcObject) v.srcObject.getTracks().forEach(t => t.stop());
        setStatus('Stream stopped');
        return;
      }
      if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
        setStatus('Browser camera access is blocked on HTTP.\nUse: python3 tools/stream_webcam.py ' + location.hostname);
        return;
      }
      try {
        v.srcObject = await navigator.mediaDevices.getUserMedia({ video: { width: 640, height: 360 } });
        await v.play();
        streaming = true;
        c.textContent = 'Stop Camera';
        setStatus('Stream started');
        sendFrame();
      } catch (err) {
        setStatus('Camera error: ' + err.message + '\nUse: python3 tools/stream_webcam.py ' + location.hostname);
      }
    };

    function sendFrame() {
      if (!streaming) return;
      if (!sending) {
        sending = true;
        drawCoverCanvas(v);
        a.toBlob(async blob => {
          try {
            const data = await postImage(blob, '/api/frame');
            if (data.ok) fpsCount++;
          } catch (e) {
            console.error(e);
          }
          sending = false;
        }, 'image/jpeg', 0.6);
      }
      const now = performance.now();
      if (now - lastFps >= 1000) {
        setStatus('Streaming TFT - ' + fpsCount + ' FPS');
        fpsCount = 0;
        lastFps = now;
      }
      requestAnimationFrame(sendFrame);
    }

    document.getElementById('colorTest').onclick = async () => {
      const res = await fetch('/test/colors');
      const data = await res.json();
      setStatus(data.ok ? 'TFT color test sent' : 'Color test failed');
    };

    async function playFreq(freq, duration = 900) {
      await fetch('/api/buzzer/tone?freq=' + encodeURIComponent(freq) + '&duration=' + encodeURIComponent(duration));
    }

    async function stopTone() {
      await fetch('/api/buzzer/stop');
    }

    document.getElementById('piano').querySelectorAll('.key').forEach(key => {
      key.addEventListener('pointerdown', ev => {
        ev.preventDefault();
        key.classList.add('active');
        key.setPointerCapture(ev.pointerId);
        playFreq(key.dataset.freq);
      });
      key.addEventListener('pointerup', ev => {
        ev.preventDefault();
        key.classList.remove('active');
        stopTone();
      });
      key.addEventListener('pointercancel', () => {
        key.classList.remove('active');
        stopTone();
      });
    });

    document.getElementById('doremi').onclick = async () => {
      setStatus('Playing Do Re Mi on buzzer');
      await fetch('/test/buzzer/doremi');
      setStatus('Do Re Mi done');
    };
    document.getElementById('stopTone').onclick = stopTone;

    function floatToPcm8(input, inputRate) {
      const ratio = inputRate / 8000;
      const outLen = Math.floor(input.length / ratio);
      const out = new Uint8Array(outLen);
      let peak = 0;
      for (let i = 0; i < outLen; i++) {
        const src = input[Math.floor(i * ratio)] || 0;
        const sample = Math.max(-1, Math.min(1, src * 2.4));
        peak = Math.max(peak, Math.abs(sample));
        out[i] = Math.round((sample * 0.5 + 0.5) * 255);
      }
      micLevel.style.width = Math.round(peak * 100) + '%';
      return out;
    }

    function queueMicBytes(bytes) {
      for (let i = 0; i < bytes.length; i++) micQueue.push(bytes[i]);
      if (micQueue.length > 2400) micQueue.splice(0, micQueue.length - 1200);
      sendMicChunk();
    }

    async function sendMicChunk() {
      if (!micRunning || micSending || micQueue.length < 320) return;
      const len = Math.min(800, micQueue.length);
      const chunk = new Uint8Array(micQueue.splice(0, len));
      micSending = true;
      try {
        const res = await fetch('/api/buzzer/audio', {
          method: 'POST',
          headers: { 'Content-Type': 'application/octet-stream' },
          body: chunk
        });
        const data = await res.json();
        setStatus('Voice streaming to buzzer - ' + data.bytes + ' bytes');
      } catch (e) {
        setStatus('Voice stream error: ' + e.message);
      }
      micSending = false;
      if (micRunning) sendMicChunk();
    }

    micBtn.onclick = async () => {
      if (micRunning) return;
      if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
        setStatus('Browser mic blocked on HTTP.\nUse: python3 tools/stream_voice_buzzer.py ' + location.hostname);
        return;
      }
      try {
        micStream = await navigator.mediaDevices.getUserMedia({
          audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false }
        });
        micCtx = new (window.AudioContext || window.webkitAudioContext)();
        micSource = micCtx.createMediaStreamSource(micStream);
        micNode = micCtx.createScriptProcessor(1024, 1, 1);
        micNode.onaudioprocess = ev => {
          if (!micRunning) return;
          queueMicBytes(floatToPcm8(ev.inputBuffer.getChannelData(0), micCtx.sampleRate));
        };
        micSource.connect(micNode);
        micNode.connect(micCtx.destination);
        micRunning = true;
        micBtn.disabled = true;
        setStatus('Voice streaming started');
      } catch (e) {
        setStatus('Mic error: ' + e.message + '\nUse: python3 tools/stream_voice_buzzer.py ' + location.hostname);
      }
    };

    micStopBtn.onclick = async () => {
      micRunning = false;
      micQueue = [];
      micLevel.style.width = '0%';
      if (micNode) micNode.disconnect();
      if (micSource) micSource.disconnect();
      if (micStream) micStream.getTracks().forEach(t => t.stop());
      if (micCtx) await micCtx.close();
      micNode = micSource = micStream = micCtx = null;
      micBtn.disabled = false;
      await stopTone();
      setStatus('Voice stopped');
    };
  </script>
</body>
</html>)webpage";

#endif
