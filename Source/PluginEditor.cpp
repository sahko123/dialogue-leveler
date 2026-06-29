// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2025 sahko123 — Dialogue Leveler VST3
#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
static const juce::Colour kBg       { 0xfff0ead8 };  // warm paper cream
static const juce::Colour kSurface  { 0xffe8e2d0 };  // inset panel
static const juce::Colour kTrack    { 0xffc4bcb0 };  // knob track ring
static const juce::Colour kAccent   { 0xff4a6282 };  // steel blue — gain waveform + UI
static const juce::Colour kAccentDim{ 0xff344862 };  // darker steel blue — section headers
static const juce::Colour kTextHi   { 0xff262016 };  // near-black ink
static const juce::Colour kTextLo   { 0xff847870 };  // warm mid-gray
static const juce::Colour kGridLine { 0xffd0c8bc };  // light warm grid lines
static const juce::Colour kCut      { 0xffaa4428 };  // muted rust — gate / negative
static const juce::Colour kClipOn   { 0xffcc2828 };  // red clip LED
static const juce::Colour kClipOff  { 0xffddd5c8 };  // dim clip LED
static const juce::Colour kWaveform { 0xff2a6050 };  // dark teal — LUFS active waveform
static const juce::Colour kTarget   { 0xff7050a8 };  // muted violet — target LUFS line

//==============================================================================
DlLookAndFeel::DlLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId,       juce::Colour(0xff262016));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xffe8e2d0));
    setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(0x00000000));
    setColour(juce::Slider::textBoxHighlightColourId,  juce::Colour(0xffa0b4cc));
    setColour(juce::Label::textColourId,               juce::Colour(0xff847870));
    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xfff0ead8));
    setColour(juce::TooltipWindow::textColourId,       juce::Colour(0xff262016));
    setColour(juce::TooltipWindow::outlineColourId,    juce::Colour(0xffc4bcb0));
}

void DlLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                      float sliderPos, float startAngle, float endAngle,
                                      juce::Slider& /*slider*/)
{
    const float cx    = x + w * 0.5f;
    const float cy    = y + h * 0.5f;
    const float r     = juce::jmin(w, h) * 0.38f;
    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    juce::Path track;
    track.addCentredArc(cx, cy, r, r, 0.0f, startAngle, endAngle, true);
    g.setColour(kTrack);
    g.strokePath(track, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    juce::Path arc;
    arc.addCentredArc(cx, cy, r, r, 0.0f, startAngle, angle, true);
    g.setColour(kAccent);
    g.strokePath(arc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));

    g.setColour(juce::Colour(0xffe8e2d0));
    g.fillEllipse(cx - r * 0.68f, cy - r * 0.68f, r * 1.36f, r * 1.36f);

    const float px = cx + r * 0.52f * std::sin(angle);
    const float py = cy - r * 0.52f * std::cos(angle);
    g.setColour(kAccent);
    g.drawLine(cx, cy, px, py, 1.8f);
}

//==============================================================================
static void setupKnob(juce::Slider& s, DlLookAndFeel& laf,
                      const juce::String& tooltip, const juce::String& suffix)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 16);
    s.setTextValueSuffix(suffix);
    s.setLookAndFeel(&laf);
    s.setTooltip(tooltip);
}

