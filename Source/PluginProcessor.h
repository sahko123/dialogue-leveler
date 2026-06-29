// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 sahko123 — Dialogue Leveler VST3
#pragma once
#include <JuceHeader.h>
#include "DSP/LoudnessDetector.h"

enum class GateState : uint8_t { Active, InHold, Frozen };

class DialogueLevelerAudioProcessor : public juce::AudioProcessor
{
public:
    DialogueLevelerAudioProcessor();
    ~DialogueLevelerAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

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

    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Applied gain and measured LUFS — written on audio thread, read by GUI timer.
    std::atomic<float> currentAppliedGainDb { 0.0f };
    std::atomic<float> currentMeasuredLufs  { -100.0f };
    static_assert(std::atomic<float>::is_always_lock_free,
                  "GUI atomics must be lock-free for real-time safety");
    float getAppliedGainDb()  const noexcept { return currentAppliedGainDb.load(std::memory_order_relaxed); }
    float getMeasuredLufs()   const noexcept { return currentMeasuredLufs.load(std::memory_order_relaxed); }

    // Clip indicators — true when gain is railing against a limit (audio thread writes, GUI reads)
    std::atomic<bool> clippingBoost { false };
    std::atomic<bool> clippingAtten { false };

    // Peak output and average gain — audio thread writes, GUI reads.
    // peakOutputDb:  hold-maximum since last reset.
    // recentPeakDb:  3-second rolling window maximum (no reset button needed).
    // avgGainDb:     Welford online mean since last reset (excludes priming samples).
    //                Sentinel -999.f means "no data yet" (show "---" in GUI).
    std::atomic<float> peakOutputDb  { -144.0f };
    std::atomic<float> recentPeakDb  { -144.0f };
    std::atomic<float> avgGainDb     { -999.0f };
    std::atomic<bool>  resetPeakNeeded { false };
    std::atomic<bool>  resetAvgNeeded  { false };
    std::atomic<bool>  resetNeeded     { false };

    // Lock-free FIFO for the scrolling gain graph (one frame per audio block).
    // GUI timer drains it; audio thread fills it.
    struct GainFrame { float gainDb; float lufsIn; GateState gate; };
    static constexpr int kFifoCapacity = 1024;
    juce::AbstractFifo gainFifo { kFifoCapacity };
    GainFrame gainFifoBuffer[kFifoCapacity];


private:
    // Cached raw param pointers — set in ctor, read lock-free on audio thread.
    // Declared after apvts to guarantee correct construction order.
    std::atomic<float>* pTargetLevel      = nullptr;
    std::atomic<float>* pMaxBoost         = nullptr;
    std::atomic<float>* pMaxAttenuation   = nullptr;
    std::atomic<float>* pAttack           = nullptr;
    std::atomic<float>* pRelease          = nullptr;
    std::atomic<float>* pDetectionWindow  = nullptr;
    std::atomic<float>* pLookahead        = nullptr;
    std::atomic<float>* pGateThreshold    = nullptr;
    std::atomic<float>* pGateHold         = nullptr;
    std::atomic<float>* pOutputTrim       = nullptr;
    std::atomic<float>* pPreGain          = nullptr;
    std::atomic<float>* pStartingGain     = nullptr;
    std::atomic<float>* pPeakLimit        = nullptr;
    std::atomic<float>* pBypass           = nullptr;

    // Welford online mean state — audio thread only, no atomics needed
    uint64_t avgGainCount_ = 0;
    double   avgGainMean_  = 0.0;

    // 4× oversampler used solely for True Peak metering (not in the audio signal path)
    std::unique_ptr<juce::dsp::Oversampling<float>> truePeakOversampler;

    // 3-second rolling True Peak window — ring buffer of per-block dBTP values, audio-thread only
    std::vector<float> tpWindow;
    int tpWindowHead     = 0;
    int tpWindowCapacity = 0;
    int tpWindowFilled   = 0;

    // DSP state — audio thread only
    LoudnessDetector detector;   // left / mono channel K-weighting
    LoudnessDetector detectorR;  // right channel K-weighting (stereo BS.1770-4)
    juce::SmoothedValue<float> outputTrimGain; // smoothed to prevent Output Trim zipper noise
    juce::SmoothedValue<float> preGainSmoothed; // smoothed to prevent Pre-Gain zipper noise
    double currentSampleRate      = 44100.0;
    int    currentWindowSamples   = 0;
    float  smoothedGainDb         = 0.0f;
    int    primingSamplesRemaining = 0; // holds gain at 0 while detector window fills

    // Peak envelope follower state — audio thread only (1ms attack, 100ms release)
    float peakEnv_ = 0.0f;

    // Gate state — audio thread only
    int gateHoldSamplesRemaining = 0;

    // Lookahead delay line — audio thread only.
    // Fixed at prepareToPlay; runtime changes to pLookahead take effect on next prepare
    // because setLatencySamples must not be called from the audio thread.
    juce::AudioBuffer<float> lookaheadBuffer;
    int maxDelaySamples         = 0;
    int lookaheadWritePos       = 0;
    int currentLookaheadSamples = 0;

    // Atomic mirrors of currentSampleRate and currentLookaheadSamples for use by
    // getTailLengthSeconds(), which may be called from a different thread than prepareToPlay.
    std::atomic<double> tailSampleRate      { 44100.0 };
    std::atomic<int>    tailLookaheadSamples { 0 };

    // resetNeeded: set on message thread (preset load), checked+cleared on audio thread

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DialogueLevelerAudioProcessor)
};
