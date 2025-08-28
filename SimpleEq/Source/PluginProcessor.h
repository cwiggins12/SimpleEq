/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>

template<typename T>
struct Fifo
{
    void prepare(int numChannels, int numSamples)
    {
        static_assert(std::is_same_v<T, juce::AudioBuffer<float>>,
            "prepare(numChannels, numSamples) should only be used when the Fifo is holding juce::AudioBuffer<float>");
        for (auto& buffer : buffers)
        {
            buffer.setSize(numChannels,
                numSamples,
                false,   //clear everything?
                true,    //including the extra space?
                true);   //avoid reallocating if you can?
            buffer.clear();
        }
    }

    void prepare(size_t numElements)
    {
        static_assert(std::is_same_v<T, std::vector<float>>,
            "prepare(numElements) should only be used when the Fifo is holding std::vector<float>");
        for (auto& buffer : buffers)
        {
            buffer.clear();
            buffer.resize(numElements, 0);
        }
    }

    bool push(const T& t)
    {
        auto write = fifo.write(1);
        if (write.blockSize1 > 0)
        {
            buffers[write.startIndex1] = t;
            return true;
        }

        return false;
    }

    bool pull(T& t)
    {
        auto read = fifo.read(1);
        if (read.blockSize1 > 0)
        {
            t = buffers[read.startIndex1];
            return true;
        }

        return false;
    }

    int getNumAvailableForReading() const
    {
        return fifo.getNumReady();
    }
private:
    static constexpr int Capacity = 30;
    std::array<T, Capacity> buffers;
    juce::AbstractFifo fifo{ Capacity };
};

enum Channel
{
    Left, //effectively 0
    Right //effectively 1
};

template<typename BlockType>
struct SingleChannelSampleFifo
{
    SingleChannelSampleFifo(Channel ch) : channelToUse(ch)
    {
        prepared.set(false);
    }

    void update(const BlockType& buffer)
    {
        jassert(prepared.get());
        jassert(buffer.getNumChannels() > channelToUse);
        auto* channelPtr = buffer.getReadPointer(channelToUse);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            pushNextSampleIntoFifo(channelPtr[i]);
        }
    }

    void prepare(int bufferSize)
    {
        prepared.set(false);
        size.set(bufferSize);

        bufferToFill.setSize(1,             //channel
            bufferSize,    //num samples
            false,         //keepExistingContent
            true,          //clear extra space
            true);         //avoid reallocating
        audioBufferFifo.prepare(1, bufferSize);
        fifoIndex = 0;
        prepared.set(true);
    }
    //==============================================================================
    int getNumCompleteBuffersAvailable() const { return audioBufferFifo.getNumAvailableForReading(); }
    bool isPrepared() const { return prepared.get(); }
    int getSize() const { return size.get(); }
    //==============================================================================
    bool getAudioBuffer(BlockType& buf) { return audioBufferFifo.pull(buf); }
private:
    Channel channelToUse;
    int fifoIndex = 0;
    Fifo<BlockType> audioBufferFifo;
    BlockType bufferToFill;
    juce::Atomic<bool> prepared = false;
    juce::Atomic<int> size = 0;

    void pushNextSampleIntoFifo(float sample)
    {
        if (fifoIndex == bufferToFill.getNumSamples())
        {
            auto ok = audioBufferFifo.push(bufferToFill);

            juce::ignoreUnused(ok);

            fifoIndex = 0;
        }

        bufferToFill.setSample(0, fifoIndex, sample);
        ++fifoIndex;
    }
};


enum Slope {
    Slope_12,
    Slope_24,
    Slope_36,
    Slope_48
};