//==============================================================================
DialogueLevelerAudioProcessorEditor::DialogueLevelerAudioProcessorEditor(
    DialogueLevelerAudioProcessor& p)
    : AudioProcessorEditor(&p), proc(p),
      aPreGain  (p.apvts, "preGain",         sPreGain),
      aStartGain(p.apvts, "startingGain",    sStartGain),
      aTarget   (p.apvts, "targetLufs",      sTarget),
      aWindow   (p.apvts, "detectionWindow", sWindow),
      aAttack   (p.apvts, "attack",          sAttack),
      aRelease  (p.apvts, "release",         sRelease),
      aBoost    (p.apvts, "maxBoost",        sBoost),
      aAtten    (p.apvts, "maxAttenuation",  sAtten),
      aPeakLimit(p.apvts, "peakLimit",       sPeakLimit),
      aGateThr  (p.apvts, "gateThreshold",   sGateThr),
      aGateHold (p.apvts, "gateHold",        sGateHold),
      aLookahead(p.apvts, "lookahead",       sLookahead),
      aTrim     (p.apvts, "outputTrim",      sTrim)
{
    setupKnob(sPreGain,   laf, "Fixed gain applied before the leveler sees the signal. "
                               "Use to bring a consistently quiet source into the leveler's "
                               "operating range without changing the target level.", " dB");
    setupKnob(sStartGain, laf, "Gain the leveler starts from at the beginning of playback. "
                               "Set this to your expected dialogue level so there is no "
                               "ramp-up from unity gain when the video begins.", " dB");
    setupKnob(sTarget,   laf, "Target loudness in LUFS. The leveler continuously rides gain "
                               "to match this level. -16 LUFS suits most streaming platforms; "
                               "-23 LUFS is the EBU R128 broadcast standard.", " LUFS");
    setupKnob(sWindow,   laf, "How many milliseconds of audio the level detector averages. "
                               "Shorter windows react faster to transients; longer windows "
                               "give a smoother, more consistent reading but respond more slowly.", " ms");
    setupKnob(sAttack,   laf, "How fast the gain drops when the signal gets louder than target. "
                               "Shorter attack = tighter control but may sound pumpy. "
                               "Longer attack = more natural, lets transients through.", " ms");
    setupKnob(sRelease,  laf, "How fast the gain rises when the signal drops below target. "
                               "Shorter release = snappy recovery. Longer release = slow, "
                               "smooth ride that sounds more like a natural vocal rider.", " ms");
    setupKnob(sBoost,    laf, "Maximum dB of gain the leveler will add. Caps how much a "
                               "very quiet passage gets boosted. Lower values keep the noise "
                               "floor from being pumped up on near-silent sections.", " dB");
    setupKnob(sAtten,     laf, "Maximum dB of gain the leveler will subtract. Caps how much "
                               "a loud passage gets cut. Lower values let peaks through if "
                               "you prefer a gentler touch on loud material.", " dB");
    setupKnob(sPeakLimit, laf, "Peak limiter threshold. When the instantaneous signal level "
                               "exceeds this, a fast (1ms) limiter overrides the LUFS leveler "
                               "to prevent transient spikes from getting through. Works best "
                               "with lookahead enabled so gain reduction arrives before the "
                               "transient. Set to 0 to disable effectively.", " dBFS");
    setupKnob(sGateThr,  laf, "Signals below this level are treated as silence. When the "
                               "gate is closed, gain is frozen — so room tone and breath "
                               "gaps are not pumped up. Set just below the quietest speech.", " dBFS");
    setupKnob(sGateHold, laf, "How long (ms) after the signal drops below gate threshold "
                               "before the gate closes and freezes gain. Prevents the gate "
                               "from chattering on breaths or short pauses mid-sentence.", " ms");
    setupKnob(sLookahead,laf, "Delays the audio path so the leveler can react before a "
                               "loud transient arrives. Produces cleaner gain changes but "
                               "adds latency. Set to 0 for live/low-latency use.", " ms");
    setupKnob(sTrim,     laf, "Final output trim applied after all leveling. Use to match "
                               "the leveled signal to your project's target loudness, or "
                               "to compensate for the leveler's average gain offset.", " dB");

    for (auto* s : { &sPreGain, &sStartGain, &sTarget, &sWindow, &sAttack, &sRelease,
                     &sBoost, &sAtten, &sPeakLimit, &sGateThr, &sGateHold, &sLookahead, &sTrim })
        addAndMakeVisible(s);

    auto setupCaption = [](juce::Label& l, const char* text)
    {
        l.setText(text, juce::dontSendNotification);
        l.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        l.setColour(juce::Label::textColourId, kTextLo);
        l.setJustificationType(juce::Justification::centred);
    };
    auto setupValue = [](juce::Label& l)
    {
        l.setFont(juce::Font(juce::FontOptions().withHeight(18.0f)));
        l.setColour(juce::Label::textColourId, kTextHi);
        l.setJustificationType(juce::Justification::centred);
        l.setText("---", juce::dontSendNotification);
    };
    setupCaption(mInLabel,   "INPUT");
    setupCaption(mGainLabel, "GAIN");
    setupCaption(mOutLabel,  "OUTPUT");
    setupValue(mIn);
    setupValue(mGain);
    setupValue(mOut);

    for (auto* l : { &mInLabel, &mIn, &mGainLabel, &mGain, &mOutLabel, &mOut })
        addAndMakeVisible(l);

    // ── Stats row setup ───────────────────────────────────────────────────────
    setupCaption(peakOutLabel,    "TRUE PEAK");
    setupCaption(recentPeakLabel, "3-SEC PEAK");
    setupCaption(avgGainLabel,    "AVG GAIN");
    setupValue(peakOutVal);
    setupValue(recentPeakVal);
    setupValue(avgGainVal);

    for (auto* l : { &peakOutLabel, &peakOutVal,
                     &recentPeakLabel, &recentPeakVal,
                     &avgGainLabel, &avgGainVal })
        addAndMakeVisible(l);

    btnResetPeak.setTooltip("Reset True Peak hold to -inf dBTP");
    btnResetPeak.onClick = [this] { proc.resetPeakNeeded.store(true, std::memory_order_release); };
    addAndMakeVisible(btnResetPeak);

    btnResetAvg.setTooltip("Reset average gain accumulator");
    btnResetAvg.onClick = [this] { proc.resetAvgNeeded.store(true, std::memory_order_release); };
    addAndMakeVisible(btnResetAvg);

    // ── Preset bar ────────────────────────────────────────────────────────────
    presetBox.setTextWhenNothingSelected("-- Preset --");
    presetBox.setTooltip("Select a saved preset to load it");
    presetBox.onChange = [this] {
        const juce::String name = presetBox.getSelectedItemText();
        if (name.isEmpty()) return;
        const juce::File f = getPresetsDir().getChildFile(name + ".xml");
        if (f.existsAsFile())
            if (auto xml = juce::XmlDocument::parse(f))
                if (xml->hasTagName(proc.apvts.state.getType()))
                {
                    proc.apvts.replaceState(juce::ValueTree::fromXml(*xml));
                    proc.resetNeeded.store(true, std::memory_order_release);
                    // Drain any pending FIFO frames before resetting the graph so
                    // stale audio-thread data doesn't overwrite the cleared arrays.
                    {
                        int s1, n1, s2, n2;
                        const int ready = proc.gainFifo.getNumReady();
                        proc.gainFifo.prepareToRead(ready, s1, n1, s2, n2);
                        proc.gainFifo.finishedRead(n1 + n2);
                    }
                    graphHead = 0;
                    graphFull = false;
                    graphGain.fill(-100.0f);
                    graphLufs.fill(-100.0f);
                    graphGate.fill(GateState::Active);
                }
    };
    addAndMakeVisible(presetBox);

    btnSave.setTooltip("Save current settings as a new preset");
    btnSave.onClick = [this] { savePreset(); };
    addAndMakeVisible(btnSave);

    btnDelete.setTooltip("Delete the selected preset");
    btnDelete.onClick = [this] { deletePreset(); };
    addAndMakeVisible(btnDelete);

    refreshPresetList();

    setOpaque(true);
    setSize(975, 420);
    startTimerHz(30);
}

