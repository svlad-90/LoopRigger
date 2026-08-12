#pragma once

#include "loop_rigger/plugin_host/PluginCatalog.h"

#include <string>
#include <vector>

namespace loop_rigger::plugin_host {

enum class PluginScanDiagnosticLevel {
    Info,
    Warning,
    Error
};

struct PluginScanDiagnostic {
    PluginScanDiagnosticLevel level = PluginScanDiagnosticLevel::Info;
    std::string message;
};

struct PluginScanRequest {
    std::vector<std::string> searchPaths;
    std::vector<PluginFormat> formats;
};

struct PluginScanResult {
    PluginCatalog catalog;
    std::vector<PluginScanDiagnostic> diagnostics;
};

class PluginScanner {
public:
    virtual ~PluginScanner() = default;

    virtual PluginScanResult scan(const PluginScanRequest& request) = 0;
};

std::string toString(PluginScanDiagnosticLevel level);

} // namespace loop_rigger::plugin_host
