#pragma once

#include <JuceHeader.h>
#include <random>

//==============================================================================
// ViolinInputConditioner: Piezo pickup correction for violin
// Compensates for piezo characteristics: high-pass filtering,
// body resonance enhancement, and transient smoothng
//==============================================================================
class ViolinInputConditioner
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        reset();

        // Piezo correction: high-pass to remove sub-bass rumble
        hpCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * 80.0f / static_cast<float>(sr));

        // Body resonance: peaking EQ at violin body resonance (~300Hz)
        float bodyFreq = 300.0f;
        float bodyQ = 2.0f;
        float bodyGain = 6.0f; // dB boost for body resonance
        float omega = 2.0f * juce::MathConstants<float>::pi * bodyFreq / static_cast<float>(sr);
        float alpha = std::sin(omega) / (2.0f * bodyQ);
        float A = std::pow(10.0f, bodyGain / 40.0f);

        bodyB0 = 1.0f + alpha * A;
        bodyB1 = -2.0f * std::cos(omega);
        bodyB2 = 1.0f - alpha * A;
        bodyA0 = 1.0f + alpha / A;
        bodyA1 = -2.0f * std::cos(omega);
        bodyA2 = 1.0f - alpha / A;

        // Normalize
        bodyB0 /= bodyA0;
        bodyB1 /= bodyA0;
        bodyB2 /= bodyA0;
        bodyA1 /= bodyA0;
        bodyA2 /= bodyA0;
    }

    void setParameters(float piezoCorrect, float bodyResonance, float brightness)
    {
        this->piezoCorrect = piezoCorrect;
        this->bodyResonance = bodyResonance;
        this->brightness = brightness;
    }

    float process(float input)
    {
        // High-pass filter for piezo correction
        hpState = input * (1.0f - hpCoeff) + hpState * hpCoeff;
        float corrected = input - hpState * piezoCorrect;

        // Body resonance filter
        float bodyOut = bodyB0 * corrected + bodyB1 * bodyX1 + bodyB2 * bodyX2
                      - bodyA1 * bodyY1 - bodyA2 * bodyY2;
        bodyX2 = bodyX1;
        bodyX1 = corrected;
        bodyY2 = bodyY1;
        bodyY1 = bodyOut;
        bodyOut = juce::jlimit(-10.0f, 10.0f, bodyOut);

        // Mix in body resonance based on parameter
        float withBody = corrected * (1.0f - bodyResonance * 0.5f) + bodyOut * (bodyResonance * 0.5f);

        // Brightness control (simple shelving)
        float brightOut = withBody * (1.0f + brightness * 0.3f);

        return juce::jlimit(-1.0f, 1.0f, brightOut);
    }

    void reset()
    {
        hpState = 0.0f;
        bodyX1 = bodyX2 = bodyY1 = bodyY2 = 0.0f;
    }

private:
    double sr = 44100.0;
    float hpCoeff = 0.99f;
    float hpState = 0.0f;

    float bodyB0 = 1.0f, bodyB1 = 0.0f, bodyB2 = 0.0f;
    float bodyA0 = 1.0f, bodyA1 = 0.0f, bodyA2 = 0.0f;
    float bodyX1 = 0.0f, bodyX2 = 0.0f, bodyY1 = 0.0f, bodyY2 = 0.0f;

    float piezoCorrect = 1.0f;
    float bodyResonance = 0.5f;
    float brightness = 0.5f;
};

//==============================================================================
// EnvelopeFollower: Bow dynamics detection
// Fast attack, slow decay for tracking bow envelope
//==============================================================================
class EnvelopeFollower
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        // Attack: 1ms, Release: 100ms (for bow envelope following)
        attackCoeff = std::exp(-1.0f / (static_cast<float>(sr) * 0.001f));
        releaseCoeff = std::exp(-1.0f / (static_cast<float>(sr) * 0.1f));
        reset();
    }

    void setSensitivity(float sensitivity)
    {
        this->sensitivity = sensitivity;
    }

    float process(float input)
    {
        float absInput = std::abs(input);

        if (absInput > envelope)
        {
            envelope = absInput + (envelope - absInput) * attackCoeff;
        }
        else
        {
            envelope = absInput + (envelope - absInput) * releaseCoeff;
        }

        // Apply sensitivity scaling
        return envelope * (0.5f + sensitivity * 0.5f);
    }

    void reset()
    {
        envelope = 0.0f;
    }

    float getCurrent() const { return envelope; }

