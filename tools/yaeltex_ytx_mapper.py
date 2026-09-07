#!/usr/bin/env python3
"""Synchronize LoopRigger Yaeltex profile MIDI bindings with a Yaeltex .ytx export."""

from __future__ import annotations

import argparse
import copy
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PROFILE = ROOT / "data" / "controller_profiles" / "yaeltex_livelooping.json"


@dataclass(frozen=True)
class HardwareMapping:
    widget_id: str
    command_type: str
    kind: str
    index: int
    config_key: str


def cc_to_message_type(message: int) -> str:
    if message == 2:
        return "control_change"
    if message == 1:
        return "note"
    raise ValueError(f"unsupported Yaeltex message id: {message}")


def message_type_to_cc(message_type: str) -> int:
    if message_type == "control_change":
        return 2
    if message_type == "note":
        return 1
    raise ValueError(f"unsupported profile MIDI type: {message_type}")


def digital_index(channel: int, cc: int) -> int:
    if channel == 0 and 32 <= cc <= 127:
        return cc - 31
    if channel == 1 and 0 <= cc <= 47:
        return cc + 97
    raise ValueError(f"no default digital index for channel {channel}, CC {cc}")


def encoder_index(cc: int) -> int:
    if 0 <= cc <= 31:
        return cc + 1
    raise ValueError(f"no default encoder index for CC {cc}")


def analog_index(cc: int) -> int:
    if cc in (32, 33, 34, 35):
        return cc - 31
    raise ValueError(f"no default analog index for CC {cc}")


def mappings() -> list[HardwareMapping]:
    result: list[HardwareMapping] = []

    def digital(widget_id: str, command_type: str, channel: int, cc: int) -> None:
        result.append(HardwareMapping(widget_id, command_type, "digital", digital_index(channel, cc), "action_config"))

    def encoder(widget_id: str, command_type: str, cc: int, config_key: str = "rotary_config") -> None:
        result.append(HardwareMapping(widget_id, command_type, "encoder", encoder_index(cc), config_key))

    def analog(widget_id: str, command_type: str, cc: int) -> None:
        result.append(HardwareMapping(widget_id, command_type, "analog", analog_index(cc), "message_config"))

    digital("transport_start", "start_transport", 0, 32)
    digital("reset_all", "reset_all", 0, 33)

    for index in range(4):
        digital(f"looper_{index + 1}", "select_looper", 0, 60 + index)
        digital(f"record_t{index + 1}", "toggle_track_recording", 1, 8 + index)
        digital(f"clear_t{index + 1}", "clear_track", 1, 12 + index)
        digital(f"select_t{index + 1}", "toggle_track_selection", 1, 16 + index)
        encoder(f"vol_pan_t{index + 1}", "set_track_volume", 16 + index)
        encoder(f"vol_pan_t{index + 1}", "set_track_pan", 16 + index, "switch_config")
        encoder(f"vol_pan_l{index + 1}", "set_looper_volume", 24 + index)
        encoder(f"sidechain_stash_t{index + 1}", "set_sidechain_parameter", 20 + index)
        encoder(f"sidechain_decay_t{index + 1}", "set_sidechain_parameter", 28 + index)

    for offset, length in enumerate((1, 2, 4, 8, 16, 32, 64, 128)):
        digital(f"sample_length_{length}", "select_sample_length", 0, 72 + offset)

    for offset in range(8):
        digital(f"top_grid_{offset + 1}", "select_clock_division", 0, 36 + offset)
        digital(f"animation_{offset + 1}", "trigger_animation_slot", 1, 20 + offset)
        digital(f"sampler_slot_{offset + 1}", "trigger_sampler_slot", 1, 40 + offset)

    for offset in range(4):
        digital(f"mute_{offset + 1}", "toggle_track_mute", 0, 44 + offset)
        digital(f"mute_{offset + 5}", "toggle_looper_mute", 0, 52 + offset)
        digital(f"mute_{offset + 9}", "toggle_track_invert", 0, 48 + offset)
        digital(f"mute_{offset + 13}", "toggle_looper_invert", 0, 56 + offset)

    routing = (
        ("routing_mic", 64),
        ("routing_synth", 65),
        ("routing_looper_all", 66),
        ("routing_recording_bus", 67),
        ("routing_looper", 68),
        ("routing_looper_2", 69),
        ("routing_looper_3", 70),
        ("routing_looper_4", 71),
    )
    for widget_id, cc in routing:
        digital(widget_id, "select_routing_source", 0, cc)

    for slot in range(10):
        digital(f"center_fx_slot_{slot + 1}", "select_center_fx_slot", 0, 88 + slot if slot < 5 else 96 + slot - 5)
    for bank in range(5):
        digital(f"center_fx_bank_{bank + 1}", "select_center_fx_bank", 0, 93 + bank if bank < 3 else 101 + bank - 3)

    for offset, widget_id in enumerate(("top_vol_drop", "top_reverb", "top_transpose", "top_phaser")):
        encoder(widget_id, "set_top_fx_parameter", offset)
    for offset, widget_id in enumerate(("center_fx_dry_wet", "center_fx_lfo1_speed", "center_fx_lfo2_speed", "center_fx_drop")):
        encoder(widget_id, "set_center_fx_parameter", 4 + offset)

    master = (
        ("master_tempo", 8),
        ("master_output_volume", 9),
        ("master_volume", 10),
        ("master_pan", 11),
        ("master_pitch_shift", 12),
        ("master_distortion", 13),
        ("master_pitch_dry_wet", 14),
        ("master_distortion_dry_wet", 15),
    )
    for widget_id, cc in master:
        encoder(widget_id, "set_master_parameter", cc)

    analog("center_fx_joystick_1", "set_center_fx_joystick", 32)
    analog("center_fx_joystick_2", "set_center_fx_joystick", 34)

    for mode in range(24):
        digital(f"remixer_mode_{mode + 1}", "trigger_remixer_mode", 0, 104 + mode)

    return result


