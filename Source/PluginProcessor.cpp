// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 sahko123 — Dialogue Leveler VST3
#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
DialogueLevelerAudioProcessor::createParameterLayout()
{
    using FloatParam  = juce::AudioParameterFloat;
    using BoolParam   = juce::AudioParameterBool;
    using PID         = juce::ParameterID;
    using Range       = juce::NormalisableRange<float>;
    using FloatAttrib = juce::AudioParameterFloatAttributes;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back(std::make_unique<FloatParam>(
        PID{"targetLufs", 1}, "Target Level",
        Range(-30.0f, 0.0f, 0.1f), -16.0f,
        FloatAttrib().withLabel("LUFS")));

    p.push_back(std::make_unique<FloatParam>(
        PID{"maxBoost", 1}, "Max Boost",
        Range(0.0f, 48.0f, 0.1f), 12.0f,
        FloatAttrib().withLabel("dB")));

    p.push_back(std::make_unique<FloatParam>(
        PID{"maxAttenuation", 1}, "Max Attenuation",
        Range(0.0f, 48.0f, 0.1f), 8.0f,
        FloatAttrib().withLabel("dB")));

    p.push_back(std::make_unique<FloatParam>(
        PID{"attack", 1}, "Attack",
        Range(1.0f, 1000.0f, 1.0f, 0.4f), 50.0f,
        FloatAttrib().withLabel("ms")));

    p.push_back(std::make_unique<FloatParam>(
        PID{"release", 1}, "Release",
        Range(10.0f, 3000.0f, 1.0f, 0.4f), 400.0f,
        FloatAttrib().withLabel("ms")));

    p.push_back(std::make_unique<FloatParam>(
        PID{"detectionWindow", 1}, "Detection Window",
        Range(50.0f, 1000.0f, 1.0f, 0.5f), 400.0f,
        FloatAttrib().withLabel("ms")));

    p.push_back(std::make_unique<FloatParam>(
        PID{"lookahead", 1}, "Lookahead",
        Range(0.0f, 500.0f, 1.0f), 0.0f,
        FloatAttrib().withLabel("ms")));

    p.push_back(std::make_unique<FloatParam>(
        PID{"gateThreshold", 1}, "Gate Threshold",
        Range(-80.0f, -20.0f, 0.5f), -55.0f,
        FloatAttrib().withLabel("dBFS")));

    p.push_back(std::make_unique<FloatParam>(
        PID{"gateHold", 1}, "Gate Hold",
        Range(0.0f, 2000.0f, 1.0f), 300.0f,
        FloatAttrib().withLabel("ms")));

    p.push_back(std::make_unique<FloatParam>(
        PID{"outputTrim", 1}, "Output Trim",
        Range(-24.0f, 24.0f, 0.1f), 0.0f,
        FloatAttrib().withLabel("dB")));

    // Added after initial release — appended so existing DAW automation indices are unaffected
    p.push_back(std::make_unique<FloatParam>(
        PID{"preGain", 1}, "Pre-Gain",
        Range(-24.0f, 24.0f, 0.1f), 0.0f,
        FloatAttrib().withLabel("dB")));

    // Added after initial release — appended so existing DAW automation indices are unaffected
    p.push_back(std::make_unique<FloatParam>(
        PID{"startingGain", 1}, "Starting Gain",
        Range(-24.0f, 24.0f, 0.1f), 0.0f,
        FloatAttrib().withLabel("dB")));

    // Added after initial release — appended so existing DAW automation indices are unaffected
    p.push_back(std::make_unique<FloatParam>(
        PID{"peakLimit", 1}, "Peak Limit",
        Range(-24.0f, 0.0f, 0.1f), -1.0f,
        FloatAttrib().withLabel("dBFS")));

    // withMeta(true) required by VST3 spec for the bypass parameter
    p.push_back(std::make_unique<BoolParam>(
        PID{"bypass", 1}, "Bypass", false,
        juce::AudioParameterBoolAttributes().withMeta(true)));

    return { p.begin(), p.end() };
}

