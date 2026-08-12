#include "loop_rigger/virtual_devices/VirtualDeviceRegistry.h"

#include <algorithm>

namespace loop_rigger::virtual_devices {

bool VirtualDeviceRegistry::registerDevice(std::shared_ptr<VirtualDeviceProvider> provider)
{
    if (!provider || provider->descriptor().id.empty()) {
        return false;
    }

    const auto duplicate = std::find_if(
        providers_.begin(),
        providers_.end(),
        [&provider](const auto& existing) {
            return existing->descriptor().id == provider->descriptor().id;
        });
    if (duplicate != providers_.end()) {
        return false;
    }

    providers_.push_back(std::move(provider));
    return true;
}

std::shared_ptr<const VirtualDeviceProvider> VirtualDeviceRegistry::findDevice(const std::string& id) const
{
    const auto found = std::find_if(
        providers_.begin(),
        providers_.end(),
        [&id](const auto& provider) {
            return provider->descriptor().id == id;
        });
    if (found == providers_.end()) {
        return {};
    }
    return *found;
}

std::vector<VirtualDeviceDescriptor> VirtualDeviceRegistry::devices() const
{
    std::vector<VirtualDeviceDescriptor> result;
    result.reserve(providers_.size());
    for (const auto& provider : providers_) {
        result.push_back(provider->descriptor());
    }
    return result;
}

} // namespace loop_rigger::virtual_devices
