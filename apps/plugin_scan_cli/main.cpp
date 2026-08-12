#include "loop_rigger/plugin_host/JucePluginScanner.h"

#include <algorithm>
#include <iostream>
#include <optional>
#include <string>

namespace {

void printUsage()
{
    std::cerr << "usage: livelooping_plugin_scan_cli [--format all|vst2|vst3|audio_unit] <plugin-path>...\n";
}

std::optional<loop_rigger::plugin_host::PluginFormat> parseFormat(const std::string& value)
{
    using loop_rigger::plugin_host::PluginFormat;

    if (value == "vst2") {
        return PluginFormat::Vst2;
    }
    if (value == "vst3") {
        return PluginFormat::Vst3;
    }
    if (value == "audio_unit" || value == "au") {
        return PluginFormat::AudioUnit;
    }
    if (value == "all") {
        return std::nullopt;
    }
    return PluginFormat::Unknown;
}

std::string printable(std::string value)
{
    std::replace(value.begin(), value.end(), '\t', ' ');
    std::replace(value.begin(), value.end(), '\n', ' ');
    std::replace(value.begin(), value.end(), '\r', ' ');
    return value;
}

} // namespace

int main(int argc, char** argv)
{
    using namespace loop_rigger::plugin_host;

    PluginScanRequest request;

    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        if (argument == "--format") {
            if (index + 1 >= argc) {
                printUsage();
                return 2;
            }

            const auto format = parseFormat(argv[++index]);
            if (format == PluginFormat::Unknown) {
                std::cerr << "unknown plugin format: " << argv[index] << "\n";
                printUsage();
                return 2;
            }
            if (format.has_value()) {
                request.formats.push_back(*format);
            }
            continue;
        }

        request.searchPaths.push_back(argument);
    }

    if (request.searchPaths.empty()) {
        printUsage();
        return 2;
    }

    auto scanner = makeJucePluginScanner();
    const auto result = scanner->scan(request);

    for (const auto& diagnostic : result.diagnostics) {
        std::cerr << toString(diagnostic.level) << ": " << diagnostic.message << "\n";
    }

    std::cout << "plugins\t" << result.catalog.plugins().size() << "\n";
    for (const auto& plugin : result.catalog.plugins()) {
        std::cout << toString(plugin.format) << "\t"
                  << printable(plugin.name) << "\t"
                  << printable(plugin.manufacturer) << "\t"
                  << printable(plugin.identifier) << "\t"
                  << printable(plugin.path) << "\n";
    }

    return 0;
}
