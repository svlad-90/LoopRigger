# FL Studio System Analysis And Native Target

This document records what the old FL Studio project did and what LoopRigger
should become. The important architectural decision is that LoopRigger should
not rebuild the old FL mixer template as a pile of hosted helper plugins. The
base performance engine should be native. Custom VST/VST3 effects should be
insertable into the native graph, but transport, loopers, routing, resampling,
sidechain, clip scheduling, controller state, and preset orchestration belong
to LoopRigger.

## Source Material

Reference repository:

- `/home/vladyslav_goncharuk/Projects/new_dev/tasks/livelooping/dev/livelooping`
- FL project: `live_looping.flp`
- FL project size: about 2.4 MiB
- FL version string found in the file: `24.1.1.4285`

The `.flp` file is binary and was inspected through visible strings, not a
complete FLP parser. The strings still expose useful structure: plugin names,
paths, mixer/control-surface names, preset XML chunks, Edison metadata, and
named routing blocks.

Visible plugin/bundle references include:

- `Turnado.dll`
- `augustusloop_x64.dll`
- `xfadelooper_fx_x64.dll`
- `InstantSampler.dll`
- `MIDI Polysher(x64).dll`
- `FabFilter Pro-Q 3.vst3`
- `FabFilter Saturn 2.vst3`
- `Manipulator.vst3`
- `Snap Heap.vst3`
- `Trackspacer25.vst3`
- `BC Protector 2 VST3.vst3`
- `BC Connector VST3.vst3`
- `ValhallaVintageVerb.vst3`
- Waves `C6` / `H-Delay` through WaveShell VST3
- `Endless Smile 64.dll`

Visible graph/control names include:

- `Mic_FX_Input`, `Mic_FX_2_Parallel`, `Mic_FX_3_Parallel`,
  `Mic_FX_4_Parallel`, `Mic_FX_Finalize`, `Mic_FX_Output`
- `Synth_FX_Input`, `Synth_FX_1_Parallel`, `Synth_FX_3`,
  `Synth_FX_3_Parallel`, `Synth_FX_4_Parallel`, `Synth_FX_Finalize`
- `L1T1SC_Filter`, `L1T2SC_Filter`, `L1T3SC_Filter`
- `Looper_2`, `Looper_3`, `Looper_4`
- `Track_2_Snare`, `Track_3_Hats`
- `REMIXER_UNIT_FX_2`
- `OneshotReverb`

The FL project also contains many Edison recording metadata strings. That
means the file is not only an abstract routing template; it also carries saved
audio/editor state.

## Old System Shape

The old project had three logical performance devices.

### Input Controller

There were two instances:

- mic input controller
- synth input controller

Each input controller worked as a pre-looper effects processor. In FL this was
implemented as a set of mixer channels:

```text
mic:
  main=15 input=14 fx1=13 fx2=12 fx3=11 fx4=10 finalize=9 out=8

synth:
  main=24 input=23 fx1=22 fx2=21 fx3=20 fx4=19 finalize=18 out=17
```

Each controller exposed:

- 4 preset pages;
- 8 presets per page;
- 32 total presets per controller;
- input volume;
- FX level;
- Turnado dictator;
- Turnado dry/wet;
- Turnado randomize;
- Turnado preset navigation;
- 8 live FX parameters;
- sidechain source levels from looper 1 tracks;
- scene navigation backed by FL patterns;
- save/delete modes;
- custom MIDI mapping mode.

An old input-controller preset was a composite snapshot, not just a plugin
preset. It stored:

- all plugin parameters for controlled mixer channels;
- plugin mix levels;
- plugin activation states;
- mixer channel volume, pan, stereo separation;
- input routing levels to the FX channels;
- active FX unit;
- custom FX-parameter mappings;
- selected Turnado patch.

The old persistence trick used FL playlist track names as storage because the
FL Python environment could not simply write normal files.

### Looper Mux

The looper mux replaced the RC-505-like part of the performance.

It contained:

- 4 loopers;
- 4 tracks per looper;
- 16 total tracks;
- selectable active looper;
- selectable sample length;
- normal and 1.5x length modes;
- record/overdub/playback/clear;
- per-looper volume/mute;
- per-track volume/mute/pan/filter;
- selected-looper resampling;
- all-loopers resampling;
- Turnado per looper;
- global drop FX;
- repeater;
- remixer;
- DAW transport reset/sync;
- sidechain routing from looper 1 tracks.

