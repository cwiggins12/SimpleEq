/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float
    sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) {
    using namespace juce;

    auto bounds = Rectangle<float>(x, y, width, height);

    auto enabled = slider.isEnabled();

    g.setColour(enabled ? Colour(97u, 18u, 167u) : Colours::darkgrey);
    g.fillEllipse(bounds);

    g.setColour(enabled ? Colour(255u, 154u, 1u) : Colours::grey);
    g.drawEllipse(bounds, 1.0f);

    if (auto* rswl = dynamic_cast<RotarySliderWithLabels*>(&slider)) {
        auto center = bounds.getCentre();
        Path p;

        Rectangle<float> r;
        r.setLeft(center.getX() - 2);
        r.setRight(center.getX() + 2);
        r.setTop(bounds.getY());
        r.setBottom(center.getY() - rswl->getTextHeight() * 1.5);
        p.addRoundedRectangle(r, 2.0f);
        jassert(rotaryStartAngle < rotaryEndAngle);
        auto sliderAngRad = jmap(sliderPosProportional, 0.0f, 1.0f, rotaryStartAngle, rotaryEndAngle);
        p.applyTransform(AffineTransform().rotated(sliderAngRad, center.getX(), center.getY()));
        g.fillPath(p);

        g.setFont(rswl->getTextHeight());
        auto text = rswl->getDisplayString();
        auto strWidth = g.getCurrentFont().getStringWidth(text);

        r.setSize(strWidth + 4, rswl->getTextHeight() + 2);
        r.setCentre(center);

        g.setColour(enabled ? Colours::black : Colours::darkgrey);
        g.fillRect(r);

        g.setColour(enabled ? Colours::white : Colours::lightgrey);
        g.drawFittedText(text, r.toNearestInt(), juce::Justification::centred, 1);
    }
}

void LookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& toggleButton, bool highlighted, bool down) {
    using namespace juce;
    if (auto* pb = dynamic_cast<PowerButton*>(&toggleButton)) {
        Path powerButton;
        auto bounds = toggleButton.getLocalBounds();
        auto size = jmin(bounds.getWidth(), bounds.getHeight()) - 12;
        auto r = bounds.withSizeKeepingCentre(size, size).toFloat();
        float ang = 30.0f;

        powerButton.addCentredArc(r.getCentreX(), r.getCentreY(), size * 0.5f, size * 0.5f, 0.0f, degreesToRadians(ang), degreesToRadians(360.0f - ang), true);
        powerButton.startNewSubPath(r.getCentreX(), r.getY());
        powerButton.lineTo(r.getCentre());
        PathStrokeType pst(2.0f, PathStrokeType::JointStyle::curved);

        auto colour = toggleButton.getToggleState() ? Colours::dimgrey : Colour(0u, 172u, 1u);
        g.setColour(colour);
        g.strokePath(powerButton, pst);
        g.drawEllipse(r, 2);
    }
    else if (auto* analyserButton = dynamic_cast<AnalyserButton*>(&toggleButton) ){
        auto colour = !toggleButton.getToggleState() ? Colours::dimgrey : Colour(0u, 172u, 1u);

        g.setColour(colour);
        auto bounds = toggleButton.getLocalBounds();
        g.drawRect(bounds);

        g.strokePath(analyserButton->randomPath, PathStrokeType(1.0f));
    }
}

