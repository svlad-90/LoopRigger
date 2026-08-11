# LiveLooping Core Prototype

This is the C++ starting point for replacing the FL Studio runtime with a
dedicated LiveLooping product.

The project is split into:

- `livelooping_core`: controller commands and performance state, with no GUI or
  audio dependency.
- built-in controller profile factories for the mic Kaoss Pad, synth Kaoss Pad,
  and Yaeltex LiveLooping surface.
- `livelooping_sim_cli`: local command-line simulator that can run in the Linux
  VM without real MIDI hardware.
- `livelooping_product`: optional JUCE app shell with separate Product and
  Pseudo Devices windows.

See `docs/control-surfaces.md` for the controller abstraction model and the
initial Korg Kaoss Pad / Yaeltex control groups.

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

The CLI simulator accepts direct logical commands and profile-driven pseudo
device commands:

```text
layout yaeltex
press mic preset_3
press synth page_2
press yaeltex looper_2
press yaeltex sample_length_8
press yaeltex record_t1
change yaeltex vol_pan_t3 0.42
```

Run the core smoke tests:

```sh
ctest --test-dir build --output-on-failure
```

Build the JUCE product shell. By default CMake fetches the pinned JUCE tag only
when `LIVELOOPING_BUILD_JUCE_APP=ON`:

```sh
cmake -S . -B build-juce -DLIVELOOPING_BUILD_JUCE_APP=ON
cmake --build build-juce --target livelooping_product
```

Use a local JUCE checkout instead of network fetches when needed:

```sh
cmake -S . -B build-juce \
  -DLIVELOOPING_BUILD_JUCE_APP=ON \
  -DLIVELOOPING_JUCE_DIR=/path/to/JUCE
cmake --build build-juce --target livelooping_product
```

On Linux, JUCE still needs system development packages from the distro. The
minimal set for this shell starts with `libfreetype-dev` or
`libfreetype6-dev`, `libfontconfig1-dev`, and the X11 development packages.
The later audio/VST host work will also need audio/plugin dependencies such as
`libasound2-dev`, `libjack-jackd2-dev`, and `ladspa-sdk`.

## Current Scope

This prototype does not host VST plugins yet. The first milestone is to prove
the control model:

- two Kaoss Pad roles for mic and synth input-controller presets;
- Yaeltex role for loop recording, routing, resampling, and sound manipulation;
- pseudo-device input path that uses the same command API as real MIDI.
- data-driven controller mapping from MIDI or pseudo-GUI widget events to
  logical commands.

The controller profiles are currently compiled into the core. A file-backed
profile format will be added after the action and widget model stabilizes.
