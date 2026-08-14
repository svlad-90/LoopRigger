#!/usr/bin/env python3
"""Generate machine-aligned pseudo-device layouts and SVG previews."""

from __future__ import annotations

import html
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


ROOT = Path(__file__).resolve().parents[1]
LAYOUT_DIR = ROOT / "data" / "control_surfaces"
PREVIEW_DIR = ROOT / "docs" / "layout_previews"


@dataclass(frozen=True)
class Rect:
    x: float
    y: float
    width: float
    height: float

    @property
    def right(self) -> float:
        return self.x + self.width

    @property
    def bottom(self) -> float:
        return self.y + self.height

    def inset(self, dx: float, dy: float) -> "Rect":
        return Rect(self.x + dx, self.y + dy, self.width - 2 * dx, self.height - 2 * dy)

    def inflate(self, dx: float, dy: float) -> "Rect":
        return Rect(self.x - dx, self.y - dy, self.width + 2 * dx, self.height + 2 * dy)

    def as_json(self) -> dict[str, float]:
        return {
            "x": round(self.x, 3),
            "y": round(self.y, 3),
            "width": round(self.width, 3),
            "height": round(self.height, 3),
        }

    def overlaps(self, other: "Rect") -> bool:
        return self.x < other.right and self.right > other.x and self.y < other.bottom and self.bottom > other.y


@dataclass(frozen=True)
class Element:
    id: str
    role: str
    shape: str
    variant: str
    bounds: Rect
    label: str = ""
    widget_id: str = ""
    group: str = ""

    def as_json(self) -> dict[str, object]:
        result: dict[str, object] = {
            "id": self.id,
            "role": self.role,
            "shape": self.shape,
            "variant": self.variant,
        }
        if self.label:
            result["label"] = self.label
        if self.widget_id:
            result["widgetId"] = self.widget_id
        if self.group:
            result["group"] = self.group
        result["bounds"] = self.bounds.as_json()
        return result


@dataclass(frozen=True)
class Layout:
    id: str
    profile_id: str
    base_width: int
    base_height: int
    elements: tuple[Element, ...]

    def as_json(self) -> dict[str, object]:
        return {
            "id": self.id,
            "profileId": self.profile_id,
            "baseWidth": self.base_width,
            "baseHeight": self.base_height,
            "elements": [element.as_json() for element in self.elements],
        }


class Builder:
    def __init__(self, layout_id: str, profile_id: str, width: int, height: int) -> None:
        self.layout = Layout(layout_id, profile_id, width, height, ())
        self.elements: list[Element] = []

    def add(
        self,
        element_id: str,
        role: str,
        shape: str,
        variant: str,
        bounds: Rect,
        label: str = "",
        widget_id: str = "",
        group: str = "",
    ) -> None:
        self.elements.append(Element(element_id, role, shape, variant, bounds, label, widget_id, group))

    def decoration(self, element_id: str, shape: str, variant: str, bounds: Rect, label: str = "") -> None:
        self.add(element_id, "decoration", shape, variant, bounds, label)

    def widget(
        self,
        element_id: str,
        shape: str,
        variant: str,
        bounds: Rect,
        widget_id: str,
        group: str,
        label: str = "",
    ) -> None:
        self.add(element_id, "widget", shape, variant, bounds, label, widget_id, group)

    def finish(self) -> Layout:
        return Layout(self.layout.id, self.layout.profile_id, self.layout.base_width, self.layout.base_height, tuple(self.elements))


def grid(x: float, y: float, cell_w: float, cell_h: float, columns: int, labels: Iterable[str], gap_x: float, gap_y: float) -> list[tuple[int, Rect, str]]:
    result: list[tuple[int, Rect, str]] = []
    for index, label in enumerate(labels):
        column = index % columns
        row = index // columns
        result.append((index, Rect(x + column * (cell_w + gap_x), y + row * (cell_h + gap_y), cell_w, cell_h), label))
    return result


