# Controller Control Inventory

This document inventories the controls from the old FL Studio system and the
new hardware-shaped LoopRigger target. It is intentionally written as a
working model: each row says what is visible on the device, what the old FL
Python scripts did with the same control or concept, and what role LoopRigger
should preserve.

## Sources

Physical/control-surface references:

- Korg Kaoss Pad KP3+ reference image supplied by the user.
- Yaeltex LiveLooping high-resolution photo and red/black control diagram
  supplied by the user.
- Current LoopRigger pseudo-device layouts:
  `data/control_surfaces/kaoss_pad.json` and
  `data/control_surfaces/yaeltex_livelooping.json`.

Old FL script references:

- Input controller:
  `../../livelooping/input_controller/korg_kaoss_pad_3_plus_input_controller.py`
- Input controller constants:
  `../../livelooping/input_controller/constants.py`
- Looper mux:
  `../../livelooping/looper_mux/korg_kaoss_pad_3_plus_looper_mux.py`
- Looper mux constants:
  `../../livelooping/looper_mux/constants.py`
- Voice synth:
  `../../livelooping/voice_synth/novation_bass_station_2_voice_synth.py`

Important limitation: the old repository has FL scripts for KP3+ input
controllers, a KP3+-driven looper mux, and the Novation Bass Station voice
synth. It does not contain a separate Yaeltex FL device script. The Yaeltex
section below therefore maps the physical Yaeltex surface to the old looper
mux concepts and to the new LoopRigger target model.

## Korg Kaoss Pad KP3+ As Input Controller

There are two identical logical instances:

- mic input controller;
- synth input controller.

The same button meaning applies to both. The only difference is the target
input bus.

| Physical control | Old FL behavior | LoopRigger role |
| --- | --- | --- |
| Number `1` | Select preset slot 1 on current page. In save mode, save to slot 1. In delete mode, delete slot 1. | `input.<mic/synth>.select_preset(1)` with mode-aware save/delete actions. |
| Number `2` | Select/save/delete preset slot 2. | Same for preset 2. |
| Number `3` | Select/save/delete preset slot 3. | Same for preset 3. |
| Number `4` | Select/save/delete preset slot 4. | Same for preset 4. |
| Number `5` | Select/save/delete preset slot 5. With `Hold`, enter save mode. With `Hold` double-click, enter MIDI mapping save mode. | Preset 5 plus explicit `preset.save_mode` and `midi_learn.capture_mode`. |
| Number `6` | Select/save/delete preset slot 6. With `Hold`, enter delete mode. | Preset 6 plus explicit `preset.delete_mode`. |
| Number `7` | Select/save/delete preset slot 7. With `Hold`, switch active FX unit to previous preset. | Preset 7 plus `input.active_fx.previous_preset()`. |
| Number `8` | Select/save/delete preset slot 8. With `Hold`, switch active FX unit to next preset. | Preset 8 plus `input.active_fx.next_preset()`. |
| `Hold + 1` | Select preset page 1. | `input.select_preset_page(1)`. |
| `Hold + 2` | Select preset page 2. | `input.select_preset_page(2)`. |
| `Hold + 3` | Select preset page 3. | `input.select_preset_page(3)`. |
| `Hold + 4` | Select preset page 4. | `input.select_preset_page(4)`. |
| Level fader | Set instrument input volume. | Native input gain before looper/routing graph. |
| `Hold + Level` | Set input FX level. | Input-controller FX chain mix/send level. |
| `FX DEPTH` | Set Turnado dictator value. | Main macro for current input FX chain. |
| `Hold + FX DEPTH` | Set Turnado dry/wet value. | Dedicated dry/wet macro for current input FX chain. |
| Touch slider 1 | Set FX parameter 1. With `Hold`, next scene. | Input FX macro 1; modifier can remain scene navigation. |
| Touch slider 2 | Set FX parameter 2. With `Hold`, previous scene. | Input FX macro 2; modifier can remain scene navigation. |
| Touch slider 3 | Set FX parameter 3. With `Hold`, turn off scene or toggle loopers-sidechain mode depending on old mode path. | Input FX macro 3; modifier should become explicit scene/off or sidechain-mode action, not ambiguous. |
| Touch slider 4 | Set FX parameter 4. | Input FX macro 4. |
| Touch slider 5 | Set FX parameter 5. With `Hold`, set sidechain source level from looper track 1. | Input FX macro 5; modifier maps to sidechain source 1. |
| Touch slider 6 | Set FX parameter 6. With `Hold`, set sidechain source level from looper track 2. | Input FX macro 6; modifier maps to sidechain source 2. |
| Touch slider 7 | Set FX parameter 7. With `Hold`, set sidechain source level from looper track 3. | Input FX macro 7; modifier maps to sidechain source 3. |
| Touch slider 8 | Set FX parameter 8. With `Hold`, set sidechain source level from looper track 4. | Input FX macro 8; modifier maps to sidechain source 4. |
| Sample bank `A` | Previous Turnado preset. | `input.turnado.previous_preset()`. |
| Sample bank `B` | Next Turnado preset. | `input.turnado.next_preset()`. |
| Sample bank `C` | Toggle Turnado on/off. | `input.turnado.toggle_enabled()`. |
| Sample bank `D` | Change active FX unit. With `Hold`, reset input controller. | `input.active_fx.next_unit()` and guarded reset. |
| Program/BPM knob event | Stash or unstash sidechain source levels. | `input.sidechain.stash_or_restore_levels()`. |
| `Hold` double-click | Randomize Turnado in the old docs. | Keep as performance-randomize, but make it optional/guarded because native/plugin effects may not support randomize uniformly. |
| `SHIFT` / hold state | Mode modifier for page selection, save/delete, FX preset navigation, sidechain, scenes. | Device-script modifier state; core should receive resolved actions, not raw shift semantics. |