//==============================================================================
DialogueLevelerAudioProcessor::DialogueLevelerAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", createParameterLayout())
{
    // Cache raw parameter pointers — safe here since apvts is already constructed.
    pTargetLevel     = apvts.getRawParameterValue("targetLufs");
    pMaxBoost        = apvts.getRawParameterValue("maxBoost");
    pMaxAttenuation  = apvts.getRawParameterValue("maxAttenuation");
    pAttack          = apvts.getRawParameterValue("attack");
    pRelease         = apvts.getRawParameterValue("release");
    pDetectionWindow = apvts.getRawParameterValue("detectionWindow");
    pLookahead       = apvts.getRawParameterValue("lookahead");
    pGateThreshold   = apvts.getRawParameterValue("gateThreshold");
    pGateHold        = apvts.getRawParameterValue("gateHold");
    pOutputTrim      = apvts.getRawParameterValue("outputTrim");
    pPreGain         = apvts.getRawParameterValue("preGain");
    pStartingGain    = apvts.getRawParameterValue("startingGain");
    pPeakLimit       = apvts.getRawParameterValue("peakLimit");

    // Catch parameter-ID typos at construction time, not during live audio
    jassert(pTargetLevel     != nullptr);
    jassert(pMaxBoost        != nullptr);
    jassert(pMaxAttenuation  != nullptr);
    jassert(pAttack          != nullptr);
    jassert(pRelease         != nullptr);
    jassert(pDetectionWindow != nullptr);
    jassert(pLookahead       != nullptr);
    jassert(pGateThreshold   != nullptr);
    jassert(pGateHold        != nullptr);
    jassert(pOutputTrim      != nullptr);
    jassert(pPreGain         != nullptr);
    jassert(pStartingGain    != nullptr);
    jassert(pPeakLimit       != nullptr);
}

DialogueLevelerAudioProcessor::~DialogueLevelerAudioProcessor() {}

//==============================================================================
const juce::String DialogueLevelerAudioProcessor::getName() const { return JucePlugin_Name; }
bool DialogueLevelerAudioProcessor::acceptsMidi()  const { return false; }
bool DialogueLevelerAudioProcessor::producesMidi() const { return false; }
bool DialogueLevelerAudioProcessor::isMidiEffect() const { return false; }
double DialogueLevelerAudioProcessor::getTailLengthSeconds() const
{
    // Tail = lookahead samples still buffered after the host's last input sample.
    // Use the atomic mirrors so this is safe to call from any thread.
    const double sr = tailSampleRate.load(std::memory_order_relaxed);
    return sr > 0.0 ? static_cast<double>(tailLookaheadSamples.load(std::memory_order_relaxed)) / sr : 0.0;
}

int  DialogueLevelerAudioProcessor::getNumPrograms()                            { return 1; }
int  DialogueLevelerAudioProcessor::getCurrentProgram()                         { return 0; }
void DialogueLevelerAudioProcessor::setCurrentProgram(int)                      {}
const juce::String DialogueLevelerAudioProcessor::getProgramName(int)           { return {}; }
void DialogueLevelerAudioProcessor::changeProgramName(int, const juce::String&) {}

