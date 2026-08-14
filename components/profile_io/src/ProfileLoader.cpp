#include "loop_rigger/profile_io/ProfileLoader.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace loop_rigger::profile_io {

namespace {

using json = nlohmann::json;

control::WidgetType parseWidgetType(const std::string& value)
{
    if (value == "button") {
        return control::WidgetType::Button;
    }
    if (value == "knob") {
        return control::WidgetType::Knob;
    }
    if (value == "fader") {
        return control::WidgetType::Fader;
    }
    if (value == "joystick") {
        return control::WidgetType::Joystick;
    }
    throw std::runtime_error("unknown widget type: " + value);
}

control::WidgetEventType parseWidgetEventType(const std::string& value)
{
    if (value == "press") {
        return control::WidgetEventType::Press;
    }
    if (value == "release") {
        return control::WidgetEventType::Release;
    }
    if (value == "change") {
        return control::WidgetEventType::Change;
    }
    throw std::runtime_error("unknown widget event type: " + value);
}

control::MidiMessageType parseMidiMessageType(const std::string& value)
{
    if (value == "note") {
        return control::MidiMessageType::Note;
    }
    if (value == "control_change") {
        return control::MidiMessageType::ControlChange;
    }
    throw std::runtime_error("unknown MIDI message type: " + value);
}

SurfaceElementRole parseSurfaceElementRole(const std::string& value)
{
    if (value == "decoration") {
        return SurfaceElementRole::Decoration;
    }
    if (value == "widget") {
        return SurfaceElementRole::Widget;
    }
    throw std::runtime_error("unknown surface element role: " + value);
}

SurfaceElementShape parseSurfaceElementShape(const std::string& value)
{
    if (value == "rect") {
        return SurfaceElementShape::Rect;
    }
    if (value == "round_rect") {
        return SurfaceElementShape::RoundRect;
    }
    if (value == "circle") {
        return SurfaceElementShape::Circle;
    }
    if (value == "text") {
        return SurfaceElementShape::Text;
    }
    if (value == "line") {
        return SurfaceElementShape::Line;
    }
    if (value == "knob") {
        return SurfaceElementShape::Knob;
    }
    if (value == "fader") {
        return SurfaceElementShape::Fader;
    }
    if (value == "joystick") {
        return SurfaceElementShape::Joystick;
    }
    throw std::runtime_error("unknown surface element shape: " + value);
}

core::ControllerId parseControllerId(const std::string& value)
{
    if (value == "pseudo_gui") {
        return core::ControllerId::PseudoGui;
    }
    if (value == "mic_kaoss_pad") {
        return core::ControllerId::MicKaossPad;
    }
    if (value == "synth_kaoss_pad") {
        return core::ControllerId::SynthKaossPad;
    }
    if (value == "yaeltex") {
        return core::ControllerId::Yaeltex;
    }
    throw std::runtime_error("unknown controller id: " + value);
}

core::InputTarget parseInputTarget(const std::string& value)
{
    if (value == "mic") {
        return core::InputTarget::Mic;
    }
    if (value == "synth") {
        return core::InputTarget::Synth;
    }
    throw std::runtime_error("unknown input target: " + value);
}

core::CommandType parseCommandType(const std::string& value)
{
    if (value == "select_input_preset_page") {
        return core::CommandType::SelectInputPresetPage;
    }
    if (value == "select_input_preset") {
        return core::CommandType::SelectInputPreset;
    }
    if (value == "set_input_volume") {
        return core::CommandType::SetInputVolume;
    }
    if (value == "set_input_fx_level") {
        return core::CommandType::SetInputFxLevel;
    }
    if (value == "set_input_fx_parameter") {
        return core::CommandType::SetInputFxParameter;
    }
    if (value == "select_looper") {
        return core::CommandType::SelectLooper;
    }
    if (value == "select_sample_length") {
        return core::CommandType::SelectSampleLength;
    }
    if (value == "toggle_track_recording") {
        return core::CommandType::ToggleTrackRecording;
    }
    if (value == "clear_track") {
        return core::CommandType::ClearTrack;
    }
    if (value == "start_resample_selected_looper") {
        return core::CommandType::StartResampleSelectedLooper;
    }
    if (value == "start_resample_all_loopers") {
        return core::CommandType::StartResampleAllLoopers;
    }
    if (value == "stop_resampling") {
        return core::CommandType::StopResampling;
    }
    if (value == "reset_looper") {
        return core::CommandType::ResetLooper;
    }
    if (value == "reset_all") {
        return core::CommandType::ResetAll;
    }
    if (value == "set_track_volume") {
        return core::CommandType::SetTrackVolume;
    }
    if (value == "set_track_pan") {
        return core::CommandType::SetTrackPan;
    }
    if (value == "set_looper_volume") {
        return core::CommandType::SetLooperVolume;
    }
    if (value == "toggle_track_selection") {
        return core::CommandType::ToggleTrackSelection;
    }
    if (value == "start_transport") {
        return core::CommandType::StartTransport;
    }
    if (value == "stop_transport") {
        return core::CommandType::StopTransport;
    }
    if (value == "restart_all_loopers") {
        return core::CommandType::RestartAllLoopers;
    }
    if (value == "select_routing_source") {
        return core::CommandType::SelectRoutingSource;
    }
    if (value == "select_routing_target") {
        return core::CommandType::SelectRoutingTarget;
    }
    if (value == "select_center_fx_slot") {
        return core::CommandType::SelectCenterFxSlot;
    }
    if (value == "select_center_fx_bank") {
        return core::CommandType::SelectCenterFxBank;
    }
    if (value == "set_center_fx_parameter") {
        return core::CommandType::SetCenterFxParameter;
    }
    if (value == "set_center_fx_joystick") {
        return core::CommandType::SetCenterFxJoystick;
    }
    if (value == "trigger_remixer_macro") {
        return core::CommandType::TriggerRemixerMacro;
    }
    if (value == "set_master_parameter") {
        return core::CommandType::SetMasterParameter;
    }
    if (value == "trigger_sampler_slot") {
        return core::CommandType::TriggerSamplerSlot;
    }
    if (value == "clear_sampler_slot") {
        return core::CommandType::ClearSamplerSlot;
    }
    throw std::runtime_error("unknown command type: " + value);
}

control::ControllerWidget parseWidget(const json& value)
{
    control::ControllerWidget widget;
    widget.id = value.at("id").get<std::string>();
    widget.label = value.at("label").get<std::string>();
    widget.type = parseWidgetType(value.at("type").get<std::string>());
    widget.group = value.value("group", "");
    widget.row = value.value("row", 0);
    widget.column = value.value("column", 0);
    widget.width = value.value("width", 1);
    widget.height = value.value("height", 1);
    return widget;
}

control::MidiBinding parseMidiBinding(const json& value)
{
    control::MidiBinding binding;
    binding.type = parseMidiMessageType(value.at("type").get<std::string>());
    binding.channel = value.value("channel", 0);
    binding.number = value.at("number").get<int>();
    return binding;
}

core::ControllerCommand parseCommand(const json& value, core::ControllerId controller)
{
    core::ControllerCommand command;
    command.controller = controller;
    command.type = parseCommandType(value.at("type").get<std::string>());
    command.inputTarget = parseInputTarget(value.value("inputTarget", "mic"));
    command.index = value.value("index", 0);
    command.secondaryIndex = value.value("secondaryIndex", 0);
    command.value = value.value("value", 0.0F);
    return command;
}

control::ControlBinding parseBinding(const json& value, core::ControllerId controller)
{
    control::ControlBinding binding;
    binding.widgetId = value.at("widgetId").get<std::string>();
    binding.widgetEventType = parseWidgetEventType(value.value("widgetEventType", "press"));
    binding.command = parseCommand(value.at("command"), controller);
    binding.useEventValue = value.value("useEventValue", false);
    binding.triggerOnNonZero = value.value("triggerOnNonZero", true);

    if (value.contains("midi")) {
        binding.midi = parseMidiBinding(value.at("midi"));
    }

    return binding;
}

SurfaceBounds parseSurfaceBounds(const json& value)
{
    SurfaceBounds bounds;
    bounds.x = value.at("x").get<float>();
    bounds.y = value.at("y").get<float>();
    bounds.width = value.at("width").get<float>();
    bounds.height = value.at("height").get<float>();
    return bounds;
}

SurfaceElement parseSurfaceElement(const json& value)
{
    SurfaceElement element;
    element.id = value.at("id").get<std::string>();
    element.role = parseSurfaceElementRole(value.value("role", "decoration"));
    element.shape = parseSurfaceElementShape(value.at("shape").get<std::string>());
    element.widgetId = value.value("widgetId", "");
    element.label = value.value("label", "");
    element.group = value.value("group", "");
    element.variant = value.value("variant", "");
    element.bounds = parseSurfaceBounds(value.at("bounds"));
    return element;
}

std::string readTextFile(const std::string& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open file: " + path);
    }

    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

std::string resolvePackagePath(const std::filesystem::path& rootPath, const std::string& value)
{
    if (value.empty()) {
        return {};
    }

    const std::filesystem::path path(value);
    if (path.is_absolute()) {
        return path.lexically_normal().string();
    }
    return (rootPath / path).lexically_normal().string();
}

} // namespace

