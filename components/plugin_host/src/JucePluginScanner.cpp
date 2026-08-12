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
    if (formatName.containsIgnoreCase("VST")) {
        return PluginFormat::Vst2;
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

bool isCandidatePath(const juce::File& file)
{
    const auto extension = file.getFileExtension();
    return extension.equalsIgnoreCase(".vst3")
        || extension.equalsIgnoreCase(".dll")
        || extension.equalsIgnoreCase(".component");
}

void addDiagnostic(PluginScanResult& result, PluginScanDiagnosticLevel level, const std::string& message)
{
    result.diagnostics.push_back({level, message});
}

class JucePluginScanner final : public PluginScanner {
public:
    JucePluginScanner()
    {
        juce::addHeadlessDefaultFormatsToManager(formatManager_);
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
        const juce::File file(path);
        if (!file.exists()) {
            addDiagnostic(result, PluginScanDiagnosticLevel::Warning, "plugin search path does not exist: " + path);
            return;
        }

        if (file.isDirectory() && !isCandidatePath(file)) {
            for (const auto& child : file.findChildFiles(juce::File::findFilesAndDirectories, true)) {
                if (isCandidatePath(child)) {
                    scanPluginCandidate(child, request, result);
                }
            }
            return;
        }

        scanPluginCandidate(file, request, result);
    }

    void scanPluginCandidate(const juce::File& file, const PluginScanRequest& request, PluginScanResult& result)
    {
        const auto jucePath = file.getFullPathName();
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