DialogueLevelerAudioProcessorEditor::~DialogueLevelerAudioProcessorEditor()
{
    stopTimer();
    for (auto* s : { &sPreGain, &sStartGain, &sTarget, &sWindow, &sAttack, &sRelease,
                     &sBoost, &sAtten, &sPeakLimit, &sGateThr, &sGateHold, &sLookahead, &sTrim })
        s->setLookAndFeel(nullptr);
}

//==============================================================================
void DialogueLevelerAudioProcessorEditor::resized()
{
    const int W = getWidth();

    // Preset bar shares the 28px title row with the plugin name
    btnSave  .setBounds(W - 132, 4, 60, 20);
    btnDelete.setBounds(W -  68, 4, 60, 20);
    presetBox.setBounds(W - 372, 4, 232, 20);

    constexpr int kGraphY  = 100;
    constexpr int kGraphH  = 73;
    constexpr int kMeterY  = kGraphY + kGraphH + 4;
    constexpr int kMeterH  = 36;
    constexpr int kStatsY  = kMeterY + kMeterH + 4;
    constexpr int kStatsH  = 40;
    constexpr int kCtrlY   = kStatsY + kStatsH + 6;

    // Meters row (INPUT / GAIN / OUTPUT)
    const int mColW = W / 3;
    constexpr int captH = 12, valH = 22;
    mInLabel  .setBounds(0,       kMeterY,         mColW, captH);
    mIn       .setBounds(0,       kMeterY + captH, mColW, valH);
    mGainLabel.setBounds(mColW,   kMeterY,         mColW, captH);
    mGain     .setBounds(mColW,   kMeterY + captH, mColW, valH);
    mOutLabel .setBounds(mColW*2, kMeterY,         mColW, captH);
    mOut      .setBounds(mColW*2, kMeterY + captH, mColW, valH);

    // Stats row — three columns: TRUE PEAK (hold+reset) | 3-SEC PEAK | AVG GAIN (reset)
    const int col1 = W / 3, col2 = 2 * W / 3;
    constexpr int btnW = 52, btnH = 18;

    peakOutLabel   .setBounds(8,            kStatsY + 2,  col1 - 16,        captH);
    peakOutVal     .setBounds(8,            kStatsY + 14, col1 - btnW - 16, valH);
    btnResetPeak   .setBounds(col1 - btnW - 4, kStatsY + 11, btnW,          btnH);

    recentPeakLabel.setBounds(col1 + 8,    kStatsY + 2,  col1 - 16,        captH);
    recentPeakVal  .setBounds(col1 + 8,    kStatsY + 14, col1 - 16,        valH);

    avgGainLabel   .setBounds(col2 + 8,    kStatsY + 2,  col1 - 16,        captH);
    avgGainVal     .setBounds(col2 + 8,    kStatsY + 14, col1 - btnW - 16, valH);
    btnResetAvg    .setBounds(W - btnW - 4, kStatsY + 11, btnW,            btnH);

    // 13 knobs across the width
    constexpr int kNumKnobs = 13;
    const int slotW = W / kNumKnobs;
    constexpr int kNameH = 22, kKnobH = 108;
    const int knobY = kCtrlY + kNameH;

    juce::Slider* knobs[] = {
        &sPreGain, &sStartGain, &sTarget, &sWindow, &sAttack, &sRelease,
        &sBoost, &sAtten, &sPeakLimit, &sGateThr, &sGateHold, &sLookahead, &sTrim
    };
    for (int i = 0; i < kNumKnobs; ++i)
        knobs[i]->setBounds(i * slotW, knobY, slotW, kKnobH);
}

