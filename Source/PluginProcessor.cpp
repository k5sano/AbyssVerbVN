#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AbyssVerbVNAudioProcessor::AbyssVerbVNAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

AbyssVerbVNAudioProcessor::~AbyssVerbVNAudioProcessor() {}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
AbyssVerbVNAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // === Violin Input Conditioning (4 params) ===
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"piezoCorrect", 1}, "Piezo Correct",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"bodyResonance", 1}, "Body Resonance",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"brightness", 1}, "Brightness",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"bowSensitivity", 1}, "Bow Sensitivity",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    // === Abyss Reverb (6 params) ===
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"reverbDecay", 1}, "Abyss Depth",
        juce::NormalisableRange<float>(0.5f, 30.0f, 0.1f, 0.4f), 6.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"reverbDampHigh", 1}, "High Damp",
        juce::NormalisableRange<float>(0.0f, 0.95f, 0.01f), 0.7f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"reverbDampLow", 1}, "Low Damp",
        juce::NormalisableRange<float>(0.0f, 0.8f, 0.01f), 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"reverbModDepth", 1}, "Mod Depth",
        juce::NormalisableRange<float>(0.0f, 3.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"reverbModRate", 1}, "Mod Rate",
        juce::NormalisableRange<float>(0.05f, 2.0f, 0.01f), 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"detuneAmount", 1}, "Detune",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    // === Vanishing Delay (5 params) ===
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"delayTime", 1}, "Delay Time",
        juce::NormalisableRange<float>(50.0f, 1500.0f, 1.0f, 0.5f), 400.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"delayFeedback", 1}, "Delay Feedback",
        juce::NormalisableRange<float>(0.0f, 0.95f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"vanishRate", 1}, "Vanish Rate",
        juce::NormalisableRange<float>(0.0f, 0.8f, 0.01f), 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"degradeAmount", 1}, "Degrade",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"driftAmount", 1}, "Drift",
        juce::NormalisableRange<float>(0.0f, 10.0f, 0.1f), 2.0f));

    // === Mix (3 params) ===
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"reverbMix", 1}, "Reverb Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.4f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"delayMix", 1}, "Delay Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"masterMix", 1}, "Master Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    return { params.begin(), params.end() };
}

//==============================================================================
void AbyssVerbVNAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Prepare all processing modules
    inputConditionerL.prepare(sampleRate);
    inputConditionerR.prepare(sampleRate);
    envelopeFollowerL.prepare(sampleRate);
    envelopeFollowerR.prepare(sampleRate);
    reverbL.prepare(sampleRate, samplesPerBlock);
    reverbR.prepare(sampleRate, samplesPerBlock);
    delayL.prepare(sampleRate, samplesPerBlock);
    delayR.prepare(sampleRate, samplesPerBlock);

    // Clear all delay lines
    reverbL.clear();
    reverbR.clear();
    delayL.clear();
    delayR.clear();

    // Initialize parameter smoother
    smoothed.reset(static_cast<float>(sampleRate));

    // Initialize smoothed parameters with current values
    rawParamBuffer[0]  = apvts.getRawParameterValue("piezoCorrect")->load();
    rawParamBuffer[1]  = apvts.getRawParameterValue("bodyResonance")->load();
    rawParamBuffer[2]  = apvts.getRawParameterValue("brightness")->load();
    rawParamBuffer[3]  = apvts.getRawParameterValue("bowSensitivity")->load();
    rawParamBuffer[4]  = apvts.getRawParameterValue("reverbDecay")->load();
    rawParamBuffer[5]  = apvts.getRawParameterValue("reverbDampHigh")->load();
    rawParamBuffer[6]  = apvts.getRawParameterValue("reverbDampLow")->load();
    rawParamBuffer[7]  = apvts.getRawParameterValue("reverbModDepth")->load();
    rawParamBuffer[8]  = apvts.getRawParameterValue("reverbModRate")->load();
    rawParamBuffer[9]  = apvts.getRawParameterValue("detuneAmount")->load();
    rawParamBuffer[10] = apvts.getRawParameterValue("delayTime")->load();
    rawParamBuffer[11] = apvts.getRawParameterValue("delayFeedback")->load();
    rawParamBuffer[12] = apvts.getRawParameterValue("vanishRate")->load();
    rawParamBuffer[13] = apvts.getRawParameterValue("degradeAmount")->load();
    rawParamBuffer[14] = apvts.getRawParameterValue("driftAmount")->load();
    rawParamBuffer[15] = apvts.getRawParameterValue("reverbMix")->load();
    rawParamBuffer[16] = apvts.getRawParameterValue("delayMix")->load();
    rawParamBuffer[17] = apvts.getRawParameterValue("masterMix")->load();
    smoothed.smooth(rawParamBuffer);

    // Reset DC blockers
    dcBlockL_x1 = dcBlockL_y1 = 0.0f;
    dcBlockR_x1 = dcBlockR_y1 = 0.0f;
}

void AbyssVerbVNAudioProcessor::releaseResources() {}