The important old behavior is that an input preset was not a simple plugin
preset. It was a composite snapshot: plugin parameters, activation states,
active FX unit, MIDI mappings, Turnado patch reference, routing levels, volume,
pan, and sidechain-related values.

## KP3+ As Old Looper Mux Reference

The old FL looper mux was driven by a KP3+ form factor, while the new physical
surface for this role is the Yaeltex. These old KP3+ bindings still explain
the behavior the Yaeltex must preserve or improve.

| Physical control | Old FL behavior | LoopRigger role |
| --- | --- | --- |
| `Hold + 1..4` | Select active looper instance 1..4. | Yaeltex `Looper 1..4` should become bank/looper select. |
| Numbers `1,2,4,8,16,32,64,128` | Select loop recording length in beats. | Native looper length selector. |
| `Hold + 2` double-click | Toggle 1.5x length mode. Then `2,4,8,16,32,64` become `3,6,12,24,48,96`. | Preserve non-power-of-two lengths as a first-class length mode. |
| Fractional length buttons `1/2..1/64` | Old script has fractional sample length constants. | Useful for repeater/granular/short capture modes, not main long-form phrase recording by default. |
| Sample bank `A` | Toggle recording track 1 in selected looper. | Yaeltex `Record T1`. |
| Sample bank `B` | Toggle recording track 2. | Yaeltex `Record T2`. |
| Sample bank `C` | Toggle recording track 3. | Yaeltex `Record T3`. |
| Sample bank `D` | Toggle recording track 4. | Yaeltex `Record T4`. |
| `Hold + A..D` | Clear track 1..4. Does not necessarily stop active recording. | Yaeltex `Clear T1..T4`; preserve quick retry behavior. |
| Level fader | Set selected looper volume. | Yaeltex `Vol/Pan L1..L4` or selected-bank volume. |
| Touch sliders 1..4 | Set selected looper track volumes. | Yaeltex `Vol/Pan T1..T4`, with script mode for volume vs pan/filter. |
| Double-click numbers `1..4` | Mute/unmute looper 1..4 by setting volume to 0/100. | Yaeltex `Mute L1..L4`. |
| Double-click numbers `5..8` | Mute/unmute track 1..4 in selected looper. | Yaeltex `Mute T1..T4`. |
| `Hold + 5` | Arm resample selected looper to target track. | Yaeltex `Resample L` arms source path through center FX bank. |
| `Hold + 6` | Arm resample all loopers to target track. | Yaeltex `Resample all` arms all-loopers source through center FX bank. |
| `Hold + double-click 5` | Previous looper Turnado preset. | Center FX-bank previous preset or selected FX slot preset. |
| `Hold + double-click 6` | Next looper Turnado preset. | Center FX-bank next preset. |
| `Hold + double-click 7` | Reset selected looper. | Yaeltex `Extra/Clear L` or guarded `reset current bank`. |
| `Hold + double-click 8` | Reset whole looper mux and leave playing mode. | Guarded global reset, not a casual top-level `Clear`. |
| `FX DEPTH` | Set selected looper Turnado dictator while playing; set tempo when stopped. | Separate native master tempo and FX macro controls; avoid overloading if Yaeltex has physical controls. |
| `Hold + FX DEPTH` | Set looper Turnado dry/wet. | Center FX-bank dry/wet. |
| `Hold + Level` | Mix in Drop FX. | Yaeltex `Drop`, `Vol/Drop`, and right-side drop controls. |
| `Hold + double-click 1` | Apply drop: disable Drop FX, restore drums/looper levels, turn off Turnado. | Native drop macro, scheduled to a quantized boundary where needed. |
| Touch sliders 5..8 | Set selected looper sidechain levels from looper 1 tracks. | Yaeltex sidechain `Vol/Stash` controls should be role-based, not hard-wired forever to L1T1..T4. |
| `Hold + touch sliders 1..8` | Set sidechain decay/tension pairs for looper 1 source tracks. | Native sidechain shape controls. |
| Program/BPM knob | Reset/sync FL Studio transport to beginning. | `transport.restart_from_beginning()` / DAW sync replacement. |