//==============================================================================
void DialogueLevelerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // Allocate detector ring buffers for worst-case window (1000 ms)
    detector.prepare(sampleRate, 1000.0f);
    detectorR.prepare(sampleRate, 1000.0f);

    // Set initial window from parameter
    const float windowMs = pDetectionWindow->load(std::memory_order_relaxed);
    currentWindowSamples = juce::jmax(1, juce::roundToInt(windowMs * sampleRate / 1000.0));
    detector.setWindowSamples(currentWindowSamples);
    detectorR.setWindowSamples(currentWindowSamples);

    // Hold gain at starting gain until the detector window fills from actual audio
    const float startGainDb = pStartingGain->load(std::memory_order_relaxed);
    primingSamplesRemaining  = currentWindowSamples;
    smoothedGainDb           = startGainDb;
    gateHoldSamplesRemaining = 0;
    peakEnv_ = 0.0f;

    // Reset peak/avg stats on each prepare (new playback session)
    peakOutputDb .store(-144.0f, std::memory_order_relaxed);
    recentPeakDb .store(-144.0f, std::memory_order_relaxed);
    avgGainDb    .store(-999.0f, std::memory_order_relaxed);
    avgGainCount_ = 0;
    avgGainMean_  = 0.0;

    // 3-second rolling True Peak window — one slot per audio block
    const int tpSlots = juce::jmax(32,
        static_cast<int>(std::ceil(3.0 * sampleRate / juce::jmax(1, samplesPerBlock))) + 8);
    tpWindow.assign(static_cast<size_t>(tpSlots), -144.0f);
    tpWindowHead     = 0;
    tpWindowCapacity = tpSlots;
    tpWindowFilled   = 0;

    // Build the True Peak oversampler for this session's channel count and block size.
    // Factor 2 → 2² = 4× oversampling; FIR equiripple for accuracy.
    const int numOutCh = juce::jmax(1, getTotalNumOutputChannels());
    const int maxBlock = juce::jmax(1, samplesPerBlock);
    truePeakOversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        numOutCh, 2,
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
        false);
    truePeakOversampler->initProcessing(static_cast<size_t>(maxBlock));

    // Smoothed gain values — 20 ms ramp prevents zipper noise under automation
    outputTrimGain.reset(sampleRate, 0.02);
    outputTrimGain.setCurrentAndTargetValue(
        juce::Decibels::decibelsToGain(pOutputTrim->load(std::memory_order_relaxed)));

    preGainSmoothed.reset(sampleRate, 0.02);
    preGainSmoothed.setCurrentAndTargetValue(
        juce::Decibels::decibelsToGain(pPreGain->load(std::memory_order_relaxed)));

    // Lookahead delay line.
    // +1 so the read pointer never collides with the write pointer at max delay.
    maxDelaySamples = static_cast<int>(std::ceil(500.0 * sampleRate / 1000.0)) + 1;
    lookaheadBuffer.setSize(2, maxDelaySamples, false, true, false); // zero-init
    lookaheadWritePos = 0;

    // Lock lookahead to the param value at prepare time.
    // Runtime param changes take effect on the next prepare (playback stop/start)
    // because setLatencySamples must not be called from the audio thread, and
    // Premiere is unreliable about mid-session PDC updates anyway.
    const float lookaheadMs = pLookahead->load(std::memory_order_relaxed);
    currentLookaheadSamples = juce::jlimit(0, maxDelaySamples - 1,
        juce::roundToInt(lookaheadMs * sampleRate / 1000.0));
    setLatencySamples(currentLookaheadSamples);
    tailSampleRate.store(sampleRate, std::memory_order_relaxed);
    tailLookaheadSamples.store(currentLookaheadSamples, std::memory_order_relaxed);

    resetNeeded.store(false, std::memory_order_relaxed);
}

void DialogueLevelerAudioProcessor::releaseResources()
{
    detector.reset();
    detectorR.reset();
    if (truePeakOversampler)
        truePeakOversampler->reset();
}

//==============================================================================
bool DialogueLevelerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

//==============================================================================
// Helper shared by processBlock and processBlockBypassed: resets all DSP state
// and restarts the priming countdown so the detector refills cleanly.
static void resetDSPState(LoudnessDetector& det, LoudnessDetector& detR,
                          float& smoothedGain,
                          int& primingCounter, int windowSamples,
                          int& gateHoldRemaining,
                          juce::AudioBuffer<float>& delayBuf, int& delayWritePos,
                          float startingGainDb, float& peakEnv)
{
    det.reset();
    detR.reset();
    smoothedGain      = startingGainDb;
    primingCounter    = windowSamples;
    gateHoldRemaining = 0;
    delayBuf.clear();
    delayWritePos     = 0;
    peakEnv           = 0.0f;
}