private:
    double sr = 44100.0;
    float attackCoeff = 0.99f;
    float releaseCoeff = 0.999f;
    float envelope = 0.0f;
    float sensitivity = 0.5f;
};

//==============================================================================
// AbyssFDNReverb: 8-line FDN with frequency-dependent damping & modulation
// Enhanced violin version with separate high/low damping and detune
//==============================================================================
class AbyssFDNReverb
{
public:
    static constexpr int NUM_LINES = 8;

    void prepare(double sampleRate, int samplesPerBlock)
    {
        sr = sampleRate;

        // Prime-based delay lengths for deep space (optimized for violin)
        const int baseLengths[NUM_LINES] = {
            1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116
        };

        for (int i = 0; i < NUM_LINES; ++i)
        {
            size_t len = static_cast<size_t>(baseLengths[i] * sr / 44100.0);
            delayLines[i].resize(len, 0.0f);
            writePos[i] = 0;
            dampState[i] = 0.0f;
        }

        // LFO phase initialization (spread for modulation)
        for (int i = 0; i < NUM_LINES; ++i)
            lfoPhase[i] = static_cast<float>(i) / NUM_LINES;
    }

    void setParameters(float decayTime, float dampHigh, float dampLow,
                       float modDepth, float modRate, float detuneAmount)
    {
        decay = decayTime;
        dampHighCoeff = dampHigh;
        dampLowCoeff = dampLow;
        this->modDepth = modDepth;
        this->modRate = modRate;
        this->detuneAmount = detuneAmount;
    }

    float process(float input)
    {
        float outputs[NUM_LINES];

        // Read from each delay line with modulation
        for (int i = 0; i < NUM_LINES; ++i)
        {
            size_t len = delayLines[i].size();

            // LFO for delay time modulation
            lfoPhase[i] += modRate * (1.0f + detuneAmount * static_cast<float>(i) * 0.1f)
                           / static_cast<float>(sr);
            if (lfoPhase[i] >= 1.0f) lfoPhase[i] -= 1.0f;
            float lfo = std::sin(2.0f * juce::MathConstants<float>::pi * lfoPhase[i]);
            float modSamples = lfo * modDepth * (static_cast<float>(sr) / 1000.0f);

            // Linear interpolation readout
            float readPosF = static_cast<float>(writePos[i]) - static_cast<float>(len) + modSamples;
            while (readPosF < 0.0f) readPosF += static_cast<float>(len);
            size_t readIdx0 = static_cast<size_t>(readPosF) % len;
            size_t readIdx1 = (readIdx0 + 1) % len;
            float frac = readPosF - std::floor(readPosF);

            outputs[i] = delayLines[i][readIdx0] * (1.0f - frac)
                       + delayLines[i][readIdx1] * frac;
        }

        // Hadamard-like feedback matrix (normalized)
        float feedback[NUM_LINES];
        const float scale = 1.0f / std::sqrt(static_cast<float>(NUM_LINES));
        for (int i = 0; i < NUM_LINES; ++i)
        {
            feedback[i] = 0.0f;
            for (int j = 0; j < NUM_LINES; ++j)
            {
                // Simple Hadamard pattern: (-1)^popcount(i&j)
                int bits = i & j;
                int popcount = 0;
                while (bits) { popcount += bits & 1; bits >>= 1; }
                float sign = (popcount % 2 == 0) ? 1.0f : -1.0f;
                feedback[i] += sign * outputs[j];
            }
            feedback[i] *= scale;
        }

        // Feedback coefficient with frequency-dependent damping
        float outputMix = 0.0f;
        for (int i = 0; i < NUM_LINES; ++i)
        {
            size_t len = delayLines[i].size();
            // RT60-based decay
            float g = std::pow(10.0f, -3.0f * static_cast<float>(len)
                      / (decay * static_cast<float>(sr)));

            // Frequency-dependent damping (biquad-style: separate high/low)
            float sig = feedback[i] * g + input / static_cast<float>(NUM_LINES);

            // High frequency damping
            float dampH = 1.0f - dampHighCoeff * 0.95f;
            dampState[i] = sig * dampH + dampState[i] * (1.0f - dampH);

            // Low frequency damping (less aggressive)
            float dampL = 1.0f - dampLowCoeff * 0.5f;
            float damp2 = dampState[i] * dampL + damp2State[i] * (1.0f - dampL);
            damp2State[i] = damp2;

            delayLines[i][writePos[i]] = damp2;
            writePos[i] = (writePos[i] + 1) % static_cast<int>(len);

            outputMix += outputs[i];
        }

        return outputMix * scale;
    }