struct ChainSettings {
    float peak1Freq{ 0 }, peak1GainInDecibels{ 0 }, peak1Quality{ 1.0f };
    float peak2Freq{ 0 }, peak2GainInDecibels{ 0 }, peak2Quality{ 1.0f };
    float peak3Freq{ 0 }, peak3GainInDecibels{ 0 }, peak3Quality{ 1.0f };
    float lowCutFreq{ 0 }, highCutFreq{ 0 };
    int lowCutSlope{ Slope::Slope_12 }, highCutSlope{ Slope::Slope_12 };
    bool lowCutBypass{ false }, highCutBypass{ false }, peak1Bypass{ false }, peak2Bypass{ false }, peak3Bypass{ false };
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

using Filter = juce::dsp::IIR::Filter<float>;
using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;
using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, Filter, Filter, CutFilter>;

enum ChainPositions {
    LowCut,
    Peak1,
    Peak2,
    Peak3,
    HighCut
};

using Coefficients = Filter::CoefficientsPtr;
void updateCoefficients(Coefficients& old, const Coefficients& replacements);

Coefficients makePeak1Filter(const ChainSettings& chainSettings, double sampleRate);
Coefficients makePeak2Filter(const ChainSettings& chainSettings, double sampleRate);
Coefficients makePeak3Filter(const ChainSettings& chainSettings, double sampleRate);

inline auto makeLowCutFilter(const ChainSettings& chainSettings, double sampleRate) {
    return juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq, sampleRate,
                                                                                      (chainSettings.lowCutSlope + 1) * 2);
}

inline auto makeHighCutFilter(const ChainSettings& chainSettings, double sampleRate) {
    return juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(chainSettings.highCutFreq, sampleRate,
                                                                                     (chainSettings.highCutSlope + 1) * 2);
}

//updated for the same reasons as updatCutFilter. The typename Coefficient was rejected by compiler. Maybe from Juce update?
template<int Index, typename ChainType>
void update(ChainType& chain, const juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>>& coefficients) {
    updateCoefficients(chain.template get<Index>().coefficients, coefficients[Index]);
    chain.template setBypassed<Index>(false);
}

// slope is 0..3 (for 12/24/36/48 dB/oct), Juce was displeased with the Slope obj and treated it like an int. So now its an int
//coeffs could probably be a less ridiculous type, but this is the only one I could get working 
//uses fallthrough on the switch to update each lower order
template <typename ChainType>
void updateCutFilter(ChainType& filterChain, const juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>>& coeffs, int slope) {
    filterChain.template setBypassed<0>(true);
    filterChain.template setBypassed<1>(true);
    filterChain.template setBypassed<2>(true);
    filterChain.template setBypassed<3>(true);

    switch (slope) {
    case 3: {
        update<3>(filterChain, coeffs);
        [[fallthrough]];
    }
    case 2: {
        update<2>(filterChain, coeffs);
        [[fallthrough]];
    }
    case 1: {
        update<1>(filterChain, coeffs);
        [[fallthrough]];
    }
    case 0: {
        update<0>(filterChain, coeffs);
        [[fallthrough]];
    }
    }
}

//template to have true logarithmic skew for frequency sliders, dont forget to cast to float :)
template <typename ValueT>
juce::NormalisableRange<ValueT> logRange(ValueT min, ValueT max)
{
    ValueT rng{ std::log(max / min) };
    return { min, max,
        [=](ValueT min, ValueT, ValueT v) { return std::exp(v * rng) * min; },
        [=](ValueT min, ValueT, ValueT v) { return std::log(v / min) / rng; }
    };
}

//==============================================================================
/**
*/
class SimpleEQFromTutorialAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    SimpleEQFromTutorialAudioProcessor();
    ~SimpleEQFromTutorialAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts{ *this, nullptr, "Parameters", createParameterLayout() };

    using BlockType = juce::AudioBuffer<float>;
    SingleChannelSampleFifo<BlockType> leftChannelFifo{ Channel::Left };
    SingleChannelSampleFifo<BlockType> rightChannelFifo{ Channel::Right };

private:
    MonoChain leftChain, rightChain;
    void updatePeakFilters(const ChainSettings& chainSettings);
    void updateLowCutFilters(const ChainSettings& chainSettings);
    void updateHighCutFilters(const ChainSettings& chainSettings);
    void updateFilters();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleEQFromTutorialAudioProcessor)
};
