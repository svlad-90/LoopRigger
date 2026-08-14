#include "loop_rigger/core/LiveLoopingEngine.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace loop_rigger::core {

namespace {

int clampIndex(int value, int limit)
{
    if (value < 0 || value >= limit) {
        throw std::out_of_range("controller command index is outside the target range");
    }
    return value;
}

float clampLevel(float value)
{
    return std::clamp(value, 0.0F, 1.0F);
}

} // namespace

LiveLoopingEngine::LiveLoopingEngine()
{
    resetAll();
}

const EngineState& LiveLoopingEngine::state() const
{
    return state_;
}

void LiveLoopingEngine::handle(const ControllerCommand& command)
{
    switch (command.type) {
    case CommandType::SelectInputPresetPage: {
        auto& input = inputState(command.inputTarget);
        input.selectedPage = clampIndex(command.index, kInputPresetPages);
        appendEvent(toString(command.inputTarget) + " preset page selected: " + std::to_string(input.selectedPage + 1));
        break;
    }
    case CommandType::SelectInputPreset: {
        auto& input = inputState(command.inputTarget);
        input.selectedPreset = clampIndex(command.index, kInputPresetsPerPage);
        appendEvent(toString(command.inputTarget) + " preset selected: " + std::to_string(input.selectedPreset + 1));
        break;
    }
    case CommandType::SetInputVolume: {
        auto& input = inputState(command.inputTarget);
        input.volume = clampLevel(command.value);
        appendEvent(toString(command.inputTarget) + " volume: " + std::to_string(input.volume));
        break;
    }
    case CommandType::SetInputFxLevel: {
        auto& input = inputState(command.inputTarget);
        input.fxLevel = clampLevel(command.value);
        appendEvent(toString(command.inputTarget) + " FX level: " + std::to_string(input.fxLevel));
        break;
    }
    case CommandType::SetInputFxParameter: {
        auto& input = inputState(command.inputTarget);
        const auto parameter = clampIndex(command.index, kInputFxParameters);
        input.fxParameters[parameter] = clampLevel(command.value);
        appendEvent(toString(command.inputTarget) + " FX parameter " + std::to_string(parameter + 1) + ": " + std::to_string(input.fxParameters[parameter]));
        break;
    }
    case CommandType::SelectLooper:
        state_.selectedLooper = clampIndex(command.index, kLoopers);
        appendEvent("looper selected: " + std::to_string(state_.selectedLooper + 1));
        break;
    case CommandType::SelectSampleLength:
        state_.selectedSampleLengthBeats = std::max(1, command.index);
        appendEvent("sample length selected: " + std::to_string(state_.selectedSampleLengthBeats) + " beats");
        break;
    case CommandType::ToggleTrackRecording: {
        const auto trackIndex = clampIndex(command.index, kTracksPerLooper);
        auto& track = state_.loopers[state_.selectedLooper].tracks[trackIndex];
        if (track.state == TrackState::Recording || track.state == TrackState::Resampling) {
            track.state = TrackState::Playing;
            state_.resampleMode = ResampleMode::Off;
            appendEvent("track stopped: L" + std::to_string(state_.selectedLooper + 1) + " T" + std::to_string(trackIndex + 1));
        } else {
            stopOtherRecordings(state_.selectedLooper, trackIndex);
            track.sampleLengthBeats = state_.selectedSampleLengthBeats;
            track.state = state_.resampleMode == ResampleMode::Off ? TrackState::Recording : TrackState::Resampling;
            appendEvent("track started: L" + std::to_string(state_.selectedLooper + 1) + " T" + std::to_string(trackIndex + 1) + " as " + toString(track.state));
        }
        break;
    }
    case CommandType::ClearTrack: {
        const auto trackIndex = clampIndex(command.index, kTracksPerLooper);
        auto& track = state_.loopers[state_.selectedLooper].tracks[trackIndex];
        track = TrackStateData{};
        appendEvent("track cleared: L" + std::to_string(state_.selectedLooper + 1) + " T" + std::to_string(trackIndex + 1));
        break;
    }
    case CommandType::SetTrackVolume: {
        const auto trackIndex = clampIndex(command.index, kTracksPerLooper);
        state_.loopers[state_.selectedLooper].tracks[trackIndex].volume = clampLevel(command.value);
        appendEvent("track volume changed");
        break;
    }
    case CommandType::SetTrackPan: {
        const auto trackIndex = clampIndex(command.index, kTracksPerLooper);
        state_.loopers[state_.selectedLooper].tracks[trackIndex].pan = clampLevel(command.value);
        appendEvent("track pan changed");
        break;
    }
    case CommandType::SetLooperVolume:
        state_.loopers[state_.selectedLooper].volume = clampLevel(command.value);
        appendEvent("looper volume changed");
        break;
    case CommandType::ToggleTrackSelection: {
        const auto trackIndex = clampIndex(command.index, kTracksPerLooper);
        auto& track = state_.loopers[state_.selectedLooper].tracks[trackIndex];
        track.selected = !track.selected;
        appendEvent("track selection toggled");
        break;
    }
    case CommandType::StartTransport:
        state_.transport.playing = true;
        appendEvent("transport started");
        break;
    case CommandType::StopTransport:
        state_.transport.playing = false;
        appendEvent("transport stopped");
        break;
    case CommandType::RestartAllLoopers:
        state_.transport.playing = true;
        ++state_.transport.restartGeneration;
        appendEvent("all loopers restart queued");
        break;
    case CommandType::SelectRoutingSource:
        state_.routing.source = static_cast<RoutingSource>(clampIndex(command.index, 9));
        appendEvent("routing source: " + toString(state_.routing.source));
        break;
    case CommandType::SelectRoutingTarget:
        state_.routing.target = static_cast<RoutingTarget>(clampIndex(command.index, 4));
        appendEvent("routing target: " + toString(state_.routing.target));
        break;
    case CommandType::SelectCenterFxSlot:
        state_.centerFx.selectedSlot = clampIndex(command.index, kCenterFxSlots);
        appendEvent("center FX slot: " + std::to_string(state_.centerFx.selectedSlot + 1));
        break;
    case CommandType::SelectCenterFxBank:
        state_.centerFx.selectedBank = clampIndex(command.index, kCenterFxBanks);
        appendEvent("center FX bank: " + std::to_string(state_.centerFx.selectedBank + 1));
        break;
    case CommandType::SetCenterFxParameter: {
        const auto parameter = clampIndex(command.index, kCenterFxParameters);
        state_.centerFx.parameters[parameter] = clampLevel(command.value);
        appendEvent("center FX parameter " + std::to_string(parameter + 1) + ": " + std::to_string(state_.centerFx.parameters[parameter]));
        break;
    }
    case CommandType::SetCenterFxJoystick: {
        const auto joystick = clampIndex(command.index, kCenterFxJoysticks);
        state_.centerFx.joysticks[joystick] = clampLevel(command.value);
        appendEvent("center FX joystick " + std::to_string(joystick + 1) + ": " + std::to_string(state_.centerFx.joysticks[joystick]));
        break;
    }
    case CommandType::TriggerRemixerMacro:
        appendEvent("remixer macro: " + std::to_string(command.index + 1));
        break;
    case CommandType::SetMasterParameter: {
        const auto parameter = clampIndex(command.index, kMasterParameters);
        state_.master.parameters[parameter] = clampLevel(command.value);
        appendEvent("master parameter " + std::to_string(parameter + 1) + ": " + std::to_string(state_.master.parameters[parameter]));
        break;
    }
    case CommandType::TriggerSamplerSlot: {
        const auto slot = clampIndex(command.index, kSamplerSlots);
        state_.sampler.slots[slot].loaded = true;
        state_.sampler.slots[slot].playing = true;
        appendEvent("sampler slot triggered: " + std::to_string(slot + 1));
        break;
    }
    case CommandType::ClearSamplerSlot: {
        const auto slot = clampIndex(command.index, kSamplerSlots);
        state_.sampler.slots[slot] = SamplerSlotState{};
        appendEvent("sampler slot cleared: " + std::to_string(slot + 1));
        break;
    }
    case CommandType::StartResampleSelectedLooper:
        state_.resampleMode = ResampleMode::SelectedLooper;
        appendEvent("resample mode: selected looper");
        break;
    case CommandType::StartResampleAllLoopers:
        state_.resampleMode = ResampleMode::AllLoopers;
        appendEvent("resample mode: all loopers");
        break;
    case CommandType::StopResampling:
        state_.resampleMode = ResampleMode::Off;
        appendEvent("resample mode off");
        break;
    case CommandType::ResetLooper:
        resetLooper(state_.selectedLooper);
        appendEvent("selected looper reset");
        break;
    case CommandType::ResetAll:
        resetAll();
        appendEvent("all state reset");
        break;
    }
}

