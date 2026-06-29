// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 sahko123 — Dialogue Leveler VST3
#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cmath>

// Sliding-window K-weighted loudness detector (ITU-R BS.1770-4).
//
// Two biquad stages are applied to the mono-averaged input before squaring:
//   Stage 1 — high-frequency shelving pre-filter  (+4 dB shelf at ~1.7 kHz)
//   Stage 2 — high-pass RLB filter                (2nd-order HP at ~38 Hz)
// Coefficients are computed via the bilinear transform in prepare() and are
// valid at any sample rate.
//
// Usage: call prepare() once from prepareToPlay(), setWindowSamples() when the
// detection-window parameter changes, then processSample() per sample on the
// mono-averaged input and getLoudnessDb() to read the current level in LUFS.

class LoudnessDetector
{
public:
    void prepare(double sampleRate, float maxWindowMs)
    {
        const int maxSamples = juce::jmax(1,
            static_cast<int>(std::ceil(sampleRate * maxWindowMs / 1000.0)));
        buffer.assign(static_cast<size_t>(maxSamples), 0.0f);
        bufferSize = maxSamples;
        driftRecomputeInterval = juce::jmax(1, static_cast<int>(sampleRate));
        computeKWeightingCoeffs(sampleRate);
        reset();
    }

    void setWindowSamples(int samples)
    {
        jassert(bufferSize > 0 && !buffer.empty()); // prepare() must be called first
        const int clamped = juce::jlimit(1, bufferSize, samples);
        if (clamped == windowSamples) return;
        if (clamped > windowSamples)
        {
            // Zero-fill newly included slots so stale data doesn't inflate the sum
            for (int i = windowSamples; i < clamped; ++i)
                buffer[static_cast<size_t>((writePos - 1 - i + bufferSize) % bufferSize)] = 0.0f;
        }
        windowSamples = clamped;
        recomputeRunningSum();
    }

    void reset()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos           = 0;
        runningSum         = 0.0;
        recomputeCountdown = driftRecomputeInterval;
        pre_x1 = pre_x2 = pre_y1 = pre_y2 = 0.0;
        rlb_x1 = rlb_x2 = rlb_y1 = rlb_y2 = 0.0;
    }

    void processSample(float monoSample) noexcept
    {
        // K-weighting filter cascade
        const double x = static_cast<double>(monoSample);

        const double pre_y = pre_b0*x + pre_b1*pre_x1 + pre_b2*pre_x2
                           - pre_a1*pre_y1 - pre_a2*pre_y2;
        pre_x2 = pre_x1; pre_x1 = x;
        pre_y2 = pre_y1; pre_y1 = pre_y;

        const double rlb_y = rlb_b0*pre_y + rlb_b1*rlb_x1 + rlb_b2*rlb_x2
                           - rlb_a1*rlb_y1 - rlb_a2*rlb_y2;
        rlb_x2 = rlb_x1; rlb_x1 = pre_y;
        rlb_y2 = rlb_y1; rlb_y1 = rlb_y;

        const float sq = static_cast<float>(rlb_y * rlb_y);

        const int evictPos = (writePos + bufferSize - windowSamples) % bufferSize;
        runningSum -= static_cast<double>(buffer[evictPos]);
        buffer[writePos] = sq;
        runningSum += static_cast<double>(sq);
        writePos = (writePos + 1) % bufferSize;

        if (runningSum < 0.0) runningSum = 0.0;

        if (--recomputeCountdown <= 0)
        {
            recomputeCountdown = driftRecomputeInterval;
            recomputeRunningSum();
        }
    }

    // Returns the raw K-weighted mean-square energy (linear, not in dB).
    double getMeanSquare() const noexcept
    {
        const double ms = runningSum / static_cast<double>(windowSamples);
        return ms < 0.0 ? 0.0 : ms;
    }

    // Returns K-weighted mean-square energy in LUFS. Returns -100 when silent.
    float getLoudnessDb() const noexcept
    {
        const double meanSquare = getMeanSquare();
        if (meanSquare < 1e-10) return -100.0f; // 1e-10 ≈ -100 dBFS
        return static_cast<float>(-0.691 + 10.0 * std::log10(meanSquare));
    }

private:
    // Bilinear-transform coefficients for ITU-R BS.1770-4 K-weighting.
    // All arithmetic in double to minimise coefficient quantisation error.
    void computeKWeightingCoeffs(double fs) noexcept
    {
        // Stage 1: high-frequency shelving pre-filter
        {
            constexpr double db = 3.99984385397;
            constexpr double f0 = 1681.974450955533;
            constexpr double Q  = 0.7071067811865476;
            const double Vh = std::pow(10.0, db / 20.0);
            const double Vb = std::pow(10.0, db / 40.0);
            const double K  = std::tan(juce::MathConstants<double>::pi * f0 / fs);
            const double d  = 1.0 + K / Q + K * K;
            pre_b0 = (Vh + Vb * K / Q + K * K) / d;
            pre_b1 = 2.0 * (K * K - Vh) / d;
            pre_b2 = (Vh - Vb * K / Q + K * K) / d;
            pre_a1 = 2.0 * (K * K - 1.0) / d;
            pre_a2 = (1.0 - K / Q + K * K) / d;
        }

        // Stage 2: high-pass RLB filter
        {
            constexpr double f0 = 38.13547087602444;
            constexpr double Q  = 0.5003270373238773;
            const double K  = std::tan(juce::MathConstants<double>::pi * f0 / fs);
            const double d  = 1.0 + K / Q + K * K;
            rlb_b0 =  1.0 / d;
            rlb_b1 = -2.0 / d;
            rlb_b2 =  1.0 / d;
            rlb_a1 = 2.0 * (K * K - 1.0) / d;
            rlb_a2 = (1.0 - K / Q + K * K) / d;
        }
    }

    void recomputeRunningSum() noexcept
    {
        runningSum = 0.0;
        for (int i = 0; i < windowSamples; ++i)
        {
            const int pos = (writePos - 1 - i + bufferSize) % bufferSize;
            runningSum += static_cast<double>(buffer[pos]);
        }
        if (runningSum < 0.0) runningSum = 0.0;
    }

    std::vector<float> buffer;
    int    bufferSize             = 1;
    int    windowSamples          = 1;
    int    writePos               = 0;
    double runningSum             = 0.0;
    int    driftRecomputeInterval = 44100;
    int    recomputeCountdown     = 44100;

    // K-weighting coefficients (set in computeKWeightingCoeffs, safe defaults = pass-through)
    double pre_b0 = 1.0, pre_b1 = 0.0, pre_b2 = 0.0;
    double pre_a1 = 0.0, pre_a2 = 0.0;
    double rlb_b0 = 1.0, rlb_b1 = 0.0, rlb_b2 = 0.0;
    double rlb_a1 = 0.0, rlb_a2 = 0.0;

    // K-weighting filter state
    double pre_x1 = 0.0, pre_x2 = 0.0, pre_y1 = 0.0, pre_y2 = 0.0;
    double rlb_x1 = 0.0, rlb_x2 = 0.0, rlb_y1 = 0.0, rlb_y2 = 0.0;
};
