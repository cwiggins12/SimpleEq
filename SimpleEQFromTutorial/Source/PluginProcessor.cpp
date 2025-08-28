/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SimpleEQFromTutorialAudioProcessor::SimpleEQFromTutorialAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{}

SimpleEQFromTutorialAudioProcessor::~SimpleEQFromTutorialAudioProcessor() {}

//==============================================================================
const juce::String SimpleEQFromTutorialAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SimpleEQFromTutorialAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SimpleEQFromTutorialAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SimpleEQFromTutorialAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SimpleEQFromTutorialAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SimpleEQFromTutorialAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SimpleEQFromTutorialAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SimpleEQFromTutorialAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SimpleEQFromTutorialAudioProcessor::getProgramName (int index)
{
    return {};
}

void SimpleEQFromTutorialAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SimpleEQFromTutorialAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock) {
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;
    spec.sampleRate = sampleRate;

    leftChain.prepare(spec);
    rightChain.prepare(spec);

    updateFilters();

    leftChannelFifo.prepare(samplesPerBlock);
    rightChannelFifo.prepare(samplesPerBlock);
}

void SimpleEQFromTutorialAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SimpleEQFromTutorialAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void SimpleEQFromTutorialAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    updateFilters();

    juce::dsp::AudioBlock<float> block(buffer);

    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);

    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);

    leftChain.process(leftContext);
    rightChain.process(rightContext);

    leftChannelFifo.update(buffer);
    rightChannelFifo.update(buffer);
}

//==============================================================================
bool SimpleEQFromTutorialAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SimpleEQFromTutorialAudioProcessor::createEditor()
{
    return new SimpleEQFromTutorialAudioProcessorEditor(*this);
    //return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void SimpleEQFromTutorialAudioProcessor::getStateInformation (juce::MemoryBlock& destData) {
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void SimpleEQFromTutorialAudioProcessor::setStateInformation (const void* data, int sizeInBytes) {
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid()) {
        apvts.replaceState(tree);
        updateFilters();
    }
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts) {
    ChainSettings settings;
    settings.lowCutFreq = apvts.getRawParameterValue("LowCut Freq")->load();
    settings.highCutFreq = apvts.getRawParameterValue("HighCut Freq")->load();
    settings.peak1Freq = apvts.getRawParameterValue("Peak 1 Freq")->load();
    settings.peak1GainInDecibels = apvts.getRawParameterValue("Peak 1 Gain")->load();
    settings.peak1Quality = apvts.getRawParameterValue("Peak 1 Quality")->load();
    settings.peak2Freq = apvts.getRawParameterValue("Peak 2 Freq")->load();
    settings.peak2GainInDecibels = apvts.getRawParameterValue("Peak 2 Gain")->load();
    settings.peak2Quality = apvts.getRawParameterValue("Peak 2 Quality")->load();
    settings.peak3Freq = apvts.getRawParameterValue("Peak 3 Freq")->load();
    settings.peak3GainInDecibels = apvts.getRawParameterValue("Peak 3 Gain")->load();
    settings.peak3Quality = apvts.getRawParameterValue("Peak 3 Quality")->load();
    settings.lowCutSlope = static_cast<Slope>(/*static_cast<int>*/(apvts.getRawParameterValue("LowCut Slope")->load()));
    settings.highCutSlope = static_cast<Slope>(/*static_cast<int>*/(apvts.getRawParameterValue("HighCut Slope")->load()));
    settings.lowCutBypass = apvts.getRawParameterValue("LowCut Bypass")->load() > 0.5f;
    settings.highCutBypass = apvts.getRawParameterValue("HighCut Bypass")->load() > 0.5f;
    settings.peak1Bypass = apvts.getRawParameterValue("Peak 1 Bypass")->load() > 0.5f;
    settings.peak2Bypass = apvts.getRawParameterValue("Peak 2 Bypass")->load() > 0.5f;
    settings.peak3Bypass = apvts.getRawParameterValue("Peak 3 Bypass")->load() > 0.5f;
    return settings;
}

Coefficients makePeak1Filter(const ChainSettings& chainSettings, double sampleRate) {
    return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, chainSettings.peak1Freq,
        chainSettings.peak1Quality, juce::Decibels::decibelsToGain(chainSettings.peak1GainInDecibels));
}

Coefficients makePeak2Filter(const ChainSettings& chainSettings, double sampleRate) {
    return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, chainSettings.peak2Freq,
        chainSettings.peak2Quality, juce::Decibels::decibelsToGain(chainSettings.peak2GainInDecibels));
}

Coefficients makePeak3Filter(const ChainSettings& chainSettings, double sampleRate) {
    return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, chainSettings.peak3Freq,
        chainSettings.peak3Quality, juce::Decibels::decibelsToGain(chainSettings.peak3GainInDecibels));
}

