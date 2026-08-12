#include "loop_rigger/control/ControlMapping.h"
#include "loop_rigger/core/LiveLoopingEngine.h"

#if LIVELOOPING_HAS_PROFILE_IO
#include "loop_rigger/profile_io/ProfileLoader.h"
#endif

#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>

using loop_rigger::core::CommandType;
using loop_rigger::core::ControllerCommand;
using loop_rigger::core::ControllerId;
using loop_rigger::core::InputTarget;
using loop_rigger::core::LiveLoopingEngine;
using loop_rigger::control::MidiMapper;
using loop_rigger::control::WidgetEvent;
using loop_rigger::control::WidgetEventType;
using loop_rigger::control::makeMicKaossPadProfile;
using loop_rigger::control::makeSynthKaossPadProfile;
using loop_rigger::control::makeYaeltexLiveLoopingProfile;
using loop_rigger::control::toString;
#if LIVELOOPING_HAS_PROFILE_IO
using loop_rigger::profile_io::loadControllerProfileFromFile;
#endif

namespace {

void printHelp()
{
    std::cout
        << "Commands:\n"
        << "  mic page <1-4>\n"
        << "  mic preset <1-8>\n"
        << "  mic volume <0..1>\n"
        << "  mic fx <0..1>\n"
        << "  synth page <1-4>\n"
        << "  synth preset <1-8>\n"
        << "  yaeltex looper <1-4>\n"
        << "  yaeltex length <beats>\n"
        << "  yaeltex rec <track 1-4>\n"
        << "  yaeltex clear <track 1-4>\n"
        << "  yaeltex resample selected|all|off\n"
        << "  yaeltex reset looper|all\n"
        << "  layout\n"
        << "  layout <mic|synth|yaeltex>\n"
        << "  press <mic|synth|yaeltex> <widget-id>\n"
        << "  change <mic|synth|yaeltex> <widget-id> <0..1>\n"
        << "  show\n"
        << "  quit\n";
}

InputTarget parseInputTarget(const std::string& token)
{
    if (token == "mic") {
        return InputTarget::Mic;
    }
    if (token == "synth") {
        return InputTarget::Synth;
    }
    throw std::runtime_error("expected mic or synth");
}

void printLayout(const MidiMapper& mapper)
{
    const auto& profile = mapper.profile();
    std::cout << profile.displayName << " [" << profile.id << "]\n";
    for (const auto& widget : profile.widgets) {
        std::cout << "  " << widget.id
                  << " label=\"" << widget.label << "\""
                  << " type=" << toString(widget.type)
                  << " group=" << widget.group
                  << " pos=" << widget.row << "," << widget.column
                  << " size=" << widget.width << "x" << widget.height
                  << "\n";
    }
}

void applyMappedCommand(LiveLoopingEngine& engine, const std::optional<ControllerCommand>& command)
{
    if (!command.has_value()) {
        std::cout << "no command emitted\n";
        return;
    }
    engine.handle(command.value());
    std::cout << engine.renderTextSnapshot() << std::endl;
}

std::string profilePath(const std::string& fileName)
{
    return std::string(LIVELOOPING_PROFILE_DIR) + "/" + fileName;
}

} // namespace

int main()
{
    LiveLoopingEngine engine;
    std::map<std::string, MidiMapper> mappers;
#if LIVELOOPING_HAS_PROFILE_IO
    mappers.emplace("mic", MidiMapper(loadControllerProfileFromFile(profilePath("kaoss_mic.json"))));
    mappers.emplace("synth", MidiMapper(loadControllerProfileFromFile(profilePath("kaoss_synth.json"))));
    mappers.emplace("yaeltex", MidiMapper(loadControllerProfileFromFile(profilePath("yaeltex_livelooping.json"))));
#else
    mappers.emplace("mic", MidiMapper(makeMicKaossPadProfile()));
    mappers.emplace("synth", MidiMapper(makeSynthKaossPadProfile()));
    mappers.emplace("yaeltex", MidiMapper(makeYaeltexLiveLoopingProfile()));
#endif

    printHelp();
    std::cout << engine.renderTextSnapshot() << std::endl;

    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line)) {
        try {
            std::istringstream input(line);
            std::string device;
            input >> device;

            if (device == "quit" || device == "exit") {
                break;
            }
            if (device == "help") {
                printHelp();
                continue;
            }
            if (device == "layout") {
                std::string profileName;
                input >> profileName;
                if (profileName.empty()) {
                    for (const auto& mapper : mappers) {
                        printLayout(mapper.second);
                    }
                } else {
                    printLayout(mappers.at(profileName));
                }
                continue;
            }
            if (device == "press" || device == "change") {
                std::string profileName;
                std::string widgetId;
                input >> profileName >> widgetId;
                float value = 1.0F;
                if (device == "change") {
                    input >> value;
                }
                applyMappedCommand(
                    engine,
                    mappers.at(profileName).mapWidget({
                        widgetId,
                        device == "change" ? WidgetEventType::Change : WidgetEventType::Press,
                        value,
                    }));
                continue;
            }
            if (device == "show" || device.empty()) {
                std::cout << engine.renderTextSnapshot() << std::endl;
                continue;
            }

            std::string action;
            input >> action;

            ControllerCommand command;
            command.controller = ControllerId::PseudoGui;

            if (device == "mic" || device == "synth") {
                command.inputTarget = parseInputTarget(device);
                if (action == "page") {
                    int page = 0;
                    input >> page;
                    command.type = CommandType::SelectInputPresetPage;
                    command.index = page - 1;
                } else if (action == "preset") {
                    int preset = 0;
                    input >> preset;
                    command.type = CommandType::SelectInputPreset;
                    command.index = preset - 1;
                } else if (action == "volume") {
                    input >> command.value;
                    command.type = CommandType::SetInputVolume;
                } else if (action == "fx") {
                    input >> command.value;
                    command.type = CommandType::SetInputFxLevel;
                } else {
                    throw std::runtime_error("unknown input-controller command");
                }
            } else if (device == "yaeltex") {
                command.controller = ControllerId::Yaeltex;
                if (action == "looper") {
                    int looper = 0;
                    input >> looper;
                    command.type = CommandType::SelectLooper;
                    command.index = looper - 1;
                } else if (action == "length") {
                    input >> command.index;
                    command.type = CommandType::SelectSampleLength;
                } else if (action == "rec") {
                    int track = 0;
                    input >> track;
                    command.type = CommandType::ToggleTrackRecording;
                    command.index = track - 1;
                } else if (action == "clear") {
                    int track = 0;
                    input >> track;
                    command.type = CommandType::ClearTrack;
                    command.index = track - 1;
                } else if (action == "resample") {
                    std::string mode;
                    input >> mode;
                    if (mode == "selected") {
                        command.type = CommandType::StartResampleSelectedLooper;
                    } else if (mode == "all") {
                        command.type = CommandType::StartResampleAllLoopers;
                    } else if (mode == "off") {
                        command.type = CommandType::StopResampling;
                    } else {
                        throw std::runtime_error("unknown resample mode");
                    }
                } else if (action == "reset") {
                    std::string scope;
                    input >> scope;
                    command.type = scope == "all" ? CommandType::ResetAll : CommandType::ResetLooper;
                } else {
                    throw std::runtime_error("unknown yaeltex command");
                }
            } else {
                throw std::runtime_error("unknown device");
            }

            engine.handle(command);
            std::cout << engine.renderTextSnapshot() << std::endl;
        } catch (const std::exception& error) {
            std::cerr << "error: " << error.what() << "\n";
        }
    }

    return 0;
}
