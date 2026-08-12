#include "loop_rigger/plugin_host/JucePluginScanner.h"

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

void testEmptyScanIsValid()
{
    auto scanner = loop_rigger::plugin_host::makeJucePluginScanner();
    const auto result = scanner->scan({});

    expect(result.catalog.plugins().empty(), "empty scan should not report plugins");
    expect(result.diagnostics.empty(), "empty scan should not report diagnostics");
}

} // namespace

int main()
{
    testEmptyScanIsValid();
    return failures == 0 ? 0 : 1;
}