//==============================================================================
void DialogueLevelerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(kBg);

    const int W = getWidth();

    // Title
    g.setColour(kTextHi);
    g.setFont(juce::Font(juce::FontOptions().withHeight(15.0f)));
    g.drawText("DIALOGUE LEVELER", 10, 6, W - 20, 18, juce::Justification::left, false);

    // Combined waveform view: INPUT LUFS (left axis) + GAIN dB (right axis), 145px total
    constexpr int kCombY = 28, kCombH = 145;
    const float maxBoost     = proc.apvts.getRawParameterValue("maxBoost")->load(std::memory_order_relaxed);
    const float maxAtten     = proc.apvts.getRawParameterValue("maxAttenuation")->load(std::memory_order_relaxed);
    const float targetLufs   = proc.apvts.getRawParameterValue("targetLufs")->load(std::memory_order_relaxed);
    const float gateThreshDb = proc.apvts.getRawParameterValue("gateThreshold")->load(std::memory_order_relaxed);
    const float graphRange   = juce::jmax(maxBoost, maxAtten, 1.0f) * 1.1f;
    paintCombined(g, { 0, kCombY, W, kCombH }, targetLufs, gateThreshDb, graphRange);

    // Meter separator lines
    constexpr int kMeterY = kCombY + kCombH + 4;  // = 177, unchanged
    g.setColour(kGridLine);
    const int col = W / 3;
    g.drawVerticalLine(col,   kMeterY, kMeterY + 34); // 34 = captH(12) + valH(22)
    g.drawVerticalLine(col*2, kMeterY, kMeterY + 34);

    // Stats row dividers (three columns)
    constexpr int kStatsY = kMeterY + 36 + 4;
    g.setColour(kGridLine);
    g.drawVerticalLine(W / 3,     kStatsY, kStatsY + 40);
    g.drawVerticalLine(2 * W / 3, kStatsY, kStatsY + 40);

    // Section headers + param names (13 knobs)
    constexpr int kCtrlY = kStatsY + 40 + 6;
    const int slotW = W / 13;

    struct Group { int col; int span; const char* name; };
    const Group groups[] = {
        { 0, 2, "PRE-GAIN"  },
        { 2, 2, "DETECTION" },
        { 4, 2, "DYNAMICS"  },
        { 6, 3, "LIMITS"    },
        { 9, 2, "GATE"      },
        {11, 1, "LOOKAHEAD" },
        {12, 1, "OUTPUT"    },
    };
    g.setFont(juce::Font(juce::FontOptions().withHeight(9.5f)));
    for (auto& gr : groups)
    {
        const int gx = gr.col * slotW;
        const int gw = gr.span * slotW;
        g.setColour(kAccentDim);
        g.drawText(gr.name, gx + 2, kCtrlY + 1, gw - 4, 12,
                   juce::Justification::centred, false);
        if (gr.col > 0)
        {
            g.setColour(kGridLine);
            g.drawVerticalLine(gx, static_cast<float>(kCtrlY),
                               static_cast<float>(getHeight()));
        }
    }

    const char* allNames[] = {
        "PRE-GAIN",  "START GAIN",
        "TARGET",    "DET WIN",
        "ATTACK",    "RELEASE",
        "MAX BOOST", "MAX ATTEN", "PEAK LIMIT",
        "GATE THR",  "GATE HOLD",
        "LOOKAHEAD", "OUT TRIM"
    };
    g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    g.setColour(kTextLo);
    for (int i = 0; i < 13; ++i)
        g.drawText(allNames[i], i * slotW, kCtrlY + 22, slotW, 12,
                   juce::Justification::centred, false);
}