The old recording behavior must be preserved in the native looper:

- if recording reaches the selected length and continues, it overdubs;
- starting a different track stops the previous recording;
- selecting another looper stops active recording;
- recording into an existing track with the same length overdubs;
- recording into an existing track with a different length clears and records
  from scratch.

## Yaeltex LiveLooping Surface

Yaeltex is the new primary surface for loopers, routing, resampling, center FX
bank, remixer, and sampler. The physical labels below come from the supplied
photo/diagram. The FL meaning comes from the old looper mux script and docs.

### Session Row

| Physical control | Old FL meaning or nearest source | LoopRigger target |
| --- | --- | --- |
| `Start` | Old looper mux `start()`: enter playing mode. | Start/restart all active tracks from beginning on a quantized boundary. This is the button for "pause, perform, then everything starts from the top". |
| `Clear` | Old global clear/reset existed but was dangerous. | Prefer non-destructive pause/stop all; destructive clear should require long press, shift, or confirmation mode. |
| `Record Ses.` | Not central in old looper mux docs. | Master/session recorder arm. |
| `Stop Rec` | Old track record stop was done by pressing the same track button. | Stop current recording/overdub/resample without stopping playback. |

### Looper And Track Zone

| Physical control | Old FL meaning or nearest source | LoopRigger target |
| --- | --- | --- |
| `Looper 1` | Select looper 1. | Select bank/looper 1. Default role can be drums. |
| `Looper 2` | Select looper 2. | Select bank/looper 2. Default role can be bass/music. |
| `Looper 3` | Select looper 3. | Select bank/looper 3. Default role can be voice/lead. |
| `Looper 4` | Select looper 4. | Select bank/looper 4. Default role can be resample/scene material. |
| `Record T1..T4` | Toggle recording for track 1..4 in selected looper. | Record/overdub selected bank track 1..4. |
| `Clear T1..T4` | Clear selected looper track 1..4. | Clear active bank track 1..4; should support quick retry during recording. |
| `Select T1..T4` | Toggle track selection state in old script. | Select target track for resample/routing/group actions. |
| `Mute T1..T4` | Mute/unmute track 1..4. | Toggle mute for active bank track 1..4. |
| `Invert mute T1..T4` | Momentary inverted mute behavior in old script. | Temporary solo/hear-only gesture for active bank track 1..4. |
| `Mute L1..L4` | Mute/unmute looper 1..4. | Toggle mute for bank/looper 1..4. |
| `Invert mute L1..L4` | Momentary inverted looper mute behavior. | Temporary solo/hear-only gesture for bank/looper 1..4. |
| `Vol/Pan T1..T4` | Old separate volume, pan, HP, LP mappings existed per selected-looper track. | Track macro encoders. Primary mode: volume/pan; alternate modes: filter HP/LP or send levels. |
| `Vol/Pan L1..L4` | Old selected-looper volume plus per-looper mute. | Bank/looper volume and pan. |

