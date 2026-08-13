#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>
#include <utility>

namespace {

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

class PluginEditorHostComponent final : public juce::Component {
public:
    explicit PluginEditorHostComponent(juce::File pluginPath)
    {
        loadPlugin(std::move(pluginPath));
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(juce::Colour(0xff101519));
        if (editor_ != nullptr) {
            return;
        }

        graphics.setColour(juce::Colours::white);
        graphics.setFont(juce::FontOptions(18.0f));
        graphics.drawFittedText(status_, getLocalBounds().reduced(24), juce::Justification::centred, 6);
    }

    void resized() override
    {
        if (editor_ != nullptr) {
            editor_->setBounds(getLocalBounds());
        }
    }

private:
    void loadPlugin(const juce::File& pluginPath)
    {
        juce::addDefaultFormatsToManager(formatManager_);

        const auto candidates = collectPluginCandidates(pluginPath);
        if (candidates.isEmpty()) {
            status_ = "No VST3 plugin found in:\n" + pluginPath.getFullPathName();
            return;
        }

        for (const auto& candidate : candidates) {
            if (tryLoadCandidate(candidate)) {
                return;
            }
        }

        if (status_.isEmpty()) {
            status_ = "VST3 plugin was found, but no editor could be opened.";
        }
    }

    bool tryLoadCandidate(const juce::File& candidate)
    {
        const auto candidatePath = candidate.getFullPathName();
        for (int index = 0; index < formatManager_.getNumFormats(); ++index) {
            auto* format = formatManager_.getFormat(index);
            if (format == nullptr || !format->fileMightContainThisPluginType(candidatePath)) {
                continue;
            }

            juce::OwnedArray<juce::PluginDescription> descriptions;
            format->findAllTypesForFile(descriptions, candidatePath);
            for (const auto* description : descriptions) {
                if (description != nullptr && tryOpenEditor(*description)) {
                    return true;
                }
            }
        }
        return false;
    }

    bool tryOpenEditor(const juce::PluginDescription& description)
    {
        juce::String error;
        plugin_ = formatManager_.createPluginInstance(description, 44100.0, 512, error);
        if (plugin_ == nullptr) {
            status_ = "Failed to create plugin instance:\n" + error;
            return false;
        }

        editor_.reset(plugin_->createEditorIfNeeded());
        if (editor_ == nullptr) {
            status_ = "Plugin instance created, but it has no editor.";
            plugin_.reset();
            return false;
        }

        addAndMakeVisible(*editor_);
        setSize(editor_->getWidth(), editor_->getHeight());
        resized();
        return true;
    }

    juce::AudioPluginFormatManager formatManager_;
    std::unique_ptr<juce::AudioPluginInstance> plugin_;
    std::unique_ptr<juce::AudioProcessorEditor> editor_;
    juce::String status_ = "Loading plugin editor...";
};

class MainWindow final : public juce::DocumentWindow {
public:
    explicit MainWindow(juce::File pluginPath)
        : DocumentWindow(
              "LoopRigger Plugin Editor Host",
              juce::Colour(0xff101519),
              DocumentWindow::closeButton | DocumentWindow::minimiseButton)
    {
        auto content = std::make_unique<PluginEditorHostComponent>(std::move(pluginPath));
        setUsingNativeTitleBar(true);
        setContentOwned(content.release(), true);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

class PluginEditorHostApplication final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "LoopRigger Plugin Editor Host"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        auto path = juce::File(LIVELOOPING_DEFAULT_PLUGIN_DIR);
        const auto args = juce::JUCEApplicationBase::getCommandLineParameterArray();
        if (!args.isEmpty()) {
            path = juce::File(args[0]);
        }

        mainWindow_ = std::make_unique<MainWindow>(path);
    }

    void shutdown() override
    {
        mainWindow_.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    std::unique_ptr<MainWindow> mainWindow_;
};

} // namespace

START_JUCE_APPLICATION(PluginEditorHostApplication)
