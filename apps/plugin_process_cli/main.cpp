#include "loop_rigger/audio/OfflineProcessing.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace {

struct ProcessOptions {
    juce::File pluginPath;
    bool hasPluginPath = false;
    double sampleRate = 44100.0;
    int blockSize = 512;
    int blocks = 4;
    float inputValue = 0.25f;
    float gain = 0.5f;
};

void printUsage()
{
    std::cerr << "usage: livelooping_plugin_process_cli [--sample-rate hz] [--block-size samples] "
                 "[--blocks count] [--input value] [--gain value] <plugin-path>\n";
}

bool parseDouble(const char* text, double& output)
{
    char* end = nullptr;
    output = std::strtod(text, &end);
    return end != text && *end == '\0';
}

bool parseInt(const char* text, int& output)
{
    char* end = nullptr;
    const auto parsed = std::strtol(text, &end, 10);
    if (end == text || *end != '\0') {
        return false;
    }
    output = static_cast<int>(parsed);
    return true;
}

bool parseFloat(const char* text, float& output)
{
    double value = 0.0;
    if (!parseDouble(text, value)) {
        return false;
    }
    output = static_cast<float>(value);
    return true;
}

bool isCandidatePath(const juce::File& file)
{
    return file.getFileExtension().equalsIgnoreCase(".vst3");
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

juce::Array<juce::File> collectPluginCandidates(const juce::File& root)
{
    juce::Array<juce::File> result;
    if (!root.exists()) {
        return result;
    }

    if (isCandidatePath(root)) {
        result.add(root);
        return result;
    }

    for (const auto& child : root.findChildFiles(juce::File::findFilesAndDirectories, true)) {
        if (isCandidatePath(child) && !isNestedInsidePluginBundle(child)) {
            result.add(child);
        }
    }
    return result;
}

std::unique_ptr<juce::AudioPluginInstance> createFirstPluginInstance(
    const juce::File& pluginPath,
    double sampleRate,
    int blockSize,
    juce::AudioPluginFormatManager& formatManager,
    juce::String& errorMessage,
    juce::PluginDescription& loadedDescription)
{
    juce::addHeadlessDefaultFormatsToManager(formatManager);

    const auto candidates = collectPluginCandidates(pluginPath);
    if (candidates.isEmpty()) {
        errorMessage = "no VST3 plugin found in " + pluginPath.getFullPathName();
        return {};
    }

    for (const auto& candidate : candidates) {
        const auto candidatePath = candidate.getFullPathName();
        for (int index = 0; index < formatManager.getNumFormats(); ++index) {
            auto* format = formatManager.getFormat(index);
            if (format == nullptr || !format->fileMightContainThisPluginType(candidatePath)) {
                continue;
            }

            juce::OwnedArray<juce::PluginDescription> descriptions;
            format->findAllTypesForFile(descriptions, candidatePath);
            for (const auto* description : descriptions) {
                if (description == nullptr) {
                    continue;
                }

                auto instance = formatManager.createPluginInstance(*description, sampleRate, blockSize, errorMessage);
                if (instance != nullptr) {
                    loadedDescription = *description;
                    return instance;
                }
            }
        }
    }

    if (errorMessage.isEmpty()) {
        errorMessage = "plugin instance creation failed";
    }
    return {};
}

bool setParameterByName(juce::AudioProcessor& processor, const juce::String& name, float value)
{
    for (auto* parameter : processor.getParameters()) {
        if (parameter != nullptr && parameter->getName(64).equalsIgnoreCase(name)) {
            parameter->setValueNotifyingHost(value);
            return true;
        }
    }
    return false;
}

juce::AudioBuffer<float> toJuceBuffer(const loop_rigger::audio::AudioBlock& block)
{
    const auto channels = static_cast<int>(block.size());
    const auto samples = block.empty() ? 0 : static_cast<int>(block.front().size());
    juce::AudioBuffer<float> buffer(channels, samples);
    for (int channel = 0; channel < channels; ++channel) {
        const auto& source = block[static_cast<std::size_t>(channel)];
        for (int sample = 0; sample < samples; ++sample) {
            buffer.setSample(channel, sample, source[static_cast<std::size_t>(sample)]);
        }
    }
    return buffer;
}

void copyFromJuceBuffer(const juce::AudioBuffer<float>& source, loop_rigger::audio::AudioBlock& target)
{
    const auto channels = std::min(source.getNumChannels(), static_cast<int>(target.size()));
    for (int channel = 0; channel < channels; ++channel) {
        auto& destination = target[static_cast<std::size_t>(channel)];
        const auto samples = std::min(source.getNumSamples(), static_cast<int>(destination.size()));
        for (int sample = 0; sample < samples; ++sample) {
            destination[static_cast<std::size_t>(sample)] = source.getSample(channel, sample);
        }
    }
}

} // namespace