//==============================================================================
void DialogueLevelerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    // Clear output channels with no corresponding input (stale audio guard)
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();
    // Guard against buffer having fewer channels than the bus reports
    const int numInputChannels = juce::jmin(getTotalNumInputChannels(), buffer.getNumChannels());
    const int numChannels      = buffer.getNumChannels();

    if (numSamples == 0 || numInputChannels == 0)
        return;

    // Apply a reset requested by setStateInformation (message-thread → audio-thread)
    const float startGainDb = pStartingGain->load(std::memory_order_relaxed);
    if (resetNeeded.exchange(false, std::memory_order_acq_rel))
        resetDSPState(detector, detectorR, smoothedGainDb, primingSamplesRemaining,
                      currentWindowSamples, gateHoldSamplesRemaining,
                      lookaheadBuffer, lookaheadWritePos, startGainDb, peakEnv_);

    // Handle peak/avg reset requests from GUI
    if (resetPeakNeeded.exchange(false, std::memory_order_acq_rel))
    {
        peakOutputDb .store(-144.0f, std::memory_order_relaxed);
        recentPeakDb .store(-144.0f, std::memory_order_relaxed);
        std::fill(tpWindow.begin(), tpWindow.end(), -144.0f);
        tpWindowHead   = 0;
        tpWindowFilled = 0;
        if (truePeakOversampler) truePeakOversampler->reset();
    }
    if (resetAvgNeeded.exchange(false, std::memory_order_acq_rel))
    {
        avgGainCount_ = 0;
        avgGainMean_  = 0.0;
        avgGainDb.store(-999.0f, std::memory_order_relaxed);
    }

    // ── Read parameters once per block (lock-free) ───────────────────────────
    const float targetDb     = pTargetLevel->load(std::memory_order_relaxed);
    const float maxBoostDb   = pMaxBoost->load(std::memory_order_relaxed);
    const float maxAttDb     = pMaxAttenuation->load(std::memory_order_relaxed);
    const float attackMs     = pAttack->load(std::memory_order_relaxed);
    const float releaseMs    = pRelease->load(std::memory_order_relaxed);
    const float windowMs     = pDetectionWindow->load(std::memory_order_relaxed);
    const float outputTrimDb = pOutputTrim->load(std::memory_order_relaxed);
    const float preGainDb    = pPreGain->load(std::memory_order_relaxed);
    const float gateThreshDb = pGateThreshold->load(std::memory_order_relaxed);
    const float gateHoldMs   = pGateHold->load(std::memory_order_relaxed);

    preGainSmoothed.setTargetValue(juce::Decibels::decibelsToGain(preGainDb));

    // Update detection window when the parameter changes (recomputes sum, no reset)
    const int newWindowSamples =
        juce::jmax(1, juce::roundToInt(windowMs * currentSampleRate / 1000.0));
    if (newWindowSamples != currentWindowSamples)
    {
        detector.setWindowSamples(newWindowSamples);
        detectorR.setWindowSamples(newWindowSamples);
        currentWindowSamples = newWindowSamples;
    }

    // One-pole smoother coefficients.
    // coeff = exp(-1/tau_samples), tau_samples = ms * sr / 1000.
    // Attack = gain decreasing (signal got loud). Release = gain increasing (signal got quiet).
    const auto attackCoeff  = static_cast<float>(
        std::exp(-1000.0 / (attackMs  * currentSampleRate)));
    const auto releaseCoeff = static_cast<float>(
        std::exp(-1000.0 / (releaseMs * currentSampleRate)));

    // Peak limiter: 1ms attack on the gain smoother when the limiter is driving,
    // 100ms envelope release so the limiter recovers quickly after each transient.
    const auto limiterAttCoeff  = static_cast<float>(std::exp(-1000.0 / (1.0   * currentSampleRate)));
    const auto peakEnvRelCoeff  = static_cast<float>(std::exp(-1000.0 / (100.0 * currentSampleRate)));
    const float peakLimitThresh = pPeakLimit->load(std::memory_order_relaxed);

    // Block-level peak pre-scan — find the worst peak anywhere in this block (accounting
    // for pre-gain) so the limiter can start reducing gain from sample 0 of the block
    // even when the loudest transient is near the end. This gives automatic intra-block
    // lookahead (~block_size/sampleRate ms) on top of whatever explicit lookahead is set.
    float blockPeakLinear = 0.0f;
    {
        const float pgApprox = preGainSmoothed.getTargetValue();
        for (int ch = 0; ch < numInputChannels; ++ch)
        {
            const float* rd = buffer.getReadPointer(ch);
            for (int s = 0; s < numSamples; ++s)
                blockPeakLinear = std::max(blockPeakLinear, std::abs(rd[s]) * pgApprox);
        }
    }
    const float blockPeakDb  = blockPeakLinear > 1e-7f
        ? juce::Decibels::gainToDecibels(blockPeakLinear) : -100.0f;
    const float blockPeakCap = peakLimitThresh - blockPeakDb;

    // Set Output Trim target — SmoothedValue ramps to it over 20 ms (no zipper)
    outputTrimGain.setTargetValue(juce::Decibels::decibelsToGain(outputTrimDb));

    const int gateHoldSamples = juce::roundToInt(gateHoldMs * currentSampleRate / 1000.0f);

    // Capture gate state at block entry so the FIFO frame represents the state the
    // block started in rather than the state it ended in (avoids one-block display lag).
    const float  entryMeasuredDb  = detector.getLoudnessDb();
    const int    entryHoldRemain  = gateHoldSamplesRemaining;

    // ── Per-sample DSP loop ──────────────────────────────────────────────────
    float lastMeasuredDb = -100.0f;
    for (int s = 0; s < numSamples; ++s)
    {
        // 1. Apply pre-gain in-place, then average to mono for detection
        const float pgLinear = preGainSmoothed.getNextValue();
        for (int ch = 0; ch < numInputChannels; ++ch)
            buffer.getWritePointer(ch)[s] *= pgLinear;

        // BS.1770-4: K-weight each channel independently, square each, then average.
        // For mono, detectorR is unused and we fall back to the single-detector path.
        float measuredDb;
        if (numInputChannels >= 2)
        {
            detector.processSample(buffer.getReadPointer(0)[s]);
            detectorR.processSample(buffer.getReadPointer(1)[s]);
            const double ms = (detector.getMeanSquare() + detectorR.getMeanSquare()) * 0.5;
            measuredDb = ms < 1e-10 ? -100.0f
                                    : static_cast<float>(-0.691 + 10.0 * std::log10(ms));
        }
        else
        {
            detector.processSample(buffer.getReadPointer(0)[s]);
            measuredDb = detector.getLoudnessDb();
        }
        lastMeasuredDb = measuredDb;

        // 2. Gate: if signal is below threshold past the hold period, freeze gain
        //    so room tone / breath gaps don't pump the level up.
        bool gateFrozen = false;
        if (measuredDb < gateThreshDb)
        {
            if (gateHoldSamplesRemaining > 0)
                --gateHoldSamplesRemaining;
            else
                gateFrozen = true;
        }
        else
        {
            gateHoldSamplesRemaining = gateHoldSamples;
        }

        // 3a. Peak envelope follower — instantaneous attack, 100ms release.
        //     Tracks per-channel max (not mono average) so a loud hard-panned transient
        //     is seen at full amplitude rather than halved by the L+R mix.
        //     Instantaneous attack catches impulses that are only 1–2 samples wide.
        float chanPeak = 0.0f;
        for (int ch = 0; ch < numInputChannels; ++ch)
            chanPeak = std::max(chanPeak, std::abs(buffer.getReadPointer(ch)[s]));
        if (chanPeak > peakEnv_)
            peakEnv_ = chanPeak;
        else
            peakEnv_ += (1.0f - peakEnvRelCoeff) * (chanPeak - peakEnv_);

        // 3b. Desired gain from LUFS leveler. During priming and gate-freeze the
        //     smoother holds its current position; during active leveling it targets
        //     the computed gain. Initialising to smoothedGainDb preserves starting
        //     gain during priming rather than ramping back to unity.
        float desiredGainDb = smoothedGainDb;
        if (primingSamplesRemaining > 0)
        {
            --primingSamplesRemaining;
        }
        else if (!gateFrozen)
        {
            desiredGainDb = juce::jlimit(-maxAttDb, maxBoostDb, targetDb - measuredDb);
        }

        // 3c. Peak limiter override — caps desired gain so output stays below peakLimitThresh.
        //     Output Trim is factored out of the cap so the threshold is an absolute ceiling
        //     on the final output (not just on the leveler gain before trim is applied).
        //     Takes the tighter of the running per-sample cap and the block pre-scan cap so
        //     the gain starts dropping from sample 0 of any block containing a peak.
        //     Active even during gate-freeze so transients can't sneak through silence.
        const float trimLinear      = outputTrimGain.getNextValue();
        const float trimDb          = juce::Decibels::gainToDecibels(trimLinear);
        const float peakEnvDb       = peakEnv_ > 1e-7f
            ? juce::Decibels::gainToDecibels(peakEnv_) : -100.0f;
        const float peakCap         = peakLimitThresh - trimDb - peakEnvDb;
        const float effectivePeakCap = juce::jmin(peakCap, blockPeakCap - trimDb);
        const bool  limiterDriving  = (effectivePeakCap < desiredGainDb);
        if (limiterDriving) desiredGainDb = effectivePeakCap;

        // 4. One-pole smooth. When the peak limiter is actively clamping, use its
        //    own 1ms attack so the gain drops fast enough to catch the transient.
        const float coeff = (desiredGainDb < smoothedGainDb)
            ? (limiterDriving ? limiterAttCoeff : attackCoeff)
            : releaseCoeff;
        smoothedGainDb   += (1.0f - coeff) * (desiredGainDb - smoothedGainDb);

        // 5. Hard clamp handles max-boost/attenuation param changes mid-stream.
        //    When the peak limiter is driving, skip the lower bound so it can always
        //    attenuate enough to stay below the peak threshold.
        const float lowerBound = limiterDriving ? -144.0f : -maxAttDb;
        smoothedGainDb = juce::jlimit(lowerBound, maxBoostDb, smoothedGainDb);

        // 6. Apply gain to audio — through lookahead delay if enabled.
        //    Detection ran on the un-delayed sample above; gain is applied to the
        //    sample that was delayed by currentLookaheadSamples, so correction
        //    arrives slightly before the event that triggered it.
        const float linearGain =
            juce::Decibels::decibelsToGain(smoothedGainDb) * trimLinear;

        if (currentLookaheadSamples > 0)
        {
            const int readPos = (lookaheadWritePos + maxDelaySamples - currentLookaheadSamples)
                                % maxDelaySamples;
            for (int ch = 0; ch < numInputChannels; ++ch)
            {
                const float delayed = lookaheadBuffer.getSample(ch, readPos);
                lookaheadBuffer.setSample(ch, lookaheadWritePos, buffer.getSample(ch, s));
                buffer.setSample(ch, s, delayed * linearGain);
            }
            lookaheadWritePos = (lookaheadWritePos + 1) % maxDelaySamples;
        }
        else
        {
            for (int ch = 0; ch < numInputChannels; ++ch)
                buffer.getWritePointer(ch)[s] *= linearGain;
        }

    }

    // Expose last-sample values for GUI meters
    currentAppliedGainDb.store(smoothedGainDb,  std::memory_order_relaxed);
    currentMeasuredLufs .store(lastMeasuredDb,  std::memory_order_relaxed);
    clippingBoost.store(smoothedGainDb >= maxBoostDb - 0.05f, std::memory_order_relaxed);
    clippingAtten.store(smoothedGainDb <= -maxAttDb  + 0.05f, std::memory_order_relaxed);

    // True Peak: 4× oversample the output block, update all-time hold and 3s rolling window
    if (truePeakOversampler)
    {
        juce::dsp::AudioBlock<float> outBlock(buffer);
        auto upBlock = truePeakOversampler->processSamplesUp(
            outBlock.getSubBlock(0, static_cast<size_t>(numSamples)));

        float tp = 0.0f;
        for (size_t ch = 0; ch < upBlock.getNumChannels(); ++ch)
        {
            const float* ptr = upBlock.getChannelPointer(ch);
            for (size_t s = 0; s < upBlock.getNumSamples(); ++s)
                tp = std::max(tp, std::abs(ptr[s]));
        }

        const float frameDb = tp > 0.0f ? juce::Decibels::gainToDecibels(tp) : -144.0f;

        // All-time hold
        if (frameDb > peakOutputDb.load(std::memory_order_relaxed))
            peakOutputDb.store(frameDb, std::memory_order_relaxed);

        // 3-second rolling window
        tpWindow[tpWindowHead] = frameDb;
        tpWindowHead = (tpWindowHead + 1) % tpWindowCapacity;
        if (tpWindowFilled < tpWindowCapacity) ++tpWindowFilled;
        float rollingMax = -144.0f;
        for (int i = 0; i < tpWindowFilled; ++i)
            rollingMax = std::max(rollingMax, tpWindow[i]);
        recentPeakDb.store(rollingMax, std::memory_order_relaxed);
    }

    // Update Welford online mean — only after priming so starting-gain hold doesn't bias it
    if (primingSamplesRemaining == 0)
    {
        ++avgGainCount_;
        avgGainMean_ += (static_cast<double>(smoothedGainDb) - avgGainMean_)
                        / static_cast<double>(avgGainCount_);
        avgGainDb.store(static_cast<float>(avgGainMean_), std::memory_order_relaxed);
    }

    // Push one frame to the scrolling graph FIFO (non-blocking drop if full).
    // Handle both ring segments: when the FIFO wraps, prepareToWrite returns n1=0,n2=1.
    // Only writing to s1 when n1>0 would commit an uninitialized slot every 1024 blocks.
    {
        int s1, n1, s2, n2;
        gainFifo.prepareToWrite(1, s1, n1, s2, n2);
        GateState gs;
        if (entryMeasuredDb >= gateThreshDb) gs = GateState::Active;
        else if (entryHoldRemain > 0)        gs = GateState::InHold;
        else                                 gs = GateState::Frozen;
        const GainFrame frame { smoothedGainDb, lastMeasuredDb, gs };
        if      (n1 > 0) gainFifoBuffer[s1] = frame;
        else if (n2 > 0) gainFifoBuffer[s2] = frame;
        gainFifo.finishedWrite((n1 > 0) ? n1 : (n2 > 0 ? n2 : 0));
    }
}

