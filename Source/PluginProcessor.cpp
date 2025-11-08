#include "PluginProcessor.h"
#include "PluginEditor.h"

VST3LoaderAudioProcessor::VST3LoaderAudioProcessor()
    : AudioProcessor (BusesProperties()
                     .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.addDefaultFormats();
}

VST3LoaderAudioProcessor::~VST3LoaderAudioProcessor() {}

void VST3LoaderAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    safelyPerform<void>([&](auto& p)
    {
        p->releaseResources();
        p->setRateAndBufferSizeDetails(sampleRate, samplesPerBlock);
        p->prepareToPlay(sampleRate, samplesPerBlock);
    });
}

void VST3LoaderAudioProcessor::reset()
{
    safelyPerform<void>([&](auto& p) { p->reset(); });
}

void VST3LoaderAudioProcessor::releaseResources()
{
    safelyPerform<void>([&](auto& p) { p->releaseResources(); });
}

double VST3LoaderAudioProcessor::getTailLengthSeconds() const
{
    return safelyPerform<double>([](auto& p) { return p->getTailLengthSeconds(); });
}

bool VST3LoaderAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return true;
}

void VST3LoaderAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages)
{
    processBlockInternal(buffer, midiMessages, true);
}

void VST3LoaderAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer,
                                                     juce::MidiBuffer& midiMessages)
{
    processBlockInternal(buffer, midiMessages, false);
}

template<typename SampleType>
void VST3LoaderAudioProcessor::processBlockInternal(juce::AudioBuffer<SampleType>& buffer,
                                                   juce::MidiBuffer& midiMessages,
                                                   bool isActive)
{
    safelyPerform<void>([&](auto& p)
    {
        if (isActive)
        {
            p->setPlayHead(getPlayHead());
        }
        
        const auto hostedPluginChannels = juce::jmax(p->getTotalNumInputChannels(),
                                                     p->getTotalNumOutputChannels());
        const auto currentChannels = buffer.getNumChannels();
        
        if (hostedPluginChannels > currentChannels)
        {
            juce::AudioBuffer<SampleType> innerBuffer(hostedPluginChannels, buffer.getNumSamples());
            for (int i = 0; i < hostedPluginChannels; ++i)
            {
                if (i < currentChannels)
                    innerBuffer.copyFrom(i, 0, buffer.getReadPointer(i), buffer.getNumSamples());
                else
                    innerBuffer.clear(i, 0, buffer.getNumSamples());
            }
            
            if (isActive)
                p->processBlock(innerBuffer, midiMessages);
            else
                p->processBlockBypassed(innerBuffer, midiMessages);
                
            for (int i = 0; i < currentChannels; ++i)
                buffer.copyFrom(i, 0, innerBuffer.getReadPointer(i), buffer.getNumSamples());
        }
        else
        {
            if (isActive)
                p->processBlock(buffer, midiMessages);
            else
                p->processBlockBypassed(buffer, midiMessages);
        }
    });
}

juce::AudioProcessorEditor* VST3LoaderAudioProcessor::createEditor()
{
    return new VST3LoaderAudioProcessorEditor(*this);
}

bool VST3LoaderAudioProcessor::isHostedPluginLoaded()
{
    return safelyPerform<bool>([](auto& p) { return p != nullptr; });
}

bool VST3LoaderAudioProcessor::isCurrentlyLoading()
{
    const juce::ScopedLock sl(innerMutex);
    return isLoading;
}

void VST3LoaderAudioProcessor::loadPlugin(const juce::String& pluginPath)
{
    if (isCurrentlyLoading()) { return; }
    
    removePreviouslyHostedPluginIfNeeded(true);
    setIsLoading(true);
    
    auto callback = [this, pluginPath](auto pluginInstance)
    {
        if (pluginInstance == nullptr)
        {
            setIsLoading(false);
            juce::MessageManager::callAsync([this]() { sendChangeMessage(); });
            return;
        }
        
        const auto desc = pluginInstance->getPluginDescription();
        const auto pluginName = desc.manufacturerName + " - " + desc.name;
        
        setHostedPluginInstance(std::move(pluginInstance));
        
        auto successfullyConfigured = true;
        successfullyConfigured &= setHostedPluginLayout();
        successfullyConfigured &= prepareHostedPluginForPlaying();
        setHostedPluginState();
        setHostedPluginPath(pluginPath);
        setHostedPluginName(pluginName);
        
        if (!successfullyConfigured)
        {
            removePreviouslyHostedPluginIfNeeded(false);
        }
        
        setIsLoading(false);
        juce::MessageManager::callAsync([this]() { sendChangeMessage(); });
    };
    
    loadPluginFromFile(pluginPath, std::move(callback));
}

