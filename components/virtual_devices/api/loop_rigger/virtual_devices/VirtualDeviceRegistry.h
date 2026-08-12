#pragma once

#include "loop_rigger/control/ControlMapping.h"

#include <memory>
#include <string>
#include <vector>

namespace loop_rigger::virtual_devices {

struct VirtualDeviceEvent {
    std::string deviceId;
    std::string widgetId;
    control::WidgetEventType type = control::WidgetEventType::Press;
    float value = 1.0F;
};

struct VirtualDeviceDescriptor {
    std::string id;
    std::string displayName;
    std::vector<control::ControllerWidget> widgets;
};

class VirtualDeviceProvider {
public:
    virtual ~VirtualDeviceProvider() = default;

    virtual const VirtualDeviceDescriptor& descriptor() const = 0;
};

class VirtualDeviceRegistry {
public:
    bool registerDevice(std::shared_ptr<VirtualDeviceProvider> provider);
    std::shared_ptr<const VirtualDeviceProvider> findDevice(const std::string& id) const;
    std::vector<VirtualDeviceDescriptor> devices() const;

private:
    std::vector<std::shared_ptr<VirtualDeviceProvider>> providers_;
};

} // namespace loop_rigger::virtual_devices
