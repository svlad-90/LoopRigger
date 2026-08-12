#include "loop_rigger/plugin_host/PluginCatalog.h"
#include "loop_rigger/plugin_host/PluginScanner.h"

#include <iostream>
#include <string>
#include <utility>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << "\n";
    }
}

loop_rigger::plugin_host::PluginDescription makePlugin(std::string identifier)
{
    loop_rigger::plugin_host::PluginDescription plugin;
    plugin.identifier = std::move(identifier);
    plugin.name = "Test Plugin";
    plugin.manufacturer = "LoopRigger";
    plugin.format = loop_rigger::plugin_host::PluginFormat::Vst3;
    plugin.path = "/plugins/Test.vst3";
    return plugin;
}

void testCatalogRejectsInvalidOrDuplicatePlugins()
{
    loop_rigger::plugin_host::PluginCatalog catalog;

    expect(!catalog.add({}), "catalog should reject plugins without identifiers");
    expect(catalog.add(makePlugin("plugin.one")), "catalog should accept first plugin");
    expect(!catalog.add(makePlugin("plugin.one")), "catalog should reject duplicate identifiers");
    expect(catalog.plugins().size() == 1, "catalog should keep accepted plugins only");
}

void testCatalogFindsPlugins()
{
    loop_rigger::plugin_host::PluginCatalog catalog;
    catalog.add(makePlugin("plugin.one"));

    const auto found = catalog.findByIdentifier("plugin.one");
    expect(found.has_value(), "catalog should find plugin by identifier");
    expect(found->format == loop_rigger::plugin_host::PluginFormat::Vst3, "found plugin should preserve format");

    const auto missing = catalog.findByIdentifier("missing");
    expect(!missing.has_value(), "catalog should return empty result for missing plugins");
}

void testStringNames()
{
    using loop_rigger::plugin_host::PluginFormat;
    using loop_rigger::plugin_host::PluginScanDiagnosticLevel;
    using loop_rigger::plugin_host::toString;

    expect(toString(PluginFormat::Vst3) == "vst3", "VST3 format should stringify");
    expect(toString(PluginFormat::AudioUnit) == "audio_unit", "AudioUnit format should stringify");
    expect(toString(PluginScanDiagnosticLevel::Warning) == "warning", "warning diagnostic should stringify");
}

} // namespace

int main()
{
    testCatalogRejectsInvalidOrDuplicatePlugins();
    testCatalogFindsPlugins();
    testStringNames();
    return failures == 0 ? 0 : 1;
}
