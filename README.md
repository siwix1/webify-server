# webify-server

Desktop streaming server for legacy Win32 applications. Creates isolated Win32 desktops, launches apps on them, captures screen content, encodes to H.264, and streams via WebRTC.

## Architecture

```
Browser Client
    ↕ WebRTC (video + input)
webify-server (Windows Service)
    ├── DesktopManager  — CreateDesktop() + LaunchApp per session
    ├── ScreenCapture   — GDI capture per desktop
    ├── Encoder         — Media Foundation H.264 (software)
    ├── SignalingServer  — WebSocket for WebRTC negotiation
    └── InputHandler    — SendInput() routed to correct desktop
```

## Building

### On Windows (native)
```bash
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

### Cross-compile from macOS
```bash
brew install mingw-w64
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/windows-cross.cmake
cmake --build build
```

### Deploy to AWS
```bash
scp build/webify-server.exe webify@<server-ip>:C:/webify/
```

## Usage

```bash
webify-server.exe --app "C:\path\to\app.exe" --port 8443
```

Options:
- `--app <path>` — Application to launch (default: notepad.exe)
- `--width <px>` — Desktop width (default: 1024)
- `--height <px>` — Desktop height (default: 768)
- `--fps <n>` — Capture framerate (default: 15)
- `--port <n>` — Signaling WebSocket port (default: 8443)
