#include "loop_rigger/plugin_host/JucePluginProbe.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

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

bool containsFormat(const std::vector<PluginFormat>& formats, PluginFormat format)
{
    return std::find(formats.begin(), formats.end(), format) != formats.end();
}

bool shouldProbeFormat(const PluginProbeRequest& request, PluginFormat format)
{
    return request.formats.empty() || containsFormat(request.formats, format);
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

bool isNestedInsidePluginBundle(const juce::File& file)
{
    auto parent = file.getParentDirectory();
    while (parent.exists() && parent != parent.getParentDirectory()) {
        if (isCandidatePath(parent)) {
            return true;
        }
        parent = parent.getParentDirectory();
    }
    return false;
}

void addDiagnostic(PluginProbeResult& result, PluginScanDiagnosticLevel level, const std::string& message)
{
    result.diagnostics.push_back({level, message});
}

class JucePluginProber final : public PluginProber {
public:
    JucePluginProber()
    {
        juce::addHeadlessDefaultFormatsToManager(formatManager_);
    }

    PluginProbeResult probe(const PluginProbeRequest& request) override
    {
        PluginProbeResult result;
        reportUnavailableFormats(request, result);
        for (const auto& path : request.searchPaths) {
            probePath(path, request, result);
            if (hasReachedLimit(request, result)) {
                break;
            }
        }
        return result;
    }

private:
    void probePath(const std::string& path, const PluginProbeRequest& request, PluginProbeResult& result)
    {
        const juce::File file(path);
        if (!file.exists()) {
            addDiagnostic(result, PluginScanDiagnosticLevel::Warning, "plugin search path does not exist: " + path);
            return;
        }

        if (file.isDirectory() && !isCandidatePath(file)) {
            for (const auto& child : file.findChildFiles(juce::File::findFilesAndDirectories, true)) {
                if (isCandidatePath(child) && !isNestedInsidePluginBundle(child)) {
                    probeCandidate(child, request, result);
                    if (hasReachedLimit(request, result)) {
                        addDiagnostic(result, PluginScanDiagnosticLevel::Info, "plugin probe limit reached");
                        return;
                    }
                }
            }
            return;
        }

        probeCandidate(file, request, result);
    }

    void probeCandidate(const juce::File& file, const PluginProbeRequest& request, PluginProbeResult& result)
    {
        const auto jucePath = file.getFullPathName();
        for (int index = 0; index < formatManager_.getNumFormats(); ++index) {
            auto* format = formatManager_.getFormat(index);
            if (format == nullptr) {
                continue;
            }

            const auto pluginFormat = pluginFormatFromName(format->getName());
            if (!shouldProbeFormat(request, pluginFormat) || !format->fileMightContainThisPluginType(jucePath)) {
                continue;
            }

            juce::OwnedArray<juce::PluginDescription> descriptions;
            format->findAllTypesForFile(descriptions, jucePath);
            for (const auto* description : descriptions) {
                if (description != nullptr) {
                    probeDescription(*description, request, result);
                    if (hasReachedLimit(request, result)) {
                        return;
                    }
                }
            }
        }
    }

    void probeDescription(
        const juce::PluginDescription& description,
        const PluginProbeRequest& request,
        PluginProbeResult& result) const
    {
        PluginProbeReport report;
        report.plugin = convertDescription(description);

        juce::String error;
        auto instance = formatManager_.createPluginInstance(description, request.sampleRate, request.blockSize, error);
        if (instance == nullptr) {
            report.errorMessage = error.toStdString();
            if (report.errorMessage.empty()) {
                report.errorMessage = "plugin instance creation failed";
            }
            result.reports.push_back(std::move(report));
            return;
        }

        report.instantiated = true;
        report.inputChannels = instance->getTotalNumInputChannels();
        report.outputChannels = instance->getTotalNumOutputChannels();
        report.parameterCount = instance->getParameters().size();
        report.programCount = instance->getNumPrograms();
        result.reports.push_back(std::move(report));
    }

    std::vector<PluginFormat> availableFormats() const
    {
        std::vector<PluginFormat> formats;
        for (int index = 0; index < formatManager_.getNumFormats(); ++index) {
            const auto* format = formatManager_.getFormat(index);
            if (format == nullptr) {
                continue;
            }

            const auto pluginFormat = pluginFormatFromName(format->getName());
            if (pluginFormat != PluginFormat::Unknown && !containsFormat(formats, pluginFormat)) {
                formats.push_back(pluginFormat);
            }
        }
        return formats;
    }

    void reportUnavailableFormats(const PluginProbeRequest& request, PluginProbeResult& result) const
    {
        const auto formats = availableFormats();
        for (const auto requestedFormat : request.formats) {
            if (!containsFormat(formats, requestedFormat)) {
                addDiagnostic(
                    result,
                    PluginScanDiagnosticLevel::Warning,
                    "requested plugin format is not available in this build: " + toString(requestedFormat));
            }
        }
    }

    static bool hasReachedLimit(const PluginProbeRequest& request, const PluginProbeResult& result)
    {
        return request.maxPlugins > 0 && result.reports.size() >= request.maxPlugins;
    }

    juce::AudioPluginFormatManager formatManager_;
};

} // namespace

std::unique_ptr<PluginProber> makeJucePluginProber()
{
    return std::make_unique<JucePluginProber>();
}

} // namespace loop_rigger::plugin_host