//==============================================================================
void RotarySliderWithLabels::paint(juce::Graphics& g) {
    using namespace juce;

    auto startAng = degreesToRadians(180.0f + 45.0f);
    auto endAng = degreesToRadians(540.0f - 45.0f);

    auto range = getRange();
    auto sliderBounds = getSliderBounds();

    getLookAndFeel().drawRotarySlider(g, sliderBounds.getX(), sliderBounds.getY(), sliderBounds.getWidth(), sliderBounds.getHeight(), 
                                      jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0), startAng, endAng, *this);

    auto center = sliderBounds.toFloat().getCentre();
    auto radius = sliderBounds.getWidth() * 0.5f;

    g.setColour(Colour(0u, 172u, 1u));
    g.setFont(getTextHeight());

    auto numChoices = labels.size();
    for (int i = 0; i < numChoices; ++i) {
        auto pos = labels[i].pos;
        jassert(0.0f <= pos);
        jassert(pos <= 1.0f);
        auto ang = jmap(pos, 0.0f, 1.0f, startAng, endAng);
        auto c = center.getPointOnCircumference(radius + getTextHeight() * 0.5f + 1, ang);

        Rectangle<float> r;
        auto str = labels[i].label;
        r.setSize(g.getCurrentFont().getStringWidth(str), getTextHeight());
        r.setCentre(c);
        r.setY(r.getY() + getTextHeight());
        g.drawFittedText(str, r.toNearestInt(), juce::Justification::centred, 1);
    }
}

juce::Rectangle<int> RotarySliderWithLabels::getSliderBounds() const {
    auto bounds = getLocalBounds();
    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());

    size -= getTextHeight() * 2;
    juce::Rectangle<int> r;
    r.setSize(size, size);
    r.setCentre(bounds.getCentreX(), 0);
    r.setY(2);
    return r;
}

juce::String RotarySliderWithLabels::getDisplayString() const {
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(param))
        return choiceParam->getCurrentChoiceName();
    juce::String str;
    bool addK = false;
    if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param)) {
        float val = getValue();
        if (val > 999.0f) {
            val /= 1000.0f;
            addK = true;
        }
        str = juce::String(val, (addK ? 2 : 0));
    }
    else {
        jassertfalse;
    }
    if (suffix.isNotEmpty()) {
        str << " ";
        if (addK)
            str << "k";
        str << suffix;
    }
    return str;
}

//==============================================================================
ResponseCurveComponent::ResponseCurveComponent(SimpleEQFromTutorialAudioProcessor& p) : audioProcessor(p), leftPathProducer(audioProcessor.leftChannelFifo),
                                                                                                           rightPathProducer(audioProcessor.rightChannelFifo){
    const auto& params = audioProcessor.getParameters();
    for (auto param : params)
        param->addListener(this);

    updateChain();

    startTimerHz(60);
}

ResponseCurveComponent::~ResponseCurveComponent() {
    const auto& params = audioProcessor.getParameters();
    for (auto param : params)
        param->removeListener(this);
}

void ResponseCurveComponent::parameterValueChanged(int parmeterIndex, float newValue) {
    parametersChanged.set(true);
}

void PathProducer::process(juce::Rectangle<float> fftBounds, double sampleRate) {
    juce::AudioBuffer<float> tempIncomingBuffer;

    while (leftChannelFifo->getNumCompleteBuffersAvailable() > 0) {
        if (leftChannelFifo->getAudioBuffer(tempIncomingBuffer)) {
            auto size = tempIncomingBuffer.getNumSamples();
            juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0, 0), monoBuffer.getReadPointer(0, size), monoBuffer.getNumSamples() - size);
            juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0, monoBuffer.getNumSamples() - size), tempIncomingBuffer.getReadPointer(0, 0), size);
            leftChannelFFTDataGenerator.produceFFTDataForRendering(monoBuffer, -48.0f);
        }
    }
    /* if there are FFT data buffers to pull
        if we can pull a buffer
            generate a path */
    auto fftSize = leftChannelFFTDataGenerator.getFFTSize();
    const auto binWidth = sampleRate / (double)fftSize;

    while (leftChannelFFTDataGenerator.getNumAvailableFFTDataBlocks() > 0) {
        std::vector<float> fftData;
        if (leftChannelFFTDataGenerator.getFFTData(fftData)) {
            pathProducer.generatePath(fftData, fftBounds, fftSize, binWidth, -48.0f);
        }
    }

    /*
    while there ar paths that can be pulled
        pull as many as we can
            display the most recent path
    */
    while (pathProducer.getNumPathsAvailable()) {
        pathProducer.getPath(leftChannelFFTPath);
    }
}

