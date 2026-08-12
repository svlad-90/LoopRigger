#include "loop_rigger/scripting/PythonScriptHost.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << "\n";
    }
}

std::string fixturePath()
{
    return std::string(LIVELOOPING_SCRIPTING_FIXTURE_DIR) + "/test_device_script.py";
}

void testPythonScriptHostLoadsDevice()
{
    auto host = loop_rigger::scripting::makePythonScriptHost();
    auto loaded = host->loadDeviceScript(fixturePath());

    expect(loaded.diagnostics.empty(), "Python script should load without diagnostics");
    expect(loaded.device != nullptr, "Python script should produce a device");
    if (!loaded.device) {
        return;
    }

    expect(loaded.device->id() == "test.device", "Python device should expose DEVICE_ID");
    expect(loaded.device->displayName() == "Test Device", "Python device should expose DISPLAY_NAME");

    loop_rigger::scripting::ScriptEvent event;
    event.deviceId = "test.device";
    event.widgetId = "record_t1";
    event.type = loop_rigger::control::WidgetEventType::Press;
    event.value = 1.0F;

    const auto actions = loaded.device->handleEvent(event);
    expect(actions.size() == 1, "Python on_event should return one action");
    if (!actions.empty()) {
        expect(actions.front().command.type == loop_rigger::core::CommandType::ToggleTrackRecording, "Python action should map command type");
        expect(actions.front().command.index == 0, "Python action should carry index");
        expect(actions.front().command.value == 1.0F, "Python action should carry value");
    }
}

} // namespace

int main()
{
    testPythonScriptHostLoadsDevice();
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