def add_text_grid(builder: Builder, prefix: str, x: float, y: float, columns: int, labels: list[str], cell_w: float = 54, cell_h: float = 30, gap_x: float = 16, gap_y: float = 20) -> None:
    for index, bounds, label in grid(x, y, cell_w, cell_h, columns, labels, gap_x, gap_y):
        builder.decoration(f"{prefix}_{index + 1}", "round_rect", "hardware_button", bounds, label)


def add_widget_grid(
    builder: Builder,
    prefix: str,
    widget_prefix: str,
    group: str,
    x: float,
    y: float,
    columns: int,
    labels: list[str],
    cell_w: float = 54,
    cell_h: float = 30,
    gap_x: float = 16,
    gap_y: float = 20,
    shape: str = "round_rect",
    variant: str = "interactive_button",
) -> None:
    for index, bounds, label in grid(x, y, cell_w, cell_h, columns, labels, gap_x, gap_y):
        builder.widget(f"{prefix}_{index + 1}", shape, variant, bounds, f"{widget_prefix}_{index + 1}", group, label)


def make_kaoss_layout() -> Layout:
    b = Builder("kaoss_pad_kp3", "kaoss.*", 1200, 820)
    b.decoration("body", "round_rect", "body", Rect(24, 24, 1152, 772))
    b.decoration("main_well", "rect", "inner_panel", Rect(236, 118, 728, 585))
    b.decoration("top_deck", "round_rect", "top_deck", Rect(202, 58, 796, 190))
    b.decoration("brand_korg", "text", "brand", Rect(252, 88, 160, 44), "KORG")
    b.decoration("brand_kaoss", "text", "brand_sub", Rect(446, 90, 230, 42), "KAOSS PAD")
    b.decoration("brand_kp3", "text", "brand_sub", Rect(840, 94, 110, 34), "KP3+")
    b.decoration("display", "round_rect", "display", Rect(432, 126, 160, 54))
    b.decoration("display_text", "text", "display_text", Rect(454, 136, 116, 36), "FLT.1")
    b.decoration("prog_button", "round_rect", "kaoss_button", Rect(614, 126, 72, 24), "PROG")
    b.decoration("write_button", "round_rect", "kaoss_button", Rect(696, 126, 80, 24), "WRITE")
    b.decoration("shift_button", "round_rect", "kaoss_button", Rect(696, 161, 80, 24), "SHIFT")
    b.decoration("program_memory_line", "line", "panel_line", Rect(322, 218, 828, 0))
    b.decoration("program_memory_label", "text", "panel_label", Rect(510, 196, 220, 18), "PROGRAM MEMORY")
    b.decoration("xy_pad", "rect", "xy_pad", Rect(292, 342, 560, 300))
    b.decoration("input_label", "text", "panel_label", Rect(52, 68, 126, 20), "INPUT VOLUME")
    b.decoration("input_knob", "knob", "metal_knob", Rect(76, 100, 68, 68))
    b.decoration("fx_label", "text", "panel_label", Rect(52, 204, 126, 20), "FX DEPTH")
    b.decoration("fx_knob", "knob", "metal_knob", Rect(76, 236, 68, 68))
    b.decoration("hold_button", "round_rect", "kaoss_light_button", Rect(64, 662, 94, 46), "HOLD")
    b.decoration("program_knob", "knob", "metal_knob", Rect(1046, 102, 68, 68), "PROGRAM\nBPM")
    b.decoration("tap_button", "round_rect", "kaoss_red_button", Rect(1026, 236, 78, 62), "TAP/\nRANGE")
    b.decoration("auto_bpm_button", "round_rect", "kaoss_light_button", Rect(1034, 376, 78, 44), "AUTO BPM")
    b.decoration("pad_motion_button", "round_rect", "kaoss_button", Rect(1034, 458, 78, 44), "PAD\nMOTION")
    b.decoration("mute_button", "round_rect", "kaoss_button", Rect(1034, 536, 78, 44), "MUTE")
    b.decoration("sampling_button", "round_rect", "kaoss_button", Rect(1024, 640, 92, 50), "SAMPLING")
    b.decoration("sample_bank_label", "text", "panel_label", Rect(390, 660, 420, 18), "SAMPLE BANK")

    for index, bounds, label in grid(322, 247, 68, 44, 8, [str(i) for i in range(1, 9)], 12, 0):
        b.widget(f"program_{index + 1}", "round_rect", "interactive_button", bounds, f"preset_{index + 1}", "presets", label)
    for index, bounds, label in grid(342, 712, 92, 46, 4, ["A", "B", "C", "D"], 58, 0):
        b.widget(f"sample_{index + 1}", "round_rect", "interactive_button", bounds, f"page_{index + 1}", "pages", label)
    b.widget("input_volume", "fader", "interactive_fader", Rect(94, 410, 24, 168), "input_volume", "levels")
    b.widget("fx_level", "fader", "interactive_fader", Rect(136, 410, 24, 168), "fx_level", "levels")
    return b.finish()