//==============================================================================
void DialogueLevelerAudioProcessorEditor::paintOverChildren(juce::Graphics& g)
{
    const int W = getWidth();
    const int slotW = W / 13;

    constexpr int kGraphY  = 100, kGraphH = 73;
    constexpr int kMeterY  = kGraphY + kGraphH + 4;
    constexpr int kMeterH  = 36;
    constexpr int kStatsY  = kMeterY + kMeterH + 4;
    constexpr int kStatsH  = 40;
    constexpr int kCtrlY   = kStatsY + kStatsH + 6;
    constexpr int kNameH   = 22;
    constexpr int kLedSize = 8;
    // Place LED in the lower portion of the header area, below the group label text
    // (which occupies the top 13px) and above the knob names (which start at kCtrlY+22).
    const int ledY = kCtrlY + kNameH - kLedSize;

    const double now      = juce::Time::getMillisecondCounterHiRes();
    const bool boostLit   = (now - lastBoostClipMs < 500.0);
    const bool attenLit   = (now - lastAttenClipMs < 500.0);

    auto drawLed = [&](int col, bool lit)
    {
        const float lx = static_cast<float>(col * slotW + slotW / 2 - kLedSize / 2);
        const float ly = static_cast<float>(ledY);
        const float sz = static_cast<float>(kLedSize);
        g.setColour(lit ? kClipOn : kClipOff);
        g.fillEllipse(lx, ly, sz, sz);
        g.setColour(lit ? kClipOn.brighter(0.3f) : kGridLine);
        g.drawEllipse(lx, ly, sz, sz, 1.0f);
    };
    // MAX BOOST is col 6, MAX ATTEN is col 7 in the 13-knob layout
    drawLed(6, boostLit);
    drawLed(7, attenLit);
}