int main(int argc, char** argv)
{
    ProcessOptions options;

    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        if (argument == "--sample-rate") {
            if (index + 1 >= argc || !parseDouble(argv[++index], options.sampleRate) || options.sampleRate <= 0.0) {
                printUsage();
                return 2;
            }
            continue;
        }
        if (argument == "--block-size") {
            if (index + 1 >= argc || !parseInt(argv[++index], options.blockSize) || options.blockSize <= 0) {
                printUsage();
                return 2;
            }
            continue;
        }
        if (argument == "--blocks") {
            if (index + 1 >= argc || !parseInt(argv[++index], options.blocks) || options.blocks <= 0) {
                printUsage();
                return 2;
            }
            continue;
        }
        if (argument == "--input") {
            if (index + 1 >= argc || !parseFloat(argv[++index], options.inputValue)) {
                printUsage();
                return 2;
            }
            continue;
        }
        if (argument == "--gain") {
            if (index + 1 >= argc || !parseFloat(argv[++index], options.gain) || options.gain < 0.0f || options.gain > 1.0f) {
                printUsage();
                return 2;
            }
            continue;
        }

        options.pluginPath = juce::File(argument);
        options.hasPluginPath = true;
    }

    if (!options.hasPluginPath) {
        printUsage();
        return 2;
    }

    juce::AudioPluginFormatManager formatManager;
    juce::String error;
    juce::PluginDescription description;
    auto instance = createFirstPluginInstance(
        options.pluginPath,
        options.sampleRate,
        options.blockSize,
        formatManager,
        error,
        description);
    if (instance == nullptr) {
        std::cerr << "error\t" << error << "\n";
        return 1;
    }

    const auto gainSet = setParameterByName(*instance, "Gain", options.gain);
    instance->setRateAndBufferSizeDetails(options.sampleRate, options.blockSize);
    instance->prepareToPlay(options.sampleRate, options.blockSize);

    loop_rigger::audio::OfflineProcessingRequest request;
    request.channels = std::max(1, instance->getTotalNumOutputChannels());
    request.blockSize = options.blockSize;
    request.blocks = options.blocks;
    request.inputValue = options.inputValue;
    request.expectedGain = options.gain;

    const auto processResult = loop_rigger::audio::runOfflineProcessingSmoke(
        request,
        [&instance](loop_rigger::audio::AudioBlock& block, std::string& errorMessage) {
            juce::AudioBuffer<float> buffer = toJuceBuffer(block);
            juce::MidiBuffer midi;
            instance->processBlock(buffer, midi);
            copyFromJuceBuffer(buffer, block);
            errorMessage.clear();
            return true;
        });

    instance->releaseResources();

    const auto ok = gainSet && processResult.processed && processResult.withinTolerance;

    std::cout << (ok ? "ok" : "failed") << "\t"
              << description.name << "\t"
              << "channels=" << processResult.channels << "\t"
              << "blocks=" << options.blocks << "\t"
              << "gain_set=" << (gainSet ? "yes" : "no") << "\t"
              << "input_peak=" << processResult.inputPeak << "\t"
              << "expected_peak=" << processResult.expectedPeak << "\t"
              << "output_peak=" << processResult.outputPeak << "\t"
              << "output_rms=" << processResult.outputRms;
    if (!processResult.errorMessage.empty()) {
        std::cout << "\t" << processResult.errorMessage;
    }
    std::cout << "\n";

    return ok ? 0 : 1;
}
