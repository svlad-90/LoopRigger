#include "loop_rigger/plugin_host/JucePluginProbe.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace {

void printUsage()
{
    std::cerr << "usage: livelooping_plugin_probe_cli [--format all|vst2|vst3|audio_unit] "
                 "[--sample-rate hz] [--block-size samples] [--max count] <plugin-path>...\n";
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

bool parseDouble(const char* text, double& output)
{
    char* end = nullptr;
    output = std::strtod(text, &end);
    return end != text && *end == '\0';
}

bool parseInt(const char* text, int& output)
{
    char* end = nullptr;
    const auto parsed = std::strtol(text, &end, 10);
    if (end == text || *end != '\0') {
        return false;
    }
    output = static_cast<int>(parsed);
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    using namespace loop_rigger::plugin_host;

    PluginProbeRequest request;

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

        if (argument == "--sample-rate") {
            if (index + 1 >= argc || !parseDouble(argv[++index], request.sampleRate) || request.sampleRate <= 0.0) {
                std::cerr << "invalid sample rate\n";
                printUsage();
                return 2;
            }
            continue;
        }

        if (argument == "--block-size") {
            if (index + 1 >= argc || !parseInt(argv[++index], request.blockSize) || request.blockSize <= 0) {
                std::cerr << "invalid block size\n";
                printUsage();
                return 2;
            }
            continue;
        }

        if (argument == "--max") {
            int maxPlugins = 0;
            if (index + 1 >= argc || !parseInt(argv[++index], maxPlugins) || maxPlugins < 0) {
                std::cerr << "invalid max plugin count\n";
                printUsage();
                return 2;
            }
            request.maxPlugins = static_cast<std::size_t>(maxPlugins);
            continue;
        }

        request.searchPaths.push_back(argument);
    }

    if (request.searchPaths.empty()) {
        printUsage();
        return 2;
    }

    auto prober = makeJucePluginProber();
    const auto result = prober->probe(request);

    for (const auto& diagnostic : result.diagnostics) {
        std::cerr << toString(diagnostic.level) << ": " << diagnostic.message << "\n";
    }

    std::cout << "probes\t" << result.reports.size() << "\n";
    for (const auto& report : result.reports) {
        std::cout << (report.instantiated ? "ok" : "failed") << "\t"
                  << toString(report.plugin.format) << "\t"
                  << printable(report.plugin.name) << "\t"
                  << printable(report.plugin.manufacturer) << "\t"
                  << report.inputChannels << "\t"
                  << report.outputChannels << "\t"
                  << report.parameterCount << "\t"
                  << report.programCount << "\t"
                  << printable(report.plugin.path) << "\t"
                  << printable(report.errorMessage) << "\n";
    }

    return 0;
}