void ResponseCurveComponent::timerCallback() {
    if (shouldShowFFTAnalysis) {
        auto fftBounds = getAnalArea().toFloat();
        auto sampleRate = audioProcessor.getSampleRate();
        leftPathProducer.process(fftBounds, sampleRate);
        rightPathProducer.process(fftBounds, sampleRate);
    }

    if (parametersChanged.compareAndSetBool(false, true))
        updateChain();

    repaint();
}

void ResponseCurveComponent::updateChain() {
    auto chainSettings = getChainSettings(audioProcessor.apvts);
    auto sampleRate = audioProcessor.getSampleRate();
    monoChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypass);
    monoChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypass);
    monoChain.setBypassed<ChainPositions::Peak1>(chainSettings.peak1Bypass);
    monoChain.setBypassed<ChainPositions::Peak2>(chainSettings.peak2Bypass);
    monoChain.setBypassed<ChainPositions::Peak3>(chainSettings.peak3Bypass);

    auto lowCutCoefficients = makeLowCutFilter(chainSettings, sampleRate);
    auto highCutCoefficients = makeHighCutFilter(chainSettings, sampleRate);
    auto peak1Coefficients = makePeak1Filter(chainSettings, sampleRate);
    auto peak2Coefficients = makePeak2Filter(chainSettings, sampleRate);
    auto peak3Coefficients = makePeak3Filter(chainSettings, sampleRate);
    updateCutFilter(monoChain.get<ChainPositions::LowCut>(), lowCutCoefficients, chainSettings.lowCutSlope);
    updateCutFilter(monoChain.get<ChainPositions::HighCut>(), highCutCoefficients, chainSettings.highCutSlope);
    updateCoefficients(monoChain.get<ChainPositions::Peak1>().coefficients, peak1Coefficients);
    updateCoefficients(monoChain.get<ChainPositions::Peak2>().coefficients, peak2Coefficients);
    updateCoefficients(monoChain.get<ChainPositions::Peak3>().coefficients, peak3Coefficients);
}

void ResponseCurveComponent::paint(juce::Graphics& g)
{
    using namespace juce;
    g.fillAll(Colours::black);

    g.drawImage(background, getLocalBounds().toFloat());

    auto responseArea = getAnalArea();

    auto w = responseArea.getWidth();

    auto& lowCut = monoChain.get<ChainPositions::LowCut>();
    auto& peak1 = monoChain.get<ChainPositions::Peak1>();
    auto& peak2 = monoChain.get<ChainPositions::Peak2>();
    auto& peak3 = monoChain.get<ChainPositions::Peak3>();
    auto& highCut = monoChain.get<ChainPositions::HighCut>();

    auto sampleRate = audioProcessor.getSampleRate();

    std::vector<double> mags;

    mags.resize(w);

    for (int i = 0; i < w; ++i) {
        double mag = 1.0f;
        auto freq = mapToLog10(double(i) / double(w), 20.0, 20000.0);

        if (!monoChain.isBypassed<ChainPositions::Peak1>())
            mag *= peak1.coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!monoChain.isBypassed<ChainPositions::Peak2>())
            mag *= peak2.coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!monoChain.isBypassed<ChainPositions::Peak3>())
            mag *= peak3.coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!monoChain.isBypassed<ChainPositions::LowCut>()) {
            if (!lowCut.isBypassed<0>())
                mag *= lowCut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!lowCut.isBypassed<1>())
                mag *= lowCut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!lowCut.isBypassed<2>())
                mag *= lowCut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!lowCut.isBypassed<3>())
                mag *= lowCut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }
        if (!monoChain.isBypassed<ChainPositions::HighCut>()) {
            if (!highCut.isBypassed<0>())
                mag *= highCut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!highCut.isBypassed<1>())
                mag *= highCut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!highCut.isBypassed<2>())
                mag *= highCut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!highCut.isBypassed<3>())
                mag *= highCut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }
        mags[i] = Decibels::gainToDecibels(mag);
    }

    Path responseCurve;

    const double outputMin = responseArea.getBottom();
    const double outputMax = responseArea.getY();
    auto map = [outputMin, outputMax](double input) {
        return jmap(input, -24.0, 24.0, outputMin, outputMax);
        };

    responseCurve.startNewSubPath(responseArea.getX(), map(mags.front()));

    for (size_t i = 1; i < mags.size(); ++i)
        responseCurve.lineTo(responseArea.getX() + i, map(mags[i]));

    if (shouldShowFFTAnalysis) {
        auto leftChannelFFTPath = leftPathProducer.getPath();
        leftChannelFFTPath.applyTransform(AffineTransform().translation(responseArea.getX(), responseArea.getY()));

        g.setColour(Colours::skyblue);
        g.strokePath(leftChannelFFTPath, PathStrokeType(1.0f));

        auto rightChannelFFTPath = rightPathProducer.getPath();
        rightChannelFFTPath.applyTransform(AffineTransform().translation(responseArea.getX(), responseArea.getY()));

        g.setColour(Colours::lightyellow);
        g.strokePath(rightChannelFFTPath, PathStrokeType(1.0f));
    }

    g.setColour(Colours::orange);
    g.drawRoundedRectangle(getRenderArea().toFloat(), 4.0f, 1.0f);

    g.setColour(Colours::white);
    g.strokePath(responseCurve, PathStrokeType(2.0f));
}

