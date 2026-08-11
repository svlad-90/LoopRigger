#include "livelooping/core/LiveLoopingEngine.h"

#include <juce_gui_extra/juce_gui_extra.h>

using livelooping::core::CommandType;
using livelooping::core::ControllerCommand;
using livelooping::core::ControllerId;
using livelooping::core::InputTarget;
using livelooping::core::LiveLoopingEngine;

namespace {

class ProductComponent final : public juce::Component,
                               private juce::Timer {
public:
    explicit ProductComponent(LiveLoopingEngine& engine)
        : engine_(engine)
    {
        snapshot_.setMultiLine(true);
        snapshot_.setReadOnly(true);
        snapshot_.setFont(juce::Font(juce::FontOptions(14.0F)));
        addAndMakeVisible(snapshot_);
        startTimerHz(15);
    }

    void resized() override
    {
        snapshot_.setBounds(getLocalBounds().reduced(12));
    }

private:
    void timerCallback() override
    {
        snapshot_.setText(engine_.renderTextSnapshot(), juce::dontSendNotification);
    }

    LiveLoopingEngine& engine_;
    juce::TextEditor snapshot_;
};

class PseudoDevicesComponent final : public juce::Component {
public:
    explicit PseudoDevicesComponent(LiveLoopingEngine& engine)
        : engine_(engine)
    {
        addInputSection("Mic Kaoss", InputTarget::Mic);
        addInputSection("Synth Kaoss", InputTarget::Synth);
        addYaeltexSection();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
        const auto rowHeight = 34;
        for (auto* child : getChildren()) {
            child->setBounds(area.removeFromTop(rowHeight).reduced(0, 3));
        }
    }

private:
    void addButton(const juce::String& text, std::function<void()> action)
    {
        auto button = std::make_unique<juce::TextButton>(text);
        button->onClick = std::move(action);
        addAndMakeVisible(*button);
        buttons_.push_back(std::move(button));
    }

    void addInputSection(const juce::String& name, InputTarget target)
    {
        addButton(name + " page 1", [this, target] {
            handle({ControllerId::PseudoGui, CommandType::SelectInputPresetPage, target, 0});
        });
        addButton(name + " preset 1", [this, target] {
            handle({ControllerId::PseudoGui, CommandType::SelectInputPreset, target, 0});
        });
        addButton(name + " preset 2", [this, target] {
            handle({ControllerId::PseudoGui, CommandType::SelectInputPreset, target, 1});
        });
    }

    void addYaeltexSection()
    {
        addButton("Yaeltex looper 1", [this] {
            handle({ControllerId::Yaeltex, CommandType::SelectLooper, InputTarget::Mic, 0});
        });
        addButton("Yaeltex looper 2", [this] {
            handle({ControllerId::Yaeltex, CommandType::SelectLooper, InputTarget::Mic, 1});
        });
        addButton("Yaeltex length 8", [this] {
            handle({ControllerId::Yaeltex, CommandType::SelectSampleLength, InputTarget::Mic, 8});
        });
        addButton("Yaeltex record T1", [this] {
            handle({ControllerId::Yaeltex, CommandType::ToggleTrackRecording, InputTarget::Mic, 0});
        });
        addButton("Yaeltex clear T1", [this] {
            handle({ControllerId::Yaeltex, CommandType::ClearTrack, InputTarget::Mic, 0});
        });
        addButton("Yaeltex resample all", [this] {
            handle({ControllerId::Yaeltex, CommandType::StartResampleAllLoopers});
        });
        addButton("Reset all", [this] {
            handle({ControllerId::PseudoGui, CommandType::ResetAll});
        });
    }

    void handle(const ControllerCommand& command)
    {
        engine_.handle(command);
    }

    LiveLoopingEngine& engine_;
    std::vector<std::unique_ptr<juce::TextButton>> buttons_;
};

class Window final : public juce::DocumentWindow {
public:
    Window(const juce::String& name, std::unique_ptr<juce::Component> content)
        : DocumentWindow(name, juce::Colours::black, DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(content.release(), true);
        centreWithSize(700, 520);
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

class App final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override
    {
        return "LiveLooping";
    }

    const juce::String getApplicationVersion() override
    {
        return "0.1.0";
    }

    void initialise(const juce::String&) override
    {
        productWindow_ = std::make_unique<Window>("LiveLooping Product", std::make_unique<ProductComponent>(engine_));
        pseudoDevicesWindow_ = std::make_unique<Window>("Pseudo Devices", std::make_unique<PseudoDevicesComponent>(engine_));
        pseudoDevicesWindow_->setTopLeftPosition(productWindow_->getRight() + 20, productWindow_->getY());
    }

    void shutdown() override
    {
        pseudoDevicesWindow_.reset();
        productWindow_.reset();
    }

private:
    LiveLoopingEngine engine_;
    std::unique_ptr<Window> productWindow_;
    std::unique_ptr<Window> pseudoDevicesWindow_;
};

} // namespace

START_JUCE_APPLICATION(App)