void VST3LoaderAudioProcessor::closeHostedPlugin()
{
    if (isCurrentlyLoading()) { return; }
    removePreviouslyHostedPluginIfNeeded(true);
}

juce::String VST3LoaderAudioProcessor::getHostedPluginLoadingError()
{
    const juce::ScopedLock sl(innerMutex);
    return hostedPluginLoadingError;
}

juce::String VST3LoaderAudioProcessor::getHostedPluginName()
{
    const juce::ScopedLock sl(innerMutex);
    return hostedPluginName;
}

juce::AudioProcessorEditor* VST3LoaderAudioProcessor::createHostedPluginEditorIfNeeded()
{
    return safelyPerform<juce::AudioProcessorEditor*>([](auto& p)
    {
        auto* editor = p->createEditorIfNeeded();
        if (editor == nullptr)
        {
            return static_cast<juce::AudioProcessorEditor*>(new juce::GenericAudioProcessorEditor(*p));
        }
        return editor;
    });
}

void VST3LoaderAudioProcessor::setIsLoading(bool value)
{
    const juce::ScopedLock sl(innerMutex);
    isLoading = value;
}

void VST3LoaderAudioProcessor::setHostedPluginInstance(std::unique_ptr<juce::AudioPluginInstance> pluginInstance)
{
    const juce::ScopedLock sl(innerMutex);
    hostedPluginInstance.reset();
    if (pluginInstance != nullptr)
    {
        hostedPluginInstance = std::move(pluginInstance);
    }
}

void VST3LoaderAudioProcessor::setHostedPluginLoadingError(juce::String value)
{
    const juce::ScopedLock sl(innerMutex);
    hostedPluginLoadingError = value;
}

void VST3LoaderAudioProcessor::setHostedPluginPath(juce::String value)
{
    const juce::ScopedLock sl(innerMutex);
    hostedPluginPath = value;
}

void VST3LoaderAudioProcessor::setHostedPluginName(juce::String value)
{
    const juce::ScopedLock sl(innerMutex);
    hostedPluginName = value;
}

juce::String VST3LoaderAudioProcessor::getHostedPluginPath()
{
    const juce::ScopedLock sl(innerMutex);
    return hostedPluginPath;
}

void VST3LoaderAudioProcessor::removePreviouslyHostedPluginIfNeeded(bool unsetError)
{
    safelyPerform<void>([](auto& p)
    {
        jassert(p->getActiveEditor() == nullptr);
    });
    
    setHostedPluginInstance(nullptr);
    setHostedPluginPath("");
    if (unsetError) { setHostedPluginLoadingError(""); }
    setIsLoading(false);
    setHostedPluginName("");
}