std::string LiveLoopingEngine::renderTextSnapshot() const
{
    std::ostringstream out;
    out << "LiveLooping state\n";
    out << "Mic   page=" << state_.mic.selectedPage + 1 << " preset=" << state_.mic.selectedPreset + 1
        << " vol=" << state_.mic.volume << " fx=" << state_.mic.fxLevel << "\n";
    out << "Synth page=" << state_.synth.selectedPage + 1 << " preset=" << state_.synth.selectedPreset + 1
        << " vol=" << state_.synth.volume << " fx=" << state_.synth.fxLevel << "\n";
    out << "Selected looper=" << state_.selectedLooper + 1
        << " sampleLength=" << state_.selectedSampleLengthBeats
        << " resample=" << toString(state_.resampleMode) << "\n";
    out << "Transport playing=" << (state_.transport.playing ? "yes" : "no")
        << " restartGeneration=" << state_.transport.restartGeneration << "\n";
    out << "Routing source=" << toString(state_.routing.source)
        << " target=" << toString(state_.routing.target)
        << " centerFx=slot " << state_.centerFx.selectedSlot + 1
        << "/bank " << state_.centerFx.selectedBank + 1 << "\n";

    for (int looper = 0; looper < kLoopers; ++looper) {
        out << "L" << looper + 1 << " vol=" << state_.loopers[looper].volume << ": ";
        for (int track = 0; track < kTracksPerLooper; ++track) {
            const auto& trackState = state_.loopers[looper].tracks[track];
            out << "T" << track + 1 << "=" << toString(trackState.state)
                << "/" << trackState.sampleLengthBeats << "b";
            if (trackState.selected) {
                out << "*";
            }
            out << " ";
        }
        out << "\n";
    }

    out << "Recent events:\n";
    for (const auto& event : state_.eventLog) {
        out << "  - " << event << "\n";
    }
    return out.str();
}