bool hasScript(const DevicePackageManifest& manifest)
{
    return !manifest.scriptPath.empty();
}

control::ControllerProfile loadControllerProfileFromJson(const std::string& jsonText)
{
    const auto document = json::parse(jsonText);
    control::ControllerProfile profile;
    profile.id = document.at("id").get<std::string>();
    profile.displayName = document.at("displayName").get<std::string>();
    profile.controller = parseControllerId(document.at("controller").get<std::string>());

    for (const auto& widget : document.at("widgets")) {
        profile.widgets.push_back(parseWidget(widget));
    }

    for (const auto& binding : document.at("bindings")) {
        profile.bindings.push_back(parseBinding(binding, profile.controller));
    }

    return profile;
}

control::ControllerProfile loadControllerProfileFromFile(const std::string& path)
{
    return loadControllerProfileFromJson(readTextFile(path));
}

ControlSurfaceLayout loadControlSurfaceLayoutFromJson(const std::string& jsonText)
{
    const auto document = json::parse(jsonText);
    ControlSurfaceLayout layout;
    layout.id = document.at("id").get<std::string>();
    layout.profileId = document.at("profileId").get<std::string>();
    layout.baseWidth = document.at("baseWidth").get<int>();
    layout.baseHeight = document.at("baseHeight").get<int>();

    for (const auto& element : document.at("elements")) {
        layout.elements.push_back(parseSurfaceElement(element));
    }

    return layout;
}