def read_json(path: Path) -> dict[str, Any]:
    with path.open(encoding="utf-8") as handle:
        return json.load(handle)


def write_json(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2)
        handle.write("\n")


def bank_zero(ytx: dict[str, Any]) -> dict[str, Any]:
    try:
        return ytx["banks"]["0"]
    except KeyError as exc:
        raise ValueError("Yaeltex export must contain banks.0") from exc


def ytx_config(ytx: dict[str, Any], mapping: HardwareMapping) -> dict[str, Any]:
    bank = bank_zero(ytx)
    collection_name = {
        "digital": "digitals",
        "encoder": "encoders",
        "analog": "analogs",
    }[mapping.kind]
    try:
        return bank[collection_name][str(mapping.index - 1)][mapping.config_key]
    except KeyError as exc:
        raise ValueError(f"missing {mapping.kind} {mapping.index} {mapping.config_key}") from exc


def midi_from_ytx(ytx: dict[str, Any], mapping: HardwareMapping) -> dict[str, int | str]:
    config = ytx_config(ytx, mapping)
    return {
        "type": cc_to_message_type(int(config["message"])),
        "channel": int(config["channel"]),
        "number": int(config["parameter"]),
    }


def update_ytx_config(config: dict[str, Any], midi: dict[str, Any]) -> None:
    config["message"] = message_type_to_cc(str(midi["type"]))
    config["channel"] = int(midi["channel"])
    config["parameter"] = int(midi["number"])
    if "parameter_lsb" in config:
        config["parameter_lsb"] = int(midi["number"])
    if "parameter_msb" in config:
        config["parameter_msb"] = 0


def update_feedback_config(container: dict[str, Any], mapping: HardwareMapping, midi: dict[str, Any]) -> None:
    if mapping.kind == "encoder" and mapping.config_key != "rotary_config":
        return
    feedback = container.get("feedback_config")
    if not isinstance(feedback, dict):
        feedback = container.get("rotation_feedback_config")
    if not isinstance(feedback, dict):
        return
    feedback["message"] = message_type_to_cc(str(midi["type"]))
    feedback["channel"] = int(midi["channel"])
    feedback["parameter"] = int(midi["number"])
    if "parameter_lsb" in feedback:
        feedback["parameter_lsb"] = int(midi["number"])
    if "parameter_msb" in feedback:
        feedback["parameter_msb"] = 0


def profile_bindings(profile: dict[str, Any]) -> dict[tuple[str, str], list[dict[str, Any]]]:
    result: dict[tuple[str, str], list[dict[str, Any]]] = {}
    for binding in profile.get("bindings", []):
        command = binding.get("command", {})
        key = (binding.get("widgetId", ""), command.get("type", ""))
        result.setdefault(key, []).append(binding)
    return result


def binding_for(mapping: HardwareMapping, bindings: dict[tuple[str, str], list[dict[str, Any]]]) -> dict[str, Any] | None:
    candidates = bindings.get((mapping.widget_id, mapping.command_type), [])
    if not candidates:
        return None
    return candidates[0]


def import_profile(ytx: dict[str, Any], profile: dict[str, Any]) -> tuple[dict[str, Any], list[str]]:
    updated = copy.deepcopy(profile)
    bindings = profile_bindings(updated)
    warnings: list[str] = []
    for mapping in mappings():
        binding = binding_for(mapping, bindings)
        if binding is None:
            warnings.append(f"missing profile binding for {mapping.widget_id}/{mapping.command_type}")
            continue
        binding["midi"] = midi_from_ytx(ytx, mapping)
    return updated, warnings


