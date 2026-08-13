#include "loop_rigger/plugin_host/JucePluginProbe.h"

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

void testEmptyProbeIsValid()
{
    auto prober = loop_rigger::plugin_host::makeJucePluginProber();
    const auto result = prober->probe({});

    expect(result.reports.empty(), "empty probe should not report plugins");
    expect(result.diagnostics.empty(), "empty probe should not report diagnostics");
}

} // namespace

int main()
{
    testEmptyProbeIsValid();
    return failures == 0 ? 0 : 1;
}