void SimpleEQFromTutorialAudioProcessor::updatePeakFilters(const ChainSettings& chainSettings) {
    auto sampleRate = getSampleRate();
    auto peak1Coefficients = makePeak1Filter(chainSettings, sampleRate);
    auto peak2Coefficients = makePeak2Filter(chainSettings, sampleRate);
    auto peak3Coefficients = makePeak3Filter(chainSettings, sampleRate);

    leftChain.setBypassed<ChainPositions::Peak1>(chainSettings.peak1Bypass);
    rightChain.setBypassed<ChainPositions::Peak1>(chainSettings.peak1Bypass);
    leftChain.setBypassed<ChainPositions::Peak1>(chainSettings.peak2Bypass);
    rightChain.setBypassed<ChainPositions::Peak1>(chainSettings.peak2Bypass);
    leftChain.setBypassed<ChainPositions::Peak1>(chainSettings.peak2Bypass);
    rightChain.setBypassed<ChainPositions::Peak1>(chainSettings.peak2Bypass);

    updateCoefficients(leftChain.get<ChainPositions::Peak1>().coefficients, peak1Coefficients);
    updateCoefficients(rightChain.get<ChainPositions::Peak1>().coefficients, peak1Coefficients);
    updateCoefficients(leftChain.get<ChainPositions::Peak2>().coefficients, peak2Coefficients);
    updateCoefficients(rightChain.get<ChainPositions::Peak2>().coefficients, peak2Coefficients);
    updateCoefficients(leftChain.get<ChainPositions::Peak3>().coefficients, peak3Coefficients);
    updateCoefficients(rightChain.get<ChainPositions::Peak3>().coefficients, peak3Coefficients);
}

void updateCoefficients(Coefficients& old, const Coefficients& replacements) {
    *old = *replacements;
}

void SimpleEQFromTutorialAudioProcessor::updateLowCutFilters(const ChainSettings &chainSettings) {
    auto lowCutCoefficients = makeLowCutFilter(chainSettings, getSampleRate());
    auto& leftLowCut = leftChain.get<LowCut>();
    auto& rightLowCut = rightChain.get<LowCut>();

    leftChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypass);
    rightChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypass);

    updateCutFilter(leftLowCut, lowCutCoefficients, chainSettings.lowCutSlope);
    updateCutFilter(rightLowCut, lowCutCoefficients, chainSettings.lowCutSlope);
}

void SimpleEQFromTutorialAudioProcessor::updateHighCutFilters(const ChainSettings& chainSettings) {
    auto highCutCoefficients = makeHighCutFilter(chainSettings, getSampleRate());
    auto& leftHighCut = leftChain.get<HighCut>();
    auto& rightHighCut = rightChain.get<HighCut>();

    leftChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypass);
    rightChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypass);

    updateCutFilter(leftHighCut, highCutCoefficients, chainSettings.highCutSlope);
    updateCutFilter(rightHighCut, highCutCoefficients, chainSettings.highCutSlope);
}

void SimpleEQFromTutorialAudioProcessor::updateFilters() {
    auto chainSettings = getChainSettings(apvts);
    updateLowCutFilters(chainSettings);
    updatePeakFilters(chainSettings);
    updateHighCutFilters(chainSettings);
}

juce::AudioProcessorValueTreeState::ParameterLayout SimpleEQFromTutorialAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>("LowCut Freq", "LowCut Freq", logRange<float>(20.0f, 20000.0f), 20.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("HighCut Freq", "HighCut Freq", logRange<float>(20.0f, 20000.0f), 20000.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak 1 Freq", "Peak 1 Freq", logRange<float>(20.0f, 20000.0f), 750.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak 1 Gain", "Peak 1 Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.5f, 1.0f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak 1 Quality", "Peak 1 Quality", juce::NormalisableRange<float>(0.1f, 10.0f, 0.05f, 1.0f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak 2 Freq", "Peak 2 Freq", logRange<float>(20.0f, 20000.0f), 750.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak 2 Gain", "Peak 2 Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.5f, 1.0f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak 2 Quality", "Peak 2 Quality", juce::NormalisableRange<float>(0.1f, 10.0f, 0.05f, 1.0f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak 3 Freq", "Peak 3 Freq", logRange<float>(20.0f, 20000.0f), 750.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak 3 Gain", "Peak 3 Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.5f, 1.0f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak 3 Quality", "Peak 3 Quality", juce::NormalisableRange<float>(0.1f, 10.0f, 0.05f, 1.0f), 1.0f));
    juce::StringArray stringArray;
    for (int i = 0; i < 4; ++i){
        juce::String str;
        str << (12 + i * 12);
        str << " db/Oct";
        stringArray.add(str);
    }
    layout.add(std::make_unique<juce::AudioParameterChoice>("LowCut Slope", "LowCutSlope", stringArray, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>("HighCut Slope", "HighCutSlope", stringArray, 0));
    layout.add(std::make_unique<juce::AudioParameterBool>("LowCut Bypass", "LowCut Bypass", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("HighCut Bypass", "HighCut Bypass", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("Peak 1 Bypass", "Peak 1 Bypass", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("Peak 2 Bypass", "Peak 2 Bypass", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("Peak 3 Bypass", "Peak 3 Bypass", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("Analyser Enabled", "Analyser Enabled", true));
    return layout;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleEQFromTutorialAudioProcessor();
}
