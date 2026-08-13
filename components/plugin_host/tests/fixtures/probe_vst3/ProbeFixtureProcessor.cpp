#include "ProbeFixtureProcessor.h"

namespace loop_rigger::plugin_host::tests {

namespace {

class ProbeFixtureEditor final : public juce::AudioProcessorEditor {
public:
    explicit ProbeFixtureEditor(ProbeFixtureProcessor& processor)
        : AudioProcessorEditor(processor)
    {
        setSize(560, 320);
    }

    void paint(juce::Graphics& graphics) override
    {
        const auto bounds = getLocalBounds().toFloat();
        graphics.fillAll(juce::Colour(0xff101519));

        graphics.setColour(juce::Colour(0xff1f2b33));
        graphics.fillRoundedRectangle(bounds.reduced(18.0f), 10.0f);

        graphics.setColour(juce::Colour(0xff18c6a7));
        graphics.drawRoundedRectangle(bounds.reduced(18.0f), 10.0f, 2.0f);

        graphics.setColour(juce::Colours::white);
        graphics.setFont(juce::FontOptions(32.0f, juce::Font::bold));
        graphics.drawText("LoopRigger Probe Fixture", getLocalBounds().withTrimmedTop(48), juce::Justification::centredTop);

        graphics.setFont(juce::FontOptions(18.0f));
        graphics.setColour(juce::Colour(0xffc8d2d8));
        graphics.drawText(
            "VST3 editor smoke: GUI loaded inside LoopRigger host",
            getLocalBounds().withTrimmedTop(102),
            juce::Justification::centredTop);

        const auto meter = juce::Rectangle<float>(110.0f, 180.0f, 340.0f, 32.0f);
        graphics.setColour(juce::Colour(0xff0b0e10));
        graphics.fillRoundedRectangle(meter, 6.0f);
        graphics.setColour(juce::Colour(0xff18c6a7));
        graphics.fillRoundedRectangle(meter.withWidth(230.0f), 6.0f);

        graphics.setColour(juce::Colours::white);
        graphics.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        graphics.drawText("2 IN / 2 OUT", getLocalBounds().withTrimmedTop(230), juce::Justification::centredTop);
    }
};

} // namespace

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
    return new ProbeFixtureEditor(*this);
}

bool ProbeFixtureProcessor::hasEditor() const
{
    return true;
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