void VST3LoaderAudioProcessor::loadPluginFromFile(const juce::String& pluginPath,
                                                  PluginLoadingCallback vst3FileLoadingCompleted)
{
    juce::MessageManager::callAsync([this, pluginPath, vst3FileLoadingCompleted]()
    {
        juce::OwnedArray<juce::PluginDescription> descs;
        
        juce::AudioPluginFormat* vst3Format = nullptr;
        for (int i = 0; i < formatManager.getNumFormats(); ++i)
        {
            if (formatManager.getFormat(i)->getName() == "VST3")
            {
                vst3Format = formatManager.getFormat(i);
                break;
            }
        }
        
        if (vst3Format == nullptr)
        {
            setHostedPluginLoadingError(juce::String(juce::CharPointer_UTF8("VST3 format not found")));
            vst3FileLoadingCompleted(nullptr);
            return;
        }
        
        vst3Format->findAllTypesForFile(descs, pluginPath);
        
        if (descs.isEmpty())
        {
            setHostedPluginLoadingError(juce::String(juce::CharPointer_UTF8("No valid VST3 found")));
            vst3FileLoadingCompleted(nullptr);
            return;
        }
        
        auto descIndex = -1;
        for (int i = 0; i < descs.size(); ++i)
        {
            if (!descs[i]->isInstrument)
            {
                descIndex = i;
                break;
            }
        }
        
        if (descIndex == -1)
        {
            setHostedPluginLoadingError(juce::String(juce::CharPointer_UTF8("Selected VST3 is not an Audio Effect")));
            vst3FileLoadingCompleted(nullptr);
            return;
        }
        
        const auto pluginDescription = *descs[descIndex];
        
        auto callback = [this, vst3FileLoadingCompleted](auto pluginInstance, const auto& errorMessage)
        {
            if (pluginInstance == nullptr)
            {
                juce::String errorMsg = errorMessage.isEmpty() ?
                    juce::String(juce::CharPointer_UTF8("Unexpected error occurred")) : errorMessage;
                setHostedPluginLoadingError(errorMsg);
                vst3FileLoadingCompleted(nullptr);
                return;
            }
            
            vst3FileLoadingCompleted(std::move(pluginInstance));
        };
        
        formatManager.createPluginInstanceAsync(pluginDescription,
                                               getSampleRate(),
                                               getBlockSize(),
                                               callback);
    });
}

bool VST3LoaderAudioProcessor::setHostedPluginLayout()
{
    safelyPerform<void>([](auto& p) { p->enableAllBuses(); });
    return true;
}

bool VST3LoaderAudioProcessor::prepareHostedPluginForPlaying()
{
    setLatencySamples(safelyPerform<int>([](auto& p) { return p->getLatencySamples(); }));
    
    safelyPerform<void>([this](auto& p)
    {
        p->setRateAndBufferSizeDetails(getSampleRate(), getBlockSize());
        p->prepareToPlay(getSampleRate(), getBlockSize());
    });
    
    return true;
}

void VST3LoaderAudioProcessor::setHostedPluginState()
{
    const juce::ScopedLock sl(innerMutex);
    
    safelyPerform<void>([this](auto& p)
    {
        if (!hostedPluginState.isEmpty())
        {
            p->setStateInformation(hostedPluginState.getData(),
                                  (int) hostedPluginState.getSize());
        }
    });
    
    hostedPluginState = juce::MemoryBlock();
}

void VST3LoaderAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    safelyPerform<void>([this, &destData](auto& p)
    {
        juce::XmlElement xml("state");
        
        auto filePathElement = std::make_unique<juce::XmlElement>(pluginPathTag);
        filePathElement->addTextElement(getHostedPluginPath());
        xml.addChildElement(filePathElement.release());
        
        juce::MemoryBlock innerState;
        p->getStateInformation(innerState);
        auto stateNode = std::make_unique<juce::XmlElement>(innerStateTag);
        stateNode->addTextElement(innerState.toBase64Encoding());
        xml.addChildElement(stateNode.release());
        
        const auto text = xml.toString();
        destData.replaceAll(text.toRawUTF8(), text.getNumBytesAsUTF8());
    });
}

void VST3LoaderAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const juce::ScopedLock sl(innerMutex);
    
    auto xml = juce::XmlDocument::parse(juce::String(juce::CharPointer_UTF8(static_cast<const char*>(data)),
                                                     (size_t) sizeInBytes));
    
    if (xml != nullptr)
    {
        if (auto* pluginPathNode = xml->getChildByName(pluginPathTag))
        {
            auto pluginPath = pluginPathNode->getAllSubText();
            juce::MemoryBlock innerState;
            auto base64String = xml->getChildElementAllSubText(innerStateTag, {});
            innerState.fromBase64Encoding(base64String);
            hostedPluginState = innerState;
            
            loadPlugin(pluginPath);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VST3LoaderAudioProcessor();
}
