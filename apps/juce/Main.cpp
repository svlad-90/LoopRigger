#include "loop_rigger/control/ControlMapping.h"
#include "loop_rigger/core/LiveLoopingEngine.h"

#if LIVELOOPING_HAS_PROFILE_IO
#include "loop_rigger/profile_io/ProfileLoader.h"
#endif

#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

using loop_rigger::control::ControllerProfile;
using loop_rigger::control::ControllerWidget;
using loop_rigger::core::LiveLoopingEngine;
using loop_rigger::control::MidiMapper;
using loop_rigger::control::WidgetEvent;
using loop_rigger::control::WidgetEventType;
using loop_rigger::control::WidgetType;
using loop_rigger::control::makeMicKaossPadProfile;
using loop_rigger::control::makeSynthKaossPadProfile;
using loop_rigger::control::makeYaeltexLiveLoopingProfile;
#if LIVELOOPING_HAS_PROFILE_IO
using loop_rigger::profile_io::ControlSurfaceLayout;
using loop_rigger::profile_io::SurfaceBounds;
using loop_rigger::profile_io::SurfaceElement;
using loop_rigger::profile_io::SurfaceElementShape;
using loop_rigger::profile_io::SurfaceElementRole;
using loop_rigger::profile_io::loadControllerProfileFromFile;
using loop_rigger::profile_io::loadControlSurfaceLayoutFromFile;
#endif

namespace {

constexpr int kGroupHeaderHeight = 22;
#if LIVELOOPING_HAS_PROFILE_IO
constexpr float kEditSnapStep = 4.0F;
#endif

juce::String displayGroupName(juce::String group)
{
    return group.replaceCharacter('_', ' ');
}

#if LIVELOOPING_HAS_PROFILE_IO
ControllerProfile loadProfileOrFallback(const char* fileName, ControllerProfile fallback)
{
    try {
        return loadControllerProfileFromFile(std::string(LIVELOOPING_PROFILE_DIR) + "/" + fileName);
    } catch (const std::exception&) {
        return fallback;
    }
}

std::optional<ControlSurfaceLayout> loadOptionalLayout(const char* fileName)
{
    try {
        return loadControlSurfaceLayoutFromFile(std::string(LIVELOOPING_LAYOUT_DIR) + "/" + fileName);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}
#endif

class ProfileSurfaceComponent final : public juce::Component,
                                      private juce::Timer,
                                      private juce::KeyListener {
public:
    ProfileSurfaceComponent(
        LiveLoopingEngine& engine,
        ControllerProfile profile
#if LIVELOOPING_HAS_PROFILE_IO
        ,
        std::optional<ControlSurfaceLayout> layout = std::nullopt,
        std::string layoutPath = {}
#endif
        )
        : engine_(engine),
          mapper_(std::move(profile)),
          kind_(mapper_.profile().id.find("yaeltex") != std::string::npos ? SurfaceKind::Yaeltex : SurfaceKind::Kaoss)
#if LIVELOOPING_HAS_PROFILE_IO
          ,
          layout_(std::move(layout)),
          layoutPath_(std::move(layoutPath))
#endif
    {
        setWantsKeyboardFocus(true);
        for (const auto& widget : mapper_.profile().widgets) {
            addWidget(widget);
        }
#if LIVELOOPING_HAS_PROFILE_IO
        if (layout_.has_value()) {
            setupEditToolbar();
            startTimerHz(30);
        }
#endif
    }

    void paint(juce::Graphics& graphics) override
    {
#if LIVELOOPING_HAS_PROFILE_IO
        if (layout_.has_value()) {
            paintLayoutSurface(graphics);
            return;
        }
#endif
        if (kind_ == SurfaceKind::Yaeltex) {
            paintYaeltex(graphics);
        } else {
            paintKaoss(graphics);
        }
    }

    void resized() override
    {
        layoutByGroup(kind_ == SurfaceKind::Yaeltex ? 14 : 12, kind_ == SurfaceKind::Yaeltex ? 10 : 8);
#if LIVELOOPING_HAS_PROFILE_IO
        layoutEditToolbar();
#endif
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        return handleKeyPress(key);
    }

    bool keyPressed(const juce::KeyPress& key, juce::Component*) override
    {
        return handleKeyPress(key);
    }

    bool handleKeyPress(const juce::KeyPress& key)
    {
#if LIVELOOPING_HAS_PROFILE_IO
        if (!layout_.has_value()) {
            return false;
        }

        if (key.getTextCharacter() == 'e' || key.getTextCharacter() == 'E') {
            setEditMode(!editMode_);
            return true;
        }
        const auto character = key.getTextCharacter();
        const auto keyCode = key.getKeyCode();
        const auto wantsUndo = (character == 'z' || character == 'Z' || character == 26 || keyCode == 'z' || keyCode == 'Z')
            && (key.getModifiers().isCtrlDown() || key.getModifiers().isCommandDown());
        if (editMode_ && wantsUndo) {
            undoLastEdit();
            return true;
        }
        if (editMode_ && selectedElement_ >= 0) {
            const auto amount = key.getModifiers().isShiftDown() ? 10.0F : 1.0F;
            const auto resize = key.getModifiers().isAltDown();
            if (keyCode == juce::KeyPress::leftKey) {
                nudgeSelected(-amount, 0.0F, resize);
                return true;
            }
            if (keyCode == juce::KeyPress::rightKey) {
                nudgeSelected(amount, 0.0F, resize);
                return true;
            }
            if (keyCode == juce::KeyPress::upKey) {
                nudgeSelected(0.0F, -amount, resize);
                return true;
            }
            if (keyCode == juce::KeyPress::downKey) {
                nudgeSelected(0.0F, amount, resize);
                return true;
            }
        }
        if (editMode_ && (key.getTextCharacter() == 's' || key.getTextCharacter() == 'S')) {
            saveLayout();
            return true;
        }
        if (editMode_ && key.getKeyCode() == juce::KeyPress::deleteKey) {
            selectedElement_ = -1;
            repaint();
            return true;
        }
#else
        juce::ignoreUnused(key);
#endif
        return false;
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
#if LIVELOOPING_HAS_PROFILE_IO
        if (!layout_.has_value()) {
            return;
        }

        grabKeyboardFocus();
        if (event.mods.isPopupMenu()) {
            if (editMode_) {
                selectedElement_ = findElementAt(event.position);
            }
            updateEditToolbar();
            repaint();
            showEditContextMenu();
            return;
        }

        if (!editMode_) {
            return;
        }

        selectedElement_ = findElementAt(event.position);
        editDragMode_ = EditDragMode::None;
        if (selectedElement_ >= 0) {
            pushUndoSnapshot();
            const auto bounds = scaledBounds(layout_->elements[static_cast<size_t>(selectedElement_)].bounds);
            editDragMode_ = resizeHandle(bounds).contains(event.position.roundToInt()) ? EditDragMode::Resize : EditDragMode::Move;
            editStartMouse_ = event.position;
            editStartBounds_ = layout_->elements[static_cast<size_t>(selectedElement_)].bounds;
        }
        updateEditToolbar();
        repaint();
#else
        juce::ignoreUnused(event);
#endif
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
#if LIVELOOPING_HAS_PROFILE_IO
        if (!editMode_ || !layout_.has_value() || selectedElement_ < 0 || editDragMode_ == EditDragMode::None) {
            return;
        }

        auto& element = layout_->elements[static_cast<size_t>(selectedElement_)];
        const auto delta = toLayoutDelta(event.position - editStartMouse_);
        auto bounds = editStartBounds_;
        if (editDragMode_ == EditDragMode::Resize) {
            bounds.width = juce::jmax(4.0F, editStartBounds_.width + delta.x);
            bounds.height = juce::jmax(4.0F, editStartBounds_.height + delta.y);
        } else {
            bounds.x = editStartBounds_.x + delta.x;
            bounds.y = editStartBounds_.y + delta.y;
        }
        element.bounds = clampBounds(snapBounds(bounds, event.mods.isShiftDown()));
        layoutByGroup(kind_ == SurfaceKind::Yaeltex ? 14 : 12, kind_ == SurfaceKind::Yaeltex ? 10 : 8);
        updateEditToolbar();
        repaint();
#else
        juce::ignoreUnused(event);
#endif
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
#if LIVELOOPING_HAS_PROFILE_IO
        juce::ignoreUnused(event);
        editDragMode_ = EditDragMode::None;
#else
        juce::ignoreUnused(event);
#endif
    }

    void timerCallback() override
    {
#if LIVELOOPING_HAS_PROFILE_IO
        if (layout_.has_value()) {
            repaint();
        }
#endif
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
        juce::String id;
        juce::String group;
        WidgetType type = WidgetType::Button;
        int row = 0;
        int column = 0;
        int width = 1;
        int height = 1;
    };

    struct WidgetVisualState {
        WidgetVisualState(bool isHover = false, bool isDown = false, bool hasControlValue = false, float controlValue = 0.0F)
            : hover(isHover), down(isDown), hasValue(hasControlValue), value(controlValue)
        {
        }

        bool hover;
        bool down;
        bool hasValue;
        float value;
    };

#if LIVELOOPING_HAS_PROFILE_IO
    enum class EditDragMode {
        None,
        Move,
        Resize
    };
#endif

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
            button->setWantsKeyboardFocus(false);
            button->addKeyListener(this);
            const auto isYaeltex = kind_ == SurfaceKind::Yaeltex;
            button->setColour(juce::TextButton::buttonColourId, isYaeltex ? juce::Colour(0xffeceff0) : juce::Colour(0xff151a1f));
            button->setColour(juce::TextButton::buttonOnColourId, isYaeltex ? juce::Colour(0xffd00010) : juce::Colour(0xff4c1018));
            button->setColour(juce::TextButton::textColourOffId, isYaeltex ? juce::Colours::black : juce::Colours::white);
            button->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            button->onClick = [this, id = widget.id] {
                dispatch({id, WidgetEventType::Press, 1.0F});
                repaint();
            };
            button->onStateChange = [this] {
                repaint();
            };
#if LIVELOOPING_HAS_PROFILE_IO
            if (layout_.has_value()) {
                button->setAlpha(0.0F);
            }
#endif
            addAndMakeVisible(*button);
            controls_.push_back(makeControl(*button, widget));
            buttons_.push_back(std::move(button));
            return;
        }

        auto slider = std::make_unique<juce::Slider>();
        slider->setWantsKeyboardFocus(false);
        slider->addKeyListener(this);
        slider->setRange(0.0, 1.0, 0.001);
        slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider->setName(widget.label);
        slider->setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffcf141c));
        slider->setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff5e666c));
        slider->setColour(juce::Slider::thumbColourId, juce::Colours::white);
        slider->setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff8a8f95));
        slider->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff151a1f));
        if (widget.type == WidgetType::Fader || (kind_ == SurfaceKind::Kaoss && widget.group == "levels")) {
            slider->setSliderStyle(juce::Slider::LinearVertical);
        } else {
            slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        }
        slider->onValueChange = [this, id = widget.id, slider = slider.get()] {
            dispatch({id, WidgetEventType::Change, static_cast<float>(slider->getValue())});
            repaint();
        };
        slider->onDragStart = [this] {
            repaint();
        };
        slider->onDragEnd = [this] {
            repaint();
        };
