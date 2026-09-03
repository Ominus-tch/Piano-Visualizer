# Piano Visualizer

A real-time piano AR visualization and VST3 instrument host for Windows.

**Piano Visualizer** combines MIDI input, camera-based augmented-reality rendering, real-time piano visualization, and VST3 instruments into a single application.

The application can render its built-in visualizer directly into the camera pipeline or capture another visualizer application, allowing the visualization to be projected onto a physical piano from an arbitrary camera angle.

## Features

- 🎹 Real-time MIDI input and visualization
- 🎥 Live camera feed
- 📐 Perspective / planar camera mapping
- ✨ Animated piano notes, particles, flashes, waves, and effects
- 🖥️ DirectX 11 rendering
- 🎛️ Dear ImGui interface
- 🎚️ VST3 plugin hosting
- 🪟 VST3 editor window support
- 📂 Drag-and-drop `.vst3` file/folder loading
- 🔌 MIDI routing to VST3 instruments
- 🎵 Real-time VST3 audio processing
- 🔊 Configurable audio output devices
- ⚙️ Configurable sample rate and audio buffer duration
- 🔇 Audio volume and mute controls
- 📷 Configurable camera formats and capture modes
- 🎞️ NV12 and MJPG camera support
- 🎥 External visualizer window capture
- 🖼️ Built-in DirectX 11 visualizer
- 📊 Audio and camera performance statistics
- 🎮 Runtime camera configuration and controls

---

## Visualizer