    void clear()
    {
        for (int i = 0; i < NUM_LINES; ++i)
        {
            std::fill(delayLines[i].begin(), delayLines[i].end(), 0.0f);
            dampState[i] = 0.0f;
            damp2State[i] = 0.0f;
        }
    }

private:
    double sr = 44100.0;
    std::vector<float> delayLines[NUM_LINES];
    int writePos[NUM_LINES] = {};
    float dampState[NUM_LINES] = {};
    float damp2State[NUM_LINES] = {};
    float lfoPhase[NUM_LINES] = {};

    float decay = 6.0f;
    float dampHighCoeff = 0.7f;
    float dampLowCoeff = 0.3f;
    float modDepth = 0.5f;
    float modRate = 0.3f;
    float detuneAmount = 0.0f;
};

//==============================================================================
// VanishingDelay: Multi-tap delay with random vanish, degrade, and drift
// Creates ethereal, disappearing echo tails
//==============================================================================
class VanishingDelay
{
public:
    static constexpr int NUM_TAPS = 3;

    void prepare(double sampleRate, int samplesPerBlock)
    {
        sr = sampleRate;
        int maxDelaySamples = static_cast<int>(sr * 2.0); // Max 2 seconds
        buffer.resize(static_cast<size_t>(maxDelaySamples), 0.0f);
        writePos = 0;

        rng.seed(42);
        for (int i = 0; i < NUM_TAPS; ++i)
        {
            tapGainTarget[i] = 1.0f;
            tapGainCurrent[i] = 1.0f;
            tapTimer[i] = 0;
            tapDrift[i] = 0.0f;
            tapDriftPhase[i] = static_cast<float>(i) * 0.33f;
            degradeLPState[i] = 0.0f;
        }
    }

    void setParameters(float delayTimeMs, float feedback, float vanishRate,
                       float degradeAmount, float driftAmount)
    {
        this->delayTimeMs = delayTimeMs;
        this->feedback = feedback;
        this->vanishRate = vanishRate;
        this->degradeAmount = degradeAmount;
        this->driftAmount = driftAmount;
    }

    float process(float input)
    {
        size_t bufSize = buffer.size();

        // Golden ratio-based tap spacing (natural feel)
        const float tapRatios[NUM_TAPS] = { 1.0f, 0.618f, 0.382f };

        float output = 0.0f;

        for (int i = 0; i < NUM_TAPS; ++i)
        {
            // Random vanish: taps randomly drop to zero
            tapTimer[i]--;
            if (tapTimer[i] <= 0)
            {
                std::uniform_real_distribution<float> dist(0.0f, 1.0f);
                float roll = dist(rng);

                if (roll < vanishRate)
                    tapGainTarget[i] = 0.0f;  // Vanish!
                else
                    tapGainTarget[i] = dist(rng) * 0.7f + 0.3f; // Return with reduced level

                // Time until next change
                std::uniform_int_distribution<int> timeDist(
                    static_cast<int>(sr * 0.05),
                    static_cast<int>(sr * 0.4)
                );
                tapTimer[i] = timeDist(rng);
            }

            // Smooth gain transition
            tapGainCurrent[i] += (tapGainTarget[i] - tapGainCurrent[i]) * 0.001f;

            // Delay time drift (floating effect)
            tapDriftPhase[i] += driftAmount * 0.1f / static_cast<float>(sr);
            if (tapDriftPhase[i] >= 1.0f) tapDriftPhase[i] -= 1.0f;
            float drift = std::sin(2.0f * juce::MathConstants<float>::pi * tapDriftPhase[i])
                        * driftAmount * (static_cast<float>(sr) / 1000.0f);

            // Read position calculation
            float delaySamples = delayTimeMs * tapRatios[i] * (static_cast<float>(sr) / 1000.0f) + drift;
            delaySamples = juce::jlimit(1.0f, static_cast<float>(bufSize - 1), delaySamples);

            float readPosF = static_cast<float>(writePos) - delaySamples;
            if (readPosF < 0.0f) readPosF += static_cast<float>(bufSize);
            size_t readIdx0 = static_cast<size_t>(readPosF) % bufSize;
            size_t readIdx1 = (readIdx0 + 1) % bufSize;
            float frac = readPosF - std::floor(readPosF);

            float tapOut = buffer[readIdx0] * (1.0f - frac) + buffer[readIdx1] * frac;

            // Degradation effects: LPF + bit reduction for ethereal decay
            float lpCoeff = 1.0f - degradeAmount * 0.9f;
            degradeLPState[i] = tapOut * (1.0f - lpCoeff) + degradeLPState[i] * lpCoeff;
            tapOut = degradeLPState[i];

            // Bit depth reduction (adds ethereal grit)
            if (degradeAmount > 0.01f)
            {
                float bits = 16.0f - degradeAmount * 12.0f; // 16bit -> 4bit
                float levels = std::pow(2.0f, bits);
                tapOut = std::round(tapOut * levels) / levels;
            }

            output += tapOut * tapGainCurrent[i];
        }

        output /= static_cast<float>(NUM_TAPS);

        // Write to buffer with feedback
        buffer[writePos] = input + output * feedback;
        writePos = (writePos + 1) % static_cast<int>(bufSize);

        return output;
    }