def make_yaeltex_layout() -> Layout:
    b = Builder("yaeltex_livelooping", "yaeltex.livelooping", 1520, 900)
    b.decoration("wood_frame", "round_rect", "wood_frame", Rect(18, 18, 1484, 864))
    b.decoration("faceplate", "rect", "faceplate", Rect(50, 50, 1420, 820))
    b.decoration("brand_main", "text", "brand", Rect(105, 82, 310, 42), "LIVELOOPING")
    b.decoration("brand_sub", "text", "brand_sub", Rect(108, 122, 150, 24), "YAELTEX")
    for name, x, y in (("tl", 66, 66), ("tr", 1454, 66), ("bl", 66, 850), ("br", 1454, 850)):
        b.decoration(f"screw_{name}", "circle", "screw", Rect(x - 8, y - 8, 16, 16))

    for element_id, bounds in (
        ("line_top", Rect(82, 178, 1356, 0)),
        ("line_middle_1", Rect(82, 330, 1356, 0)),
        ("line_middle_2", Rect(82, 470, 1356, 0)),
        ("line_left_split", Rect(610, 178, 0, 650)),
        ("line_right_split", Rect(1044, 70, 0, 758)),
    ):
        b.decoration(element_id, "line", "panel_line", bounds)

    for index, bounds, label in grid(760, 86, 54, 30, 4, ["4", "2", "1", "1/2", "1/4", "1/8", "1/16", "1/32"], 18, 20):
        b.decoration(f"top_grid_{index + 1}", "round_rect", "hardware_button", bounds, label)

    b.widget("restart_all_loopers", "round_rect", "interactive_button", Rect(330, 112, 88, 52), "restart_all_loopers", "session", "Restart")
    b.widget("reset_all", "round_rect", "interactive_button", Rect(450, 112, 76, 52), "reset_all", "session", "Clear")
    b.widget("transport_start", "round_rect", "interactive_button", Rect(550, 112, 76, 52), "transport_start", "session", "Start")
    b.widget("transport_stop", "round_rect", "interactive_button", Rect(650, 112, 76, 52), "transport_stop", "session", "Stop")

    mute_labels = [f"Mute T{i}" for i in range(1, 5)] + [f"Mute L{i}" for i in range(1, 5)] + [f"Inv T{i}" for i in range(1, 5)] + [f"Inv L{i}" for i in range(1, 5)]
    add_text_grid(b, "mute", 158, 220, 8, mute_labels, 54, 30, 18, 22)

    top_knobs = [("top_vol_drop", "Vol/Drop"), ("top_reverb", "Reverb"), ("top_transpose", "Transpose"), ("top_phaser", "Phaser")]
    for index, (element_id, label) in enumerate(top_knobs):
        b.decoration(element_id, "knob", "panel_knob", Rect(1066 + index * 106, 102, 58, 58), label)

    center_knobs = [
        ("center_fx_dry_wet", "center_fx_dry_wet", "Dry/Wet"),
        ("center_fx_lfo1_speed", "center_fx_lfo1_speed", "LFO1 Speed"),
        ("center_fx_lfo2_speed", "center_fx_lfo2_speed", "LFO2 Speed"),
        ("center_fx_drop", "center_fx_drop", "Drop FX"),
    ]
    for index, (element_id, widget_id, label) in enumerate(center_knobs):
        b.widget(element_id, "knob", "interactive_knob", Rect(654 + index * 100, 260, 50, 50), widget_id, "center_fx_parameter", label)

    source_buttons = [
        ("routing_mic", "MIC"),
        ("routing_synth", "SYNTH"),
        ("routing_looper_all", "LOOPER ALL"),
        ("routing_recording_bus", "RECORD"),
        ("routing_looper", "LOOPER"),
        ("routing_looper_2", "LOOPER 2"),
        ("routing_looper_3", "LOOPER 3"),
        ("routing_looper_4", "LOOPER 4"),
    ]
    for index, bounds, (widget_id, label) in grid(1062, 222, 80, 30, 4, source_buttons, 10, 22):
        b.widget(widget_id, "round_rect", "interactive_button", bounds, widget_id, "routing_source", label)

    fx_slots = [f"FX{i}" for i in range(1, 6)] + ["B1"] + [f"FX{i}" for i in range(6, 11)] + ["B4"]
    fx_widgets = [f"center_fx_slot_{i}" for i in range(1, 6)] + ["center_fx_bank_1"] + [f"center_fx_slot_{i}" for i in range(6, 11)] + ["center_fx_bank_4"]
    for index, bounds, label in grid(620, 356, 58, 30, 6, fx_slots, 14, 22):
        b.widget(f"fx_grid_{index + 1}", "round_rect", "interactive_button", bounds, fx_widgets[index], "center_fx_slot" if "FX" in label else "center_fx_bank", label)
    bank_extra = [("center_fx_bank_2", "B2"), ("center_fx_bank_3", "B3"), ("center_fx_bank_5", "B5")]
    for index, bounds, (widget_id, label) in grid(620, 448, 58, 30, 3, bank_extra, 14, 0):
        b.widget(f"fx_bank_extra_{index + 1}", "round_rect", "interactive_button", bounds, widget_id, "center_fx_bank", label)

    b.widget("center_fx_joystick_1", "joystick", "joystick_value_1", Rect(638, 522, 96, 96), "center_fx_joystick_1", "center_fx_joystick", "Range 1")
    b.widget("center_fx_joystick_2", "joystick", "joystick_value_2", Rect(838, 522, 96, 96), "center_fx_joystick_2", "center_fx_joystick", "Range 2")

    for index, bounds, label in grid(112, 360, 76, 64, 4, [f"Looper {i}" for i in range(1, 5)], 24, 0):
        b.widget(f"looper_{index + 1}", "round_rect", "interactive_button", bounds, f"looper_{index + 1}", "looper_select", label)
    lengths = [1, 2, 4, 8, 16, 32, 64, 128]
    for index, bounds, label in grid(112, 462, 52, 40, 4, [str(length) for length in lengths], 16, 14):
        b.widget(f"length_{label}", "round_rect", "interactive_button", bounds, f"sample_length_{label}", "sample_length", label)
    for index, bounds, label in grid(112, 560, 76, 54, 4, [f"Record T{i}" for i in range(1, 5)], 24, 0):
        b.widget(f"record_t{index + 1}", "round_rect", "interactive_button", bounds, f"record_t{index + 1}", "track_record", label)
    for index, bounds, label in grid(112, 642, 76, 48, 4, [f"Clear T{i}" for i in range(1, 5)], 24, 0):
        b.widget(f"clear_t{index + 1}", "round_rect", "interactive_button", bounds, f"clear_t{index + 1}", "track_clear", label)
    for index, bounds, label in grid(120, 732, 56, 56, 4, [f"Vol/Pan T{i}" for i in range(1, 5)], 92, 0):
        b.widget(f"vol_pan_t{index + 1}", "knob", "interactive_knob", bounds, f"vol_pan_t{index + 1}", "track_volume_pan", label)
    b.widget("resample_selected", "round_rect", "interactive_button", Rect(430, 858, 104, 30), "resample_selected", "resampling", "Resample L")
    b.widget("resample_all", "round_rect", "interactive_button", Rect(552, 858, 104, 30), "resample_all", "resampling", "Resample all")

    macro_widgets = [
        ("remixer_freeze", "FRZ"),
        ("remixer_drop", "Drop"),
        ("remixer_extra_1", "Extra"),
        ("remixer_record", "REC"),
        ("remixer_reset_current", "RST"),
        ("remixer_reset_all", "RST all"),
        ("remixer_extra_2", "Extra 2"),
        ("remixer_stop", "STOP"),
    ]
    for index, bounds, (widget_id, label) in grid(1062, 362, 48, 30, 4, macro_widgets, 12, 22):
        b.widget(widget_id, "round_rect", "interactive_button", bounds, widget_id, "remixer_macro", label)
    add_text_grid(b, "remixer_mode", 1062, 466, 4, ["Gate", "Gate all", "Extra 3", "REV", "CTRL", "min/max", "Nat.", "Reverb", "I", "V", "Harm.", "Delay", "II", "VI", "Melod.", "Phaser", "III", "VII", "CLEAR", ""], 48, 30, 12, 18)

    master_widgets = [
        ("master_tempo", "Master tempo"),
        ("master_pitch_shift", "Pitch shift"),
        ("master_volume", "Master vol."),
        ("master_pitch_dry_wet", "Pitch dry/wet"),
        ("master_output_volume", "VOLUME"),
        ("master_distortion", "Distortion"),
        ("master_pan", "Pan"),
        ("master_distortion_dry_wet", "Dist dry/wet"),
    ]
    for row in range(4):
        for column in range(2):
            index = row * 2 + column
            widget_id, label = master_widgets[index]
            b.widget(widget_id, "knob", "interactive_knob", Rect(1342 + column * 86, 330 + row * 94, 44, 44), widget_id, "master_parameter", label)

    arcade_variants = ["arcade_green", "arcade_red", "arcade_blue", "arcade_yellow"] * 2
    for index, bounds, label in grid(1078, 728, 68, 68, 4, [str(i) for i in range(1, 9)], 12, 12):
        b.widget(f"sampler_slot_{index + 1}", "round_rect", arcade_variants[index], bounds, f"sampler_slot_{index + 1}", "sampler", label)

    return b.finish()