#if LIVELOOPING_HAS_PROFILE_IO
        if (layout_.has_value()) {
            slider->setAlpha(0.0F);
        }
#endif
        addAndMakeVisible(*slider);
        controls_.push_back(makeControl(*slider, widget));
        sliders_.push_back(std::move(slider));
    }

#if LIVELOOPING_HAS_PROFILE_IO
    juce::Rectangle<int> scaledBounds(const SurfaceBounds& bounds) const
    {
        const auto transform = layoutTransform();
        return {
            static_cast<int>(std::round(transform.offsetX + bounds.x * transform.scale)),
            static_cast<int>(std::round(transform.offsetY + bounds.y * transform.scale)),
            static_cast<int>(std::round(bounds.width * transform.scale)),
            static_cast<int>(std::round(bounds.height * transform.scale)),
        };
    }

    struct LayoutTransform {
        float scale = 1.0F;
        float offsetX = 0.0F;
        float offsetY = 0.0F;
    };

    LayoutTransform layoutTransform() const
    {
        auto area = getLocalBounds();
        area.removeFromTop(editToolbarHeight());
        const auto scaleX = layout_->baseWidth > 0 ? static_cast<float>(area.getWidth()) / static_cast<float>(layout_->baseWidth) : 1.0F;
        const auto scaleY = layout_->baseHeight > 0 ? static_cast<float>(area.getHeight()) / static_cast<float>(layout_->baseHeight) : 1.0F;
        const auto scale = juce::jmin(scaleX, scaleY);
        const auto canvasWidth = static_cast<float>(layout_->baseWidth) * scale;
        const auto canvasHeight = static_cast<float>(layout_->baseHeight) * scale;
        return {
            scale,
            static_cast<float>(area.getX()) + (static_cast<float>(area.getWidth()) - canvasWidth) * 0.5F,
            static_cast<float>(area.getY()) + (static_cast<float>(area.getHeight()) - canvasHeight) * 0.5F,
        };
    }

    juce::Point<float> toLayoutDelta(juce::Point<float> screenDelta) const
    {
        const auto transform = layoutTransform();
        const auto scale = transform.scale > 0.0F ? transform.scale : 1.0F;
        return {screenDelta.x / scale, screenDelta.y / scale};
    }

    SurfaceBounds clampBounds(SurfaceBounds bounds) const
    {
        bounds.width = juce::jmax(4.0F, bounds.width);
        bounds.height = juce::jmax(4.0F, bounds.height);
        bounds.x = juce::jlimit(0.0F, static_cast<float>(layout_->baseWidth) - bounds.width, bounds.x);
        bounds.y = juce::jlimit(0.0F, static_cast<float>(layout_->baseHeight) - bounds.height, bounds.y);
        return bounds;
    }

    SurfaceBounds snapBounds(SurfaceBounds bounds, bool bypassSnap) const
    {
        if (!snapToGrid_ || bypassSnap) {
            return bounds;
        }

        bounds.x = snapValue(bounds.x);
        bounds.y = snapValue(bounds.y);
        bounds.width = snapValue(bounds.width);
        bounds.height = snapValue(bounds.height);
        return bounds;
    }

    static float snapValue(float value)
    {
        return std::round(value / kEditSnapStep) * kEditSnapStep;
    }

    juce::Rectangle<int> resizeHandle(juce::Rectangle<int> bounds) const
    {
        return {bounds.getRight() - 10, bounds.getBottom() - 10, 14, 14};
    }

    int findElementAt(juce::Point<float> position) const
    {
        for (int index = static_cast<int>(layout_->elements.size()) - 1; index >= 0; --index) {
            if (scaledBounds(layout_->elements[static_cast<size_t>(index)].bounds).contains(position.roundToInt())) {
                return index;
            }
        }
        return -1;
    }

    const SurfaceElement* findLayoutWidget(const juce::String& widgetId) const
    {
        if (!layout_.has_value()) {
            return nullptr;
        }

        for (const auto& element : layout_->elements) {
            if (element.role == SurfaceElementRole::Widget && element.widgetId == widgetId.toStdString()) {
                return &element;
            }
        }
        return nullptr;
    }

    const Control* findControl(const std::string& widgetId) const
    {
        const auto id = juce::String(widgetId);
        for (const auto& control : controls_) {
            if (control.id == id) {
                return &control;
            }
        }
        return nullptr;
    }

    WidgetVisualState visualStateFor(const SurfaceElement& element) const
    {
        WidgetVisualState state;
        if (element.role != SurfaceElementRole::Widget) {
            return state;
        }

        if (const auto* control = findControl(element.widgetId)) {
            state.hover = control->component != nullptr && control->component->isMouseOver();
            state.down = control->component != nullptr && control->component->isMouseButtonDown();
            if (const auto* slider = dynamic_cast<const juce::Slider*>(control->component)) {
                state.hasValue = true;
                state.value = static_cast<float>(juce::jlimit(0.0, 1.0, slider->getValue()));
            }
        }
        return state;
    }

    void paintLayoutSurface(juce::Graphics& graphics)
    {
        graphics.fillAll(kind_ == SurfaceKind::Yaeltex ? juce::Colour(0xff090909) : juce::Colour(0xfff5f5f5));

        if (editModeButton_ != nullptr) {
            graphics.setColour(juce::Colour(0xee151b20));
            graphics.fillRect(getLocalBounds().withHeight(editToolbarHeight()));
        }

        for (const auto& element : layout_->elements) {
            paintLayoutDecoration(graphics, element);
        }
        paintEditOverlay(graphics);
    }

    void paintEditOverlay(juce::Graphics& graphics)
    {
        if (!editMode_) {
            return;
        }

        for (int index = 0; index < static_cast<int>(layout_->elements.size()); ++index) {
            const auto& element = layout_->elements[static_cast<size_t>(index)];
            const auto bounds = scaledBounds(element.bounds);
            const auto selected = index == selectedElement_;
            graphics.setColour(selected ? juce::Colour(0xffffd21f) : juce::Colour(0x6631b8d8));
            graphics.drawRect(bounds, selected ? 2 : 1);
            if (selected) {
                graphics.fillRect(resizeHandle(bounds));
                graphics.setColour(juce::Colour(0xdd000000));
                auto labelBounds = bounds.withHeight(20).translated(0, -22);
                labelBounds.setWidth(juce::jmax(labelBounds.getWidth(), 260));
                graphics.fillRect(labelBounds);
                graphics.setColour(juce::Colours::white);
                const auto text = juce::String(element.id) + "  x=" + juce::String(element.bounds.x, 1)
                    + " y=" + juce::String(element.bounds.y, 1)
                    + " шир=" + juce::String(element.bounds.width, 1)
                    + " выс=" + juce::String(element.bounds.height, 1);
                graphics.drawText(text, labelBounds.reduced(4, 0), juce::Justification::centredLeft);
            }
        }
    }

    void paintLayoutDecoration(juce::Graphics& graphics, const SurfaceElement& element)
    {
        const auto bounds = scaledBounds(element.bounds);
        const auto variant = juce::String(element.variant);
        const auto darkPanel = kind_ == SurfaceKind::Yaeltex ? juce::Colours::black : juce::Colour(0xff1d2023);
        const auto softPanel = kind_ == SurfaceKind::Yaeltex ? juce::Colour(0xff101010) : juce::Colour(0xff252b30);
        const auto border = kind_ == SurfaceKind::Yaeltex ? juce::Colour(0xffd00010) : juce::Colour(0xff454b4f);
        const auto state = visualStateFor(element);

        switch (element.shape) {
        case SurfaceElementShape::RoundRect:
            if (variant == "wood_frame") {
                graphics.setColour(juce::Colour(0xffc6904d));
                graphics.fillRoundedRectangle(bounds.toFloat(), 18.0F);
                graphics.setColour(juce::Colour(0xff7a4b21));
                graphics.drawRoundedRectangle(bounds.toFloat(), 18.0F, 4.0F);
            } else if (variant == "hardware_button" || variant == "interactive_button") {
                drawHardwareButton(graphics, bounds, element.label, buttonFill(juce::Colour(0xffe9edee), state), juce::Colours::black, 5.0F, state);
            } else if (variant == "kaoss_button") {
                drawHardwareButton(graphics, bounds, element.label, juce::Colour(0xff8090a0), juce::Colours::black, 6.0F);
            } else if (variant == "kaoss_light_button") {
                drawHardwareButton(graphics, bounds, element.label, juce::Colour(0xffbfc6db), juce::Colours::black, 6.0F);
            } else if (variant == "kaoss_red_button") {
                drawHardwareButton(graphics, bounds, element.label, juce::Colour(0xffd35a70), juce::Colours::black, 30.0F);
            } else if (variant.startsWith("arcade_")) {
                drawYaeltexArcadeButton(graphics, bounds.getX(), bounds.getY(), buttonFill(arcadeColour(variant), state), element.label, state);
            } else {
                graphics.setColour(variant == "display" ? juce::Colours::black : (variant == "top_deck" ? softPanel : darkPanel));
                graphics.fillRoundedRectangle(bounds.toFloat(), variant == "top_deck" ? 6.0F : 14.0F);
                graphics.setColour(border);
                graphics.drawRoundedRectangle(bounds.toFloat(), variant == "top_deck" ? 6.0F : 14.0F, 2.0F);
            }
            break;
        case SurfaceElementShape::Rect:
            if (variant == "xy_pad") {
                drawKaossPad(graphics, bounds);
            } else {
                graphics.setColour(variant == "inner_panel" ? juce::Colour(0xff131619) : darkPanel);
                graphics.fillRect(bounds);
                graphics.setColour(variant == "inner_panel" ? juce::Colour(0xff131619) : border);
                graphics.drawRect(bounds, variant == "faceplate" ? 2 : 1);
            }
            break;
        case SurfaceElementShape::Text:
            graphics.setColour(juce::Colours::white);
            graphics.setFont(layoutFont(variant));
            graphics.drawText(element.label, bounds, juce::Justification::centredLeft);
            break;
        case SurfaceElementShape::Circle:
            if (variant == "screw") {
                drawScrew(graphics, bounds.getCentreX(), bounds.getCentreY());
            } else if (variant == "led_red") {
                graphics.setColour(juce::Colour(0xffff3648));
                graphics.fillEllipse(bounds.toFloat());
            } else {
                graphics.setColour(darkPanel);
                graphics.fillEllipse(bounds.toFloat());
                graphics.setColour(border);
                graphics.drawEllipse(bounds.toFloat(), 2.0F);
            }
            break;
        case SurfaceElementShape::Line:
            graphics.setColour(juce::Colour(0xffeeeeee));
            graphics.drawLine(
                static_cast<float>(bounds.getX()),
                static_cast<float>(bounds.getY()),
                static_cast<float>(bounds.getRight()),
                static_cast<float>(bounds.getBottom()),
                2.0F);
            break;
        case SurfaceElementShape::Knob:
            if (variant == "metal_knob") {
                drawKaossMetalKnob(graphics, bounds.getCentreX(), bounds.getCentreY(), bounds.getWidth() / 2, element.label);
            } else {
                drawKnob(graphics, bounds.getCentreX(), bounds.getCentreY(), juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2, element.label, state);
            }
            break;
        case SurfaceElementShape::Fader:
            drawFader(graphics, bounds, state);
            break;
        case SurfaceElementShape::Joystick:
            drawJoystick(graphics, bounds, element.label, variant == "joystick_value_2" ? "Value 2" : "Value 1", state);
            break;
        }
    }
