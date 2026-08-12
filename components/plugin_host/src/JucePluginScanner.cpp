#include "loop_rigger/plugin_host/JucePluginScanner.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <algorithm>
#include <memory>
#include <string>

namespace loop_rigger::plugin_host {

namespace {

PluginFormat pluginFormatFromName(const juce::String& formatName)
{
    if (formatName.containsIgnoreCase("VST3")) {
        return PluginFormat::Vst3;
    }
    if (formatName.containsIgnoreCase("AudioUnit")) {
        return PluginFormat::AudioUnit;
    }
    return PluginFormat::Unknown;
}

bool shouldScanFormat(const PluginScanRequest& request, PluginFormat format)
{
    return request.formats.empty()
        || std::find(request.formats.begin(), request.formats.end(), format) != request.formats.end();
}

PluginDescription convertDescription(const juce::PluginDescription& description)
{
    PluginDescription result;
    result.identifier = description.createIdentifierString().toStdString();
    result.name = description.name.toStdString();
    result.manufacturer = description.manufacturerName.toStdString();
    result.format = pluginFormatFromName(description.pluginFormatName);
    result.path = description.fileOrIdentifier.toStdString();
    result.category = description.category.toStdString();
    result.instrument = description.isInstrument;
    return result;
}

class JucePluginScanner final : public PluginScanner {
public:
    JucePluginScanner()
    {
        formatManager_.addDefaultFormats();
    }

    PluginScanResult scan(const PluginScanRequest& request) override
    {
        PluginScanResult result;
        for (const auto& path : request.searchPaths) {
            scanPath(path, request, result);
        }
        return result;
    }

private:
    void scanPath(const std::string& path, const PluginScanRequest& request, PluginScanResult& result)
    {
        const juce::String jucePath(path);
        for (int index = 0; index < formatManager_.getNumFormats(); ++index) {
            auto* format = formatManager_.getFormat(index);
            if (format == nullptr) {
                continue;
            }

            const auto pluginFormat = pluginFormatFromName(format->getName());
            if (!shouldScanFormat(request, pluginFormat) || !format->fileMightContainThisPluginType(jucePath)) {
                continue;
            }

            juce::OwnedArray<juce::PluginDescription> descriptions;
            format->findAllTypesForFile(descriptions, jucePath);
            for (const auto* description : descriptions) {
                if (description != nullptr) {
                    result.catalog.add(convertDescription(*description));
                }
            }
        }
    }

    juce::AudioPluginFormatManager formatManager_;
};

} // namespace

std::unique_ptr<PluginScanner> makeJucePluginScanner()
{
    return std::make_unique<JucePluginScanner>();
}

} // namespace loop_rigger::plugin_host
