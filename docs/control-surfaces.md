# Control Surfaces

LoopRigger must not hard-code a specific MIDI controller into the product
logic. Physical devices, pseudo devices, keyboard shortcuts, and future OSC or
network controllers should all translate into the same logical control actions.

## Layers

```text
Physical input
  MIDI note/CC, pseudo-GUI button, keyboard shortcut
        |
Controller profile
  device layout, widgets, value transforms, modifier rules
        |
Logical action
  stable product command such as looper.select or input.set_volume
        |
Core entity
  InputController, LooperMux, Looper, Track, Remixer, FxUnit
```

The core only receives logical actions. It should not know whether an action
came from a Korg Kaoss Pad, the Yaeltex controller, or a pseudo-device window.

## Entity Actions

### Input Controller

Target instances:

- `input.mic`
- `input.synth`

Actions:

- `input.select_preset_page(page)`
- `input.select_preset(slot)`
- `input.set_volume(value)`
- `input.set_fx_level(value)`
- `input.set_fx_parameter(parameter, value)`
- `input.save_preset()`
- `input.delete_preset()`
- `input.set_active_fx_unit(unit)`
- `input.next_fx_unit_preset()`
- `input.previous_fx_unit_preset()`
- `input.set_turnado_dictator(value)`
- `input.set_turnado_dry_wet(value)`
- `input.randomize_turnado()`
- `input.next_turnado_preset()`
- `input.previous_turnado_preset()`
- `input.next_scene()`
- `input.previous_scene()`
- `input.turn_off_scene()`
- `input.set_loopers_sidechain_enabled(enabled)`
- `input.set_sidechain_source_level(source, value)`
- `input.stash_sidechain_source_levels()`

### Looper Mux

Target instance:

- `looper_mux`

Actions:

- `looper.select(looper)`
- `looper.set_sample_length(beats)`
- `track.toggle_record(track)`
- `track.clear(track)`
- `track.select(track)`
- `track.set_volume(track, value)`
- `track.set_pan(track, value)`
- `track.set_high_pass(track, value)`
- `track.set_low_pass(track, value)`
- `looper.set_volume(looper, value)`
- `looper.toggle_mute(looper)`
- `track.toggle_mute(track)`
- `looper.reset_current()`
- `looper.reset_all()`
- `resample.selected_looper()`
- `resample.all_loopers()`
- `resample.stop()`
- `routing.select_source(source)`
- `routing.select_target(target)`
- `transport.start()`
- `transport.clear()`
- `transport.stop_recording()`
- `transport.sync_master_tempo()`

### Remixer And Sound Manipulation

Target instance:

- `remixer`

Actions:

- `remixer.select_unit(unit)`
- `remixer.play_one_shot(slot)`
- `remixer.clear_slot(slot)`
- `remixer.set_range(range, value)`
- `remixer.set_range_parameter(range, parameter, value)`
- `remixer.enable_effect(effect, enabled)`
- `remixer.set_effect_parameter(effect, parameter, value)`
- `remixer.reset_effect_parameters()`

## Korg Kaoss Pad Role

LoopRigger uses two Korg Kaoss Pad 3+ devices as input-controller surfaces:

- one for microphone presets and FX parameterization;
- one for synth presets and FX parameterization.

The important physical controls are:

- number buttons `1..8`;
- sample bank buttons `A..D`;
- `HOLD`;
- `SHIFT`;
- `FX DEPTH`;
- `INPUT VOLUME`;
- vertical level fader;
- touch pad in eight-slider mode;
- `PROGRAM/BPM` knob.

The same physical layout should bind to either `input.mic` or `input.synth`
through a profile target. The profile decides whether `HOLD`, `SHIFT`, double
clicks, or alternate MIDI channels act as modifiers.

## Yaeltex Role

The Yaeltex surface is the primary performance surface for loop recording,
routing, resampling, and sound manipulation.

The high-resolution controller photo should be treated as the visual reference
for pseudo-device layout. The pseudo GUI does not need to be a photorealistic
copy, but it should preserve the same spatial zones and relative control sizes:
small rectangular buttons for discrete actions, rotary controls for continuous
parameters, the two central range joysticks, and the large colored performance
buttons in the bottom-right corner.

Initial control groups from the supplied layout:

- session controls: `Start`, `Clear`, `Record Ses.`, `Stop Rec`;
- looper selection: `Looper 1..4`;
- track recording: `Record T1..T4`;
- track clear: `Clear T1..T4`;
- track select: `Select T1..T4`;
- track mute and inverted mute: `Mute T1..T4`, `Invert mute T1..T4`;
- looper mute and inverted mute: `Mute L1..L4`, `Invert mute L1..L4`;
- sample length: `1`, `2`, `4`, `8`, `16`, `32`, `64`, `128`, plus
  fractional variants `1/2`, `1/4`, `1/8`, `1/16`, `1/32`, `1/64`;
- resampling controls: `Record`, `Resample L`, `Resample all`,
  `Extra/Clear L`;
- track knobs: `Vol/Pan T1..T4`, `Vol/Pan L1..L4`;
- drop/remix controls: `FX1..FX10`, `B1..B5`, `RESAMPLE`, `Value 1`,
  `Value 2`, `Range 1`, `Range 2`;
- source/target routing: `MIC`, `SYNTH`, `LOOPER ALL`, `LOOPER`,
  `LOOPER 2`, `LOOPER 3`, `LOOPER 4`, `RECORD`;
- effect groups: freeze, drop, gate, reverse, reverb, delay, phaser,
  pitch shift, distortion, harmonizer, melodic;
- master controls: `Master tempo/update DAW transport`, `Master vol.`,
  `VOLUME`;
- eight large numbered controls `1..8` for performance slots or banks.

These labels should become stable widget ids in a Yaeltex controller profile.
The MIDI note/CC numbers can change without changing the logical action ids.

## Profile Requirements

Each controller profile should declare:

- `id`: stable profile id;
- `display_name`: user-facing name;
- `target`: default logical target, if the whole device controls one entity;
- `widgets`: physical or pseudo controls with stable ids;
- `bindings`: widget event to logical action mapping;
- `modifiers`: hold, shift, double click, long press, alternate channel, or
  value threshold rules;
- `feedback`: optional LED/value feedback from core state back to hardware.

Pseudo-device GUI should be generated from the same `widgets` and `bindings`
model. That keeps hardware-free tests honest: the pseudo window exercises the
same action graph as the real MIDI controller.
