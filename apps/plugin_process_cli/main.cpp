#include <juce_audio_processors/juce_audio_processors.h>

#include <algorithm>
#include <cmath>
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

float peakMagnitude(const juce::AudioBuffer<float>& buffer)
{
    float peak = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
        peak = std::max(peak, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));
    }
    return peak;
}

float rmsMagnitude(const juce::AudioBuffer<float>& buffer)
{
    double sum = 0.0;
    int count = 0;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
        const auto* samples = buffer.getReadPointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
            sum += static_cast<double>(samples[sample]) * static_cast<double>(samples[sample]);
            ++count;
        }
    }
    return count > 0 ? static_cast<float>(std::sqrt(sum / static_cast<double>(count))) : 0.0f;
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

    juce::AudioBuffer<float> buffer(std::max(1, instance->getTotalNumOutputChannels()), options.blockSize);
    juce::MidiBuffer midi;
    float outputPeak = 0.0f;
    float outputRms = 0.0f;
    for (int block = 0; block < options.blocks; ++block) {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
            buffer.clear(channel, 0, options.blockSize);
            for (int sample = 0; sample < options.blockSize; ++sample) {
                buffer.setSample(channel, sample, options.inputValue);
            }
        }

        instance->processBlock(buffer, midi);
        outputPeak = peakMagnitude(buffer);
        outputRms = rmsMagnitude(buffer);
    }

    instance->releaseResources();

    const auto expectedPeak = std::abs(options.inputValue * options.gain);
    const auto peakDelta = std::abs(outputPeak - expectedPeak);
    const auto ok = gainSet && peakDelta < 0.0005f;

    std::cout << (ok ? "ok" : "failed") << "\t"
              << description.name << "\t"
              << "channels=" << buffer.getNumChannels() << "\t"
              << "blocks=" << options.blocks << "\t"
              << "gain_set=" << (gainSet ? "yes" : "no") << "\t"
              << "input_peak=" << std::abs(options.inputValue) << "\t"
              << "expected_peak=" << expectedPeak << "\t"
              << "output_peak=" << outputPeak << "\t"
              << "output_rms=" << outputRms << "\n";

    return ok ? 0 : 1;
}