//==============================================================================
void DialogueLevelerAudioProcessorEditor::paintCombined(
    juce::Graphics& g, juce::Rectangle<int> area,
    float targetLufs, float gateThreshDb, float rangeDb)
{
    const int x0 = area.getX(), y0 = area.getY();
    const int W  = area.getWidth(), H = area.getHeight();

    // Left axis: LUFS (0 at top, -60 at bottom)
    constexpr float kTopLufs = 0.0f, kBotLufs = -60.0f;
    const float lufsScale = (float)H / (kTopLufs - kBotLufs);
    auto lufsToY = [&](float lufs) -> float {
        return y0 + (kTopLufs - juce::jlimit(kBotLufs, kTopLufs, lufs)) * lufsScale;
    };

    // Right axis: gain dB centred at 0
    const float gcy    = y0 + H * 0.5f;
    const float gscale = (H * 0.46f) / rangeDb;
    auto gainToY = [&](float db) -> float {
        return gcy - juce::jlimit(-rangeDb, rangeDb, db) * gscale;
    };

    g.setColour(kSurface);
    g.fillRect(area);

    const int numPts = graphFull ? kGraphPoints : graphHead;
    const int si     = graphFull ? graphHead : 0;
    constexpr int kLaneH = 6;
    const int laneY = y0 + H - kLaneH;

    // ── Gate-state tint (background) and lane (bottom strip) ─────────────────
    // Three visual states per frame:
    //   Active  → dim green lane, no tint
    //   InHold  → orange lane, orange tint (signal below threshold, hold counting down)
    //   Frozen  → bright green lane, green tint (hold expired, gain is frozen)
    {
        int blockStart = -1;
        GateState blockState = GateState::Active;

        auto drawBlock = [&](int from, int to, GateState st)
        {
            const float px1 = x0 + (float)from * W / kGraphPoints;
            const float px2 = x0 + (float)to   * W / kGraphPoints;
            switch (st)
            {
                case GateState::Active:
                    g.setColour(kWaveform.withAlpha(0.18f));
                    g.fillRect(px1, (float)laneY, px2 - px1, (float)kLaneH);
                    break;
                case GateState::InHold:
                    g.setColour(kCut.withAlpha(0.07f));
                    g.fillRect(px1, (float)y0, px2 - px1, (float)(H - kLaneH));
                    g.setColour(kCut.withAlpha(0.50f));
                    g.fillRect(px1, (float)laneY, px2 - px1, (float)kLaneH);
                    break;
                case GateState::Frozen:
                    g.setColour(kWaveform.withAlpha(0.07f));
                    g.fillRect(px1, (float)y0, px2 - px1, (float)(H - kLaneH));
                    g.setColour(kWaveform.withAlpha(0.65f));
                    g.fillRect(px1, (float)laneY, px2 - px1, (float)kLaneH);
                    break;
            }
        };

        for (int i = 0; i <= numPts; ++i)
        {
            const bool atEnd   = (i == numPts);
            const int  idx     = atEnd ? 0 : (si + i) % kGraphPoints;
            const bool hasData = !atEnd && (graphLufs[idx] > -99.0f);
            const GateState cur = hasData ? graphGate[idx] : GateState::Active;

            if (!hasData || atEnd)
            {
                if (blockStart >= 0) { drawBlock(blockStart, i, blockState); blockStart = -1; }
            }
            else if (blockStart < 0)
            {
                blockStart = i;  blockState = cur;
            }
            else if (cur != blockState)
            {
                drawBlock(blockStart, i, blockState);
                blockStart = i;  blockState = cur;
            }
        }
    }

    // ── LUFS horizontal grid (left-axis labels) ────────────────────────────────
    g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    for (float lufs = -10.0f; lufs >= kBotLufs; lufs -= 10.0f)
    {
        const int gy = juce::roundToInt(lufsToY(lufs));
        g.setColour(kGridLine);
        g.drawHorizontalLine(gy, (float)x0, (float)(x0 + W));
        g.setColour(kTextLo);
        g.drawText(juce::String(juce::roundToInt(lufs)),
                   x0 + 3, gy - 6, 26, 12, juce::Justification::left, false);
    }

    // ── Gain zero line + right-axis tick labels (no extra full-width lines) ────
    g.setColour(juce::Colour(0xffc0bcd0));  // light blue-gray, visible but unobtrusive
    g.drawHorizontalLine(juce::roundToInt(gcy), (float)x0, (float)(x0 + W));
    g.setColour(kAccent.withAlpha(0.55f));
    g.drawText("0", x0 + W - 28, juce::roundToInt(gcy) - 6, 24, 12,
               juce::Justification::right, false);

    const float tickDb = rangeDb <= 15.0f ? 3.0f : rangeDb <= 30.0f ? 6.0f : 12.0f;
    for (float db = tickDb; db < rangeDb; db += tickDb)
    {
        for (float sign : { 1.0f, -1.0f })
        {
            const int gy = juce::roundToInt(gainToY(sign * db));
            if (gy <= y0 || gy >= y0 + H) continue;
            g.setColour(kAccent.withAlpha(0.35f));
            g.drawHorizontalLine(gy, (float)(x0 + W - 10), (float)(x0 + W));
            const int dbInt = juce::roundToInt(sign * db);
            g.setColour(kAccent.withAlpha(0.55f));
            g.drawText((dbInt > 0 ? "+" : "") + juce::String(dbInt),
                       x0 + W - 30, gy - 6, 20, 12, juce::Justification::right, false);
        }
    }

    // ── Gate threshold line (orange, left-axis reference) ─────────────────────
    {
        const int gy = juce::roundToInt(lufsToY(gateThreshDb));
        g.setColour(kCut);
        g.drawHorizontalLine(gy, (float)x0, (float)(x0 + W));
        g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        g.drawText("GATE THR", x0 + 3, gy + 2, 54, 10, juce::Justification::left, false);
    }

    // ── Target LUFS line (muted violet, distinct from gain waveform) ─────────────
    {
        const int ty = juce::roundToInt(lufsToY(targetLufs));
        g.setColour(kTarget);
        g.drawHorizontalLine(ty, (float)x0, (float)(x0 + W));
        g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        g.drawText("TARGET", x0 + 3, ty - 11, 42, 10, juce::Justification::left, false);
    }

    // ── LUFS waveform (three segments: green=Active, orange=InHold, dim=Frozen) ─
    if (numPts >= 2)
    {
        pathActive.clear(); pathHold.clear(); pathFrozen.clear();
        bool sActive = false, sHold = false, sFrozen = false;
        for (int i = 0; i < numPts; ++i)
        {
            const int   idx  = (si + i) % kGraphPoints;
            const float lufs = graphLufs[idx];
            if (lufs <= -99.0f) { sActive = sHold = sFrozen = false; continue; }
            const float px = x0 + (float)i * W / kGraphPoints;
            const float py = lufsToY(lufs);
            const GateState gs = graphGate[idx];

            // Reset sub-paths that are no longer the current state
            if (gs != GateState::Active)  sActive  = false;
            if (gs != GateState::InHold)  sHold    = false;
            if (gs != GateState::Frozen)  sFrozen  = false;

            juce::Path& path    = gs == GateState::Active ? pathActive
                                : gs == GateState::InHold ? pathHold : pathFrozen;
            bool&       started = gs == GateState::Active ? sActive
                                : gs == GateState::InHold ? sHold   : sFrozen;

            if (!started) { path.startNewSubPath(px, py); started = true; }
            else            path.lineTo(px, py);
        }
        g.setColour(kWaveform);
        g.strokePath(pathActive, juce::PathStrokeType(1.5f));
        g.setColour(kCut);
        g.strokePath(pathHold,   juce::PathStrokeType(1.5f));
        g.setColour(kTextLo);
        g.strokePath(pathFrozen, juce::PathStrokeType(1.0f));
    }

    // ── Gain waveform (cyan, right-axis scale) ────────────────────────────────
    if (numPts >= 2)
    {
        gainPath.clear();
        bool started = false;
        for (int i = 0; i < numPts; ++i)
        {
            const int   idx  = (si + i) % kGraphPoints;
            // Skip slots that the LUFS path treats as uninitialised (no-signal sentinel).
            // Without this, zero-initialised gain slots draw a spurious line to 0 dB.
            if (graphLufs[idx] <= -99.0f) { started = false; continue; }
            const float px  = x0 + (float)i * W / kGraphPoints;
            const float py  = gainToY(graphGain[idx]);
            if (!started) { gainPath.startNewSubPath(px, py); started = true; }
            else            gainPath.lineTo(px, py);
        }
        g.setColour(kAccent);
        g.strokePath(gainPath, juce::PathStrokeType(1.5f));
    }

    // ── Gate transition markers (vertical time indicators) ────────────────────
    // Full-height semi-transparent line at each state change, coloured by new state:
    //   → InHold : orange  (signal just dropped below threshold)
    //   → Frozen : green   (hold window elapsed, gate has fired)
    //   → Active : dim     (gate released, leveler re-engaged)
    if (numPts >= 2)
    {
        GateState prev = graphGate[si];
        for (int i = 1; i < numPts; ++i)
        {
            const int idx = (si + i) % kGraphPoints;
            if (graphLufs[idx] <= -99.0f) { prev = GateState::Active; continue; }
            const GateState cur = graphGate[idx];
            if (cur != prev)
            {
                const float px = x0 + (float)i * W / kGraphPoints;
                const juce::Colour mc = cur == GateState::InHold  ? kCut
                                      : cur == GateState::Frozen   ? kWaveform
                                      : kTextLo;
                g.setColour(mc.withAlpha(0.35f));
                g.drawVerticalLine(juce::roundToInt(px), (float)y0, (float)(y0 + H));
                prev = cur;
            }
        }
    }

    // ── Panel labels ──────────────────────────────────────────────────────────
    g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    g.setColour(kWaveform.withAlpha(0.8f));
    g.drawText("INPUT LUFS", x0 + 3, y0 + 2, 64, 10, juce::Justification::left, false);
    g.setColour(kAccent.withAlpha(0.8f));
    g.drawText("GAIN dB", x0 + W - 46, y0 + 2, 42, 10, juce::Justification::right, false);

    g.setColour(kGridLine);
    g.drawRect(area);
}