def write_layout(layout: Layout) -> None:
    path = LAYOUT_DIR / f"{layout.id if layout.id != 'kaoss_pad_kp3' else 'kaoss_pad'}.json"
    path.write_text(json.dumps(layout.as_json(), indent=2) + "\n", encoding="utf-8")


def render_svg(layout: Layout) -> str:
    def colour(element: Element) -> str:
        if element.variant == "wood_frame":
            return "#c6904d"
        if element.variant in {"body", "top_deck", "inner_panel", "faceplate"}:
            return "#111820" if layout.id == "kaoss_pad_kp3" else "#050505"
        if element.variant.startswith("arcade_green"):
            return "#24d947"
        if element.variant.startswith("arcade_red"):
            return "#e32626"
        if element.variant.startswith("arcade_blue"):
            return "#20aeea"
        if element.variant.startswith("arcade_yellow"):
            return "#ffd21f"
        if element.role == "widget":
            return "#e9edee" if element.shape == "round_rect" else "#20262c"
        return "#22282d"

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {layout.base_width} {layout.base_height}" width="{layout.base_width}" height="{layout.base_height}">',
        '<rect width="100%" height="100%" fill="#f3f3f3"/>',
    ]
    for element in layout.elements:
        x = element.bounds.x
        y = element.bounds.y
        w = element.bounds.width
        h = element.bounds.height
        stroke = "#d00010" if element.variant == "faceplate" else ("#9ca4a8" if element.role == "widget" else "#454b4f")
        fill = colour(element)
        if element.shape in {"rect", "fader"}:
            parts.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{fill}" stroke="{stroke}" stroke-width="2"/>')
        elif element.shape == "round_rect":
            parts.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="8" fill="{fill}" stroke="{stroke}" stroke-width="2"/>')
        elif element.shape in {"circle", "knob"}:
            parts.append(f'<ellipse cx="{x + w / 2}" cy="{y + h / 2}" rx="{w / 2}" ry="{h / 2}" fill="{fill}" stroke="{stroke}" stroke-width="2"/>')
        elif element.shape == "joystick":
            parts.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="12" fill="#111" stroke="#333" stroke-width="4"/>')
        elif element.shape == "line":
            parts.append(f'<line x1="{x}" y1="{y}" x2="{x + w}" y2="{y + h}" stroke="#eee" stroke-width="2"/>')
        elif element.shape == "text":
            parts.append(f'<text x="{x}" y="{y + h * 0.75}" fill="#fff" font-size="{min(h, 34)}" font-family="Arial" font-weight="700">{html.escape(element.label)}</text>')
        if element.label and element.shape not in {"text", "knob", "fader", "joystick"}:
            parts.append(f'<text x="{x + w / 2}" y="{y + h / 2 + 4}" text-anchor="middle" fill="#111" font-size="12" font-family="Arial" font-weight="700">{html.escape(element.label)}</text>')
    parts.append("</svg>")
    return "\n".join(parts)


