#include "ProbeFixtureProcessor.h"

namespace loop_rigger::plugin_host::tests {

ProbeFixtureProcessor::ProbeFixtureProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

const juce::String ProbeFixtureProcessor::getName() const
{
    return "LoopRigger Probe Fixture";
}

void ProbeFixtureProcessor::prepareToPlay(double, int)
{
}

void ProbeFixtureProcessor::releaseResources()
{
}

void ProbeFixtureProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    buffer.clear();
}

void ProbeFixtureProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer&)
{
    buffer.clear();
}

bool ProbeFixtureProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& input = layouts.getMainInputChannelSet();
    const auto& output = layouts.getMainOutputChannelSet();
    return input == output && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}

bool ProbeFixtureProcessor::acceptsMidi() const
{
    return false;
}

bool ProbeFixtureProcessor::producesMidi() const
{
    return false;
}

bool ProbeFixtureProcessor::isMidiEffect() const
{
    return false;
}

double ProbeFixtureProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

juce::AudioProcessorEditor* ProbeFixtureProcessor::createEditor()
{
    return nullptr;
}

bool ProbeFixtureProcessor::hasEditor() const
{
    return false;
}

int ProbeFixtureProcessor::getNumPrograms()
{
    return 1;
}

int ProbeFixtureProcessor::getCurrentProgram()
{
    return 0;
}

void ProbeFixtureProcessor::setCurrentProgram(int)
{
}

const juce::String ProbeFixtureProcessor::getProgramName(int)
{
    return "Default";
}

void ProbeFixtureProcessor::changeProgramName(int, const juce::String&)
{
}

void ProbeFixtureProcessor::getStateInformation(juce::MemoryBlock&)
{
}

void ProbeFixtureProcessor::setStateInformation(const void*, int)
{
}

} // namespace loop_rigger::plugin_host::tests

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new loop_rigger::plugin_host::tests::ProbeFixtureProcessor();
}