### Length And Repeater Zones

| Physical control | Old FL meaning or nearest source | LoopRigger target |
| --- | --- | --- |
| Length `1` | Select 1 beat. | Main loop length 1 beat. |
| Length `2` | Select 2 beats, or 3 beats in 1.5x mode. | Main loop length 2/3 beats depending on length mode. |
| Length `4` | Select 4 beats, or 6 beats in 1.5x mode. | Main loop length 4/6 beats. |
| Length `8` | Select 8 beats, or 12 beats in 1.5x mode. | Main loop length 8/12 beats. |
| Length `16` | Select 16 beats, or 24 beats in 1.5x mode. | Main loop length 16/24 beats. |
| Length `32` | Select 32 beats, or 48 beats in 1.5x mode. | Main loop length 32/48 beats. |
| Length `64` | Select 64 beats, or 96 beats in 1.5x mode. | Main loop length 64/96 beats. |
| Length `128` | Select 128 beats. | Main loop length 128 beats. |
| Fractional `1/2..1/64` | Old script has fractional length constants. | Short capture/repeater/granular quantization modes. |
| Top-right `4,2,1,1/2,1/4,1/8,1/16,1/32` | Old repeater lengths. | Repeater gate/record/playback lengths. |
| `Extra` near length zone | Old `Extra 1` toggled an extra state. | Candidate for 1.5x length modifier or secondary length bank. |
| `Reset Vol+Pan` | Reset gesture visible on Yaeltex diagram. | Reset selected track/bank volume and pan to defaults. |

### Center FX Bank

| Physical control | Old FL meaning or nearest source | LoopRigger target |
| --- | --- | --- |
| `FX1..FX10` | Select FX slot 1..10 in current FX bank. | Select effect slot inside `yaeltex.center_fx_bank`. |
| `B1..B5` | Select FX bank 1..5. | Select center FX bank. |
| `Dry/Wet` | Set selected FX dry/wet. | Center FX-bank wet mix on the resample/performance path. |
| `LFO1 Speed` | Old center FX extra parameter or macro source. | FX macro parameter, probably animation/rate. |
| `LFO2 Speed` | Old center FX extra parameter or macro source. | FX macro parameter, probably secondary animation/rate. |
| `Drop FX App./Sel.` | Old extra parameter/drop selector. | Select/apply drop/FX macro path. |
| `Value 1` joystick | Old FX X1/Y1 pair. | Two-dimensional macro for selected FX slot. |
| `Range 1` joystick frame | Old FX X1/Y1 range. | Range/macro bank for joystick 1. |
| `Value 2` joystick | Old FX X2/Y2 pair. | Second two-dimensional macro for selected FX slot. |
| `Range 2` joystick frame | Old FX X2/Y2 range. | Range/macro bank for joystick 2. |
| `Ani.1..Ani.8` | Select FX animation 1..8. | Recall stored center-FX macro positions/animations. |
| `On/off` | Visible on Yaeltex diagram near animation controls. | Enable/disable selected center FX slot or animation. |
| `Default` | Visible on Yaeltex diagram. | Reset selected center FX slot parameters. |
| `Pr.1..Pr.6` | Visible preset buttons. | Center FX preset/macro presets. |
| `Sidechain Vol/Stash T1..T4` | Old looper/input sidechain level/stash behavior. | Sidechain amount/stash controls by source role. |
| `Sidechain Dec/Ten T1..T4` | Old sidechain decay/tension controls. | Native sidechain envelope shape controls. |

Resampling must pass through this center FX bank before capture:

```text
selected source -> routing -> yaeltex.center_fx_bank
  -> resample recorder -> loop track or sampler slot
```

### Routing And Remixer Zone

| Physical control | Old FL meaning or nearest source | LoopRigger target |
| --- | --- | --- |
| `MIC` | Activate remixer unit MIC. | Select mic as remixer/resample source. |
| `SYNTH` | Activate remixer unit SYNTH. | Select synth as remixer/resample source. |
| `LOOPER ALL` | Activate all-loopers remixer unit. | Select all loopers as source. |
| `RECORD` | Visible source/control button. | Select recording bus or arm record-from-remixer depending on mode. |
| `LOOPER` | Activate looper 1 remixer unit. | Select looper/bank 1 as source. |
| `LOOPER 2` | Activate looper 2 remixer unit. | Select looper/bank 2 as source. |
| `LOOPER 3` | Activate looper 3 remixer unit. | Select looper/bank 3 as source. |
| `LOOPER 4` | Activate looper 4 remixer unit. | Select looper/bank 4 as source. |
| `FRZ` / freeze current | Old remixer/reset/freeze-like effect group. | Momentary freeze on selected remixer unit or center FX source. |
| `Drop` | Old drop manager and remixer drop effect. | Trigger/drop macro. |
| `Extra 1..3` | Spare old extra buttons. | Mode-specific macros; keep scriptable. |
| `SEQ REC` / `STOP SEQ REC` | Visible on Yaeltex diagram. | Future sequenced performance capture. |
| `RST` / `RST all` | Reset current/all remixer or FX parameters. | Guarded reset current source/all remixer params. |
| `Gate current` / `Gate all` | Visible on Yaeltex diagram. | Gate selected source or all selected sources. |
| `CTRL all` | Visible on Yaeltex diagram. | Apply current macro/FX to all selected sources. |
| `min/maj`, `Nat.`, `Harm.`, `Melod.` | Visible harmonic mode buttons. | Pitch/harmony scale mode for remixer/pitch effects. |
| Roman `I..VIII` | Visible scale-degree buttons. | Harmonic/pitch target or scene-degree macros. |
| `Reverse` | Old remixer reverse effect. | Momentary/latching reverse for selected source. |
| `Reverb` | Old remixer reverb effect. | Enable/momentary reverb for selected source. |
| `Delay` | Old remixer delay effect. | Enable/momentary delay for selected source. |
| `Phaser` | Old remixer phaser effect. | Enable/momentary phaser for selected source. |
| `CLEAR` | Old remixer clear mode. | Clear selected sampler/remixer slot depending on active mode. |

### Master And Sampler Zone

| Physical control | Old FL meaning or nearest source | LoopRigger target |
| --- | --- | --- |
| `Master tempo/update DAW transport` | Old tempo/sync behavior; tempo change allowed only when stopped. | Native master tempo plus restart/sync action. |
| `Pitch shift` | Old remixer pitch shift level. | Pitch-shift amount for selected remixer/source or master performance bus. |
| `Master vol.` | Master volume. | Native master output gain. |
| `Pitch shift dry/wet` | Old pitch dry/wet/reset pairing. | Pitch effect mix. |
| `VOLUME` | Old remixer unit volume. | Selected remixer/source volume. |
| `Distortion` | Old remixer distortion level. | Selected remixer/source distortion amount. |
| `Pan` / `Vol/Pan L4` | Old remixer pan and looper pan roles. | Selected remixer/source pan or bank 4 pan depending on mode. |
| `Distortion dry/wet` | Old distortion/reset pairing. | Distortion effect mix. |
| Colored button `1` | Old remixer one-shot slot 1. | Sampler slot 1: play, arm record from post-FX resample bus, or clear in clear mode. |
| Colored button `2` | Old remixer one-shot slot 2. | Sampler slot 2. |
| Colored button `3` | Old remixer one-shot slot 3. | Sampler slot 3. |
| Colored button `4` | Old remixer one-shot slot 4. | Sampler slot 4. |
| Colored button `5` | Old remixer one-shot slot 5. | Sampler slot 5. |
| Colored button `6` | Old remixer one-shot slot 6. | Sampler slot 6. |
| Colored button `7` | Old remixer one-shot slot 7. | Sampler slot 7. |
| Colored button `8` | Old remixer one-shot slot 8. | Sampler slot 8. |

