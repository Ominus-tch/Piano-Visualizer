# Piano AR Visualizer

A real-time augmented reality piano visualizer that uses a camera feed and MIDI input to overlay animated notes onto a physical piano.

The project uses perspective mapping to transform a virtual piano visualization onto the camera's view, allowing MIDI notes to appear at the corresponding piano keys.

## Features

- 🎥 Live camera input
- 🎹 MIDI note detection
- ✨ Real-time animated note visualization
- 📐 Perspective / planar mapping
- 🖱️ Interactive calibration by selecting points on the camera feed
- 🖥️ DirectX 11 rendering
- 🎛️ ImGui-based interface
- ⚡ High-performance C++ implementation

## How It Works

The application combines a live camera feed with a virtual 3D representation of a piano.

The basic pipeline is:

```text
Camera
   │
   ▼
Camera Feed
   │
   ▼
Calibration
   │
   ▼
Perspective Mapping
   │
   ├───────────────┐
   │               │
   ▼               ▼
Piano Geometry    MIDI Input
   │               │
   └───────┬───────┘
           ▼
    Note Visualization
           │
           ▼
      Camera Overlay
```

The user first calibrates the piano area by selecting points on the camera feed. These points define the plane onto which the virtual piano visualization is mapped.

When a MIDI note is received, the corresponding key is identified and a visual note is generated at that position. The note then moves through the mapped piano space in real time.

Technology
C++
DirectX 11
Dear ImGui
Media Foundation
Windows MIDI
Windows API

## Calibration

The piano is calibrated directly from the camera view.

The planned calibration process is:

1. Start the camera.
2. Display the live camera feed.
3. Select the required points on the piano.
4. Calculate the perspective transformation.
5. Map virtual piano coordinates onto the selected region.
6. Render MIDI-driven notes using the resulting transformation.

This allows the visualization to remain aligned with the physical piano even when the camera is positioned at an angle.

## Development Status

### 🚧 Early development

Currently being developed:

* DirectX 11 window
* ImGui interface
* Camera device detection
* Media Foundation camera access
* Live camera visualization
* Camera calibration
* Perspective transformation
* MIDI input
* Piano key detection/mapping
* Animated notes
* 3D piano visualization
* Final rendering pipeline

Requirements
Windows 10/11
Visual Studio 2022 or newer
C++20 or newer
DirectX 11 compatible GPU
A compatible camera
A MIDI input device or MIDI software

## Building

Clone the repository:
```bash
git clone https://github.com/Ominus-tch/Piano-Visualizer.git
```
Open the Visual Studio solution and build the project using the desired configuration.

The project currently targets Windows and uses DirectX 11 and Windows Media Foundation.

## Goals

The long-term goal is to create a visually accurate real-time piano visualization that makes it appear as though animated notes are physically moving toward the keys of the piano.

The system is designed to work with different camera angles by using perspective transformation rather than relying on a fixed camera position.

## License

See [LICENSE](LICENSE) for the project's license.