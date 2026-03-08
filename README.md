# Ray Tracer

An interactive desktop ray tracer with a full GUI. Adjust spheres, camera, and materials in real time, then render scenes with reflections, shadows, and anti-aliasing.

**Download:** [raytracer-cpp.alexstath.com](https://raytracer-cpp.alexstath.com)

## Screenshots

### App Interface

[placeholder - screenshot of the full app window with sidebar controls and a rendered scene]

### Example Renders

[placeholder - render of the default scene with mirror sphere, colored spheres, and checkerboard floor]

[placeholder - render with modified sphere positions/colors]

[placeholder - close-up render showing reflections and shadows]

[placeholder - high-res 4K render]

## Features

- **Interactive GUI** — sidebar controls for camera, spheres, materials, and render settings
- **Live preview** — watch the render build progressively line-by-line
- **Reflections** — recursive ray bouncing with per-material reflectivity
- **Shadows** — hard shadow casting from multiple light sources
- **Anti-aliasing** — supersampled AA from 1x up to 64x samples per pixel
- **Blinn-Phong shading** — ambient, diffuse, and specular lighting with Reinhard tone mapping
- **Checkerboard floor** — infinite procedural plane with reflections
- **Per-sphere editing** — position, radius, color, reflectivity, shininess, and enable/disable
- **Camera controls** — position, look-at target, and field of view
- **BMP export** — save renders at any resolution up to 4K via native file dialog
- **Cross-platform** — macOS (Apple Silicon + Intel) and Linux

## Download

| Platform | Download |
|----------|----------|
| macOS (Apple Silicon) | [RayTracer-mac-apple-silicon.zip](https://raytracer-cpp.alexstath.com/download/RayTracer-mac-apple-silicon.zip) |
| macOS (Intel) | [RayTracer-mac-intel.zip](https://raytracer-cpp.alexstath.com/download/RayTracer-mac-intel.zip) |
| Linux | [RayTracer-linux.AppImage](https://raytracer-cpp.alexstath.com/download/RayTracer-linux.AppImage) |

### macOS

```bash
# Extract the zip, then remove the quarantine flag (required once)
xattr -cr ~/Downloads/Ray\ Tracer.app

# Double-click the app to open
```

### Linux

```bash
chmod +x RayTracer-linux.AppImage
./RayTracer-linux.AppImage
```

## Building from Source

The GUI app uses Electron with the ray tracing engine running in a Web Worker.

```bash
cd electron
npm install
npm start          # run in development
npm run build:mac  # build macOS zip
npm run build:linux # build Linux AppImage
```

### C++ CLI Version

A standalone C++ command-line ray tracer is also included for scripting and batch rendering.

```bash
# Compile
g++ -O3 -std=c++17 src/raytracer.cpp -o raytracer -lm

# Render
./raytracer -w 1920 -h 1080 -s 4 -o render.bmp
```

#### CLI Options

| Flag | Description | Default |
|------|-------------|---------|
| `-w` | Image width | 1920 |
| `-h` | Image height | 1080 |
| `-s` | AA samples per axis (total = s*s) | 4 |
| `-o` | Output filename | render.bmp |

## How It Works

The ray tracer casts rays from the camera through each pixel into the scene. For each ray:

1. **Intersection** — test against all spheres and the floor plane, find the closest hit
2. **Shading** — compute Blinn-Phong lighting from each light source (ambient + diffuse + specular)
3. **Shadows** — cast a shadow ray toward each light; skip lighting contribution if occluded
4. **Reflections** — if the material is reflective, recursively trace a reflected ray (up to 5 bounces)
5. **Tone mapping** — apply Reinhard tone mapping and gamma correction to the final color

Anti-aliasing works by casting multiple rays per pixel in a grid pattern and averaging the results.

## Project Structure

```
├── electron/               # Electron GUI app
│   ├── main.js             # Main process (window, file dialog)
│   ├── preload.js          # Context bridge for IPC
│   └── renderer/
│       ├── index.html      # GUI (controls + preview canvas)
│       └── worker.js       # Ray tracer engine (Web Worker)
├── src/
│   ├── raytracer.cpp       # C++ CLI version
│   ├── raytracer_engine.h  # C++ engine (header-only)
│   └── main_gui.cpp        # C++ SDL2/ImGui GUI (reference)
├── web/                    # Download website
├── Dockerfile
└── docker-compose.yml
```

## License

MIT
