#pragma once

#include "loop_rigger/core/ControllerCommand.h"
#include "loop_rigger/core/EngineState.h"

#include <string>

namespace loop_rigger::core {

class LiveLoopingEngine {
public:
    LiveLoopingEngine();

    const EngineState& state() const;
    void handle(const ControllerCommand& command);
    std::string renderTextSnapshot() const;

private:
    EngineState state_;

    InputControllerState& inputState(InputTarget target);
    void appendEvent(std::string event);
    void resetAll();
    void resetLooper(int looperIndex);
    void stopOtherRecordings(int looperIndex, int trackIndex);
};

std::string toString(ControllerId controller);
std::string toString(InputTarget target);
std::string toString(TrackState state);
std::string toString(ResampleMode mode);

} // namespace loop_rigger::core

