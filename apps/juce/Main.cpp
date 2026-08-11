#include "livelooping/core/ControlMapping.h"
#include "livelooping/core/LiveLoopingEngine.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
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

constexpr int kGroupHeaderHeight = 22;

juce::String displayGroupName(juce::String group)
{
    return group.replaceCharacter('_', ' ');
}

class ProfileSurfaceComponent final : public juce::Component {
public:
    ProfileSurfaceComponent(LiveLoopingEngine& engine, ControllerProfile profile)
        : engine_(engine),
          mapper_(std::move(profile)),
          kind_(mapper_.profile().id.find("yaeltex") != std::string::npos ? SurfaceKind::Yaeltex : SurfaceKind::Kaoss)
    {
        for (const auto& widget : mapper_.profile().widgets) {
            addWidget(widget);
        }
    }

    void paint(juce::Graphics& graphics) override
    {
        if (kind_ == SurfaceKind::Yaeltex) {
            paintYaeltex(graphics);
        } else {
            paintKaoss(graphics);
        }
    }

    void resized() override
    {
        layoutByGroup(kind_ == SurfaceKind::Yaeltex ? 14 : 12, kind_ == SurfaceKind::Yaeltex ? 10 : 8);
    }

private:
    enum class SurfaceKind {
        Kaoss,
        Yaeltex
    };

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
        group.label->setText(displayGroupName(groupName), juce::dontSendNotification);
        group.label->setJustificationType(juce::Justification::centredLeft);
        group.label->setColour(juce::Label::textColourId, juce::Colours::white);
        group.label->setFont(juce::Font(juce::FontOptions(16.0F)));
        addAndMakeVisible(*group.label);
        return group;
    }

    void addWidget(const ControllerWidget& widget)
    {
        ensureGroup(widget);

        if (widget.type == WidgetType::Button) {
            auto button = std::make_unique<juce::TextButton>(widget.label);
            const auto isYaeltex = kind_ == SurfaceKind::Yaeltex;
            button->setColour(juce::TextButton::buttonColourId, isYaeltex ? juce::Colour(0xffeceff0) : juce::Colour(0xff151a1f));
            button->setColour(juce::TextButton::buttonOnColourId, isYaeltex ? juce::Colour(0xffd00010) : juce::Colour(0xff4c1018));
            button->setColour(juce::TextButton::textColourOffId, isYaeltex ? juce::Colours::black : juce::Colours::white);
            button->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
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
        slider->setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffcf141c));
        slider->setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff5e666c));
        slider->setColour(juce::Slider::thumbColourId, juce::Colours::white);
        slider->setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff8a8f95));
        slider->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff151a1f));
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

    void paintKaoss(juce::Graphics& graphics)
    {
        graphics.fillAll(juce::Colour(0xff20262b));

        auto body = getLocalBounds().reduced(12);
        graphics.setColour(juce::Colour(0xff111417));
        graphics.fillRoundedRectangle(body.toFloat(), 8.0F);
        graphics.setColour(juce::Colour(0xff4b5258));
        graphics.drawRoundedRectangle(body.toFloat(), 8.0F, 2.0F);

        graphics.setColour(juce::Colours::white);
        graphics.setFont(juce::Font(juce::FontOptions(28.0F)));
        graphics.drawText("KAOSS PAD", body.removeFromTop(54), juce::Justification::centredLeft);

        const auto pad = kaossPadBounds();
        graphics.setColour(juce::Colour(0xff090b0e));
        graphics.fillRoundedRectangle(pad.toFloat(), 8.0F);
        graphics.setColour(juce::Colour(0xffd01824));
        graphics.drawRoundedRectangle(pad.toFloat(), 8.0F, 4.0F);
        graphics.setColour(juce::Colour(0x55d01824));
        for (int x = 1; x < 8; ++x) {
            const auto gx = pad.getX() + x * pad.getWidth() / 8;
            graphics.drawVerticalLine(gx, static_cast<float>(pad.getY()), static_cast<float>(pad.getBottom()));
        }
        for (int y = 1; y < 6; ++y) {
            const auto gy = pad.getY() + y * pad.getHeight() / 6;
            graphics.drawHorizontalLine(gy, static_cast<float>(pad.getX()), static_cast<float>(pad.getRight()));
        }
        graphics.setColour(juce::Colour(0x77ff2638));
        for (int index = 0; index < 16; ++index) {
            const auto cellX = index % 8;
            const auto cellY = (index * 5) % 6;
            graphics.fillRect(
                pad.getX() + cellX * pad.getWidth() / 8 + 4,
                pad.getY() + cellY * pad.getHeight() / 6 + 4,
                pad.getWidth() / 8 - 8,
                pad.getHeight() / 6 - 8);
        }
    }

    void paintYaeltex(juce::Graphics& graphics)
    {
        graphics.fillAll(juce::Colours::black);

        auto body = getLocalBounds().reduced(12);
        graphics.setColour(juce::Colours::black);
        graphics.fillRect(body);
        graphics.setColour(juce::Colour(0xffd00010));
        graphics.drawRect(body, 2);

        graphics.setColour(juce::Colours::white);
        graphics.setFont(juce::Font(juce::FontOptions(28.0F)));
        graphics.drawText("LIVELOOPING", body.removeFromTop(48), juce::Justification::centredLeft);

        graphics.setColour(juce::Colour(0xffd00010));
        graphics.drawVerticalLine(getWidth() * 2 / 3, 64.0F, static_cast<float>(getHeight() - 18));
        graphics.drawHorizontalLine(320, 18.0F, static_cast<float>(getWidth() - 18));
        graphics.drawHorizontalLine(548, 18.0F, static_cast<float>(getWidth() - 18));
    }

    juce::Rectangle<int> kaossPadBounds() const
    {
        auto area = getLocalBounds().reduced(24);
        return {area.getX() + 250, area.getY() + 220, 430, 300};
    }

    juce::Rectangle<int> groupOrigin(const juce::String& groupName) const
    {
        if (kind_ == SurfaceKind::Kaoss) {
            if (groupName == "presets") {
                return {250, 92, 76, 48};
            }
            if (groupName == "pages") {
                return {250, 154, 112, 48};
            }
            if (groupName == "levels") {
                return {46, 190, 94, 96};
            }
            if (groupName == "fx_parameters") {
                return {118, 562, 86, 86};
            }
            return {40, 620, 92, 58};
        }

        if (groupName == "looper_select") {
            return {32, 90, 128, 74};
        }
        if (groupName == "sample_length") {
            return {32, 212, 128, 74};
        }
        if (groupName == "track_record") {
            return {32, 394, 128, 74};
        }
        if (groupName == "track_clear") {
            return {32, 510, 128, 74};
        }
        if (groupName == "track_volume_pan") {
            return {590, 388, 102, 92};
        }
        if (groupName == "resampling") {
            return {590, 90, 128, 74};
        }
        if (groupName == "session") {
            return {590, 214, 128, 74};
        }
        return {32, 650, 110, 70};
    }

    void layoutByGroup(int horizontalGap, int verticalGap)
    {
        for (auto& group : groups_) {
            const auto origin = groupOrigin(group.name);
            group.label->setBounds(origin.getX(), std::max(18, origin.getY() - kGroupHeaderHeight - 8), 260, kGroupHeaderHeight);

            for (auto& control : controls_) {
                if (control.group != group.name) {
                    continue;
                }

                const auto x = origin.getX() + control.column * (origin.getWidth() + horizontalGap);
                const auto y = origin.getY() + control.row * (origin.getHeight() + verticalGap);
                control.component->setBounds(x, y, control.width * origin.getWidth(), control.height * origin.getHeight());
            }
        }
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
    SurfaceKind kind_ = SurfaceKind::Kaoss;
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
        pseudoDevicesWindow_->setSize(1360, 860);
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
