#include "PluginProcessor.h"
#include "PluginEditor.h"

VST3LoaderAudioProcessorEditor::VST3LoaderAudioProcessorEditor (VST3LoaderAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    pluginListBox = std::make_unique<VST3ListBox>();
    pluginListBox->onPluginSelected = [this](const juce::String& path)
    {
        loadPlugin(path);
    };
    
    audioProcessor.addChangeListener(this);
    addAndMakeVisible(pluginListBox.get());
    
    addAndMakeVisible(pluginListBoxCover);
    
    // 버튼 스타일링
    loadPluginButton.setButtonText("Load Plugin");
    loadPluginButton.addListener(this);
    loadPluginButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffA4193D));
    loadPluginButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffFFDFB9));
    loadPluginButton.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    
    closePluginButton.setButtonText("Close Plugin");
    closePluginButton.addListener(this);
    closePluginButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffA4193D));
    closePluginButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffFFDFB9));
    closePluginButton.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffFFDFB9));
    statusLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff1a1a1a));
    
    addAndMakeVisible(loadPluginButton);
    addAndMakeVisible(closePluginButton);
    addAndMakeVisible(statusLabel);
    
    setHostedPluginEditorIfNeeded();
    processorStateChanged(false);
    
    startTimer(500);
}

VST3LoaderAudioProcessorEditor::~VST3LoaderAudioProcessorEditor()
{
    audioProcessor.removeChangeListener(this);
    stopTimer();
}

void VST3LoaderAudioProcessorEditor::loadPlugin(const juce::String& filePath)
{
    hostedPluginEditor.reset();
    setLoadingState();
    
    threadPool.addJob([this, filePath]()
    {
        juce::Thread::sleep(5);
        audioProcessor.loadPlugin(filePath);
    });
}

void VST3LoaderAudioProcessorEditor::closePlugin()
{
    hostedPluginEditor.reset();
    audioProcessor.closeHostedPlugin();
    processorStateChanged(false);
}

void VST3LoaderAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    setHostedPluginEditorIfNeeded();
    processorStateChanged(true);
}

void VST3LoaderAudioProcessorEditor::setHostedPluginEditorIfNeeded()
{
    if (!audioProcessor.isHostedPluginLoaded()) { return; }
    
    const auto newEditor = audioProcessor.createHostedPluginEditorIfNeeded();
    if (newEditor == nullptr) { return; }
    
    if (hostedPluginEditor.get() != newEditor)
    {
        hostedPluginEditor.reset(newEditor);
        addAndMakeVisible(hostedPluginEditor.get());
        hostedPluginEditor.get()->addComponentListener(this);
    }
}

void VST3LoaderAudioProcessorEditor::setLoadingState()
{
    loadPluginButton.setEnabled(false);
    pluginListBoxCover.setVisible(true);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffFFDFB9));
    statusLabel.setText("Loading...", juce::dontSendNotification);
}

void VST3LoaderAudioProcessorEditor::processorStateChanged(const bool shouldShowPluginLoadingError)
{
    const auto isHostedPluginLoaded = audioProcessor.isHostedPluginLoaded();
    const auto pluginLoadingError = audioProcessor.getHostedPluginLoadingError();
    
    pluginListBox->setVisible(!isHostedPluginLoaded);
    pluginListBoxCover.setVisible(false);
    loadPluginButton.setVisible(!isHostedPluginLoaded);
    loadPluginButton.setEnabled(pluginListBox->isPluginSelected());
    closePluginButton.setVisible(isHostedPluginLoaded);
    
    if (isHostedPluginLoaded)
    {
        juce::String labelText = audioProcessor.getHostedPluginName();
        
        if (hostedPluginEditor == nullptr)
        {
            labelText = labelText + " (no editor)";
        }
        
        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffFFDFB9));
        statusLabel.setText(labelText, juce::dontSendNotification);
    }
    else
    {
        const auto isShowingError = shouldShowPluginLoadingError && !pluginLoadingError.isEmpty();
        const auto labelText = isShowingError ? pluginLoadingError : juce::String("No plugin loaded");
        statusLabel.setColour(juce::Label::textColourId,
                            isShowingError ? juce::Colours::red : juce::Colour(0xffFFDFB9));
        statusLabel.setText(labelText, juce::dontSendNotification);
    }
    
    setSize(getEditorWidth(), getEditorHeight());
    repaint();
}

void VST3LoaderAudioProcessorEditor::buttonClicked(juce::Button* button)
{
    if (button == &loadPluginButton)
    {
        juce::String selectedPlugin = pluginListBox->getSelectedPlugin();
        if (selectedPlugin.isNotEmpty())
        {
            loadPlugin(selectedPlugin);
        }
    }
    else if (button == &closePluginButton)
    {
        closePlugin();
    }
}

void VST3LoaderAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour(0xff1a1a1a));
}

void VST3LoaderAudioProcessorEditor::componentMovedOrResized(juce::Component &component,
                                                            bool wasMoved,
                                                            bool wasResized)
{
    if (&component == hostedPluginEditor.get() && wasResized)
    {
        setSize(getEditorWidth(), getEditorHeight());
    }
}

void VST3LoaderAudioProcessorEditor::resized()
{
    if (hostedPluginEditor != nullptr)
    {
        hostedPluginEditor->setTopLeftPosition(0, 0);
    }
    
    pluginListBox->setBounds(0, 0, getEditorWidth(), browserHeight);
    pluginListBoxCover.setBounds(0, 0, getEditorWidth(), browserHeight);
    loadPluginButton.setBounds(margin, getButtonOriginY(),
                              getBounds().getWidth() - 2 * margin, buttonHeight);
    closePluginButton.setBounds(margin, getButtonOriginY(),
                               getBounds().getWidth() - 2 * margin, buttonHeight);
    statusLabel.setBounds(margin, getLabelOriginY(),
                         getBounds().getWidth() - 2 * margin, labelHeight);
}

void VST3LoaderAudioProcessorEditor::timerCallback()
{
    setWantsKeyboardFocus(true);
    grabKeyboardFocus();
    stopTimer();
}