InputControllerState& LiveLoopingEngine::inputState(InputTarget target)
{
    return target == InputTarget::Mic ? state_.mic : state_.synth;
}

void LiveLoopingEngine::appendEvent(std::string event)
{
    state_.eventLog.push_back(std::move(event));
    if (state_.eventLog.size() > 10) {
        state_.eventLog.erase(state_.eventLog.begin());
    }
}

void LiveLoopingEngine::resetAll()
{
    state_ = EngineState{};
    state_.mic.fxParameters.fill(0.0F);
    state_.synth.fxParameters.fill(0.0F);
    for (int looper = 0; looper < kLoopers; ++looper) {
        resetLooper(looper);
    }
}

void LiveLoopingEngine::resetLooper(int looperIndex)
{
    auto& looper = state_.loopers[clampIndex(looperIndex, kLoopers)];
    looper = LooperState{};
    for (auto& track : looper.tracks) {
        track.sampleLengthBeats = state_.selectedSampleLengthBeats;
    }
}

void LiveLoopingEngine::stopOtherRecordings(int looperIndex, int trackIndex)
{
    for (int looper = 0; looper < kLoopers; ++looper) {
        for (int track = 0; track < kTracksPerLooper; ++track) {
            if (looper == looperIndex && track == trackIndex) {
                continue;
            }
            auto& candidate = state_.loopers[looper].tracks[track];
            if (candidate.state == TrackState::Recording || candidate.state == TrackState::Resampling) {
                candidate.state = TrackState::Playing;
            }
        }
    }
}

std::string toString(ControllerId controller)
{
    switch (controller) {
    case ControllerId::MicKaossPad:
        return "mic-kaoss";
    case ControllerId::SynthKaossPad:
        return "synth-kaoss";
    case ControllerId::Yaeltex:
        return "yaeltex";
    case ControllerId::PseudoGui:
        return "pseudo-gui";
    }
    return "unknown";
}

std::string toString(InputTarget target)
{
    return target == InputTarget::Mic ? "mic" : "synth";
}

std::string toString(TrackState state)
{
    switch (state) {
    case TrackState::Empty:
        return "empty";
    case TrackState::Recording:
        return "recording";
    case TrackState::Playing:
        return "playing";
    case TrackState::Resampling:
        return "resampling";
    }
    return "unknown";
}

std::string toString(ResampleMode mode)
{
    switch (mode) {
    case ResampleMode::Off:
        return "off";
    case ResampleMode::SelectedLooper:
        return "selected-looper";
    case ResampleMode::AllLoopers:
        return "all-loopers";
    }
    return "unknown";
}

std::string toString(RoutingSource source)
{
    switch (source) {
    case RoutingSource::Mic:
        return "mic";
    case RoutingSource::Synth:
        return "synth";
    case RoutingSource::SelectedLooper:
        return "selected-looper";
    case RoutingSource::Looper1:
        return "looper-1";
    case RoutingSource::Looper2:
        return "looper-2";
    case RoutingSource::Looper3:
        return "looper-3";
    case RoutingSource::Looper4:
        return "looper-4";
    case RoutingSource::AllLoopers:
        return "all-loopers";
    case RoutingSource::RecordingBus:
        return "recording-bus";
    }
    return "unknown";
}

std::string toString(RoutingTarget target)
{
    switch (target) {
    case RoutingTarget::SelectedTrack:
        return "selected-track";
    case RoutingTarget::SelectedLooper:
        return "selected-looper";
    case RoutingTarget::Sampler:
        return "sampler";
    case RoutingTarget::Master:
        return "master";
    }
    return "unknown";
}

} // namespace loop_rigger::core