    void clear()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        for (int i = 0; i < NUM_TAPS; ++i)
        {
            degradeLPState[i] = 0.0f;
            tapGainCurrent[i] = 1.0f;
            tapGainTarget[i] = 1.0f;
        }
    }

private:
    double sr = 44100.0;
    std::vector<float> buffer;
    int writePos = 0;

    float delayTimeMs = 400.0f;
    float feedback = 0.5f;
    float vanishRate = 0.3f;
    float degradeAmount = 0.3f;
    float driftAmount = 2.0f;

    float tapGainTarget[NUM_TAPS] = {};
    float tapGainCurrent[NUM_TAPS] = {};
    int tapTimer[NUM_TAPS] = {};
    float tapDrift[NUM_TAPS] = {};
    float tapDriftPhase[NUM_TAPS] = {};
    float degradeLPState[NUM_TAPS] = {};

    std::mt19937 rng;
};

//==============================================================================
// SmoothedParameters: 18-parameter smoothing for glitch-free modulation
//==============================================================================
struct SmoothedParameters
{
    // Violin input conditioning
    float piezoCorrect = 0.5f, bodyResonance = 0.5f, brightness = 0.5f, bowSensitivity = 0.5f;
    // Reverb
    float reverbDecay = 6.0f, reverbDampHigh = 0.7f, reverbDampLow = 0.3f;
    float reverbModDepth = 0.5f, reverbModRate = 0.3f, detuneAmount = 0.0f;
    // Delay
    float delayTime = 400.0f, delayFeedback = 0.5f;
    float vanishRate = 0.3f, degradeAmount = 0.3f, driftAmount = 2.0f;
    // Mix
    float reverbMix = 0.4f, delayMix = 0.3f, masterMix = 0.5f;

    void reset(float sampleRate)
    {
        // ~10ms ramp time for smooth transitions
        smoothingCoeff = std::exp(-1.0f / (sampleRate * 0.01f));
    }

    void smooth(const float* rawTargets)
    {
        // Order must match parameter indices
        float* targets[] = {
            &piezoCorrect, &bodyResonance, &brightness, &bowSensitivity,
            &reverbDecay, &reverbDampHigh, &reverbDampLow, &reverbModDepth, &reverbModRate, &detuneAmount,
            &delayTime, &delayFeedback, &vanishRate, &degradeAmount, &driftAmount,
            &reverbMix, &delayMix, &masterMix
        };

        for (size_t i = 0; i < 18; ++i)
        {
            *targets[i] += (rawTargets[i] - *targets[i]) * (1.0f - smoothingCoeff);
        }
    }

private:
    float smoothingCoeff = 0.999f;
};

//==============================================================================
// Main Processor
//==============================================================================
class AbyssVerbVNAudioProcessor : public juce::AudioProcessor
{
public:
    AbyssVerbVNAudioProcessor();
    ~AbyssVerbVNAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Processing modules (stereo)
    ViolinInputConditioner inputConditionerL, inputConditionerR;
    EnvelopeFollower envelopeFollowerL, envelopeFollowerR;
    AbyssFDNReverb reverbL, reverbR;
    VanishingDelay delayL, delayR;

    // Parameter smoothing
    SmoothedParameters smoothed;
    float rawParamBuffer[18];

    // DC blocking
    float dcBlockL_x1 = 0.0f, dcBlockL_y1 = 0.0f;
    float dcBlockR_x1 = 0.0f, dcBlockR_y1 = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AbyssVerbVNAudioProcessor)
};