The project includes an almost completely rewritten **DirectX 11 implementation of [MIDIVisualizer by kosua20](https://github.com/kosua20/MIDIVisualizer)**.

The built-in visualizer is rendered directly into a DirectX 11 texture and integrated into the application's rendering pipeline.

The visualizer supports animated piano keys, notes, flashes, particles, waves, and other effect layers.

You can also select **`Capture Other Visualizer Window`** to capture an external visualizer instead.

This allows the application to use either:

1. The built-in DirectX 11 visualizer
2. An external visualizer application

The external capture system is useful when you want to use a different visualization application while still using Piano Visualizer for the camera, MIDI, and AR projection pipeline.

---

## MIDI

MIDI input is used as the primary source for the piano visualization.

Incoming MIDI events can simultaneously drive:

- The visualizer
- The physical piano mapping
- A VST3 instrument

This allows a single MIDI performance to control both the visual output and the instrument producing the sound.

---

## VST3

The application includes a VST3 hosting system for loading and playing software instruments.

VST3 plugins can be loaded by dragging a `.vst3` file or folder onto the application window.

When a plugin is loaded, the application:

1. Loads the VST3 module
2. Creates the VST3 component and controller
3. Initializes the audio processing system
4. Connects MIDI input to the plugin
5. Creates the plugin editor when available
6. Routes the plugin's audio output to the selected audio device

### VST3 Editor

Plugins that provide an editor can have their GUI displayed in a separate native window.

The application also handles the editor's lifecycle when changing or unloading plugins.

### Audio Output

The audio system supports configurable output devices.

You can select the device used for VST3 audio output and configure properties such as:

- Output device
- Sample rate
- Audio buffer duration
- Volume
- Mute state

The audio engine can reconfigure the output device while the application is running.

VST3 output is handled dynamically rather than assuming a fixed stereo configuration, allowing plugins with different output channel configurations to be processed.

---

## Camera & AR

The camera feed provides the background onto which the piano visualization is projected.

The AR system uses planar calibration to determine the position and orientation of the physical piano in the camera image.

### Calibration

Press **`Choose Points`** and follow the instructions to define the piano surface.

The calibration establishes the relationship between the physical piano and the virtual piano used by the visualizer.

Additional parameters can then be adjusted to refine the projection:

- Plane width scale
- Plane height scale
- Field of view
- Camera offsets
- Perspective mapping

**`Show Debug Lines`** can be enabled to make the calibration and projection easier to inspect.

### Camera Formats

The camera system supports multiple capture formats, including:

- **NV12**
- **MJPG**

NV12 is preferred when available. If the requested NV12 configuration cannot be established, the camera system can fall back to MJPG.

MJPG frames are decoded before being uploaded to the DirectX 11 rendering pipeline, while NV12 can be processed using the GPU.

### Camera Modes

Available camera resolutions and frame rates are detected from the camera's native Media Foundation modes.

A specific camera mode can be selected, including its:

- Resolution
- Frame rate
- Pixel format

The application can also switch camera modes at runtime.

If a requested mode cannot be established, the previous working camera configuration is restored where possible.

### Camera Controls

Camera controls exposed by the device can be configured through the application when supported by the camera.

This allows supported camera properties to be adjusted without leaving the application.

---

## External Visualizer Capture

Instead of using the built-in visualizer, an external visualizer window can be captured.

To use this mode:

1. Select **`Capture Other Visualizer Window`**
2. Refresh the available windows
3. Select the visualizer window you want to capture

The captured visualizer is then integrated into the camera/AR rendering pipeline.

This makes the application compatible with visualizers other than the built-in MIDIVisualizer implementation.

---

## Rendering

The application uses **DirectX 11** for rendering.

The main rendering pipeline consists of:

```text
Camera
   │
   ▼
Camera Frame
   │
   ├── NV12 → GPU conversion
   │
   └── MJPG → CPU/WIC decoding
   │
   ▼
Camera Texture
   │
   ├───────────────┐
   │               │
   ▼               ▼
AR Projection   Visualizer
   │               │
   └───────┬───────┘
           ▼
     Final Composition
           │
           ▼
        Output
```

The visualizer is rendered into textures rather than directly rendering everything to the application's final framebuffer.

This makes it possible to compose the visualizer with the camera feed and external visualizer sources.

---

## Technologies

- **C++**
- **DirectX 11**
- **VST3 SDK**
- **Dear ImGui**
- **Windows API**
- **Windows Media Foundation**
- **Windows Imaging Component (WIC)**
- **Windows MIDI**
- **libremidi**
- **WASAPI**
- **[MIDIVisualizer](https://github.com/kosua20/MIDIVisualizer)**

---

## Usage

### 1. Calibrate the Piano

Press **`Choose Points`** and follow the instructions.

This establishes the virtual piano surface used by the AR projection.

Enable **`Show Debug Lines`** if you need to inspect the calibration.

Adjust:

- **Plane Height Scale**
- **Plane Width Scale**
- **FOV**
- **Offsets**

until the virtual piano aligns correctly with the physical piano.

### 2. Choose a Visualizer

Choose between:

- **Built-in Visualizer**
- **Capture Other Visualizer Window**

### Built-in Visualizer

1. Connect a MIDI device if one was not automatically detected.
2. Configure the visualizer and effect layers.
3. Play MIDI notes and verify that the visualization follows the performance.

### External Visualizer

1. Open the visualizer application you want to capture.
2. Refresh the available windows.
3. Select the visualizer window.
4. Verify that the captured visualization is correctly aligned with the piano.

### 3. Configure Audio

If you want to hear the piano through a VST3 instrument:

1. Drag a `.vst3` plugin or plugin folder onto the application.
2. The plugin will be loaded automatically.
3. Select the desired audio output device.
4. Configure the sample rate and buffer duration if necessary.
5. Adjust volume or mute state as required.

The incoming MIDI performance can then control both the VST3 instrument and the visualizer simultaneously.

### 4. Save Your Configuration

Remember to save your configuration after calibration and setup.

Configuration is maintained separately for the main application and the built-in MIDIVisualizer.

---

## Requirements

- Windows 10 / 11
- A MIDI input device or MIDI software
- A compatible camera for AR functionality
- A VST3 instrument/plugin for audio functionality

The repository contains the required project dependencies and configuration.

---

## Building

Clone the repository:

```bash
git clone https://github.com/Ominus-tch/Piano-Visualizer.git
```

Open the project in **Visual Studio** and build/run using the included project configuration.

---

## Development Status

The main systems are currently functional:

- ✅ DirectX 11 rendering
- ✅ MIDI input and visualization
- ✅ Built-in DirectX 11 MIDIVisualizer
- ✅ External visualizer window capture
- ✅ Camera input
- ✅ Camera perspective mapping
- ✅ Camera mode enumeration
- ✅ NV12 camera support
- ✅ MJPG camera support
- ✅ Camera mode switching
- ✅ Camera controls
- ✅ VST3 plugin loading
- ✅ VST3 MIDI routing
- ✅ VST3 audio processing
- ✅ Configurable audio output devices
- ✅ Configurable audio sample rate
- ✅ Configurable audio buffer duration
- ✅ VST3 editor windows
- ✅ VST3 drag-and-drop
- ✅ Audio diagnostics/statistics
- ✅ Camera diagnostics/statistics
- 🚧 UI improvements
- 🚧 Additional visual effects
- 🚧 Optimization and cleanup

---

## Credits

The built-in visualizer is based on the original **[MIDIVisualizer](https://github.com/kosua20/MIDIVisualizer)** project by **[kosua20](https://github.com/kosua20)**.

Piano Visualizer contains a substantially rewritten DirectX 11 implementation adapted specifically for integration with the application's camera, MIDI, and VST3 systems.

---

## License

See [LICENSE](LICENSE) for the project's license.