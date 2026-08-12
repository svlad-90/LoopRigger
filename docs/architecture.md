# LoopRigger Architecture

LoopRigger is built as a component-oriented C++ product. Each component owns
its public component API, implementation, tests, and local documentation.
The top-level `include/loop_rigger` directory is reserved for a future SDK
facade, not for component internals.

## Component Layout

```text
components/<component>/
  api/              public API consumed by other components
  src/              component implementation
  tests/
    unit_tests/     focused tests for internal algorithms
    component_tests/ tests through the component API
  docs/             ownership notes and contracts
```

Current components:

- `core`: engine state, commands, loopers, and performance state transitions.
- `control`: controller widgets, MIDI/widget events, controller profiles, and
  event-to-command mapping.
- `profile_io`: JSON loading for controller profiles and control-surface
  layouts.
- `scripting`: Python control-adapter host for FL Studio-style device scripts.
- `virtual_devices`: device descriptors, virtual surfaces, and pseudo-device
  events independent of a concrete renderer.

Reserved components:

- `midi`: physical MIDI input/output ports, device matching, and MIDI learn.
- `audio`: realtime audio engine, loop buffers, transport, and routing.
- `plugin_host`: VST/AU plugin scanning, chains, parameters, and snapshots.
- `ui_model`: product state snapshots prepared for UI renderers.
- `juce_app`: JUCE standalone product window and pseudo-device renderer.

## Dependency Direction

The intended dependency graph is one-way:

```text
core
  <- control
  <- profile_io
  <- scripting
  <- virtual_devices
  <- midi
  <- audio
  <- plugin_host
  <- ui_model
  <- juce_app
```

More specifically:

- `core` must not depend on JUCE, Python, MIDI backends, plugin hosting, or
  file formats.
- `control` may depend on `core` command types and produces `core` commands.
- `profile_io` may depend on `control` and `core` data types, but not on JUCE.
- `scripting` is a control adapter. It may translate MIDI/widget events into
  actions, but it must not run inside realtime audio callbacks.
- `juce_app` may consume all API components, but no component should depend on
  `juce_app`.

## Python Device Scripting

Python scripting is the official adapter layer for custom controllers and
virtual devices. It follows the same role as FL Studio MIDI scripts: fast
iteration on mappings, modes, pages, modifiers, and feedback without rebuilding
the C++ product.

Python scripts are allowed to:

- describe a device and its logical controls;
- translate physical MIDI events into control actions;
- translate pseudo-device widget events into control actions;
- keep per-device mode/page/modifier state;
- drive MIDI LED or pseudo-device feedback from engine state.

Python scripts are not allowed to:

- process audio buffers;
- host or process plugins;
- run inside the audio callback;
- own timing-critical loop state.

The future event flow should be:

```text
physical MIDI event -> midi -> scripting -> control command -> core
pseudo widget event -> virtual_devices -> scripting -> control command -> core
engine state change -> scripting -> MIDI/pseudo feedback
```

The first scripting implementation should be embedded Python with a narrow C++
host API. Dynamic C++ device plugins are intentionally deferred because a
cross-platform C++ DLL/SO ABI would add packaging and compatibility cost early.

## Data Ownership

Shared data currently lives under:

```text
data/controller_profiles/
data/control_surfaces/
```

Longer term, device-owned data should move toward device packages:

```text
devices/<device-id>/
  device.json
  layout.json
  script.py
```

The C++ product should continue to support declarative-only devices where no
`script.py` is present.