void ResponseCurveComponent::resized() {
    using namespace juce;
    background = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    Graphics g(background);

    Array<float> freqs{ 20, /*30, 40,*/ 50, 100, 200, /*300, 400,*/ 500, 1000, 2000, /*3000, 4000,*/ 5000, 10000, 20000 };

    auto renderArea = getAnalArea();
    auto left = renderArea.getX();
    auto right = renderArea.getRight();
    auto top = renderArea.getY();
    auto bottom = renderArea.getBottom();
    auto width = renderArea.getWidth();

    Array<float> xs;
    for (auto f : freqs) {
        auto normX = mapFromLog10(f, 20.0f, 20000.0f);
        xs.add(left + width * normX);
    }

    g.setColour(Colours::dimgrey);
    for (auto x : xs) {
        g.drawVerticalLine(x, top, bottom);
    }
    Array<float> gain{ -24, -12, 0, 12, 24 };
    for (auto gDb : gain) {
        auto y = jmap(gDb, -24.0f, 24.0f, float(bottom), float(top));
        g.setColour(gDb == 0.0f ? Colour(0u, 172u, 1u) : Colours::darkgrey);
        g.drawHorizontalLine(y, left, right);
    }
    
    g.setColour(Colours::lightgrey);
    const int fontHeight = 10;
    g.setFont(fontHeight);
    for (int i = 0; i < freqs.size(); ++i) {
        auto f = freqs[i];
        auto x = xs[i];

        bool addK = false;
        String str;
        if (f > 999.0f) {
            addK = true;
            f /= 1000.0f;
        }
        str << f;
        if (addK)
            str << "k";
        str << "Hz";

        auto textWidth = g.getCurrentFont().getStringWidth(str);

        Rectangle<int> r;
        r.setSize(textWidth, fontHeight);
        r.setCentre(x, 0);
        r.setY(1);

        g.drawFittedText(str, r, juce::Justification::centred, 1);
    }

    for (auto gDb : gain) {
        auto y = jmap(gDb, -24.0f, 24.0f, float(bottom), float(top));
        String str;
        if (gDb > 0)
            str << "+";
        str << gDb;

        auto textWidth = g.getCurrentFont().getStringWidth(str);

        Rectangle<int> r;
        r.setSize(textWidth, fontHeight);
        r.setX(getWidth() - textWidth);
        r.setCentre(r.getCentreX(), y);
        
        g.setColour(gDb == 0.0f ? Colour(0u, 172u, 1u) : Colours::lightgrey);
        g.drawFittedText(str, r, juce::Justification::centred, 1);

        //THIS CAN BE USED FOR METER TEXT LATER :)
        str.clear();
        str << (gDb - 24.0f);

        r.setX(1);
        textWidth = g.getCurrentFont().getStringWidth(str);
        r.setSize(textWidth, fontHeight);
        g.setColour(Colours::lightgrey);
        g.drawFittedText(str, r, juce::Justification::centred, 1);
    }
}

