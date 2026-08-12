#pragma once

#include <optional>
#include <string>
#include <vector>

namespace loop_rigger::plugin_host {

enum class PluginFormat {
    Unknown,
    Vst2,
    Vst3,
    AudioUnit
};

struct PluginDescription {
    std::string identifier;
    std::string name;
    std::string manufacturer;
    PluginFormat format = PluginFormat::Unknown;
    std::string path;
    std::string category;
    bool instrument = false;
};

class PluginCatalog {
public:
    bool add(PluginDescription description);
    const std::vector<PluginDescription>& plugins() const;
    std::optional<PluginDescription> findByIdentifier(const std::string& identifier) const;

private:
    std::vector<PluginDescription> plugins_;
};

std::string toString(PluginFormat format);

} // namespace loop_rigger::plugin_host