def validate(layout: Layout) -> list[str]:
    def visual_bounds(element: Element) -> Rect:
        if element.shape == "knob":
            # JUCE knob paint extends past the hit circle by tick marks and a label.
            return Rect(element.bounds.x - 18, element.bounds.y - 18, element.bounds.width + 36, element.bounds.height + 49)
        if element.variant.startswith("arcade_"):
            # The rendered arcade cap includes a shadow and corner number label.
            return Rect(element.bounds.x, element.bounds.y, max(element.bounds.width, 73), max(element.bounds.height, 73))
        if element.shape == "joystick":
            return Rect(element.bounds.x, element.bounds.y, element.bounds.width, element.bounds.height + 26)
        return element.bounds

    messages: list[str] = []
    for element in layout.elements:
        bounds = visual_bounds(element)
        if bounds.x < 0 or bounds.y < 0 or bounds.right > layout.base_width or bounds.bottom > layout.base_height:
            messages.append(f"{layout.id}: element outside canvas: {element.id}")
    widgets = [element for element in layout.elements if element.role == "widget"]
    for index, left in enumerate(widgets):
        for right in widgets[index + 1 :]:
            if visual_bounds(left).overlaps(visual_bounds(right)):
                messages.append(f"{layout.id}: widget overlap: {left.id} / {right.id}")
    return messages


def write_previews(layouts: Iterable[Layout]) -> None:
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)
    report_lines = ["# Layout Preview Report", ""]
    for layout in layouts:
        (PREVIEW_DIR / f"{layout.id}.svg").write_text(render_svg(layout), encoding="utf-8")
        messages = validate(layout)
        report_lines.append(f"## {layout.id}")
        report_lines.append(f"- elements: {len(layout.elements)}")
        report_lines.append(f"- widgets: {sum(1 for element in layout.elements if element.role == 'widget')}")
        report_lines.append(f"- validation: {'pass' if not messages else 'fail'}")
        for message in messages:
            report_lines.append(f"  - {message}")
        report_lines.append("")
    (PREVIEW_DIR / "README.md").write_text("\n".join(report_lines), encoding="utf-8")


def main() -> int:
    layouts = [make_kaoss_layout(), make_yaeltex_layout()]
    for layout in layouts:
        write_layout(layout)
    write_previews(layouts)
    messages = [message for layout in layouts for message in validate(layout)]
    if messages:
        print("\n".join(messages))
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