The sampler zone is not extra track capacity. It is a separate instrument fed
from the post-FX resample bus.

## Novation Bass Station 2 Voice Synth Reference

This controller is part of the old FL project rather than the current
Yaeltex/two-Kaoss hardware plan. It is still useful because it documents the
old voice-sampling workflows.

| Physical control | Old FL behavior | LoopRigger role |
| --- | --- | --- |
| Sub Oscillator Octave switch | Start/stop recording the crossfade loop when in crossfade-loop mode. | Future voice-synth capture control. |
| Oscillator Wave switch | Select crossfade-loop mode or one-shot sample mode; entering one-shot mode clears old one-shots. | Future voice-synth mode selector. |
| Keyboard notes | In one-shot mode, note press starts recording that note's sample; note release stops recording; later note plays it. | Sampler voice mapping, possibly separate from Yaeltex eight-slot sampler. |
| ADSR faders | Attack, decay, sustain, release for Crossfade Loop Synth. | Native/sample-synth envelope macros. |
| Distortion knob | Crossfade-loop saturation amount. | Voice-synth saturation amount. |
| Osc Filter Mod | Crossfade-loop saturation shape. | Voice-synth saturation shape. |
| Mixer Osc 1 | Crossfade-loop start point. | Sample start macro. |
| Mixer Osc 2 | Crossfade-loop end point. | Sample end macro. |
| Filter Mod Env Depth | Hard sync cycle. | Voice-synth hard-sync cycle. |
| Filter LFO 2 Depth | Hard sync detune. | Voice-synth hard-sync detune. |
| Mixer Ext/Ring/Noise | Crossfade-loop record feedback. | Voice capture feedback/layering. |
| Mixer Sub Osc | Voice-synth volume; in one-shot mode, InstantSampler volume. | Voice synth/sampler volume. |
| Filter Override | Crossfade-loop beat divisor. | Buffer-size divisor. |
| Filter Resonance | Quantized crossfade-loop beat count. | Buffer-size beats. |
| Porta Glide Time | Snap Heap dry/wet; turns Snap Heap off at zero. | Voice FX dry/wet with bypass at zero. |
| LFO1 button | Next Snap Heap preset. | Voice FX next preset. |
| LFO2 button | Previous Snap Heap preset. | Voice FX previous preset. |
| Osc Coarse | Snap Heap macro 1. | Voice FX macro 1. |
| Osc Fine | Snap Heap macro 2. | Voice FX macro 2. |
| Osc Mod Env Depth | Snap Heap macro 3. | Voice FX macro 3. |
| Osc LFO1 Depth | Snap Heap macro 4. | Voice FX macro 4. |
| Osc Pulse Width | Snap Heap macro 5. | Voice FX macro 5. |
| LFO1 knob | Snap Heap macro 6. | Voice FX macro 6. |
| LFO2 knob | Snap Heap macro 7. | Voice FX macro 7. |
| Mod wheel | Microphone pan. | Mic monitor/input pan. |
| Pitch wheel | Reset mic and synth pan to center. | Pan reset. |
| Filter Frequency | Synth pan. | Voice synth pan. |

## Migration Notes

- The new product should preserve behavior, not raw KP3+ shortcuts. Yaeltex has
  enough dedicated controls to avoid overloading `Hold` as heavily as the old
  KP3+ looper mux did.
- The two Kaoss Pads remain good input-controller surfaces: presets, pages,
  input volume, FX level, Turnado-style macros, eight FX parameters, sidechain
  source levels, and scene navigation.
- Yaeltex should own native loopers, center FX bank, source routing,
  resampling, remixer, and sampler slots.
- Resampling should always be modeled as a route through
  `yaeltex.center_fx_bank`, even when a mode bypasses or neutralizes some FX.
- Sidechain should move from fixed `looper 1 track 1..4` assumptions to named
  source roles such as kick, snare, hats, percussion, or arbitrary user
  assignments.
