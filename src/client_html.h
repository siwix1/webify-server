#pragma once

// Inline HTML/JS client served at http://server:8080/
static const char* CLIENT_HTML = R"HTML(
<!DOCTYPE html>
<html>
<head>
<title>webify</title>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body { background: #1a1a2e; display: flex; justify-content: center;
         align-items: center; height: 100vh; font-family: system-ui; }
  #container { position: relative; border: 2px solid #16213e;
               box-shadow: 0 0 30px rgba(0,0,0,0.5); }
  canvas { display: block; cursor: default; }
  #status { position: fixed; top: 10px; right: 10px; color: #0f0;
            font-size: 12px; font-family: monospace;
            background: rgba(0,0,0,0.7); padding: 6px 10px;
            border-radius: 4px; }
  #status.disconnected { color: #f44; }
</style>
</head>
<body>
<div id="container">
  <canvas id="screen"></canvas>
</div>
<div id="status">Connecting...</div>

<script>
const canvas = document.getElementById('screen');
const ctx = canvas.getContext('2d');
const status = document.getElementById('status');

let ws;
let frameCount = 0;
let lastFpsTime = Date.now();
let fps = 0;

function connect() {
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
  ws = new WebSocket(`${proto}//${location.host}/ws`);
  ws.binaryType = 'arraybuffer';

  ws.onopen = () => {
    status.className = '';
    status.textContent = 'Connected';
  };

  ws.onmessage = (e) => {
    if (e.data instanceof ArrayBuffer) {
      // Binary = BMP frame
      const blob = new Blob([e.data], { type: 'image/bmp' });
      const url = URL.createObjectURL(blob);
      const img = new Image();
      img.onload = () => {
        if (canvas.width !== img.width || canvas.height !== img.height) {
          canvas.width = img.width;
          canvas.height = img.height;
        }
        ctx.drawImage(img, 0, 0);
        URL.revokeObjectURL(url);

        frameCount++;
        const now = Date.now();
        if (now - lastFpsTime >= 1000) {
          fps = frameCount;
          frameCount = 0;
          lastFpsTime = now;
        }
        status.textContent = `${fps} fps`;
      };
      img.src = url;
    }
  };

  ws.onclose = () => {
    status.className = 'disconnected';
    status.textContent = 'Disconnected — reconnecting...';
    setTimeout(connect, 2000);
  };

  ws.onerror = () => {
    ws.close();
  };
}

// Mouse events
canvas.addEventListener('mousemove', (e) => {
  if (!ws || ws.readyState !== 1) return;
  const rect = canvas.getBoundingClientRect();
  const x = Math.round((e.clientX - rect.left) * (canvas.width / rect.width));
  const y = Math.round((e.clientY - rect.top) * (canvas.height / rect.height));
  ws.send(JSON.stringify({ type: 'mousemove', x, y }));
});

canvas.addEventListener('mousedown', (e) => {
  if (!ws || ws.readyState !== 1) return;
  e.preventDefault();
  ws.send(JSON.stringify({ type: 'mousedown', button: e.button }));
});

canvas.addEventListener('mouseup', (e) => {
  if (!ws || ws.readyState !== 1) return;
  e.preventDefault();
  ws.send(JSON.stringify({ type: 'mouseup', button: e.button }));
});

canvas.addEventListener('wheel', (e) => {
  if (!ws || ws.readyState !== 1) return;
  e.preventDefault();
  ws.send(JSON.stringify({ type: 'wheel', delta: -e.deltaY }));
}, { passive: false });

canvas.addEventListener('contextmenu', (e) => e.preventDefault());

// Keyboard events
document.addEventListener('keydown', (e) => {
  if (!ws || ws.readyState !== 1) return;
  e.preventDefault();
  ws.send(JSON.stringify({ type: 'keydown', key: e.key, code: e.code,
                           keyCode: e.keyCode, shift: e.shiftKey,
                           ctrl: e.ctrlKey, alt: e.altKey }));
});

document.addEventListener('keyup', (e) => {
  if (!ws || ws.readyState !== 1) return;
  e.preventDefault();
  ws.send(JSON.stringify({ type: 'keyup', key: e.key, code: e.code,
                           keyCode: e.keyCode }));
});

connect();
</script>
</body>
</html>
)HTML";
