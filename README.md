# LiveLooping Core Prototype

This is the C++ starting point for replacing the FL Studio runtime with a
dedicated LiveLooping product.

The project is split into:

- `livelooping_core`: controller commands and performance state, with no GUI or
  audio dependency.
- `livelooping_sim_cli`: local command-line simulator that can run in the Linux
  VM without real MIDI hardware.
- `livelooping_product`: optional JUCE app shell with separate Product and
  Pseudo Devices windows.

The intended production stack is CMake + C++17 + JUCE. JUCE is selected because
it supports standalone audio apps, audio/MIDI device handling, plugin hosting,
and GUI on Windows, macOS, and Linux.

## Local Build

Build the core and CLI simulator:

```sh
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --target livelooping_sim_cli
./build/livelooping_sim_cli
```

Run the core smoke tests:

```sh
ctest --test-dir build --output-on-failure
```

Build the JUCE product shell after providing a local JUCE checkout:

```sh
cmake -S . -B build-juce \
  -DLIVELOOPING_BUILD_JUCE_APP=ON \
  -DLIVELOOPING_JUCE_DIR=/path/to/JUCE
cmake --build build-juce --target livelooping_product
```

## Current Scope

This prototype does not host VST plugins yet. The first milestone is to prove
the control model:

- two Kaoss Pad roles for mic and synth input-controller presets;
- Yaeltex role for loop recording, routing, resampling, and sound manipulation;
- pseudo-device input path that uses the same command API as real MIDI.
