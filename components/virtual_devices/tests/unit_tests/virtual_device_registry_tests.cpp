#include "loop_rigger/virtual_devices/VirtualDeviceRegistry.h"

#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace {

class FakeProvider final : public loop_rigger::virtual_devices::VirtualDeviceProvider {
public:
    explicit FakeProvider(loop_rigger::virtual_devices::VirtualDeviceDescriptor descriptor)
        : descriptor_(std::move(descriptor))
    {
    }

    const loop_rigger::virtual_devices::VirtualDeviceDescriptor& descriptor() const override
    {
        return descriptor_;
    }

private:
    loop_rigger::virtual_devices::VirtualDeviceDescriptor descriptor_;
};

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << "\n";
    }
}

std::shared_ptr<FakeProvider> makeProvider(std::string id)
{
    loop_rigger::virtual_devices::VirtualDeviceDescriptor descriptor;
    descriptor.id = std::move(id);
    descriptor.displayName = "Test Device";
    descriptor.widgets.push_back({"button_1", "Button 1", loop_rigger::control::WidgetType::Button, "test", 0, 0, 1, 1});
    return std::make_shared<FakeProvider>(std::move(descriptor));
}

void testRegistryRejectsInvalidDevices()
{
    loop_rigger::virtual_devices::VirtualDeviceRegistry registry;

    expect(!registry.registerDevice(nullptr), "registry should reject null providers");
    expect(!registry.registerDevice(makeProvider("")), "registry should reject providers without an id");
}

void testRegistryFindsUniqueDevices()
{
    loop_rigger::virtual_devices::VirtualDeviceRegistry registry;

    expect(registry.registerDevice(makeProvider("yaeltex")), "registry should accept the first Yaeltex provider");
    expect(!registry.registerDevice(makeProvider("yaeltex")), "registry should reject duplicate ids");
    expect(registry.registerDevice(makeProvider("kaoss.mic")), "registry should accept a second unique provider");

    const auto found = registry.findDevice("yaeltex");
    expect(found != nullptr, "registered device should be findable");
    expect(found->descriptor().widgets.size() == 1, "registered descriptor should expose widgets");

    const auto missing = registry.findDevice("missing");
    expect(missing == nullptr, "missing device should not be found");

    const auto devices = registry.devices();
    expect(devices.size() == 2, "registry should list accepted devices only");
}

} // namespace

int main()
{
    testRegistryRejectsInvalidDevices();
    testRegistryFindsUniqueDevices();
    return failures == 0 ? 0 : 1;
}