FL mixer-channel constants show the routing template:

```text
master=0
mic_route=8
synth_route=17
recording_bus=26
recording_bus_feedback_loop=27
loopers_all_fx=30
loopers_all=31
loopers_all_without_leads=32

looper 1: looper=34 fx1=36 tracks=37..40
looper 2: looper=42 fx1=44 tracks=45..48
looper 3: looper=50 fx1=52 tracks=53..56
looper 4: looper=58 fx1=60 tracks=61..64

fx_unit_in=69
fx_unit_blocks=71..75
fx_unit_out=76
repeater=78
remixer inputs=80..86
remixer fx=87..88
```

The old actual track recording was implemented through Augustus Loop instances.
LoopRigger should not depend on that. Native loop buffers should replace this
base behavior.

### Voice Synth

The Bass Station script controlled a voice-synth subsystem:

- crossfade-loop mode using Crossfade Loop Synth;
- one-shot sample mode using InstantSampler;
- Snap Heap FX macro control;
- synth volume/pan;
- mic pan;
- record start/stop routing;
- mode switching between crossfade and one-shot workflows.

This should be treated as a secondary domain after the core looper/input
controller system is stable.

## Native Target

LoopRigger should own the following features natively.

### Native Session Graph

Replace FL mixer-channel numbers with named graph nodes and buses:

```text
hardware_input.mic
hardware_input.synth

input_controller.mic
input_controller.synth

looper[1..4]
looper[1..4].track[1..4]

recording_bus
resample_bus.source
resample_bus.post_fx
resample_bus.target
yaeltex.center_fx_bank
sampler.slot[1..8]

sidechain.looper1.track[1..4]
drop_fx
repeater
remixer
master
monitor
```

The graph should support:

- stable node IDs;
- plugin chains on selected nodes;
- native routing matrix;
- send/return levels;
- per-node mute/solo/volume/pan;
- parameter snapshots;
- deterministic offline rendering for tests.

### Native Loopers

Loopers should be implemented in C++ inside LoopRigger, not by hosting a delay
plugin. Core behavior:

- fixed beat-length buffers;
- sample-accurate write/read cursors;
- record;
- overdub;
- playback;
- clear;
- per-track volume/pan/filter;
- per-looper volume/mute;
- quantized start/stop;
- transport-aware timing;
- optional free-run mode later.

Important new feature: a track should be able to restart playback from its
beginning on command. This is not the same as unmuting: it resets the playback
cursor to clip start, then plays from there on a quantized boundary.

### Native Resampling

Resampling should become more efficient and more flexible than the old FL
routing trick.

The Yaeltex controller defines the intended performance route: resampling is
not a direct looper-to-looper copy. The selected source must pass through the
central Yaeltex FX bank before it is captured into a loop track or sampler
slot:

```text
MIC / SYNTH / selected looper / all loopers / recording bus
        |
routing matrix
        |
yaeltex.center_fx_bank
        |
resample recorder
        |
target looper track or sampler.slot[1..8]
```

The center FX bank is a native graph node that can host custom effects and
performance macros, but the resample route itself belongs to LoopRigger. The
bottom-right eight numbered Yaeltex controls are modeled as sampler or
one-shot slots fed from the post-FX resample bus, not as more looper tracks.

Required modes:

- resample selected looper into a target track;
- resample all loopers into a target track;
- resample selected tracks into a target track;
- resample selected source through the center FX bank into a sampler slot;
- resample with or without selected FX chains;
- keep source tracks;
- clear source tracks after successful capture;
- arm target and commit on a bar boundary;
- dry-run/offline render for tests.

The engine should not copy audio through arbitrary plugin-host paths when a
native buffer sum is sufficient. Internal track buffers can be mixed directly
into a target buffer, with plugin processing applied only where explicitly
requested.

### Track Orchestration

LoopRigger should make the 16 looper tracks orchestratable as a performance
surface, not only individually controllable tracks.

Target capabilities:

- named groups, for example drums, bass, leads, textures;
- actions over groups: mute, unmute, restart, clear, resample, send to FX;
- macros over arbitrary track selections;
- scene snapshots for track states;
- scheduled changes on beat/bar boundaries;
- temporary performance gestures that restore previous state on release.

This should cover the old drop/remixer workflow and extend it into a more
general arrangement layer.