#endif

    juce::Font layoutFont(const juce::String& variant) const
    {
        if (variant == "brand") {
            return juce::Font(juce::FontOptions(34.0F).withStyle("Bold"));
        }
        if (variant == "brand_sub") {
            return juce::Font(juce::FontOptions(18.0F).withStyle("Bold"));
        }
        if (variant == "display_text") {
            return juce::Font(juce::FontOptions(30.0F).withStyle("Bold"));
        }
        if (variant == "panel_label") {
            return juce::Font(juce::FontOptions(12.0F).withStyle("Bold"));
        }
        return juce::Font(juce::FontOptions(24.0F).withStyle("Bold"));
    }

    juce::Colour arcadeColour(const juce::String& variant) const
    {
        if (variant == "arcade_red") {
            return juce::Colour(0xffe32626);
        }
        if (variant == "arcade_blue") {
            return juce::Colour(0xff20aeea);
        }
        if (variant == "arcade_yellow") {
            return juce::Colour(0xffffd21f);
        }
        return juce::Colour(0xff24d947);
    }

    juce::Colour buttonFill(juce::Colour base, const WidgetVisualState& state) const
    {
        if (state.down) {
            return base.interpolatedWith(kind_ == SurfaceKind::Yaeltex ? juce::Colour(0xffd00010) : juce::Colour(0xffff3155), 0.45F);
        }
        if (state.hover) {
            return base.brighter(0.22F);
        }
        return base;
    }

    void drawFader(juce::Graphics& graphics, juce::Rectangle<int> area, WidgetVisualState state = WidgetVisualState())
    {
        const auto rail = area.withSizeKeepingCentre(18, area.getHeight());
        graphics.setColour(juce::Colour(0xff050505));
        graphics.fillRoundedRectangle(rail.toFloat(), 5.0F);
        if (state.hasValue) {
            const auto fillTop = rail.getBottom() - static_cast<int>(std::round(static_cast<float>(rail.getHeight()) * state.value));
            graphics.setColour(juce::Colour(0xff31b8d8));
            graphics.fillRoundedRectangle(rail.withTop(fillTop).toFloat(), 5.0F);
        }
        graphics.setColour(juce::Colour(0xffd8d0c6));
        const auto thumbY = state.hasValue
            ? area.getBottom() - static_cast<int>(std::round(static_cast<float>(area.getHeight()) * state.value)) - 9
            : area.getY() + area.getHeight() / 2 - 9;
        graphics.fillRoundedRectangle(area.withHeight(18).withY(thumbY).toFloat(), 3.0F);
    }

    void drawKaossPad(juce::Graphics& graphics, juce::Rectangle<int> pad)
    {
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
        float corner = 4.0F,
        WidgetVisualState state = WidgetVisualState())
    {
        if (state.down) {
            area.translate(1, 1);
        }
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
        if (state.hover || state.down) {
            graphics.setColour(state.down ? juce::Colour(0x99ffffff) : juce::Colour(0x55ffffff));
            graphics.drawRoundedRectangle(area.reduced(1).toFloat(), corner, state.down ? 2.0F : 1.2F);
        }
    }

    void drawKnob(juce::Graphics& graphics, int centreX, int centreY, int radius, const juce::String& label, WidgetVisualState state = WidgetVisualState())
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
        if (state.hover || state.down) {
            graphics.setColour(state.down ? juce::Colour(0x7731b8d8) : juce::Colour(0x4425a9c8));
            graphics.fillEllipse(static_cast<float>(centreX - radius), static_cast<float>(centreY - radius), static_cast<float>(radius * 2), static_cast<float>(radius * 2));
        }
        graphics.setColour(juce::Colour(0xff303030));
        graphics.drawEllipse(static_cast<float>(centreX - radius), static_cast<float>(centreY - radius), static_cast<float>(radius * 2), static_cast<float>(radius * 2), 2.0F);
        graphics.setColour(juce::Colour(0xfff2f2f2));
        const auto value = state.hasValue ? state.value : 0.68F;
        const auto angle = juce::MathConstants<float>::pi * (0.72F + value * 1.56F);
        const auto pointerInner = static_cast<float>(radius * 0.15F);
        const auto pointerOuter = static_cast<float>(radius * 0.9F);
        graphics.drawLine(
            static_cast<float>(centreX) + std::cos(angle) * pointerInner,
            static_cast<float>(centreY) + std::sin(angle) * pointerInner,
            static_cast<float>(centreX) + std::cos(angle) * pointerOuter,
            static_cast<float>(centreY) + std::sin(angle) * pointerOuter,
            5.0F);
        drawPanelLabel(graphics, label, {centreX - 52, centreY + radius + 15, 104, 16}, 9.5F);
    }

    void drawJoystick(juce::Graphics& graphics, juce::Rectangle<int> area, const juce::String& rangeLabel, const juce::String& valueLabel, WidgetVisualState state = WidgetVisualState())
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
        const auto offset = state.hasValue ? static_cast<int>(std::round((state.value - 0.5F) * 22.0F)) : 0;
        graphics.fillEllipse(static_cast<float>(well.getCentreX() - 14 + offset), static_cast<float>(well.getCentreY() - 14), 28.0F, 28.0F);
        graphics.setColour(juce::Colour(0xff565656));
        graphics.drawLine(static_cast<float>(well.getCentreX() - 12 + offset), static_cast<float>(well.getCentreY() - 12), static_cast<float>(well.getCentreX() + 12 + offset), static_cast<float>(well.getCentreY() + 12), 3.0F);
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

    void drawYaeltexArcadeButton(juce::Graphics& graphics, int x, int y, juce::Colour colour, const juce::String& label, WidgetVisualState state = WidgetVisualState())
    {
        if (state.down) {
            x += 2;
            y += 2;
        }
        graphics.setColour(juce::Colour(0xaa000000));
        graphics.fillEllipse(static_cast<float>(x + 5), static_cast<float>(y + 5), 68.0F, 68.0F);
        graphics.setColour(colour);
        graphics.fillEllipse(static_cast<float>(x), static_cast<float>(y), 68.0F, 68.0F);
        graphics.setColour(colour.brighter(0.45F));
        graphics.fillEllipse(static_cast<float>(x + 8), static_cast<float>(y + 7), 30.0F, 18.0F);
        graphics.setColour(juce::Colour(0xff101010));
        graphics.drawEllipse(static_cast<float>(x), static_cast<float>(y), 68.0F, 68.0F, 2.0F);
        if (state.hover || state.down) {
            graphics.setColour(state.down ? juce::Colour(0xaaffffff) : juce::Colour(0x66ffffff));
            graphics.drawEllipse(static_cast<float>(x + 2), static_cast<float>(y + 2), 64.0F, 64.0F, state.down ? 3.0F : 2.0F);
        }
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

        auto body = surfaceBodyBounds();
        graphics.setColour(juce::Colour(0xff1d2023));
        graphics.fillRoundedRectangle(body.toFloat(), 18.0F);
        graphics.setColour(juce::Colour(0xff454b4f));
        graphics.drawRoundedRectangle(body.toFloat(), 18.0F, 3.0F);
        graphics.setColour(juce::Colour(0xff131619));
        graphics.fillRect(body.reduced(220, 100));

        auto topDeck = body.reduced(180, 36).removeFromTop(190);
        graphics.setColour(juce::Colour(0xff252b30));
        graphics.fillRoundedRectangle(topDeck.toFloat(), 6.0F);

        drawPanelLabel(graphics, "INPUT VOLUME", {body.getX() + 40, body.getY() + 52, 110, 22}, 11.0F);
        drawPanelLabel(graphics, "FX DEPTH", {body.getX() + 40, body.getY() + 186, 110, 22}, 11.0F);
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

        drawPanelLabel(graphics, "SAMPLE BANK", {body.getX() + 360, body.getBottom() - 154, 420, 18}, 12.0F);
    }

    void paintYaeltex(juce::Graphics& graphics)
    {
        graphics.fillAll(juce::Colour(0xff090909));

        auto frame = getLocalBounds().reduced(18);
        graphics.setColour(juce::Colour(0xffc6904d));
        graphics.fillRoundedRectangle(frame.toFloat(), 18.0F);
        graphics.setColour(juce::Colour(0xff7a4b21));
        graphics.drawRoundedRectangle(frame.toFloat(), 18.0F, 4.0F);

        auto body = surfaceBodyBounds();
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
            drawKnob(graphics, body.getX() + 105 + i * 145, body.getY() + 705, 28, "Vol/Pan T" + juce::String(i + 1));
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
        const auto body = surfaceBodyBounds();
        return {body.getX() + 270, body.getY() + 320, 560, 300};
    }

    juce::Rectangle<int> surfaceBodyBounds() const
    {
        if (kind_ == SurfaceKind::Yaeltex) {
            return getLocalBounds().reduced(18).reduced(26);
        }
        return getLocalBounds().reduced(22);
    }

    juce::Rectangle<int> gridCell(const juce::Rectangle<int>& origin, int row, int column, int horizontalGap, int verticalGap, int width = 1, int height = 1) const
    {
        return {
            origin.getX() + column * (origin.getWidth() + horizontalGap),
            origin.getY() + row * (origin.getHeight() + verticalGap),
            width * origin.getWidth() + (width - 1) * horizontalGap,
            height * origin.getHeight() + (height - 1) * verticalGap,
        };
    }

    juce::Rectangle<int> yaeltexGroupOrigin(const juce::String& groupName) const
    {
        const auto body = surfaceBodyBounds();
        if (groupName == "looper_select") {
            return {body.getX() + 58, body.getY() + 308, 76, 64};
        }
        if (groupName == "sample_length") {
            return {body.getX() + 58, body.getY() + 406, 52, 40};
        }
        if (groupName == "track_record") {
            return {body.getX() + 58, body.getY() + 512, 76, 56};
        }
        if (groupName == "track_clear") {
            return {body.getX() + 58, body.getY() + 604, 76, 48};
        }
        if (groupName == "track_volume_pan") {
            return {body.getX() + 66, body.getY() + 666, 78, 78};
        }
        if (groupName == "resampling") {
            return {body.getX() + 360, body.getY() + 794, 104, 54};
        }
        if (groupName == "session") {
            return {body.getX() + 404, body.getY() + 54, 76, 54};
        }
        return {body.getX() + 40, body.getY() + 650, 90, 56};
    }

    juce::Rectangle<int> kaossGroupOrigin(const juce::String& groupName) const
    {
        const auto body = surfaceBodyBounds();
        if (groupName == "presets") {
            return {body.getX() + 300, body.getY() + 225, 68, 44};
        }
        if (groupName == "pages") {
            return {body.getX() + 320, body.getBottom() - 94, 92, 52};
        }
        if (groupName == "levels") {
            return {body.getX() + 62, body.getY() + 76, 44, 110};
        }
        if (groupName == "fx_parameters") {
            return {body.getX() + 382, body.getY() + 600, 78, 82};
        }
        return {body.getX() + 40, body.getY() + 620, 92, 58};
    }

    juce::Rectangle<int> controlBounds(const Control& control, int horizontalGap, int verticalGap) const
    {
#if LIVELOOPING_HAS_PROFILE_IO
        if (const auto* element = findLayoutWidget(control.id)) {
            return scaledBounds(element->bounds);
        }
#endif
        if (kind_ == SurfaceKind::Kaoss) {
            const auto body = surfaceBodyBounds();
            if (control.id == "input_volume") {
                return {body.getX() + 72, body.getY() + 78, 24, 112};
            }
            if (control.id == "fx_level") {
                return {body.getX() + 72, body.getY() + 214, 24, 112};
            }
            if (control.group == "pages") {
                return gridCell(kaossGroupOrigin(control.group), 0, control.column, 58, 0, control.width, control.height);
            }
            if (control.group == "presets") {
                return gridCell(kaossGroupOrigin(control.group), control.row, control.column, 12, 8, control.width, control.height);
            }
            if (control.group == "fx_parameters") {
                return gridCell(kaossGroupOrigin(control.group), control.row, control.column, 13, 8, control.width, control.height);
            }
            return gridCell(kaossGroupOrigin(control.group), control.row, control.column, horizontalGap, verticalGap, control.width, control.height);
        }

        if (control.group == "sample_length") {
            return gridCell(yaeltexGroupOrigin(control.group), control.row, control.column, 12, 12, control.width, control.height);
        }
        if (control.group == "track_volume_pan") {
            return gridCell(yaeltexGroupOrigin(control.group), control.row, control.column, 67, 10, control.width, control.height);
        }
        if (control.group == "resampling") {
            return gridCell(yaeltexGroupOrigin(control.group), control.row, control.column, 18, 10, control.width, control.height);
        }
        if (control.group == "session") {
            return gridCell(yaeltexGroupOrigin(control.group), control.row, control.column, 20, 10, control.width, control.height);
        }
        return gridCell(yaeltexGroupOrigin(control.group), control.row, control.column, 14, 10, control.width, control.height);
    }

    void layoutByGroup(int horizontalGap, int verticalGap)
    {
        for (auto& group : groups_) {
            const auto origin = kind_ == SurfaceKind::Kaoss ? kaossGroupOrigin(group.name) : yaeltexGroupOrigin(group.name);
            group.label->setVisible(false);
            group.label->setBounds(origin.getX(), std::max(18, origin.getY() - kGroupHeaderHeight - 8), 260, kGroupHeaderHeight);

            for (auto& control : controls_) {
                if (control.group != group.name) {
                    continue;
                }

                control.component->setBounds(controlBounds(control, horizontalGap, verticalGap));
            }
        }
    }

    Control makeControl(juce::Component& component, const ControllerWidget& widget) const
    {
        return Control{
            &component,
            juce::String(widget.id),
            juce::String(widget.group),
            widget.type,
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

#if LIVELOOPING_HAS_PROFILE_IO
    int editToolbarHeight() const
    {
        if (!layout_.has_value()) {
            return 0;
        }
        return getWidth() < 620 ? 78 : 48;
    }

    enum EditMenuItem {
        ToggleEditMode = 1,
        SaveLayout,
        UndoEdit,
        ToggleSnap,
        CopyElementId,
        ClearSelection,
    };

    juce::String editButtonText() const
    {
        if (compactToolbar_) {
            return editMode_ ? "Выкл." : "Ред.";
        }
        return editMode_ ? "Выключить" : "Правка";
    }

    juce::String saveButtonText() const
    {
        return compactToolbar_ ? "Сохр." : "Сохранить";
    }

    juce::String undoButtonText() const
    {
        return compactToolbar_ ? "Отм." : "Отменить";
    }

    juce::String snapButtonText() const
    {
        return compactToolbar_ ? "Сетка" : "Сетка 4";
    }

    void showEditContextMenu()
    {
        juce::PopupMenu menu;
        menu.addItem(ToggleEditMode, editMode_ ? "Выключить правку" : "Включить правку", true, editMode_);
        menu.addSeparator();
        menu.addItem(SaveLayout, "Сохранить layout", editMode_ && !layoutPath_.empty());
        menu.addItem(UndoEdit, "Отменить", editMode_ && !undoStack_.empty());
        menu.addItem(ToggleSnap, "Сетка 4 px", editMode_, snapToGrid_);

        if (editMode_ && selectedElement_ >= 0) {
            menu.addSeparator();
            menu.addItem(CopyElementId, "Скопировать ID", true);
            menu.addItem(ClearSelection, "Снять выделение", true);
        }

        const juce::Component::SafePointer<ProfileSurfaceComponent> safeThis(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [safeThis](int result) mutable {
            if (safeThis != nullptr) {
                safeThis->handleEditContextMenu(result);
            }
        });
    }

    void handleEditContextMenu(int result)
    {
        switch (result) {
        case ToggleEditMode:
            setEditMode(!editMode_);
            break;
        case SaveLayout:
            saveLayout();
            break;
        case UndoEdit:
            undoLastEdit();
            break;
        case ToggleSnap:
            snapToGrid_ = !snapToGrid_;
            editorStatus_ = snapToGrid_ ? "Сетка: 4 px" : "Сетка: выкл.";
            updateEditToolbar();
            repaint();
            break;
        case CopyElementId:
            if (layout_.has_value() && selectedElement_ >= 0) {
                juce::SystemClipboard::copyTextToClipboard(
                    juce::String(layout_->elements[static_cast<size_t>(selectedElement_)].id));
                editorStatus_ = "ID скопирован";
                updateEditToolbar();
            }
            break;
        case ClearSelection:
            selectedElement_ = -1;
            editorStatus_.clear();
            updateEditToolbar();
            repaint();
            break;
        default:
            break;
        }
    }

    void setupEditToolbar()
    {
        editModeButton_ = makeToolbarButton(editButtonText());
        editModeButton_->onClick = [this] {
            setEditMode(!editMode_);
        };

        saveLayoutButton_ = makeToolbarButton(saveButtonText());
        saveLayoutButton_->onClick = [this] {
            saveLayout();
        };

        undoButton_ = makeToolbarButton(undoButtonText());
        undoButton_->onClick = [this] {
            undoLastEdit();
        };

        snapButton_ = makeToolbarButton(snapButtonText());
        snapButton_->setClickingTogglesState(true);
        snapButton_->setToggleState(snapToGrid_, juce::dontSendNotification);
        snapButton_->onClick = [this] {
            snapToGrid_ = snapButton_ != nullptr && snapButton_->getToggleState();
            editorStatus_ = snapToGrid_ ? "Сетка: 4 px" : "Сетка: выкл.";
            updateEditToolbar();
            repaint();
        };

        editorStatusLabel_ = std::make_unique<juce::Label>();
        editorStatusLabel_->setJustificationType(juce::Justification::centredLeft);
        editorStatusLabel_->setColour(juce::Label::textColourId, juce::Colours::white);
        editorStatusLabel_->setColour(juce::Label::backgroundColourId, juce::Colour(0x66182126));
        editorStatusLabel_->setFont(juce::Font(juce::FontOptions(13.0F)));
        addAndMakeVisible(*editorStatusLabel_);

        updateEditToolbar();
    }

    std::unique_ptr<juce::TextButton> makeToolbarButton(const juce::String& text)
    {
        auto button = std::make_unique<juce::TextButton>(text);
        button->setWantsKeyboardFocus(false);
        button->setColour(juce::TextButton::buttonColourId, juce::Colour(0xee20272d));
        button->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffd00010));
        button->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        button->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        addAndMakeVisible(*button);
        return button;
    }

    void layoutEditToolbar()
    {
        if (editModeButton_ == nullptr) {
            return;
        }

        const auto wasCompact = compactToolbar_;
        compactToolbar_ = getWidth() < 520;

        auto toolbar = getLocalBounds().reduced(10);
        toolbar.setHeight(editToolbarHeight() - 12);
        toolbar.translate(0, 6);

        auto buttonRow = toolbar.withHeight(30);
        const auto gap = compactToolbar_ ? 6 : 8;
        const auto compactWidth = juce::jmax(42, (buttonRow.getWidth() - gap * 3) / 4);
        const auto editWidth = compactToolbar_ ? compactWidth : 96;
        const auto saveWidth = compactToolbar_ ? compactWidth : 104;
        const auto undoWidth = compactToolbar_ ? compactWidth : 92;
        const auto snapWidth = compactToolbar_ ? compactWidth : 92;

        editModeButton_->setBounds(buttonRow.removeFromLeft(editWidth));
        buttonRow.removeFromLeft(gap);
        saveLayoutButton_->setBounds(buttonRow.removeFromLeft(saveWidth));
        buttonRow.removeFromLeft(gap);
        undoButton_->setBounds(buttonRow.removeFromLeft(undoWidth));
        buttonRow.removeFromLeft(gap);
        snapButton_->setBounds(buttonRow.removeFromLeft(snapWidth));

        if (getWidth() < 620) {
            editorStatusLabel_->setBounds(toolbar.withTrimmedTop(36).withHeight(30));
        } else {
            buttonRow.removeFromLeft(10);
            editorStatusLabel_->setBounds(buttonRow);
        }

        if (compactToolbar_ != wasCompact) {
            updateEditToolbar();
        }
    }

    void updateEditToolbar()
    {
        if (editModeButton_ == nullptr) {
            return;
        }

        editModeButton_->setButtonText(editButtonText());
        saveLayoutButton_->setButtonText(saveButtonText());
        undoButton_->setButtonText(undoButtonText());
        snapButton_->setButtonText(snapButtonText());
        editModeButton_->setToggleState(editMode_, juce::dontSendNotification);
        saveLayoutButton_->setEnabled(editMode_ && !layoutPath_.empty());
        undoButton_->setEnabled(editMode_ && !undoStack_.empty());
        snapButton_->setEnabled(editMode_);
        snapButton_->setToggleState(snapToGrid_, juce::dontSendNotification);
        editorStatusLabel_->setText(editorStatus_.isNotEmpty() ? editorStatus_ : selectedElementSummary(), juce::dontSendNotification);
        editorStatusLabel_->setTooltip(
            "E — правка, S — сохранить, Ctrl+Z — отменить, стрелки — сдвиг, Shift — шаг 10, Alt — размер, Shift+drag — без сетки");
    }

    juce::String selectedElementSummary() const
    {
        if (!editMode_) {
            return "Режим правки выключен";
        }
        if (!layout_.has_value() || selectedElement_ < 0) {
            return "Ничего не выбрано";
        }

        const auto& element = layout_->elements[static_cast<size_t>(selectedElement_)];
        return juce::String(element.id)
            + "  x=" + juce::String(element.bounds.x, 1)
            + " y=" + juce::String(element.bounds.y, 1)
            + " шир=" + juce::String(element.bounds.width, 1)
            + " выс=" + juce::String(element.bounds.height, 1);
    }

    void nudgeSelected(float primaryDelta, float secondaryDelta, bool resize)
    {
        if (!layout_.has_value() || selectedElement_ < 0) {
            return;
        }

        pushUndoSnapshot();
        auto& element = layout_->elements[static_cast<size_t>(selectedElement_)];
        auto bounds = element.bounds;
        if (resize) {
            bounds.width += primaryDelta;
            bounds.height += secondaryDelta;
        } else {
            bounds.x += primaryDelta;
            bounds.y += secondaryDelta;
        }
        element.bounds = clampBounds(snapBounds(bounds, !snapToGrid_));
        editorStatus_.clear();
        layoutByGroup(kind_ == SurfaceKind::Yaeltex ? 14 : 12, kind_ == SurfaceKind::Yaeltex ? 10 : 8);
        updateEditToolbar();
        repaint();
    }

    void setEditMode(bool enabled)
    {
        editMode_ = enabled;
        selectedElement_ = -1;
        editDragMode_ = EditDragMode::None;
        editorStatus_ = editMode_
            ? "Правка: стрелки двигают, Alt меняет размер, Shift меняет шаг"
            : "Режим правки выключен";
        for (auto& control : controls_) {
            if (control.component != nullptr) {
                control.component->setInterceptsMouseClicks(!editMode_, !editMode_);
            }
        }
        grabKeyboardFocus();
        updateEditToolbar();
        repaint();
    }

    static const char* roleName(SurfaceElementRole role)
    {
        return role == SurfaceElementRole::Widget ? "widget" : "decoration";
    }

    static const char* shapeName(SurfaceElementShape shape)
    {
        switch (shape) {
        case SurfaceElementShape::Rect:
            return "rect";
        case SurfaceElementShape::RoundRect:
            return "round_rect";
        case SurfaceElementShape::Circle:
            return "circle";
        case SurfaceElementShape::Text:
            return "text";
        case SurfaceElementShape::Line:
            return "line";
        case SurfaceElementShape::Knob:
            return "knob";
        case SurfaceElementShape::Fader:
            return "fader";
        case SurfaceElementShape::Joystick:
            return "joystick";
        }
        return "rect";
    }

    static std::string jsonEscape(const std::string& value)
    {
        std::ostringstream out;
        for (const auto ch : value) {
            switch (ch) {
            case '\\':
                out << "\\\\";
                break;
            case '"':
                out << "\\\"";
                break;
            case '\n':
                out << "\\n";
                break;
            default:
                out << ch;
                break;
            }
        }
        return out.str();
    }

    static void writeJsonString(std::ostream& out, const char* key, const std::string& value, int indent, bool comma = true)
    {
        out << std::string(static_cast<size_t>(indent), ' ') << "\"" << key << "\": \"" << jsonEscape(value) << "\"";
        if (comma) {
            out << ",";
        }
        out << "\n";
    }

    static void writeJsonNumber(std::ostream& out, const char* key, float value, int indent, bool comma = true)
    {
        out << std::string(static_cast<size_t>(indent), ' ') << "\"" << key << "\": " << value;
        if (comma) {
            out << ",";
        }
        out << "\n";
    }

    void saveLayout()
    {
        if (!layout_.has_value() || layoutPath_.empty()) {
            editorStatus_ = "Нет пути layout";
            updateEditToolbar();
            return;
        }

        std::ofstream out(layoutPath_);
        if (!out) {
            editorStatus_ = "Не удалось сохранить";
            updateEditToolbar();
            return;
        }

        out << "{\n";
        writeJsonString(out, "id", layout_->id, 2);
        writeJsonString(out, "profileId", layout_->profileId, 2);
        out << "  \"baseWidth\": " << layout_->baseWidth << ",\n";
        out << "  \"baseHeight\": " << layout_->baseHeight << ",\n";
        out << "  \"elements\": [\n";
        for (size_t index = 0; index < layout_->elements.size(); ++index) {
            const auto& element = layout_->elements[index];
            out << "    {\n";
            writeJsonString(out, "id", element.id, 6);
            writeJsonString(out, "role", roleName(element.role), 6);
            writeJsonString(out, "shape", shapeName(element.shape), 6);
            writeJsonString(out, "variant", element.variant, 6);
            if (!element.label.empty()) {
                writeJsonString(out, "label", element.label, 6);
            }
            if (!element.widgetId.empty()) {
                writeJsonString(out, "widgetId", element.widgetId, 6);
            }
            if (!element.group.empty()) {
                writeJsonString(out, "group", element.group, 6);
            }
            out << "      \"bounds\": {\n";
            writeJsonNumber(out, "x", element.bounds.x, 8);
            writeJsonNumber(out, "y", element.bounds.y, 8);
            writeJsonNumber(out, "width", element.bounds.width, 8);
            writeJsonNumber(out, "height", element.bounds.height, 8, false);
            out << "      }\n";
            out << "    }" << (index + 1 == layout_->elements.size() ? "\n" : ",\n");
        }
        out << "  ]\n";
        out << "}\n";
        editorStatus_ = "Сохранено " + juce::Time::getCurrentTime().formatted("%H:%M:%S");
        updateEditToolbar();
    }

    void pushUndoSnapshot()
    {
        if (!layout_.has_value()) {
            return;
        }

        std::vector<SurfaceBounds> snapshot;
        snapshot.reserve(layout_->elements.size());
        for (const auto& element : layout_->elements) {
            snapshot.push_back(element.bounds);
        }
        undoStack_.push_back(std::move(snapshot));
        constexpr size_t kMaxUndoSteps = 128;
        if (undoStack_.size() > kMaxUndoSteps) {
            undoStack_.erase(undoStack_.begin());
        }
    }

    void undoLastEdit()
    {
        if (!layout_.has_value() || undoStack_.empty()) {
            return;
        }

        const auto snapshot = std::move(undoStack_.back());
        undoStack_.pop_back();
        const auto count = juce::jmin(snapshot.size(), layout_->elements.size());
        for (size_t index = 0; index < count; ++index) {
            layout_->elements[index].bounds = snapshot[index];
        }
        editorStatus_ = "Отменено";
        layoutByGroup(kind_ == SurfaceKind::Yaeltex ? 14 : 12, kind_ == SurfaceKind::Yaeltex ? 10 : 8);
        updateEditToolbar();
        repaint();
    }