juce::Rectangle<int> ResponseCurveComponent::getRenderArea() {
    auto bounds = getLocalBounds();
    bounds.removeFromTop(12);
    bounds.removeFromBottom(2);
    bounds.removeFromLeft(20);
    bounds.removeFromRight(20);
    return bounds;
}

juce::Rectangle<int> ResponseCurveComponent::getAnalArea() {
    auto bounds = getRenderArea();
    bounds.removeFromTop(4);
    bounds.removeFromBottom(4);
    return bounds;
}

//==============================================================================
SimpleEQFromTutorialAudioProcessorEditor::SimpleEQFromTutorialAudioProcessorEditor(SimpleEQFromTutorialAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
    peak1FreqSlider(*audioProcessor.apvts.getParameter("Peak 1 Freq"), "Hz"),
    peak2FreqSlider(*audioProcessor.apvts.getParameter("Peak 2 Freq"), "Hz"),
    peak3FreqSlider(*audioProcessor.apvts.getParameter("Peak 3 Freq"), "Hz"),
    peak1GainSlider(*audioProcessor.apvts.getParameter("Peak 1 Gain"), "dB"),
    peak2GainSlider(*audioProcessor.apvts.getParameter("Peak 2 Gain"), "dB"),
    peak3GainSlider(*audioProcessor.apvts.getParameter("Peak 3 Gain"), "dB"),
    peak1QualitySlider(*audioProcessor.apvts.getParameter("Peak 1 Quality"), ""),
    peak2QualitySlider(*audioProcessor.apvts.getParameter("Peak 2 Quality"), ""),
    peak3QualitySlider(*audioProcessor.apvts.getParameter("Peak 3 Quality"), ""),
    lowCutSlopeSlider(*audioProcessor.apvts.getParameter("LowCut Slope"), "dB/Oct"),
    highCutSlopeSlider(*audioProcessor.apvts.getParameter("HighCut Slope"), "dB/Oct"),
    lowCutFreqSlider(*audioProcessor.apvts.getParameter("LowCut Freq"), "Hz"),
    highCutFreqSlider(*audioProcessor.apvts.getParameter("HighCut Freq"), "Hz"),

    responseCurveComponent(audioProcessor),

    peak1FreqSliderAttachment(audioProcessor.apvts, "Peak 1 Freq", peak1FreqSlider),
    peak1GainSliderAttachment(audioProcessor.apvts, "Peak 1 Gain", peak1GainSlider),
    peak1QualitySliderAttachment(audioProcessor.apvts, "Peak 1 Quality", peak1QualitySlider),
    peak2FreqSliderAttachment(audioProcessor.apvts, "Peak 2 Freq", peak2FreqSlider),
    peak2GainSliderAttachment(audioProcessor.apvts, "Peak 2 Gain", peak2GainSlider),
    peak2QualitySliderAttachment(audioProcessor.apvts, "Peak 2 Quality", peak2QualitySlider),
    peak3FreqSliderAttachment(audioProcessor.apvts, "Peak 3 Freq", peak3FreqSlider),
    peak3GainSliderAttachment(audioProcessor.apvts, "Peak 3 Gain", peak3GainSlider),
    peak3QualitySliderAttachment(audioProcessor.apvts, "Peak 3 Quality", peak3QualitySlider),
    lowCutFreqSliderAttachment(audioProcessor.apvts, "LowCut Freq", lowCutFreqSlider),
    highCutFreqSliderAttachment(audioProcessor.apvts, "HighCut Freq", highCutFreqSlider),
    lowCutSlopeSliderAttachment(audioProcessor.apvts, "LowCut Slope", lowCutSlopeSlider),
    highCutSlopeSliderAttachment(audioProcessor.apvts, "HighCut Slope", highCutSlopeSlider),
    lowCutBypassButtonAttachment(audioProcessor.apvts, "LowCut Bypass", lowCutBypassButton),
    highCutBypassButtonAttachment(audioProcessor.apvts, "HighCut Bypass", highCutBypassButton),
    peak1BypassButtonAttachment(audioProcessor.apvts, "Peak 1 Bypass", peak1BypassButton),
    peak2BypassButtonAttachment(audioProcessor.apvts, "Peak 2 Bypass", peak2BypassButton),
    peak3BypassButtonAttachment(audioProcessor.apvts, "Peak 3 Bypass", peak3BypassButton),
    analyserEnabledButtonAttachment(audioProcessor.apvts, "Analyzer Enabled", analyserEnabledButton)
{
    peak1FreqSlider.labels.add({ 0.0f, "20Hz"});
    peak1FreqSlider.labels.add({ 1.0f, "20khz" });
    peak1GainSlider.labels.add({ 0.0f, "-24dB" });
    peak1GainSlider.labels.add({ 1.0f, "24dB" });
    peak1QualitySlider.labels.add({ 0.0f, "0.1" });
    peak1QualitySlider.labels.add({ 1.0f, "10.0" });
    peak2FreqSlider.labels.add({ 0.0f, "20Hz" });
    peak2FreqSlider.labels.add({ 1.0f, "20khz" });
    peak2GainSlider.labels.add({ 0.0f, "-24dB" });
    peak2GainSlider.labels.add({ 1.0f, "24dB" });
    peak2QualitySlider.labels.add({ 0.0f, "0.1" });
    peak2QualitySlider.labels.add({ 1.0f, "10.0" });
    peak3FreqSlider.labels.add({ 0.0f, "20Hz" });
    peak3FreqSlider.labels.add({ 1.0f, "20khz" });
    peak3GainSlider.labels.add({ 0.0f, "-24dB" });
    peak3GainSlider.labels.add({ 1.0f, "24dB" });
    peak3QualitySlider.labels.add({ 0.0f, "0.1" });
    peak3QualitySlider.labels.add({ 1.0f, "10.0" });
    lowCutFreqSlider.labels.add({ 0.0f, "20Hz" });
    lowCutFreqSlider.labels.add({ 1.0f, "20khz" });
    lowCutSlopeSlider.labels.add({ 0.0f, "12" });
    lowCutSlopeSlider.labels.add({ 1.0f, "48" });
    highCutFreqSlider.labels.add({ 0.0f, "20Hz" });
    highCutFreqSlider.labels.add({ 1.0f, "20khz" });
    highCutSlopeSlider.labels.add({ 0.0f, "12" });
    highCutSlopeSlider.labels.add({ 1.0f, "48" });
    
    for (auto* comp : getComps()) {
        addAndMakeVisible(comp);
    }

    peak1BypassButton.setLookAndFeel(&lnf);
    peak2BypassButton.setLookAndFeel(&lnf);
    peak3BypassButton.setLookAndFeel(&lnf);
    lowCutBypassButton.setLookAndFeel(&lnf);
    highCutBypassButton.setLookAndFeel(&lnf);
    analyserEnabledButton.setLookAndFeel(&lnf);

    auto safePtr = juce::Component::SafePointer<SimpleEQFromTutorialAudioProcessorEditor>(this);
    peak1BypassButton.onClick = [safePtr]() {
        if (auto* comp = safePtr.getComponent()) {
            auto bypassed = comp->peak1BypassButton.getToggleState();
            comp->peak1FreqSlider.setEnabled(!bypassed);
            comp->peak1GainSlider.setEnabled(!bypassed);
            comp->peak1QualitySlider.setEnabled(!bypassed);
        }
    };

    peak2BypassButton.onClick = [safePtr]() {
        if (auto* comp = safePtr.getComponent()) {
            auto bypassed = comp->peak2BypassButton.getToggleState();
            comp->peak2FreqSlider.setEnabled(!bypassed);
            comp->peak2GainSlider.setEnabled(!bypassed);
            comp->peak2QualitySlider.setEnabled(!bypassed);
        }
    };

    peak3BypassButton.onClick = [safePtr]() {
        if (auto* comp = safePtr.getComponent()) {
            auto bypassed = comp->peak3BypassButton.getToggleState();
            comp->peak3FreqSlider.setEnabled(!bypassed);
            comp->peak3GainSlider.setEnabled(!bypassed);
            comp->peak3QualitySlider.setEnabled(!bypassed);
        }
    };
    lowCutBypassButton.onClick = [safePtr]() {
        if (auto* comp = safePtr.getComponent()) {
            auto bypassed = comp->lowCutBypassButton.getToggleState();
            comp->lowCutFreqSlider.setEnabled(!bypassed);
            comp->lowCutSlopeSlider.setEnabled(!bypassed);
        }
    };

    highCutBypassButton.onClick = [safePtr]() {
        if (auto* comp = safePtr.getComponent()) {
            auto bypassed = comp->highCutBypassButton.getToggleState();
            comp->highCutFreqSlider.setEnabled(!bypassed);
            comp->highCutSlopeSlider.setEnabled(!bypassed);
        }
    };

    analyserEnabledButton.onClick = [safePtr]() {
        if (auto* comp = safePtr.getComponent()) {
            auto enabled = comp->analyserEnabledButton.getToggleState();
            comp->responseCurveComponent.toggleAnalysisEnablement(enabled);
        }
    };

    setSize (600, 480);
}

SimpleEQFromTutorialAudioProcessorEditor::~SimpleEQFromTutorialAudioProcessorEditor() {
    peak1BypassButton.setLookAndFeel(nullptr);
    peak2BypassButton.setLookAndFeel(nullptr);
    peak3BypassButton.setLookAndFeel(nullptr);
    lowCutBypassButton.setLookAndFeel(nullptr);
    highCutBypassButton.setLookAndFeel(nullptr);
    analyserEnabledButton.setLookAndFeel(nullptr);
}

//==============================================================================
void SimpleEQFromTutorialAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (Colours::black);
}

