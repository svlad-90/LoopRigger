#include "livelooping/core/ControlMapping.h"
#include "livelooping/core/LiveLoopingEngine.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <cmath>
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

    void drawScrew(juce::Graphics& graphics, int x, int y)
    {
        graphics.setColour(juce::Colour(0xff050505));
        graphics.fillEllipse(static_cast<float>(x - 5), static_cast<float>(y - 5), 10.0F, 10.0F);
        graphics.setColour(juce::Colour(0xff343434));
        graphics.drawLine(static_cast<float>(x - 3), static_cast<float>(y + 2), static_cast<float>(x + 3), static_cast<float>(y - 2), 1.2F);
    }

    void drawPanelLabel(juce::Graphics& graphics, const juce::String& text, juce::Rectangle<int> area, float size = 11.0F)
    {
        graphics.setColour(juce::Colour(0xfff0f0f0));
        graphics.setFont(juce::Font(juce::FontOptions(size).withStyle("Bold")));
        graphics.drawText(text, area, juce::Justification::centred);
    }

    void drawHardwareButton(
        juce::Graphics& graphics,
        juce::Rectangle<int> area,
        const juce::String& text,
        juce::Colour fill,
        juce::Colour textColour,
        float corner = 4.0F)
    {
        graphics.setColour(juce::Colour(0x77000000));
        graphics.fillRoundedRectangle(area.translated(2, 2).toFloat(), corner);
        graphics.setColour(fill);
        graphics.fillRoundedRectangle(area.toFloat(), corner);
        graphics.setColour(juce::Colour(0xff9ca4a8));
        graphics.drawRoundedRectangle(area.toFloat(), corner, 1.0F);
        if (text.isNotEmpty()) {
            graphics.setColour(textColour);
            graphics.setFont(juce::Font(juce::FontOptions(12.0F).withStyle("Bold")));
            graphics.drawText(text, area.reduced(3), juce::Justification::centred);
        }
    }

    void drawKnob(juce::Graphics& graphics, int centreX, int centreY, int radius, const juce::String& label)
    {
        graphics.setColour(juce::Colour(0xffd8d8d8));
        for (int tick = 0; tick < 19; ++tick) {
            const auto angle = juce::MathConstants<float>::pi * (0.72F + static_cast<float>(tick) * 1.56F / 18.0F);
            const auto inner = static_cast<float>(radius + 9);
            const auto outer = static_cast<float>(radius + 18);
            const auto x1 = static_cast<float>(centreX) + std::cos(angle) * inner;
            const auto y1 = static_cast<float>(centreY) + std::sin(angle) * inner;
            const auto x2 = static_cast<float>(centreX) + std::cos(angle) * outer;
            const auto y2 = static_cast<float>(centreY) + std::sin(angle) * outer;
            graphics.drawLine(x1, y1, x2, y2, 2.0F);
        }

        graphics.setColour(juce::Colour(0xff070707));
        graphics.fillEllipse(static_cast<float>(centreX - radius), static_cast<float>(centreY - radius), static_cast<float>(radius * 2), static_cast<float>(radius * 2));
        graphics.setColour(juce::Colour(0xff303030));
        graphics.drawEllipse(static_cast<float>(centreX - radius), static_cast<float>(centreY - radius), static_cast<float>(radius * 2), static_cast<float>(radius * 2), 2.0F);
        graphics.setColour(juce::Colour(0xfff2f2f2));
        graphics.fillRect(centreX + radius / 3, centreY - 3, radius, 6);
        drawPanelLabel(graphics, label, {centreX - 52, centreY + radius + 15, 104, 16}, 9.5F);
    }

    void drawJoystick(juce::Graphics& graphics, juce::Rectangle<int> area, const juce::String& rangeLabel, const juce::String& valueLabel)
    {
        graphics.setColour(juce::Colour(0xff111111));
        graphics.fillRoundedRectangle(area.toFloat(), 12.0F);
        graphics.setColour(juce::Colour(0xff282828));
        graphics.drawRoundedRectangle(area.toFloat(), 12.0F, 4.0F);
        auto well = area.reduced(17);
        graphics.setColour(juce::Colour(0xff050505));
        graphics.fillRoundedRectangle(well.toFloat(), 7.0F);
        graphics.setColour(juce::Colour(0xff1f1f1f));
        graphics.drawRoundedRectangle(well.toFloat(), 7.0F, 2.0F);
        graphics.setColour(juce::Colour(0xff202020));
        graphics.fillEllipse(static_cast<float>(well.getCentreX() - 14), static_cast<float>(well.getCentreY() - 14), 28.0F, 28.0F);
        graphics.setColour(juce::Colour(0xff565656));
        graphics.drawLine(static_cast<float>(well.getCentreX() - 12), static_cast<float>(well.getCentreY() - 12), static_cast<float>(well.getCentreX() + 12), static_cast<float>(well.getCentreY() + 12), 3.0F);
        drawPanelLabel(graphics, valueLabel, {area.getRight() - 22, area.getY() + 8, 18, area.getHeight() - 16}, 12.0F);
        drawPanelLabel(graphics, rangeLabel, {area.getX(), area.getBottom() + 8, area.getWidth(), 18}, 13.0F);
    }

    void drawYaeltexButtonGrid(juce::Graphics& graphics, int x, int y, int columns, int rows, const juce::StringArray& labels)
    {
        int labelIndex = 0;
        for (int row = 0; row < rows; ++row) {
            for (int column = 0; column < columns; ++column) {
                const auto label = labelIndex < labels.size() ? labels[labelIndex] : juce::String();
                drawHardwareButton(graphics, {x + column * 70, y + row * 50, 54, 30}, label, juce::Colour(0xffe9edee), juce::Colours::black, 5.0F);
                ++labelIndex;
            }
        }
    }

    void drawYaeltexArcadeButton(juce::Graphics& graphics, int x, int y, juce::Colour colour, const juce::String& label)
    {
        graphics.setColour(juce::Colour(0xaa000000));
        graphics.fillEllipse(static_cast<float>(x + 5), static_cast<float>(y + 5), 68.0F, 68.0F);
        graphics.setColour(colour);
        graphics.fillEllipse(static_cast<float>(x), static_cast<float>(y), 68.0F, 68.0F);
        graphics.setColour(colour.brighter(0.45F));
        graphics.fillEllipse(static_cast<float>(x + 8), static_cast<float>(y + 7), 30.0F, 18.0F);
        graphics.setColour(juce::Colour(0xff101010));
        graphics.drawEllipse(static_cast<float>(x), static_cast<float>(y), 68.0F, 68.0F, 2.0F);
        drawPanelLabel(graphics, label, {x + 48, y + 48, 24, 18}, 13.0F);
    }

    void drawKaossMetalKnob(juce::Graphics& graphics, int centreX, int centreY, int radius, const juce::String& label)
    {
        graphics.setColour(juce::Colour(0xffc7b7a2));
        graphics.fillEllipse(static_cast<float>(centreX - radius), static_cast<float>(centreY - radius), static_cast<float>(radius * 2), static_cast<float>(radius * 2));
        graphics.setColour(juce::Colour(0xff2f2926));
        graphics.fillEllipse(static_cast<float>(centreX - radius + 10), static_cast<float>(centreY - radius + 10), static_cast<float>((radius - 10) * 2), static_cast<float>((radius - 10) * 2));
        graphics.setColour(juce::Colour(0xfff4eadc));
        graphics.drawLine(static_cast<float>(centreX), static_cast<float>(centreY), static_cast<float>(centreX + radius - 5), static_cast<float>(centreY - 8), 3.0F);
        graphics.setColour(juce::Colour(0xffd6d9dc));
        graphics.setFont(juce::Font(juce::FontOptions(11.0F).withStyle("Bold")));
        graphics.drawText(label, centreX - 52, centreY - radius - 34, 104, 28, juce::Justification::centred);
    }

    void paintKaoss(juce::Graphics& graphics)
    {
        graphics.fillAll(juce::Colour(0xfff5f5f5));

        auto body = getLocalBounds().reduced(22);
        graphics.setColour(juce::Colour(0xff1d2023));
        graphics.fillRoundedRectangle(body.toFloat(), 18.0F);
        graphics.setColour(juce::Colour(0xff454b4f));
        graphics.drawRoundedRectangle(body.toFloat(), 18.0F, 3.0F);
        graphics.setColour(juce::Colour(0xff131619));
        graphics.fillRect(body.reduced(220, 100));

        auto topDeck = body.reduced(180, 36).removeFromTop(190);
        graphics.setColour(juce::Colour(0xff252b30));
        graphics.fillRoundedRectangle(topDeck.toFloat(), 6.0F);

        drawKaossMetalKnob(graphics, body.getX() + 88, body.getY() + 116, 34, "INPUT\nVOLUME");
        drawKaossMetalKnob(graphics, body.getX() + 88, body.getY() + 254, 34, "FX DEPTH");
        drawKaossMetalKnob(graphics, body.getRight() - 88, body.getY() + 116, 34, "PROGRAM\nBPM");

        graphics.setColour(juce::Colour(0xff070707));
        graphics.fillRoundedRectangle(static_cast<float>(body.getX() + 66), static_cast<float>(body.getY() + 360), 18.0F, 230.0F, 5.0F);
        graphics.setColour(juce::Colour(0xffd8d0c6));
        graphics.fillRoundedRectangle(static_cast<float>(body.getX() + 47), static_cast<float>(body.getY() + 346), 58.0F, 18.0F, 3.0F);
        drawHardwareButton(graphics, {body.getX() + 42, body.getY() + 640, 94, 46}, "HOLD", juce::Colour(0xffbec3dc), juce::Colours::black, 6.0F);

        graphics.setColour(juce::Colours::white);
        graphics.setFont(juce::Font(juce::FontOptions(34.0F).withStyle("Bold")));
        graphics.drawText("KORG", body.getX() + 220, body.getY() + 42, 160, 44, juce::Justification::centredLeft);
        graphics.setFont(juce::Font(juce::FontOptions(31.0F)));
        graphics.drawText("KAOSS PAD", body.getX() + 405, body.getY() + 42, 240, 44, juce::Justification::centredLeft);
        graphics.setFont(juce::Font(juce::FontOptions(24.0F).withStyle("Bold")));
        graphics.drawText("KP3+", body.getRight() - 300, body.getY() + 50, 120, 34, juce::Justification::centred);

        auto display = juce::Rectangle<int>(body.getX() + 410, body.getY() + 104, 160, 54);
        graphics.setColour(juce::Colours::black);
        graphics.fillRoundedRectangle(display.toFloat(), 4.0F);
        graphics.setColour(juce::Colour(0xffff3155));
        graphics.setFont(juce::Font(juce::FontOptions(34.0F).withStyle("Bold")));
        graphics.drawText("FLT.1", display, juce::Justification::centred);
        drawHardwareButton(graphics, {display.getRight() + 22, display.getY(), 72, 24}, "PROG", juce::Colours::black, juce::Colours::white, 4.0F);
        drawHardwareButton(graphics, {display.getRight() + 104, display.getY(), 80, 24}, "WRITE", juce::Colour(0xff8190a0), juce::Colours::black, 4.0F);
        drawHardwareButton(graphics, {display.getRight() + 104, display.getY() + 35, 80, 24}, "SHIFT", juce::Colour(0xff8190a0), juce::Colours::black, 4.0F);

        graphics.setColour(juce::Colour(0xffb4b8bb));
        graphics.drawLine(static_cast<float>(body.getX() + 214), static_cast<float>(body.getY() + 196), static_cast<float>(body.getRight() - 214), static_cast<float>(body.getY() + 196), 2.0F);
        drawPanelLabel(graphics, "PROGRAM MEMORY", {body.getX() + 430, body.getY() + 176, 220, 18}, 12.0F);

        const auto pad = kaossPadBounds();
        graphics.setColour(juce::Colour(0xff2e3337));
        graphics.fillRoundedRectangle(pad.expanded(42, 38).toFloat(), 8.0F);
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

        drawHardwareButton(graphics, {body.getRight() - 126, body.getY() + 214, 78, 62}, "TAP/\nRANGE", juce::Colour(0xffd35a70), juce::Colours::black, 30.0F);
        drawHardwareButton(graphics, {body.getRight() - 118, body.getY() + 354, 78, 44}, "AUTO BPM", juce::Colour(0xffbfc6db), juce::Colours::black, 6.0F);
        drawHardwareButton(graphics, {body.getRight() - 118, body.getY() + 436, 78, 44}, "PAD\nMOTION", juce::Colour(0xff8090a0), juce::Colours::black, 6.0F);
        drawHardwareButton(graphics, {body.getRight() - 118, body.getY() + 514, 78, 44}, "MUTE", juce::Colour(0xff8090a0), juce::Colours::black, 6.0F);
        drawHardwareButton(graphics, {body.getRight() - 128, body.getY() + 618, 92, 50}, "SAMPLING", juce::Colour(0xff6d7f90), juce::Colours::black, 6.0F);

        drawPanelLabel(graphics, "SAMPLE BANK", {body.getX() + 335, body.getBottom() - 138, 420, 18}, 12.0F);
        drawHardwareButton(graphics, {body.getX() + 320, body.getBottom() - 94, 92, 52}, "A", juce::Colour(0xff3ccc78), juce::Colours::black, 6.0F);
        drawHardwareButton(graphics, {body.getX() + 470, body.getBottom() - 94, 92, 52}, "B", juce::Colour(0xffe38298), juce::Colours::black, 6.0F);
        drawHardwareButton(graphics, {body.getX() + 620, body.getBottom() - 94, 92, 52}, "C", juce::Colour(0xffe38298), juce::Colours::black, 6.0F);
        drawHardwareButton(graphics, {body.getX() + 770, body.getBottom() - 94, 92, 52}, "D", juce::Colour(0xffffd27d), juce::Colours::black, 6.0F);
    }

    void paintYaeltex(juce::Graphics& graphics)
    {
        graphics.fillAll(juce::Colour(0xff090909));

        auto frame = getLocalBounds().reduced(18);
        graphics.setColour(juce::Colour(0xffc6904d));
        graphics.fillRoundedRectangle(frame.toFloat(), 18.0F);
        graphics.setColour(juce::Colour(0xff7a4b21));
        graphics.drawRoundedRectangle(frame.toFloat(), 18.0F, 4.0F);

        auto body = frame.reduced(26);
        graphics.setColour(juce::Colours::black);
        graphics.fillRect(body);
        graphics.setColour(juce::Colour(0xffd00010));
        graphics.drawRect(body, 2);

        drawScrew(graphics, body.getX() + 14, body.getY() + 14);
        drawScrew(graphics, body.getRight() - 14, body.getY() + 14);
        drawScrew(graphics, body.getX() + 14, body.getBottom() - 14);
        drawScrew(graphics, body.getRight() - 14, body.getBottom() - 14);

        graphics.setColour(juce::Colours::white);
        graphics.setFont(juce::Font(juce::FontOptions(34.0F).withStyle("Bold")));
        graphics.drawText("LIVELOOPING", body.getX() + 58, body.getY() + 24, 300, 42, juce::Justification::centredLeft);
        graphics.setFont(juce::Font(juce::FontOptions(18.0F).withStyle("Bold")));
        graphics.drawText("YAELTEX", body.getX() + 62, body.getY() + 64, 150, 24, juce::Justification::centredLeft);

        graphics.setColour(juce::Colour(0xffeeeeee));
        const int left = body.getX() + 34;
        const int top = body.getY() + 114;
        const int right = body.getRight() - 34;
        graphics.drawLine(static_cast<float>(left), static_cast<float>(top), static_cast<float>(right), static_cast<float>(top), 2.0F);
        graphics.drawLine(static_cast<float>(left), static_cast<float>(top + 160), static_cast<float>(right), static_cast<float>(top + 160), 2.0F);
        graphics.drawLine(static_cast<float>(left), static_cast<float>(top + 310), static_cast<float>(right), static_cast<float>(top + 310), 2.0F);
        graphics.drawVerticalLine(body.getX() + 560, static_cast<float>(top), static_cast<float>(body.getBottom() - 40));
        graphics.drawVerticalLine(body.getX() + 995, static_cast<float>(body.getY() + 18), static_cast<float>(body.getBottom() - 40));

        drawYaeltexButtonGrid(graphics, body.getX() + 720, body.getY() + 24, 4, 2, {"4", "2", "1", "1/2", "1/4", "1/8", "1/16", "1/32"});
        drawYaeltexButtonGrid(graphics, body.getX() + 118, body.getY() + 158, 8, 2, {"Mute T1", "Mute T2", "Mute T3", "Mute T4", "Mute L1", "Mute L2", "Mute L3", "Mute L4", "Inv T1", "Inv T2", "Inv T3", "Inv T4", "Inv L1", "Inv L2", "Inv L3", "Inv L4"});
        drawYaeltexButtonGrid(graphics, body.getX() + 622, body.getY() + 312, 6, 2, {"FX1", "FX2", "FX3", "FX4", "FX5", "B1", "FX6", "FX7", "FX8", "FX9", "FX10", "B4"});
        drawYaeltexButtonGrid(graphics, body.getX() + 1010, body.getY() + 330, 4, 7, {"FRZ", "Drop", "Extra", "REC", "RST", "RST all", "Extra 2", "STOP", "Gate", "Gate all", "Extra 3", "REV", "CTRL", "min/max", "Nat.", "Reverb", "I", "V", "Harm.", "Delay", "II", "VI", "Melod.", "Phaser", "III", "VII", "CLEAR", ""});
        drawYaeltexButtonGrid(graphics, body.getX() + 622, body.getY() + 636, 8, 2, {"Ani.1", "Ani.2", "Ani.3", "Ani.4", "Ani.5", "Ani.6", "Ani.7", "Ani.8", "On/off", "Default", "Pr.1", "Pr.2", "Pr.3", "Pr.4", "Pr.5", "Pr.6"});

        for (int i = 0; i < 4; ++i) {
            drawKnob(graphics, body.getX() + 650 + i * 120, body.getY() + 262, 24, i == 0 ? "Dry/Wet" : (i == 1 ? "LFO1 Speed" : (i == 2 ? "LFO2 Speed" : "Drop FX")));
            drawKnob(graphics, body.getX() + 875 + i * 120, body.getY() + 110, 24, i == 0 ? "Vol/Drop" : (i == 1 ? "Reverb" : (i == 2 ? "Transpose" : "Phaser")));
            drawKnob(graphics, body.getX() + 100 + i * 145, body.getY() + 620, 28, "Vol/Pan T" + juce::String(i + 1));
            drawKnob(graphics, body.getX() + 665 + i * 116, body.getY() + 744, 23, "Sidechain");
        }

        drawJoystick(graphics, {body.getX() + 610, body.getY() + 420, 96, 96}, "Range 1", "Value 1");
        drawJoystick(graphics, {body.getX() + 820, body.getY() + 420, 96, 96}, "Range 2", "Value 2");

        const int masterX = body.getX() + 1245;
        drawKnob(graphics, masterX, body.getY() + 330, 30, "Master tempo");
        drawKnob(graphics, masterX + 120, body.getY() + 330, 30, "Pitch shift");
        drawKnob(graphics, masterX, body.getY() + 465, 30, "Master vol.");
        drawKnob(graphics, masterX + 120, body.getY() + 465, 30, "Pitch dry/wet");
        drawKnob(graphics, masterX, body.getY() + 600, 30, "VOLUME");
        drawKnob(graphics, masterX + 120, body.getY() + 600, 30, "Distortion");
        drawKnob(graphics, masterX, body.getY() + 735, 30, "Pan");
        drawKnob(graphics, masterX + 120, body.getY() + 735, 30, "Dist dry/wet");

        drawYaeltexArcadeButton(graphics, body.getX() + 1130, body.getY() + 648, juce::Colour(0xff24d947), "1");
        drawYaeltexArcadeButton(graphics, body.getX() + 1212, body.getY() + 648, juce::Colour(0xffe32626), "2");
        drawYaeltexArcadeButton(graphics, body.getX() + 1294, body.getY() + 648, juce::Colour(0xff20aeea), "3");
        drawYaeltexArcadeButton(graphics, body.getX() + 1376, body.getY() + 648, juce::Colour(0xffffd21f), "4");
        drawYaeltexArcadeButton(graphics, body.getX() + 1130, body.getY() + 735, juce::Colour(0xff24d947), "5");
        drawYaeltexArcadeButton(graphics, body.getX() + 1212, body.getY() + 735, juce::Colour(0xffe32626), "6");
        drawYaeltexArcadeButton(graphics, body.getX() + 1294, body.getY() + 735, juce::Colour(0xff20aeea), "7");
        drawYaeltexArcadeButton(graphics, body.getX() + 1376, body.getY() + 735, juce::Colour(0xffffd21f), "8");
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
                return {285, 225, 68, 44};
            }
            if (groupName == "pages") {
                return {285, 730, 86, 48};
            }
            if (groupName == "levels") {
                return {64, 100, 78, 90};
            }
            if (groupName == "fx_parameters") {
                return {292, 612, 78, 82};
            }
            return {40, 620, 92, 58};
        }

        if (groupName == "looper_select") {
            return {64, 330, 82, 72};
        }
        if (groupName == "sample_length") {
            return {64, 420, 56, 46};
        }
        if (groupName == "track_record") {
            return {64, 525, 82, 62};
        }
        if (groupName == "track_clear") {
            return {64, 618, 82, 54};
        }
        if (groupName == "track_volume_pan") {
            return {88, 702, 94, 86};
        }
        if (groupName == "resampling") {
            return {430, 692, 108, 58};
        }
        if (groupName == "session") {
            return {760, 72, 82, 54};
        }
        return {32, 650, 110, 70};
    }

    void layoutByGroup(int horizontalGap, int verticalGap)
    {
        for (auto& group : groups_) {
            const auto origin = groupOrigin(group.name);
            group.label->setVisible(false);
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
        pseudoDevicesWindow_->setSize(1520, 900);
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
