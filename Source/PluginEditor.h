#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class AbyssVerbVNAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    AbyssVerbVNAudioProcessorEditor(AbyssVerbVNAudioProcessor&);
    ~AbyssVerbVNAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    AbyssVerbVNAudioProcessor& audioProcessor;

    // Slider with label and attachment
    struct KnobWithLabel
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    // Violin Input Conditioning (4 knobs)
    KnobWithLabel piezoCorrectKnob, bodyResonanceKnob, brightnessKnob, bowSensitivityKnob;

    // Abyss Reverb (6 knobs)
    KnobWithLabel reverbDecayKnob, reverbDampHighKnob, reverbDampLowKnob;
    KnobWithLabel reverbModDepthKnob, reverbModRateKnob, detuneKnob;

    // Vanishing Delay (5 knobs)
    KnobWithLabel delayTimeKnob, delayFeedbackKnob, vanishRateKnob;
    KnobWithLabel degradeKnob, driftKnob;

    // Mix (3 knobs)
    KnobWithLabel reverbMixKnob, delayMixKnob, masterMixKnob;

    void setupKnob(KnobWithLabel& knob, const juce::String& paramId,
                   const juce::String& labelText);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AbyssVerbVNAudioProcessorEditor)
};