//==============================================================================
void DialogueLevelerAudioProcessorEditor::timerCallback()
{
    // Drain FIFO → gain + LUFS ring buffers (same head so they stay in lock-step)
    {
        int s1, n1, s2, n2;
        proc.gainFifo.prepareToRead(proc.gainFifo.getNumReady(), s1, n1, s2, n2);
        auto consume = [&](int start, int count)
        {
            for (int i = 0; i < count; ++i)
            {
                graphGain[graphHead] = proc.gainFifoBuffer[start + i].gainDb;
                graphLufs[graphHead] = proc.gainFifoBuffer[start + i].lufsIn;
                graphGate[graphHead] = proc.gainFifoBuffer[start + i].gate;
                graphHead = (graphHead + 1) % kGraphPoints;
                if (graphHead == 0) graphFull = true;
            }
        };
        consume(s1, n1);
        consume(s2, n2);
        proc.gainFifo.finishedRead(n1 + n2);
    }

    // Update clip LED timestamps
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (proc.clippingBoost.load(std::memory_order_relaxed)) lastBoostClipMs = now;
    if (proc.clippingAtten.load(std::memory_order_relaxed)) lastAttenClipMs = now;

    // Update meter labels
    const float inLufs  = proc.getMeasuredLufs();
    const float gainDb  = proc.getAppliedGainDb();
    // Adding gain dB to LUFS is only a valid approximation when gain is constant
    // over the full integration window. Hide the output when there is no signal.
    const float outLufs = (inLufs > -99.0f) ? inLufs + gainDb : -100.0f;

    auto lufsStr = [](float v) -> juce::String
    { return v <= -99.0f ? "-inf" : juce::String(v, 1) + " LUFS"; };
    auto gainStr = [](float v) -> juce::String
    { return (v >= 0 ? "+" : "") + juce::String(v, 1) + " dB"; };

    mIn  .setText(lufsStr(inLufs),  juce::dontSendNotification);
    mGain.setText(gainStr(gainDb),  juce::dontSendNotification);
    mOut .setText(lufsStr(outLufs), juce::dontSendNotification);
    mGain.setColour(juce::Label::textColourId,
                    gainDb > 0.5f  ? kAccent :
                    gainDb < -0.5f ? kCut   : kTextHi);

    // Peak output hold
    auto peakStr = [](float v) -> juce::String
    { return v <= -150.0f ? "---" : juce::String(v, 1) + " dBTP"; };
    auto peakColour = [](float v) -> juce::Colour
    { return v >= -1.0f ? kClipOn : v >= -6.0f ? kCut : kTextHi; };

    const float peak = proc.peakOutputDb.load(std::memory_order_relaxed);
    peakOutVal.setText(peakStr(peak), juce::dontSendNotification);
    peakOutVal.setColour(juce::Label::textColourId, peakColour(peak));

    // 3-second rolling True Peak
    const float recent = proc.recentPeakDb.load(std::memory_order_relaxed);
    recentPeakVal.setText(peakStr(recent), juce::dontSendNotification);
    recentPeakVal.setColour(juce::Label::textColourId, peakColour(recent));

    // Average gain
    const float avg = proc.avgGainDb.load(std::memory_order_relaxed);
    auto avgStr = [](float v) -> juce::String
    { return v <= -100.0f ? "---" : (v >= 0.0f ? "+" : "") + juce::String(v, 1) + " dB"; };
    avgGainVal.setText(avgStr(avg), juce::dontSendNotification);
    avgGainVal.setColour(juce::Label::textColourId,
                         avg <= -100.0f ? kTextHi :
                         avg >   0.5f   ? kAccent  :
                         avg <  -0.5f   ? kCut    : kTextHi);

    repaint();
}