ControlSurfaceLayout loadControlSurfaceLayoutFromFile(const std::string& path)
{
    return loadControlSurfaceLayoutFromJson(readTextFile(path));
}

DevicePackageManifest loadDevicePackageManifestFromJson(const std::string& jsonText, const std::string& rootPath)
{
    const auto document = json::parse(jsonText);
    const std::filesystem::path packageRoot(rootPath);

    DevicePackageManifest manifest;
    manifest.id = document.at("id").get<std::string>();
    manifest.displayName = document.at("displayName").get<std::string>();
    manifest.rootPath = packageRoot.lexically_normal().string();
    manifest.controllerProfilePath = resolvePackagePath(packageRoot, document.at("controllerProfile").get<std::string>());
    manifest.controlSurfaceLayoutPath = resolvePackagePath(packageRoot, document.at("controlSurface").get<std::string>());
    manifest.scriptPath = resolvePackagePath(packageRoot, document.value("script", ""));
    return manifest;
}

DevicePackageManifest loadDevicePackageManifestFromFile(const std::string& path)
{
    const auto rootPath = std::filesystem::path(path).parent_path();
    return loadDevicePackageManifestFromJson(readTextFile(path), rootPath.string());
}

LoadedDevicePackage loadDevicePackageFromDirectory(const std::string& directoryPath)
{
    const auto manifestPath = std::filesystem::path(directoryPath) / "device.json";
    LoadedDevicePackage package;
    package.manifest = loadDevicePackageManifestFromFile(manifestPath.string());
    package.controllerProfile = loadControllerProfileFromFile(package.manifest.controllerProfilePath);
    package.controlSurfaceLayout = loadControlSurfaceLayoutFromFile(package.manifest.controlSurfaceLayoutPath);
    return package;
}

} // namespace loop_rigger::profile_io
