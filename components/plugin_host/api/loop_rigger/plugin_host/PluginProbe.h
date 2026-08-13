#pragma once

#include "loop_rigger/plugin_host/PluginCatalog.h"
#include "loop_rigger/plugin_host/PluginScanner.h"

#include <cstddef>
#include <string>
#include <vector>

namespace loop_rigger::plugin_host {

struct PluginProbeRequest {
    std::vector<std::string> searchPaths;
    std::vector<PluginFormat> formats;
    double sampleRate = 44100.0;
    int blockSize = 512;
    std::size_t maxPlugins = 16;
};

struct PluginProbeReport {
    PluginDescription plugin;
    bool instantiated = false;
    std::string errorMessage;
    int inputChannels = 0;
    int outputChannels = 0;
    int parameterCount = 0;
    int programCount = 0;
};

struct PluginProbeResult {
    std::vector<PluginProbeReport> reports;
    std::vector<PluginScanDiagnostic> diagnostics;
};

class PluginProber {
public:
    virtual ~PluginProber() = default;

    virtual PluginProbeResult probe(const PluginProbeRequest& request) = 0;
};

} // namespace loop_rigger::plugin_host
