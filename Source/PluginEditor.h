// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2025 sahko123 — Dialogue Leveler VST3
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>

//==============================================================================
class DlLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DlLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider&) override;
};

//==============================================================================
class DialogueLevelerAudioProcessorEditor
    : public juce::AudioProcessorEditor,
      private juce::Timer
{
public:
    explicit DialogueLevelerAudioProcessorEditor(DialogueLevelerAudioProcessor&);
    ~DialogueLevelerAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintCombined(juce::Graphics& g, juce::Rectangle<int> area,
                       float targetLufs, float gateThreshDb, float rangeDb);

    DialogueLevelerAudioProcessor& proc;
    DlLookAndFeel laf;
    juce::TooltipWindow tooltipWin { this, 600 };

    // ── Knobs (left to right) ─────────────────────────────────────────────────
    juce::Slider sPreGain, sStartGain, sTarget, sWindow, sAttack, sRelease;
    juce::Slider sBoost, sAtten, sPeakLimit, sGateThr, sGateHold, sLookahead, sTrim;

    // ── APVTS attachments (declared after sliders — destroyed first) ──────────
    using Att = juce::AudioProcessorValueTreeState::SliderAttachment;
    Att aPreGain, aStartGain, aTarget, aWindow, aAttack, aRelease;
    Att aBoost, aAtten, aPeakLimit, aGateThr, aGateHold, aLookahead, aTrim;

    // ── Meter labels ─────────────────────────────────────────────────────────
    juce::Label mInLabel, mIn, mGainLabel, mGain, mOutLabel, mOut;

    // ── Stats row (peak output hold | 3s rolling peak | average gain) ───────────
    juce::Label      peakOutLabel,    peakOutVal;
    juce::Label      recentPeakLabel, recentPeakVal;
    juce::Label      avgGainLabel,    avgGainVal;
    juce::TextButton btnResetPeak { "Reset" };
    juce::TextButton btnResetAvg  { "Reset" };

    // ── Scrolling graphs (gain + LUFS level, same ring buffer / head) ────────
    static constexpr int kGraphPoints = 500;
    std::array<float,     kGraphPoints> graphGain {};
    std::array<float,     kGraphPoints> graphLufs {};  // raw measured LUFS per frame
    std::array<GateState, kGraphPoints> graphGate {};  // per-frame gate state
    int  graphHead = 0;
    bool graphFull = false;

    // ── Reused Path objects for paintCombined (avoids per-frame heap allocation) ─
    juce::Path pathActive, pathHold, pathFrozen, gainPath;

    // ── Clip LED state (held for 500 ms after last clip) ─────────────────────
    double lastBoostClipMs = -9999.0;
    double lastAttenClipMs = -9999.0;

    // ── Preset bar ────────────────────────────────────────────────────────────
    juce::ComboBox presetBox;
    juce::TextButton btnSave  { "Save" };
    juce::TextButton btnDelete{ "Delete" };

    juce::File getPresetsDir() const;
    void refreshPresetList();
    void savePreset();
    void deletePreset();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DialogueLevelerAudioProcessorEditor)
};
