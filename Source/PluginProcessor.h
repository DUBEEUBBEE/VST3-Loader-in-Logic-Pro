#pragma once
#include <JuceHeader.h>

class VST3LoaderAudioProcessor : public juce::AudioProcessor,
                                  public juce::ChangeBroadcaster
{
public:
    VST3LoaderAudioProcessor();
    ~VST3LoaderAudioProcessor() override;
    
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    
    const juce::String getName() const override { return "VST3Loader"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;
    
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    
    // Public API
    bool isHostedPluginLoaded();
    bool isCurrentlyLoading();
    void loadPlugin(const juce::String& pluginPath);
    void closeHostedPlugin();
    juce::String getHostedPluginLoadingError();
    juce::String getHostedPluginName();
    juce::AudioProcessorEditor* createHostedPluginEditorIfNeeded();
    
private:
    juce::CriticalSection innerMutex;
    juce::AudioPluginFormatManager formatManager;
    std::unique_ptr<juce::AudioPluginInstance> hostedPluginInstance;
    
    bool isLoading = false;
    juce::String hostedPluginLoadingError;
    juce::String hostedPluginPath;
    juce::String hostedPluginName;
    juce::MemoryBlock hostedPluginState;
    
    static constexpr const char* innerStateTag = "inner_state";
    static constexpr const char* pluginPathTag = "plugin_path";
    
    using PluginLoadingCallback = std::function<void(std::unique_ptr<juce::AudioPluginInstance>)>;
    
    void setIsLoading(bool value);
    void setHostedPluginInstance(std::unique_ptr<juce::AudioPluginInstance> pluginInstance);
    void setHostedPluginLoadingError(juce::String value);
    void setHostedPluginPath(juce::String value);
    void setHostedPluginName(juce::String value);
    juce::String getHostedPluginPath();
    
    void removePreviouslyHostedPluginIfNeeded(bool unsetError);
    void loadPluginFromFile(const juce::String& pluginPath, PluginLoadingCallback callback);
    bool setHostedPluginLayout();
    bool prepareHostedPluginForPlaying();
    void setHostedPluginState();
    
    template<typename T>
    T safelyPerform(std::function<T(std::unique_ptr<juce::AudioPluginInstance>&)> operation) const
    {
        const juce::ScopedLock sl(innerMutex);
        if (hostedPluginInstance == nullptr) { return T(); }
        return operation(const_cast<std::unique_ptr<juce::AudioPluginInstance>&>(hostedPluginInstance));
    }
    
    template<typename SampleType>
    void processBlockInternal(juce::AudioBuffer<SampleType>& buffer,
                             juce::MidiBuffer& midiMessages,
                             bool isActive);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VST3LoaderAudioProcessor)
};
