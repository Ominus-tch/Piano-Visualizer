# Piano AR Visualizer

A real-time piano visualization and VST3 instrument host for Windows.

**Piano AR Visualizer** combines MIDI input, camera-based rendering, real-time visualization, and VST3 instruments into a single application.

## Features

* 🎹 Real-time MIDI visualization
* 🎥 Live camera feed
* 📐 Perspective / planar mapping
* ✨ Animated piano notes and effects
* 🖥️ DirectX 11 rendering
* 🎛️ Dear ImGui interface
* 🎚️ VST3 plugin hosting
* 🪟 VST3 editor window support
* 📂 Drag-and-drop `.vst3` file/folder loading
* 🔌 MIDI routing to VST3 instruments
* 🖼️ Built-in or external visualizer support

## Visualizer

The project includes an almost completely rewritten **DirectX 11 implementation of [MIDIVisualizer](https://github.com/kosua20/MIDIVisualizer)** by [kosua20](https://github.com/kosua20).

The built-in visualizer is rendered directly into a texture and integrated into the application's rendering pipeline.

You can also select **`Capture Other Visualzer Window`** to capture an external visualizer instead. This allows the application to use either its built-in visualizer or another visualizer application as the visualization source.

## VST3

The application includes a full VST3 hosting system.

VST3 plugins can be loaded by simply dragging a `.vst3` file or folder onto the application. The plugin is initialized and its editor window is created when available.

Incoming MIDI can be routed to the VST3 plugin while simultaneously driving the visualizer, allowing the same MIDI performance to control both the instrument and visualization.

## Camera & AR

The camera feed can be used as the background for the visualization. Calibration and perspective mapping allow the visualizer to be aligned with a physical piano from different camera angles.

## Technologies

* C++
* DirectX 11
* VST3 SDK
* Dear ImGui
* Windows API
* Windows Media Foundation
* Windows MIDI
* [MIDIVisualizer](https://github.com/kosua20/MIDIVisualizer)

## Usage

1. Press **Choose Points** and follow the instructions. This will set up the virtual piano dimensions.
2. You may need to enable **Show Debug Lines**.
3. Adjust **Plane Height/Width Scale** to the desired levels.
4. Adjust **FOV/Offsets** as needed.
5. Choose either **Built-in visualizer** or **Capture Other Visualzer Window**.

### Built-in Visualizer

1. If a MIDI device wasn't automatically connected, connect it.
2. Set up the effect layers.

### Other Visualizer

1. Refresh the available windows.
2. Select the visualizer window to capture.

Don't forget to save your configurations!

Configuration is saved separately for the main application and the built-in MIDIVisualizer.

### VST Audio

If you want to hear the piano, simply drag a `.vst3` plugin onto the application window.

The plugin will be loaded and initialized automatically.

## Requirements

* Windows 10/11
* Visual Studio Code
* MIDI input device or MIDI software
* Camera for AR functionality
* VST3 instrument/plugin for VST3 functionality

No additional development environment is required beyond Visual Studio Code and the dependencies included with the project.

## Building

Clone the repository:

```bash
git clone https://github.com/Ominus-tch/Piano-Visualizer.git
```

Open the project in **Visual Studio Code** and build/run using the included project configuration.

## Development Status

The main systems are currently functional:

* ✅ DirectX 11 rendering
* ✅ MIDI input and visualization
* ✅ Built-in DirectX 11 MIDIVisualizer
* ✅ External visualizer window capture
* ✅ Camera input and perspective mapping
* ✅ VST3 plugin loading
* ✅ VST3 audio processing
* ✅ VST3 editor windows
* ✅ VST3 drag-and-drop
* 🚧 UI improvements
* 🚧 Additional visual effects
* 🚧 Optimization and cleanup

## License

See [LICENSE](LICENSE) for the project's license.
