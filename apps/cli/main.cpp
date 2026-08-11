#include "livelooping/core/LiveLoopingEngine.h"

#include <iostream>
#include <sstream>
#include <string>

using livelooping::core::CommandType;
using livelooping::core::ControllerCommand;
using livelooping::core::ControllerId;
using livelooping::core::InputTarget;
using livelooping::core::LiveLoopingEngine;

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

} // namespace

int main()
{
    LiveLoopingEngine engine;
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

