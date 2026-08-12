#include "loop_rigger/scripting/ScriptDevice.h"

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

void testDiagnosticNames()
{
    using loop_rigger::scripting::ScriptDiagnosticLevel;
    using loop_rigger::scripting::toString;

    expect(toString(ScriptDiagnosticLevel::Info) == "info", "info diagnostic should stringify");
    expect(toString(ScriptDiagnosticLevel::Warning) == "warning", "warning diagnostic should stringify");
    expect(toString(ScriptDiagnosticLevel::Error) == "error", "error diagnostic should stringify");
}

} // namespace

int main()
{
    testDiagnosticNames();
    return failures == 0 ? 0 : 1;
}