bool AbyssVerbVNAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void AbyssVerbVNAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Fetch raw parameter values (once per block)
    rawParamBuffer[0]  = apvts.getRawParameterValue("piezoCorrect")->load();
    rawParamBuffer[1]  = apvts.getRawParameterValue("bodyResonance")->load();
    rawParamBuffer[2]  = apvts.getRawParameterValue("brightness")->load();
    rawParamBuffer[3]  = apvts.getRawParameterValue("bowSensitivity")->load();
    rawParamBuffer[4]  = apvts.getRawParameterValue("reverbDecay")->load();
    rawParamBuffer[5]  = apvts.getRawParameterValue("reverbDampHigh")->load();
    rawParamBuffer[6]  = apvts.getRawParameterValue("reverbDampLow")->load();
    rawParamBuffer[7]  = apvts.getRawParameterValue("reverbModDepth")->load();
    rawParamBuffer[8]  = apvts.getRawParameterValue("reverbModRate")->load();
    rawParamBuffer[9]  = apvts.getRawParameterValue("detuneAmount")->load();
    rawParamBuffer[10] = apvts.getRawParameterValue("delayTime")->load();
    rawParamBuffer[11] = apvts.getRawParameterValue("delayFeedback")->load();
    rawParamBuffer[12] = apvts.getRawParameterValue("vanishRate")->load();
    rawParamBuffer[13] = apvts.getRawParameterValue("degradeAmount")->load();
    rawParamBuffer[14] = apvts.getRawParameterValue("driftAmount")->load();
    rawParamBuffer[15] = apvts.getRawParameterValue("reverbMix")->load();
    rawParamBuffer[16] = apvts.getRawParameterValue("delayMix")->load();
    rawParamBuffer[17] = apvts.getRawParameterValue("masterMix")->load();

    auto* channelL = buffer.getWritePointer(0);
    auto* channelR = buffer.getWritePointer(totalNumInputChannels > 1 ? 1 : 0);

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        // Smooth parameters (per-sample)
        smoothed.smooth(rawParamBuffer);

        // Apply smoothed parameters
        inputConditionerL.setParameters(smoothed.piezoCorrect, smoothed.bodyResonance, smoothed.brightness);
        inputConditionerR.setParameters(smoothed.piezoCorrect, smoothed.bodyResonance, smoothed.brightness);
        envelopeFollowerL.setSensitivity(smoothed.bowSensitivity);
        envelopeFollowerR.setSensitivity(smoothed.bowSensitivity);
        reverbL.setParameters(smoothed.reverbDecay, smoothed.reverbDampHigh, smoothed.reverbDampLow,
                             smoothed.reverbModDepth, smoothed.reverbModRate, smoothed.detuneAmount);
        reverbR.setParameters(smoothed.reverbDecay, smoothed.reverbDampHigh, smoothed.reverbDampLow,
                             smoothed.reverbModDepth, smoothed.reverbModRate, smoothed.detuneAmount);
        delayL.setParameters(smoothed.delayTime, smoothed.delayFeedback,
                            smoothed.vanishRate, smoothed.degradeAmount, smoothed.driftAmount);
        delayR.setParameters(smoothed.delayTime * 1.07f, smoothed.delayFeedback,
                            smoothed.vanishRate, smoothed.degradeAmount, smoothed.driftAmount * 1.15f);

        // Store original dry signal
        float dryL = channelL[sample];
        float dryR = channelR[sample];

        // Input conditioning (piezo correction)
        float conditionedL = inputConditionerL.process(dryL);
        float conditionedR = inputConditionerR.process(dryR);

        // Envelope following (for potential dynamic modulation)
        float envL = envelopeFollowerL.process(conditionedL);
        float envR = envelopeFollowerR.process(conditionedR);

        // Signal flow: Input -> Delay -> Reverb -> Mix
        float delOutL = delayL.process(conditionedL);
        float delOutR = delayR.process(conditionedR);

        float reverbInL = conditionedL + delOutL * smoothed.delayMix;
        float reverbInR = conditionedR + delOutR * smoothed.delayMix;

        float revOutL = reverbL.process(reverbInL);
        float revOutR = reverbR.process(reverbInR);

        // Combine wet signals
        float wetL = revOutL * smoothed.reverbMix + delOutL * smoothed.delayMix;
        float wetR = revOutR * smoothed.reverbMix + delOutR * smoothed.delayMix;

        // DC blocking (prevents offset accumulation)
        const float dcCoeff = 0.995f;
        float dcOutL = wetL - dcBlockL_x1 + dcCoeff * dcBlockL_y1;
        dcBlockL_x1 = wetL;
        dcBlockL_y1 = dcOutL;
        wetL = dcOutL;

        float dcOutR = wetR - dcBlockR_x1 + dcCoeff * dcBlockR_y1;
        dcBlockR_x1 = wetR;
        dcBlockR_y1 = dcOutR;
        wetR = dcOutR;

        // Dry/wet mix
        channelL[sample] = dryL * (1.0f - smoothed.masterMix) + wetL * smoothed.masterMix;
        channelR[sample] = dryR * (1.0f - smoothed.masterMix) + wetR * smoothed.masterMix;
    }
}

//==============================================================================
juce::AudioProcessorEditor* AbyssVerbVNAudioProcessor::createEditor()
{
    return new AbyssVerbVNAudioProcessorEditor(*this);
}

bool AbyssVerbVNAudioProcessor::hasEditor() const { return true; }

const juce::String AbyssVerbVNAudioProcessor::getName() const { return JucePlugin_Name; }
bool AbyssVerbVNAudioProcessor::acceptsMidi() const { return false; }
bool AbyssVerbVNAudioProcessor::producesMidi() const { return false; }
bool AbyssVerbVNAudioProcessor::isMidiEffect() const { return false; }
double AbyssVerbVNAudioProcessor::getTailLengthSeconds() const { return 10.0; }

int AbyssVerbVNAudioProcessor::getNumPrograms() { return 1; }
int AbyssVerbVNAudioProcessor::getCurrentProgram() { return 0; }
void AbyssVerbVNAudioProcessor::setCurrentProgram(int) {}
const juce::String AbyssVerbVNAudioProcessor::getProgramName(int) { return {}; }
void AbyssVerbVNAudioProcessor::changeProgramName(int, const juce::String&) {}

void AbyssVerbVNAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AbyssVerbVNAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AbyssVerbVNAudioProcessor();
}