#endif

    LiveLoopingEngine& engine_;
    MidiMapper mapper_;
    SurfaceKind kind_ = SurfaceKind::Kaoss;
#if LIVELOOPING_HAS_PROFILE_IO
    std::optional<ControlSurfaceLayout> layout_;
    std::string layoutPath_;
    bool editMode_ = false;
    int selectedElement_ = -1;
    EditDragMode editDragMode_ = EditDragMode::None;
    juce::Point<float> editStartMouse_;
    SurfaceBounds editStartBounds_;
    std::vector<std::vector<SurfaceBounds>> undoStack_;
    bool snapToGrid_ = true;
    bool compactToolbar_ = false;
    juce::String editorStatus_;
    std::unique_ptr<juce::TextButton> editModeButton_;
    std::unique_ptr<juce::TextButton> saveLayoutButton_;
    std::unique_ptr<juce::TextButton> undoButton_;
    std::unique_ptr<juce::TextButton> snapButton_;
    std::unique_ptr<juce::Label> editorStatusLabel_;
#endif
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

std::unique_ptr<ProfileSurfaceComponent> makeProfileSurface(
    LiveLoopingEngine& engine,
    ControllerProfile profile,
    const char* profileFileName,
    const char* layoutFileName)
{
#if LIVELOOPING_HAS_PROFILE_IO
    profile = loadProfileOrFallback(profileFileName, std::move(profile));
    const auto layoutPath = std::string(LIVELOOPING_LAYOUT_DIR) + "/" + layoutFileName;
    return std::make_unique<ProfileSurfaceComponent>(engine, std::move(profile), loadOptionalLayout(layoutFileName), layoutPath);
#else
    juce::ignoreUnused(profileFileName);
    juce::ignoreUnused(layoutFileName);
    return std::make_unique<ProfileSurfaceComponent>(engine, std::move(profile));
#endif
}

class PseudoDevicesComponent final : public juce::Component {
public:
    explicit PseudoDevicesComponent(LiveLoopingEngine& engine)
    {
        tabs_.setTabBarDepth(28);
        tabs_.addTab("Mic Kaoss", juce::Colours::darkslategrey,
            makeProfileSurface(engine, makeMicKaossPadProfile(), "kaoss_mic.json", "kaoss_pad.json").release(),
            true);
        tabs_.addTab("Synth Kaoss", juce::Colours::darkslategrey,
            makeProfileSurface(engine, makeSynthKaossPadProfile(), "kaoss_synth.json", "kaoss_pad.json").release(),
            true);
        tabs_.addTab("Yaeltex", juce::Colours::darkslategrey,
            makeProfileSurface(engine, makeYaeltexLiveLoopingProfile(), "yaeltex_livelooping.json", "yaeltex_livelooping.json").release(),
            true);
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
