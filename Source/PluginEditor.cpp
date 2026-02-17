#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AbyssVerbV1AudioProcessorEditor::AbyssVerbV1AudioProcessorEditor(AbyssVerbV1AudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(900, 620);

    // === Violin Input Conditioning Section ===
    setupKnob(piezoCorrectKnob,    "piezoCorrect",    "PIEZO CORRECT");
    setupKnob(bodyResonanceKnob,    "bodyResonance",    "BODY RESONANCE");
    setupKnob(brightnessKnob,       "brightness",       "BRIGHTNESS");
    setupKnob(bowSensitivityKnob,  "bowSensitivity",  "BOW SENSITIVITY");

    // === Abyss Reverb Section ===
    setupKnob(reverbDecayKnob,     "reverbDecay",     "ABYSS DEPTH");
    setupKnob(reverbDampHighKnob,  "reverbDampHigh",  "HIGH DAMP");
    setupKnob(reverbDampLowKnob,   "reverbDampLow",   "LOW DAMP");
    setupKnob(reverbModDepthKnob,  "reverbModDepth",  "MOD DEPTH");
    setupKnob(reverbModRateKnob,   "reverbModRate",   "MOD RATE");
    setupKnob(detuneKnob,          "detuneAmount",    "DETUNE");

    // === Vanishing Delay Section ===
    setupKnob(delayTimeKnob,       "delayTime",       "DELAY TIME");
    setupKnob(delayFeedbackKnob,   "delayFeedback",   "FEEDBACK");
    setupKnob(vanishRateKnob,      "vanishRate",      "VANISH");
    setupKnob(degradeKnob,         "degradeAmount",   "DEGRADE");
    setupKnob(driftKnob,           "driftAmount",     "DRIFT");

    // === Mix Section ===
    setupKnob(reverbMixKnob,       "reverbMix",       "REVERB MIX");
    setupKnob(delayMixKnob,        "delayMix",        "DELAY MIX");
    setupKnob(masterMixKnob,       "masterMix",       "MASTER MIX");
}

AbyssVerbV1AudioProcessorEditor::~AbyssVerbV1AudioProcessorEditor() {}

void AbyssVerbV1AudioProcessorEditor::setupKnob(KnobWithLabel& knob,
                                                const juce::String& paramId,
                                                const juce::String& labelText)
{
    knob.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 65, 16);
    knob.slider.setColour(juce::Slider::rotarySliderFillColourId,
                          juce::Colour(0xFF4A9EBF));
    knob.slider.setColour(juce::Slider::rotarySliderOutlineColourId,
                          juce::Colour(0xFF1A2A3A));
    knob.slider.setColour(juce::Slider::textBoxTextColourId,
                          juce::Colour(0xFFAADDEE));
    knob.slider.setColour(juce::Slider::textBoxOutlineColourId,
                          juce::Colours::transparentBlack);
    addAndMakeVisible(knob.slider);

    knob.label.setText(labelText, juce::dontSendNotification);
    knob.label.setJustificationType(juce::Justification::centred);
    knob.label.setFont(juce::Font(10.0f));
    knob.label.setColour(juce::Label::textColourId, juce::Colour(0xFF6699AA));
    addAndMakeVisible(knob.label);

    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, paramId, knob.slider);
}