def export_ytx(profile: dict[str, Any], ytx: dict[str, Any]) -> tuple[dict[str, Any], list[str]]:
    updated = copy.deepcopy(ytx)
    bindings = profile_bindings(profile)
    warnings: list[str] = []
    bank = bank_zero(updated)
    collection_names = {"digital": "digitals", "encoder": "encoders", "analog": "analogs"}

    for mapping in mappings():
        binding = binding_for(mapping, bindings)
        if binding is None:
            warnings.append(f"missing profile binding for {mapping.widget_id}/{mapping.command_type}")
            continue
        midi = binding.get("midi")
        if not isinstance(midi, dict):
            warnings.append(f"profile binding has no MIDI data: {mapping.widget_id}/{mapping.command_type}")
            continue

        collection = bank[collection_names[mapping.kind]]
        container = collection[str(mapping.index - 1)]
        update_ytx_config(container[mapping.config_key], midi)
        update_feedback_config(container, mapping, midi)

    return updated, warnings


def check_profile(ytx: dict[str, Any], profile: dict[str, Any]) -> tuple[list[str], list[str]]:
    bindings = profile_bindings(profile)
    mismatches: list[str] = []
    warnings: list[str] = []
    for mapping in mappings():
        binding = binding_for(mapping, bindings)
        if binding is None:
            warnings.append(f"missing profile binding for {mapping.widget_id}/{mapping.command_type}")
            continue
        midi = binding.get("midi")
        if not isinstance(midi, dict):
            warnings.append(f"profile binding has no MIDI data: {mapping.widget_id}/{mapping.command_type}")
            continue
        actual = midi_from_ytx(ytx, mapping)
        expected = {key: midi[key] for key in ("type", "channel", "number")}
        if actual != expected:
            mismatches.append(f"{mapping.widget_id}/{mapping.command_type}: profile {expected} != ytx {actual}")
    return mismatches, warnings


def print_inspection(ytx: dict[str, Any]) -> None:
    hwconfig = ytx.get("hwconfig", {})
    bank = bank_zero(ytx)
    print(f"device_name: {hwconfig.get('device_name', '<unknown>')}")
    print(f"banks: {len(ytx.get('banks', {}))}")
    print(f"digitals: {len(bank.get('digitals', {}))}")
    print(f"encoders: {len(bank.get('encoders', {}))}")
    print(f"analogs: {len(bank.get('analogs', {}))}")
    print(f"semantic mappings: {len(mappings())}")


def cmd_inspect(args: argparse.Namespace) -> int:
    print_inspection(read_json(args.ytx))
    return 0


def cmd_check(args: argparse.Namespace) -> int:
    mismatches, warnings = check_profile(read_json(args.ytx), read_json(args.profile))
    for warning in warnings:
        print(f"warning: {warning}", file=sys.stderr)
    for mismatch in mismatches:
        print(f"mismatch: {mismatch}", file=sys.stderr)
    print(f"checked {len(mappings())} Yaeltex mappings: {len(mismatches)} mismatch(es), {len(warnings)} warning(s)")
    return 1 if mismatches else 0


def cmd_import_profile(args: argparse.Namespace) -> int:
    profile, warnings = import_profile(read_json(args.ytx), read_json(args.profile))
    for warning in warnings:
        print(f"warning: {warning}", file=sys.stderr)
    write_json(args.output, profile)
    print(f"wrote profile: {args.output}")
    return 0


def cmd_export_ytx(args: argparse.Namespace) -> int:
    ytx, warnings = export_ytx(read_json(args.profile), read_json(args.ytx))
    for warning in warnings:
        print(f"warning: {warning}", file=sys.stderr)
    write_json(args.output, ytx)
    print(f"wrote Yaeltex export: {args.output}")
    return 0


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    subcommands = result.add_subparsers(required=True)

    inspect_parser = subcommands.add_parser("inspect", help="summarize a Yaeltex .ytx JSON export")
    inspect_parser.add_argument("ytx", type=Path)
    inspect_parser.set_defaults(func=cmd_inspect)

    check_parser = subcommands.add_parser("check", help="compare profile MIDI bindings with a .ytx export")
    check_parser.add_argument("--ytx", type=Path, required=True)
    check_parser.add_argument("--profile", type=Path, default=DEFAULT_PROFILE)
    check_parser.set_defaults(func=cmd_check)

    import_parser = subcommands.add_parser("import-profile", help="write profile MIDI bindings from a .ytx export")
    import_parser.add_argument("--ytx", type=Path, required=True)
    import_parser.add_argument("--profile", type=Path, default=DEFAULT_PROFILE)
    import_parser.add_argument("--output", type=Path, default=DEFAULT_PROFILE)
    import_parser.set_defaults(func=cmd_import_profile)

    export_parser = subcommands.add_parser("export-ytx", help="write a .ytx export from profile MIDI bindings")
    export_parser.add_argument("--profile", type=Path, default=DEFAULT_PROFILE)
    export_parser.add_argument("--ytx", type=Path, required=True)
    export_parser.add_argument("--output", type=Path, required=True)
    export_parser.set_defaults(func=cmd_export_ytx)

    return result


def main() -> int:
    args = parser().parse_args()
    try:
        return int(args.func(args))
    except (OSError, ValueError, KeyError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