void SimpleEQFromTutorialAudioProcessorEditor::resized() {
    auto bounds = getLocalBounds();

    auto analyserEnabledArea = bounds.removeFromTop(25);
    analyserEnabledArea.setWidth(100);
    analyserEnabledArea.setX(5);
    analyserEnabledArea.removeFromTop(2);
    analyserEnabledButton.setBounds(analyserEnabledArea);
    bounds.removeFromTop(5);

    auto responseArea = bounds.removeFromTop(bounds.getHeight() * 0.25f);

    responseCurveComponent.setBounds(responseArea);

    bounds.removeFromTop(5);

    auto lowCutArea = bounds.removeFromLeft(bounds.getWidth() * 0.33f);
    auto highCutArea = bounds.removeFromRight(bounds.getWidth() * 0.5f);

    lowCutBypassButton.setBounds(lowCutArea.removeFromTop(25));
    lowCutFreqSlider.setBounds(lowCutArea.removeFromTop(lowCutArea.getHeight() * 0.5f));
    lowCutSlopeSlider.setBounds(lowCutArea);

    highCutBypassButton.setBounds(highCutArea.removeFromTop(25));
    highCutFreqSlider.setBounds(highCutArea.removeFromTop(highCutArea.getHeight() * 0.5f));
    highCutSlopeSlider.setBounds(highCutArea);

    peak1BypassButton.setBounds(bounds.removeFromTop(25));
    peak1FreqSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.33f));
    peak1GainSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.5f));
    peak1QualitySlider.setBounds(bounds);
}

std::vector<juce::Component*> SimpleEQFromTutorialAudioProcessorEditor::getComps() {
    return {
        &peak1FreqSlider,
        &peak1GainSlider,
        &peak1QualitySlider,
        &peak2FreqSlider,
        &peak2GainSlider,
        &peak2QualitySlider,
        &peak3FreqSlider,
        &peak3GainSlider,
        &peak3QualitySlider,
        &lowCutFreqSlider,
        &highCutFreqSlider,
        &lowCutSlopeSlider,
        &highCutSlopeSlider,
        &responseCurveComponent,
        &lowCutBypassButton,
        &highCutBypassButton,
        &peak1BypassButton,
        &peak2BypassButton,
        &peak3BypassButton,
        &analyserEnabledButton
    };
}