### Clip Launching

The new product should support Ableton-like clip switching by bars.

Concepts:

- each looper track can hold multiple clips;
- a clip has a length in beats/bars;
- each track has one active clip and optionally one queued clip;
- launching a clip schedules it for the next quantization boundary;
- clip launch quantization can be 1 beat, 1 bar, 2 bars, 4 bars, or custom;
- stop, restart, and clear can be scheduled in the same way;
- a scene can launch a set of clips together.

This is one of the biggest advantages over the old FL setup. The old system had
one live buffer per track. LoopRigger should grow toward multiple clips per
track without losing the direct KP3/Yaeltex-style live controls.

### Native Sidechain

The old sidechain depended on FL Peak Controller, filters, and Trackspacer-like
behavior. LoopRigger should own the envelope/control layer:

- envelope followers for selected source tracks;
- frequency bands for kick/snare/hats-like sources;
- decay/tension/shape controls;
- destination maps for input controllers and loopers;
- native gain ducking;
- optional external sidechain-capable VST insertion later.

Native sidechain makes the system portable and testable without relying on FL
control surfaces.

### Native Presets And Snapshots

Input-controller presets should become explicit project data:

```text
Preset
  plugin chain definitions
  plugin parameter snapshots
  plugin bypass/mix states
  routing levels
  macro mappings
  active FX unit
  Turnado/custom-plugin patch reference
  scenes/automation references
```

Unlike the old FL track-name storage, this should be normal JSON or a compact
project file under LoopRigger control.

### Plugin Hosting Role

VST/VST3 plugins should be custom inserts, not the foundation of the transport.

Allowed plugin roles:

- input-controller color/effects;
- looper insert effects;
- send effects;
- drop effects;
- synth effects;
- mastering/monitoring;
- experimental clip/track processors.

Native roles:

- audio device I/O;
- transport;
- clock;
- loop buffers;
- clip launcher;
- resampling;
- routing;
- gain/pan/mute;
- sidechain envelope/control;
- controller state machine;
- project/session persistence.

## Controller Target

Physical MIDI devices and pseudo devices should feed the same command layer.

The old setup was KP3+ and Bass Station specific, but the target should be:

- logical command IDs are stable;
- device profiles bind MIDI/widgets to command IDs;
- Python scripts can add modes, pages, modifiers, and gestures;
- pseudo GUI emits the same widget events as a MIDI controller;
- MIDI learn can bind any controller to the same command layer.

This supports the current Yaeltex + two Kaoss pads setup and keeps the product
open to other controllers.

## Migration Plan

Phase 1: documentation and domain model.

- Keep this document updated as the source-of-truth for old FL behavior.
- Add project/session data types for named buses, loopers, tracks, clips,
  plugin chains, and controllers.
- Expand command vocabulary to cover old input-controller, looper-mux, and new
  orchestration commands.

Phase 2: native looper and offline graph.

- Implement native track buffers.
- Implement offline graph rendering.
- Add record/play/clear/overdub tests without audio hardware.
- Add restart-from-beginning behavior.
- Add simple resampling selected tracks into a target track.

Phase 3: realtime shell.

- Add JUCE audio device selection.
- Run native graph in realtime.
- Add MIDI input adapter.
- Route pseudo GUI and MIDI to the same commands.

Phase 4: plugin inserts.

- Load VST3 plugins into graph nodes.
- Store and restore plugin snapshots.
- Support input-controller preset chains.
- Validate with deterministic fixture first, then real plugins.

Phase 5: clip scenes and orchestration.

- Add multiple clips per track.
- Add quantized clip launching.
- Add scene launch across tracks.
- Add group/macros for drums, bass, leads, textures.

Phase 6: advanced performance features.

- Native sidechain engine.
- Drop/remixer generalized into orchestration macros.
- More efficient resampling paths.
- Voice-synth replacement or integration.

## Open Questions

- Should the first native looper support only stereo tracks, or should track
  channel count be configurable from the beginning?
- Should clip launching be built into the first looper API, or added after one
  buffer-per-track works reliably?
- Should plugin snapshots be stored as normalized parameter values only, or
  should plugin state chunks be stored when available?
- Which parts of the voice synth should be native, and which can remain hosted
  plugin workflows?
- Do we want to import any data from `live_looping.flp`, or only use it as a
  behavioral reference?