//==============================================================================
juce::File DialogueLevelerAudioProcessorEditor::getPresetsDir() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("DialogueLeveler/Presets");
}

void DialogueLevelerAudioProcessorEditor::refreshPresetList()
{
    presetBox.clear(juce::dontSendNotification);
    const juce::File dir = getPresetsDir();
    if (!dir.exists()) return;
    auto files = dir.findChildFiles(juce::File::findFiles, false, "*.xml");
    files.sort();
    int id = 1;
    for (const auto& f : files)
        presetBox.addItem(f.getFileNameWithoutExtension(), id++);
}

void DialogueLevelerAudioProcessorEditor::savePreset()
{
    auto* w = new juce::AlertWindow("Save Preset", "Enter a name for this preset:",
                                     juce::MessageBoxIconType::NoIcon);
    w->addTextEditor("name", "My Preset", "");
    w->addButton("Save",   1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<juce::AlertWindow> safeW(w);
    juce::Component::SafePointer<DialogueLevelerAudioProcessorEditor> safeThis(this);
    w->enterModalState(true,
        juce::ModalCallbackFunction::create([safeThis, safeW](int result) mutable
        {
            if (safeThis == nullptr) return;
            if (result == 1 && safeW != nullptr)
            {
                const juce::String name = juce::File::createLegalFileName(
                    safeW->getTextEditorContents("name").trim());
                if (!name.isEmpty())
                {
                    const juce::File dir = safeThis->getPresetsDir();
                    dir.createDirectory();
                    const juce::File f = dir.getChildFile(name + ".xml");
                    if (!f.isAChildOf(dir)) return;
                    if (auto xml = safeThis->proc.apvts.copyState().createXml())
                        xml->writeTo(f);
                    safeThis->refreshPresetList();
                    for (int i = 0; i < safeThis->presetBox.getNumItems(); ++i)
                        if (safeThis->presetBox.getItemText(i) == name)
                            { safeThis->presetBox.setSelectedItemIndex(i, juce::dontSendNotification); break; }
                }
            }
        }), true);
}

void DialogueLevelerAudioProcessorEditor::deletePreset()
{
    const juce::String name = presetBox.getSelectedItemText();
    if (name.isEmpty()) return;

    juce::Component::SafePointer<DialogueLevelerAudioProcessorEditor> safeThis(this);
    juce::AlertWindow::showOkCancelBox(
        juce::MessageBoxIconType::WarningIcon,
        "Delete Preset",
        "Delete \"" + name + "\"? This cannot be undone.",
        "Delete", "Cancel",
        this,
        juce::ModalCallbackFunction::create([safeThis, name](int result)
        {
            if (result == 1 && safeThis)
            {
                const juce::File dir = safeThis->getPresetsDir();
                const juce::File f   = dir.getChildFile(name + ".xml");
                if (!f.isAChildOf(dir)) return;
                f.deleteFile();
                safeThis->refreshPresetList();
            }
        }));
}