// Called by the host/JUCE wrapper when getBypassParameter() is enabled.
// We keep the detector warm so un-bypass doesn't start from a cold state.
void DialogueLevelerAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer,
                                                          juce::MidiBuffer& midi)
{
    // Base handles pass-through (in-place buffer) and clears extra output channels
    AudioProcessor::processBlockBypassed(buffer, midi);

    const int numSamples = buffer.getNumSamples();
    const int numInputChannels = juce::jmin(getTotalNumInputChannels(), buffer.getNumChannels());

    if (numSamples == 0 || numInputChannels == 0)
        return;

    // Honor state-reload resets during bypass so un-bypass starts clean
    const float startGainDbB = pStartingGain->load(std::memory_order_relaxed);
    if (resetNeeded.exchange(false, std::memory_order_acq_rel))
        resetDSPState(detector, detectorR, smoothedGainDb, primingSamplesRemaining,
                      currentWindowSamples, gateHoldSamplesRemaining,
                      lookaheadBuffer, lookaheadWritePos, startGainDbB, peakEnv_);
    if (resetPeakNeeded.exchange(false, std::memory_order_acq_rel))
    {
        peakOutputDb.store(-144.0f, std::memory_order_relaxed);
        recentPeakDb.store(-144.0f, std::memory_order_relaxed);
        std::fill(tpWindow.begin(), tpWindow.end(), -144.0f);
        tpWindowHead   = 0;
        tpWindowFilled = 0;
        if (truePeakOversampler) truePeakOversampler->reset();
    }
    if (resetAvgNeeded.exchange(false, std::memory_order_acq_rel))
    {
        avgGainCount_ = 0;
        avgGainMean_  = 0.0;
        avgGainDb.store(-999.0f, std::memory_order_relaxed);
    }

    // Advance the pre-gain smoother so its internal state stays consistent with
    // the active path. Without this, a click occurs when un-bypassing during a ramp.
    preGainSmoothed.setTargetValue(
        juce::Decibels::decibelsToGain(pPreGain->load(std::memory_order_relaxed)));

    for (int s = 0; s < numSamples; ++s)
    {
        // Apply the same pre-gain as the active path so the LUFS meter is consistent
        // across bypass toggles. Without this, the meter jumps if Pre-Gain != 0 dB.
        const float pgLinear = preGainSmoothed.getNextValue();
        if (numInputChannels >= 2)
        {
            detector.processSample(buffer.getReadPointer(0)[s] * pgLinear);
            detectorR.processSample(buffer.getReadPointer(1)[s] * pgLinear);
        }
        else
        {
            detector.processSample(buffer.getReadPointer(0)[s] * pgLinear);
        }

        if (primingSamplesRemaining > 0)
            --primingSamplesRemaining;
    }

    // Keep the lookahead delay line primed so un-bypass is seamless.
    // Without this, stale/zero samples would play out on the first post-bypass block.
    if (currentLookaheadSamples > 0)
    {
        for (int s = 0; s < numSamples; ++s)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                lookaheadBuffer.setSample(ch, lookaheadWritePos, buffer.getSample(ch, s));
            lookaheadWritePos = (lookaheadWritePos + 1) % maxDelaySamples;
        }
    }

    float bypassMeasuredDb;
    if (numInputChannels >= 2)
    {
        const double ms = (detector.getMeanSquare() + detectorR.getMeanSquare()) * 0.5;
        bypassMeasuredDb = ms < 1e-10 ? -100.0f
                                      : static_cast<float>(-0.691 + 10.0 * std::log10(ms));
    }
    else
    {
        bypassMeasuredDb = detector.getLoudnessDb();
    }
    currentAppliedGainDb.store(0.0f,            std::memory_order_relaxed);
    currentMeasuredLufs .store(bypassMeasuredDb, std::memory_order_relaxed);

    {
        int s1, n1, s2, n2;
        gainFifo.prepareToWrite(1, s1, n1, s2, n2);
        const GainFrame bypassFrame { 0.0f, bypassMeasuredDb, GateState::Active };
        if      (n1 > 0) gainFifoBuffer[s1] = bypassFrame;
        else if (n2 > 0) gainFifoBuffer[s2] = bypassFrame;
        gainFifo.finishedWrite((n1 > 0) ? n1 : (n2 > 0 ? n2 : 0));
    }
}

//==============================================================================
bool DialogueLevelerAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* DialogueLevelerAudioProcessor::createEditor()
{
    return new DialogueLevelerAudioProcessorEditor(*this);
}

juce::AudioProcessorParameter* DialogueLevelerAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter("bypass");
}

//==============================================================================
void DialogueLevelerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void DialogueLevelerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            // Signal the audio thread to reset DSP state on the next block.
            // (setStateInformation runs on the message thread; direct mutation would race.)
            resetNeeded.store(true, std::memory_order_release);
        }
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DialogueLevelerAudioProcessor();
}