void AbyssVerbV1AudioProcessorEditor::paint(juce::Graphics& g)
{
    // Dark gradient background (standard, no image loading)
    juce::ColourGradient gradient(
        juce::Colour(0xFF0A0E14), 0.0f, 0.0f,
        juce::Colour(0xFF050810), 0.0f, static_cast<float>(getHeight()),
        false);
    gradient.addColour(0.5, juce::Colour(0xFF0D1520));
    g.setGradientFill(gradient);
    g.fillAll();

    // Subtle light particles (abyss/deep space theme)
    juce::Random paintRandom;
    paintRandom.setSeed(12345);
    for (int i = 0; i < 50; ++i)
    {
        float x = paintRandom.nextFloat() * getWidth();
        float y = paintRandom.nextFloat() * getHeight();
        float alpha = paintRandom.nextFloat() * 0.12f + 0.02f;
        float size = paintRandom.nextFloat() * 2.0f + 0.5f;
        g.setColour(juce::Colour(0xFF4488AA).withAlpha(alpha));
        g.fillEllipse(x, y, size, size);
    }

    // Title
    g.setColour(juce::Colour(0xFF5EAABB));
    g.setFont(juce::Font(32.0f, juce::Font::bold));
    g.drawText("ABYSSVERB V1", getLocalBounds().removeFromTop(45),
               juce::Justification::centred);

    // Subtitle
    g.setFont(juce::Font(12.0f));
    g.setColour(juce::Colour(0xFF447788));
    g.drawText("VIOLIN-OPTIMIZED REVERB", getLocalBounds().removeFromTop(45)
               .withTrimmedTop(30), juce::Justification::centred);

    // Section divider lines
    g.setColour(juce::Colour(0xFF1A3344));
    g.drawLine(20.0f, 158.0f, getWidth() - 20.0f, 158.0f, 1.0f);
    g.drawLine(20.0f, 310.0f, getWidth() - 20.0f, 310.0f, 1.0f);
    g.drawLine(20.0f, 485.0f, getWidth() - 20.0f, 485.0f, 1.0f);

    // Section labels
    g.setColour(juce::Colour(0xFF3A6677));
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText("// VIOLIN INPUT", 25, 55, 150, 18, juce::Justification::centredLeft);
    g.drawText("// ABYSS REVERB", 25, 165, 150, 18, juce::Justification::centredLeft);
    g.drawText("// VANISHING DELAY", 25, 318, 150, 18, juce::Justification::centredLeft);
    g.drawText("// MIX", 25, 495, 150, 18, juce::Justification::centredLeft);
}

void AbyssVerbV1AudioProcessorEditor::resized()
{
    const int knobWidth = 110;
    const int knobHeight = 85;
    const int labelHeight = 16;
    const int marginX = 35;
    const int spacingX = 155;

    // Layout helper lambda
    auto placeKnob = [&](KnobWithLabel& knob, int x, int y)
    {
        knob.slider.setBounds(x, y, knobWidth, knobHeight);
        knob.label.setBounds(x, y + knobHeight, knobWidth, labelHeight);
    };

    // === Violin Input Conditioning (4 knobs) ===
    // Center the 4 knobs in the top section
    int inputStartX = (getWidth() - (4 * spacingX - 35)) / 2 + 10;
    int inputY = 70;
    placeKnob(piezoCorrectKnob,   inputStartX,                  inputY);
    placeKnob(bodyResonanceKnob,   inputStartX + spacingX,       inputY);
    placeKnob(brightnessKnob,      inputStartX + spacingX * 2,   inputY);
    placeKnob(bowSensitivityKnob, inputStartX + spacingX * 3,   inputY);

    // === Abyss Reverb (6 knobs) ===
    // 2 rows of 3 knobs
    int reverbStartX = (getWidth() - (3 * spacingX - 35)) / 2 + 10;
    int reverbY1 = 178;
    int reverbY2 = 285;
    placeKnob(reverbDecayKnob,    reverbStartX,                  reverbY1);
    placeKnob(reverbDampHighKnob, reverbStartX + spacingX,       reverbY1);
    placeKnob(reverbDampLowKnob,  reverbStartX + spacingX * 2,   reverbY1);
    placeKnob(reverbModDepthKnob, reverbStartX,                  reverbY2);
    placeKnob(reverbModRateKnob,  reverbStartX + spacingX,       reverbY2);
    placeKnob(detuneKnob,         reverbStartX + spacingX * 2,   reverbY2);

    // === Vanishing Delay (5 knobs) ===
    int delayStartX = (getWidth() - (5 * spacingX - 35)) / 2 + 10;
    int delayY = 328;
    placeKnob(delayTimeKnob,     delayStartX,                  delayY);
    placeKnob(delayFeedbackKnob, delayStartX + spacingX,       delayY);
    placeKnob(vanishRateKnob,    delayStartX + spacingX * 2,   delayY);
    placeKnob(degradeKnob,       delayStartX + spacingX * 3,   delayY);
    placeKnob(driftKnob,         delayStartX + spacingX * 4,   delayY);

    // === Mix (3 knobs) ===
    // Centered at bottom
    int mixStartX = (getWidth() - (3 * spacingX - 35)) / 2 + 10;
    int mixY = 505;
    placeKnob(reverbMixKnob, mixStartX,                  mixY);
    placeKnob(delayMixKnob,  mixStartX + spacingX,       mixY);
    placeKnob(masterMixKnob, mixStartX + spacingX * 2,   mixY);
}
