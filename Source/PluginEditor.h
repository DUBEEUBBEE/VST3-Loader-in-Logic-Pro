#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "VST3FileBrowser.h"

class VST3LoaderAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::ChangeListener,
                                       public juce::Button::Listener,
                                       public juce::ComponentListener,
                                       private juce::Timer
{
public:
    VST3LoaderAudioProcessorEditor (VST3LoaderAudioProcessor&);
    ~VST3LoaderAudioProcessorEditor() override;
    
    void paint (juce::Graphics&) override;
    void resized() override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void buttonClicked(juce::Button*) override;
    void componentMovedOrResized (juce::Component& component, bool wasMoved, bool wasResized) override;

private:
    class SemiTransparentComponent : public juce::Component
    {
    public:
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colours::white.withAlpha(0.1f));
        }
    };
    
    void timerCallback() override;
    
    VST3LoaderAudioProcessor& audioProcessor;
    juce::ThreadPool threadPool {1};
    std::unique_ptr<juce::AudioProcessorEditor> hostedPluginEditor;
    
    void loadPlugin(const juce::String& filePath);
    void closePlugin();
    void setHostedPluginEditorIfNeeded();
    
    std::unique_ptr<VST3ListBox> pluginListBox;
    SemiTransparentComponent pluginListBoxCover;
    juce::TextButton loadPluginButton;
    juce::TextButton closePluginButton;
    juce::Label statusLabel;
    
    void setLoadingState();
    void processorStateChanged(bool shouldShowPluginLoadingError);
    
    static constexpr int defaultEditorWidth = 650;
    static constexpr int margin = 10;
    static constexpr int browserHeight = 435; // 검색박스 포함
    static constexpr int labelHeight = 30;
    static constexpr int buttonHeight = 30;
    static constexpr int buttonTopspacing = 5;
    
    int getEditorWidth()
    {
        return hostedPluginEditor == nullptr ? defaultEditorWidth : hostedPluginEditor->getWidth();
    }
    
    int getHostedPluginEditorOrPluginListHeight()
    {
        return hostedPluginEditor == nullptr ? browserHeight : hostedPluginEditor->getHeight();
    }
    
    int getEditorHeight()
    {
        const auto buttonViewHeight = buttonHeight + buttonTopspacing;
        return getHostedPluginEditorOrPluginListHeight() + buttonViewHeight + labelHeight;
    }
    
    int getButtonOriginY()
    {
        return getHostedPluginEditorOrPluginListHeight() + buttonTopspacing;
    }
    
    int getLabelOriginY()
    {
        return getButtonOriginY() + buttonHeight;
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VST3LoaderAudioProcessorEditor)
};
