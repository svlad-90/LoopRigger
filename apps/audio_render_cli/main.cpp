#include "loop_rigger/audio/HostGraphProcessor.h"
#include "loop_rigger/audio/HostGraphRuntime.h"
#include "loop_rigger/audio/HostGraphSpec.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

struct RenderOptions {
    std::string outputPath;
    double sampleRate = 48000.0;
    int blockSize = 256;
    double seconds = 8.0;
};

void printUsage()
{
    std::cerr << "usage: livelooping_audio_render_cli [--sample-rate hz] [--block-size samples] "
                 "[--seconds seconds] <output.wav>\n";
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

float clampAudio(float value)
{
    return std::max(-1.0F, std::min(1.0F, value));
}

std::int16_t toPcm16(float value)
{
    return static_cast<std::int16_t>(std::lrint(clampAudio(value) * 32767.0F));
}

void writeU16(std::ofstream& stream, std::uint16_t value)
{
    stream.put(static_cast<char>(value & 0xFFU));
    stream.put(static_cast<char>((value >> 8U) & 0xFFU));
}

void writeU32(std::ofstream& stream, std::uint32_t value)
{
    stream.put(static_cast<char>(value & 0xFFU));
    stream.put(static_cast<char>((value >> 8U) & 0xFFU));
    stream.put(static_cast<char>((value >> 16U) & 0xFFU));
    stream.put(static_cast<char>((value >> 24U) & 0xFFU));
}

bool writeWav(
    const std::string& path,
    const std::vector<float>& left,
    const std::vector<float>& right,
    int sampleRate)
{
    const auto samples = std::min(left.size(), right.size());
    const auto dataBytes = static_cast<std::uint32_t>(samples * 2U * sizeof(std::int16_t));

    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        return false;
    }

    stream.write("RIFF", 4);
    writeU32(stream, 36U + dataBytes);
    stream.write("WAVE", 4);
    stream.write("fmt ", 4);
    writeU32(stream, 16U);
    writeU16(stream, 1U);
    writeU16(stream, 2U);
    writeU32(stream, static_cast<std::uint32_t>(sampleRate));
    writeU32(stream, static_cast<std::uint32_t>(sampleRate * 2 * sizeof(std::int16_t)));
    writeU16(stream, static_cast<std::uint16_t>(2 * sizeof(std::int16_t)));
    writeU16(stream, 16U);
    stream.write("data", 4);
    writeU32(stream, dataBytes);

    for (std::size_t index = 0; index < samples; ++index) {
        writeU16(stream, static_cast<std::uint16_t>(toPcm16(left[index])));
        writeU16(stream, static_cast<std::uint16_t>(toPcm16(right[index])));
    }

    return stream.good();
}

float noteEnvelope(double beatPosition, double noteBeat, double lengthBeats)
{
    const auto local = std::fmod(beatPosition - noteBeat + 64.0, lengthBeats);
    if (local > 0.45) {
        return 0.0F;
    }
    const auto attack = std::min(1.0, local / 0.02);
    const auto release = std::max(0.0, 1.0 - (local / 0.45));
    return static_cast<float>(attack * release);
}

float sine(double phase)
{
    return static_cast<float>(std::sin(phase));
}

void addTone(
    loop_rigger::audio::AudioBlock& block,
    int startSample,
    double sampleRate,
    double frequency,
    double gain,
    double pan,
    double bpm,
    double beatOffset,
    double noteLengthBeats)
{
    const auto samples = block.empty() ? 0 : static_cast<int>(block.front().size());
    for (int sample = 0; sample < samples; ++sample) {
        const auto absoluteSample = startSample + sample;
        const auto time = static_cast<double>(absoluteSample) / sampleRate;
        const auto beatPosition = time * bpm / 60.0;
        const auto envelope = noteEnvelope(beatPosition, beatOffset, noteLengthBeats);
        const auto value = sine(2.0 * kPi * frequency * time) * static_cast<float>(gain) * envelope;
        block[0][static_cast<std::size_t>(sample)] += value * static_cast<float>(1.0 - pan);
        block[1][static_cast<std::size_t>(sample)] += value * static_cast<float>(pan);
    }
}

