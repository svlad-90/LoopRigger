#include "loop_rigger/plugin_host/PluginCatalog.h"
#include "loop_rigger/plugin_host/PluginScanner.h"

#include <algorithm>
#include <utility>

namespace loop_rigger::plugin_host {

bool PluginCatalog::add(PluginDescription description)
{
    if (description.identifier.empty()) {
        return false;
    }

    const auto duplicate = std::find_if(
        plugins_.begin(),
        plugins_.end(),
        [&description](const auto& existing) {
            return existing.identifier == description.identifier;
        });
    if (duplicate != plugins_.end()) {
        return false;
    }

    plugins_.push_back(std::move(description));
    return true;
}

const std::vector<PluginDescription>& PluginCatalog::plugins() const
{
    return plugins_;
}

std::optional<PluginDescription> PluginCatalog::findByIdentifier(const std::string& identifier) const
{
    const auto found = std::find_if(
        plugins_.begin(),
        plugins_.end(),
        [&identifier](const auto& plugin) {
            return plugin.identifier == identifier;
        });
    if (found == plugins_.end()) {
        return std::nullopt;
    }
    return *found;
}

std::string toString(PluginFormat format)
{
    switch (format) {
    case PluginFormat::Unknown:
        return "unknown";
    case PluginFormat::Vst3:
        return "vst3";
    case PluginFormat::AudioUnit:
        return "audio_unit";
    }
    return "unknown";
}

std::string toString(PluginScanDiagnosticLevel level)
{
    switch (level) {
    case PluginScanDiagnosticLevel::Info:
        return "info";
    case PluginScanDiagnosticLevel::Warning:
        return "warning";
    case PluginScanDiagnosticLevel::Error:
        return "error";
    }
    return "unknown";
}

} // namespace loop_rigger::plugin_host
