#include "livelooping/core/ControlMapping.h"
#include "livelooping/core/LiveLoopingEngine.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>
#include <utility>
#include <vector>

using livelooping::core::ControllerProfile;
using livelooping::core::ControllerWidget;
using livelooping::core::LiveLoopingEngine;
using livelooping::core::MidiMapper;
using livelooping::core::WidgetEvent;
using livelooping::core::WidgetEventType;
using livelooping::core::WidgetType;
using livelooping::core::makeMicKaossPadProfile;
using livelooping::core::makeSynthKaossPadProfile;
using livelooping::core::makeYaeltexLiveLoopingProfile;

namespace {

constexpr int kCellWidth = 92;
constexpr int kCellHeight = 58;
constexpr int kGap = 8;
constexpr int kGroupHeaderHeight = 22;

class ProfileSurfaceComponent final : public juce::Component {
public:
    ProfileSurfaceComponent(LiveLoopingEngine& engine, ControllerProfile profile)
        : engine_(engine),
          mapper_(std::move(profile))
    {
        for (const auto& widget : mapper_.profile().widgets) {
            addWidget(widget);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
        for (auto& group : groups_) {
            const auto groupHeight = group.requiredRows * kCellHeight + kGroupHeaderHeight + kGap;
            auto groupArea = area.removeFromTop(groupHeight).reduced(0, 3);
            group.label->setBounds(groupArea.removeFromTop(kGroupHeaderHeight));
            const auto gridArea = groupArea.reduced(0, 2);

            for (auto& control : controls_) {
                if (control.group != group.name) {
                    continue;
                }

                auto bounds = juce::Rectangle<int>(
                    gridArea.getX() + control.column * kCellWidth,
                    gridArea.getY() + control.row * kCellHeight,
                    control.width * kCellWidth - kGap,
                    control.height * kCellHeight - kGap);
                control.component->setBounds(bounds);
            }
        }
    }

private:
    struct Group {
        juce::String name;
        std::unique_ptr<juce::Label> label;
        int requiredRows = 1;
    };

    struct Control {
        juce::Component* component = nullptr;
        juce::String group;
        int row = 0;
        int column = 0;
        int width = 1;
        int height = 1;
    };

    Group& ensureGroup(const ControllerWidget& widget)
    {
        const auto groupName = juce::String(widget.group);
        for (auto& group : groups_) {
            if (group.name == groupName) {
                group.requiredRows = juce::jmax(group.requiredRows, widget.row + widget.height);
                return group;
            }
        }

        groups_.push_back({});
        auto& group = groups_.back();
        group.name = groupName;
        group.requiredRows = juce::jmax(1, widget.row + widget.height);
        group.label = std::make_unique<juce::Label>();
        group.label->setText(groupName, juce::dontSendNotification);
        group.label->setJustificationType(juce::Justification::centredLeft);
        group.label->setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(*group.label);
        return group;
    }

    void addWidget(const ControllerWidget& widget)
    {
        ensureGroup(widget);

        if (widget.type == WidgetType::Button) {
            auto button = std::make_unique<juce::TextButton>(widget.label);
            button->onClick = [this, id = widget.id] {
                dispatch({id, WidgetEventType::Press, 1.0F});
            };
            addAndMakeVisible(*button);
            controls_.push_back(makeControl(*button, widget));
            buttons_.push_back(std::move(button));
            return;
        }

        auto slider = std::make_unique<juce::Slider>();
        slider->setRange(0.0, 1.0, 0.001);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
        slider->setName(widget.label);
        if (widget.type == WidgetType::Fader) {
            slider->setSliderStyle(juce::Slider::LinearVertical);
        } else {
            slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        }
        slider->onValueChange = [this, id = widget.id, slider = slider.get()] {
            dispatch({id, WidgetEventType::Change, static_cast<float>(slider->getValue())});
        };
        addAndMakeVisible(*slider);
        controls_.push_back(makeControl(*slider, widget));
        sliders_.push_back(std::move(slider));
    }

    Control makeControl(juce::Component& component, const ControllerWidget& widget) const
    {
        return Control{
            &component,
            juce::String(widget.group),
            widget.row,
            widget.column,
            widget.width,
            widget.height,
        };
    }

    void dispatch(const WidgetEvent& event)
    {
        const auto command = mapper_.mapWidget(event);
        if (command.has_value()) {
            engine_.handle(command.value());
        }
    }

    LiveLoopingEngine& engine_;
    MidiMapper mapper_;
    std::vector<Group> groups_;
    std::vector<Control> controls_;
    std::vector<std::unique_ptr<juce::TextButton>> buttons_;
    std::vector<std::unique_ptr<juce::Slider>> sliders_;
};

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
    {
        tabs_.setTabBarDepth(28);
        tabs_.addTab("Mic Kaoss", juce::Colours::darkslategrey,
            std::make_unique<ProfileSurfaceComponent>(engine, makeMicKaossPadProfile()).release(), true);
        tabs_.addTab("Synth Kaoss", juce::Colours::darkslategrey,
            std::make_unique<ProfileSurfaceComponent>(engine, makeSynthKaossPadProfile()).release(), true);
        tabs_.addTab("Yaeltex", juce::Colours::darkslategrey,
            std::make_unique<ProfileSurfaceComponent>(engine, makeYaeltexLiveLoopingProfile()).release(), true);
        addAndMakeVisible(tabs_);
    }

    void resized() override
    {
        tabs_.setBounds(getLocalBounds());
    }

private:
    juce::TabbedComponent tabs_{juce::TabbedButtonBar::TabsAtTop};
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
        pseudoDevicesWindow_->setSize(920, 720);
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