void addClick(
    loop_rigger::audio::AudioBlock& block,
    int startSample,
    double sampleRate,
    double bpm)
{
    const auto samples = block.empty() ? 0 : static_cast<int>(block.front().size());
    for (int sample = 0; sample < samples; ++sample) {
        const auto absoluteSample = startSample + sample;
        const auto time = static_cast<double>(absoluteSample) / sampleRate;
        const auto beatPosition = time * bpm / 60.0;
        const auto localBeat = std::fmod(beatPosition, 1.0);
        if (localBeat > 0.04) {
            continue;
        }
        const auto decay = std::max(0.0, 1.0 - localBeat / 0.04);
        const auto click = sine(2.0 * kPi * 1760.0 * time) * static_cast<float>(0.2 * decay);
        block[0][static_cast<std::size_t>(sample)] += click;
        block[1][static_cast<std::size_t>(sample)] += click;
    }
}

bool render(const RenderOptions& options)
{
    auto runtime = loop_rigger::audio::buildHostGraphRuntime(loop_rigger::audio::makeDefaultLiveLoopingGraphSpec());
    if (!runtime.errors.empty()) {
        for (const auto& error : runtime.errors) {
            std::cerr << "graph error: " << error << "\n";
        }
        return false;
    }

    const auto channels = 2;
    const auto totalSamples = std::max(1, static_cast<int>(std::lrint(options.seconds * options.sampleRate)));
    auto buffers = loop_rigger::audio::makeHostGraphBuffers(runtime, {channels, options.blockSize});
    std::vector<float> left(static_cast<std::size_t>(totalSamples), 0.0F);
    std::vector<float> right(static_cast<std::size_t>(totalSamples), 0.0F);

    constexpr double bpm = 124.0;
    float peak = 0.0F;
    int rendered = 0;
    while (rendered < totalSamples) {
        loop_rigger::audio::clearHostGraphBuffers(buffers);
        const auto currentBlockSize = std::min(options.blockSize, totalSamples - rendered);
        buffers.config.blockSize = currentBlockSize;

        auto* track1 = loop_rigger::audio::findNodeBuffer(buffers, runtime, "looper_1_track_1");
        auto* track2 = loop_rigger::audio::findNodeBuffer(buffers, runtime, "looper_2_track_1");
        auto* track3 = loop_rigger::audio::findNodeBuffer(buffers, runtime, "looper_3_track_1");
        auto* track4 = loop_rigger::audio::findNodeBuffer(buffers, runtime, "looper_4_track_1");
        auto* sampler = loop_rigger::audio::findNodeBuffer(buffers, runtime, "one_shot_sampler");

        if (track1 != nullptr) {
            addTone(*track1, rendered, options.sampleRate, 55.0, 0.34, 0.47, bpm, 0.0, 1.0);
        }
        if (track2 != nullptr) {
            addTone(*track2, rendered, options.sampleRate, 110.0, 0.2, 0.25, bpm, 0.5, 1.0);
        }
        if (track3 != nullptr) {
            addTone(*track3, rendered, options.sampleRate, 220.0, 0.16, 0.75, bpm, 0.25, 0.5);
        }
        if (track4 != nullptr) {
            addTone(*track4, rendered, options.sampleRate, 440.0, 0.12, 0.55, bpm, 0.75, 0.5);
        }
        if (sampler != nullptr) {
            addClick(*sampler, rendered, options.sampleRate, bpm);
        }

        const auto result = loop_rigger::audio::processHostGraphRuntime(runtime, buffers);
        peak = std::max(peak, result.masterPeak);

        const auto* master = loop_rigger::audio::findNodeBuffer(buffers, runtime, "master");
        if (master != nullptr && master->size() >= 2) {
            for (int sample = 0; sample < currentBlockSize; ++sample) {
                left[static_cast<std::size_t>(rendered + sample)] = (*master)[0][static_cast<std::size_t>(sample)];
                right[static_cast<std::size_t>(rendered + sample)] = (*master)[1][static_cast<std::size_t>(sample)];
            }
        }

        rendered += currentBlockSize;
    }

    if (!writeWav(options.outputPath, left, right, static_cast<int>(options.sampleRate))) {
        std::cerr << "failed to write wav: " << options.outputPath << "\n";
        return false;
    }

    std::cout << "wrote\t" << options.outputPath << "\n";
    std::cout << "seconds\t" << options.seconds << "\n";
    std::cout << "sample_rate\t" << static_cast<int>(options.sampleRate) << "\n";
    std::cout << "peak\t" << peak << "\n";
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    RenderOptions options;

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
        if (argument == "--seconds") {
            if (index + 1 >= argc || !parseDouble(argv[++index], options.seconds) || options.seconds <= 0.0) {
                printUsage();
                return 2;
            }
            continue;
        }
        if (!options.outputPath.empty()) {
            printUsage();
            return 2;
        }
        options.outputPath = argument;
    }

    if (options.outputPath.empty()) {
        printUsage();
        return 2;
    }

    return render(options) ? 0 : 1;
}
