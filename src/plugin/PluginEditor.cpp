#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

using namespace dunecrusher;

namespace {

constexpr int kEditorWidth = 540;
constexpr int kEditorHeight = 392;

const juce::Colour kBgOuterTop(0xFF1A1920);
const juce::Colour kBgOuterBot(0xFF101015);
const juce::Colour kCardBg(0xFF1E1D25);
const juce::Colour kCardBorder(0xFF32303D);
const juce::Colour kOledBg(0xFF0C0E13);
const juce::Colour kOledText(0xFFF6C677);
const juce::Colour kTextPrimary(0xFFF2EFEA);
const juce::Colour kTextMuted(0xFF8C8798);

const juce::Colour kDriveAccent(0xFFF59E0B);
const juce::Colour kFilterAccent(0xFFEAB308);
const juce::Colour kCompAccent(0xFFF97316);
const juce::Colour kSlamAccent(0xFFEF4444);
const juce::Colour kMasterAccent(0xFF2DD4BF);

void drawCornerScrew(juce::Graphics& g, float cx, float cy) {
    constexpr float r = 4.2f;
    g.setColour(juce::Colour(0xFF09090C));
    g.fillEllipse(cx - r - 0.8f, cy - r - 0.8f, (r + 0.8f) * 2.0f, (r + 0.8f) * 2.0f);

    juce::ColourGradient grad(juce::Colour(0xFF474452), cx - r, cy - r,
                              juce::Colour(0xFF212028), cx + r, cy + r, false);
    g.setGradientFill(grad);
    g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);

    g.setColour(juce::Colour(0xFF111015));
    g.drawLine(cx - 2.4f, cy - 1.4f, cx + 2.4f, cy + 1.4f, 1.1f);
}

void drawModuleCard(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& tag,
                    juce::Colour accent, bool active) {
    const auto rf = r.toFloat();

    // Soft drop shadow
    g.setColour(juce::Colours::black.withAlpha(0.32f));
    g.fillRoundedRectangle(rf.translated(0.0f, 2.0f), 10.0f);

    // Card surface
    juce::ColourGradient fillGrad(kCardBg.brighter(0.04f), rf.getX(), rf.getY(),
                                  kCardBg.darker(0.10f), rf.getX(), rf.getBottom(), false);
    g.setGradientFill(fillGrad);
    g.fillRoundedRectangle(rf, 10.0f);

    // Subtle accent top bar
    g.setColour(accent.withAlpha(active ? 0.55f : 0.14f));
    g.fillRoundedRectangle(rf.getX() + 12.0f, rf.getY() + 1.0f, rf.getWidth() - 24.0f, 2.0f, 1.0f);

    // Border
    g.setColour(active ? kCardBorder.brighter(0.08f) : kCardBorder.darker(0.25f));
    g.drawRoundedRectangle(rf.reduced(0.5f), 10.0f, 1.0f);

    // Divider below header
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawHorizontalLine(r.getY() + 29, rf.getX() + 10.0f, rf.getRight() - 10.0f);

    // Right-aligned technical tag
    g.setColour(kTextMuted.withAlpha(active ? 0.72f : 0.35f));
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.drawText(tag, r.getX() + 120, r.getY() + 7, r.getWidth() - 130, 16,
               juce::Justification::centredRight, false);
}

void drawSegmentedMeter(juce::Graphics& g, juce::Rectangle<float> bounds, float level,
                        bool isGainReduction) {
    constexpr int kSegments = 20;
    const float gap = 2.0f;
    const float segW = (bounds.getWidth() - gap * static_cast<float>(kSegments - 1)) / static_cast<float>(kSegments);
    const float clamped = std::clamp(level, 0.0f, 1.0f);

    for (int i = 0; i < kSegments; ++i) {
        const float frac = static_cast<float>(i + 1) / static_cast<float>(kSegments);
        const int drawIdx = isGainReduction ? (kSegments - 1 - i) : i;
        const float x = bounds.getX() + static_cast<float>(drawIdx) * (segW + gap);
        const juce::Rectangle<float> seg(x, bounds.getY(), segW, bounds.getHeight());

        juce::Colour onColour;
        if (isGainReduction) {
            onColour = (frac > 0.65f) ? kSlamAccent : kCompAccent;
        } else {
            if (frac > 0.88f) onColour = kSlamAccent;
            else if (frac > 0.65f) onColour = kDriveAccent;
            else onColour = kMasterAccent;
        }

        const bool lit = (clamped >= (static_cast<float>(i) + 0.35f) / static_cast<float>(kSegments));
        g.setColour(lit ? onColour : juce::Colour(0xFF1D1D25));
        g.fillRoundedRectangle(seg, 1.5f);
    }
}

} // namespace

// ============================================================================
// Custom metallic rotary knob with glowing value arc & live readout
// ============================================================================
class DuneCrusherEditor::RotaryKnob final : public juce::Slider {
public:
    RotaryKnob(int paramId, juce::String label, juce::Colour accent,
               std::function<void(int, float)> onUserChange)
        : paramId_(paramId),
          label_(std::move(label)),
          accent_(accent),
          onUserChange_(std::move(onUserChange)) {
        setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        setMouseDragSensitivity(165);
        setVelocityBasedMode(false);
        setPopupDisplayEnabled(false, false, nullptr);
    }

    void setModuleActive(bool active) {
        if (moduleActive_ != active) {
            moduleActive_ = active;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();
        const float dialAreaH = bounds.getHeight() - 28.0f;
        const float diameter = std::min(bounds.getWidth() - 10.0f, dialAreaH - 4.0f);
        const float radius = diameter * 0.5f;
        const float cx = bounds.getCentreX();
        const float cy = bounds.getY() + radius + 4.0f;

        constexpr float kStartAngle = juce::MathConstants<float>::pi * 1.25f;
        constexpr float kEndAngle = juce::MathConstants<float>::pi * 2.75f;
        const double prop = valueToProportionOfLength(getValue());
        const float angle = kStartAngle + static_cast<float>(prop) * (kEndAngle - kStartAngle);

        const float arcRadius = radius + 2.5f;

        // Background track arc
        juce::Path trackArc;
        trackArc.addCentredArc(cx, cy, arcRadius, arcRadius, 0.0f, kStartAngle, kEndAngle, true);
        g.setColour(juce::Colour(0xFF101015));
        g.strokePath(trackArc, juce::PathStrokeType(3.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Active value arc
        const auto activeCol = moduleActive_ ? accent_ : kTextMuted.withAlpha(0.35f);
        if (prop > 0.002) {
            juce::Path valArc;
            valArc.addCentredArc(cx, cy, arcRadius, arcRadius, 0.0f, kStartAngle, angle, true);
            if (moduleActive_) {
                g.setColour(activeCol.withAlpha(0.22f));
                g.strokePath(valArc, juce::PathStrokeType(6.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            g.setColour(activeCol);
            g.strokePath(valArc, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Knob drop shadow
        const float capR = radius - 3.0f;
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillEllipse(cx - capR, cy - capR + 2.2f, capR * 2.0f, capR * 2.0f);

        // Outer metallic rim
        juce::ColourGradient rimGrad(juce::Colour(0xFF474454), cx - capR, cy - capR,
                                     juce::Colour(0xFF17161D), cx + capR, cy + capR, false);
        g.setGradientFill(rimGrad);
        g.fillEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);

        // Inner brushed dial face
        const float faceR = capR - 2.6f;
        juce::ColourGradient faceGrad(juce::Colour(0xFF2D2B36), cx - faceR * 0.5f, cy - faceR,
                                      juce::Colour(0xFF191820), cx + faceR * 0.5f, cy + faceR, false);
        g.setGradientFill(faceGrad);
        g.fillEllipse(cx - faceR, cy - faceR, faceR * 2.0f, faceR * 2.0f);

        // Subtle inner specular ring
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawEllipse(cx - faceR + 0.5f, cy - faceR + 0.5f, (faceR - 0.5f) * 2.0f, (faceR - 0.5f) * 2.0f, 1.0f);

        // Pointer notch
        const float ptrOuter = faceR - 2.0f;
        const float ptrInner = faceR * 0.34f;
        const float sinA = std::sin(angle);
        const float cosA = -std::cos(angle);
        const float x1 = cx + sinA * ptrInner;
        const float y1 = cy + cosA * ptrInner;
        const float x2 = cx + sinA * ptrOuter;
        const float y2 = cy + cosA * ptrOuter;

        g.setColour(moduleActive_ ? activeCol.brighter(0.25f) : kTextMuted.withAlpha(0.5f));
        g.drawLine(x1, y1, x2, y2, 2.3f);

        // Label
        const int labelY = static_cast<int>(bounds.getBottom()) - 26;
        g.setColour(moduleActive_ ? kTextPrimary : kTextMuted.withAlpha(0.55f));
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.drawText(label_, 0, labelY, getWidth(), 13, juce::Justification::centred, false);

        // Subtle numeric readout under label
        const auto valStr = juce::String(formatParamDisplay(paramId_, static_cast<float>(getValue())));
        g.setColour(moduleActive_ ? kTextMuted : kTextMuted.withAlpha(0.38f));
        g.setFont(juce::FontOptions(9.5f, juce::Font::plain));
        g.drawText(valStr, 0, labelY + 13, getWidth(), 12, juce::Justification::centred, false);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        userInteracting_ = true;
        juce::Slider::mouseDown(e);
        notifyUser(static_cast<float>(getValue()));
    }

    void mouseUp(const juce::MouseEvent& e) override {
        juce::Slider::mouseUp(e);
        userInteracting_ = false;
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override {
        userInteracting_ = true;
        juce::Slider::mouseWheelMove(e, wheel);
        notifyUser(static_cast<float>(getValue()));
        userInteracting_ = false;
    }

    void valueChanged() override {
        juce::Slider::valueChanged();
        if (userInteracting_ || isMouseButtonDown()) {
            notifyUser(static_cast<float>(getValue()));
        }
    }

private:
    void notifyUser(float normVal) {
        if (onUserChange_) onUserChange_(paramId_, normVal);
    }

    int paramId_;
    juce::String label_;
    juce::Colour accent_;
    std::function<void(int, float)> onUserChange_;
    bool moduleActive_ = true;
    bool userInteracting_ = false;
};

// ============================================================================
// Glowing LED module power button + title
// ============================================================================
class DuneCrusherEditor::PowerLedButton final : public juce::ToggleButton {
public:
    PowerLedButton(juce::String title, juce::Colour accent)
        : title_(std::move(title)), accent_(accent) {
        setClickingTogglesState(true);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*isDown*/) override {
        const auto b = getLocalBounds().toFloat();
        const bool on = getToggleState();
        const float ledR = 4.5f;
        const float ledCx = b.getX() + 9.0f;
        const float ledCy = b.getCentreY();

        if (on) {
            g.setColour(accent_.withAlpha(0.28f));
            g.fillEllipse(ledCx - ledR * 2.0f, ledCy - ledR * 2.0f, ledR * 4.0f, ledR * 4.0f);
            g.setColour(accent_);
            g.fillEllipse(ledCx - ledR, ledCy - ledR, ledR * 2.0f, ledR * 2.0f);
            g.setColour(juce::Colours::white.withAlpha(0.65f));
            g.fillEllipse(ledCx - 1.8f, ledCy - 2.0f, 2.4f, 2.4f);
        } else {
            g.setColour(juce::Colour(0xFF101015));
            g.fillEllipse(ledCx - ledR, ledCy - ledR, ledR * 2.0f, ledR * 2.0f);
            g.setColour(juce::Colour(0xFF3E3B4A));
            g.drawEllipse(ledCx - ledR, ledCy - ledR, ledR * 2.0f, ledR * 2.0f, 1.0f);
        }

        g.setColour(on ? kTextPrimary : (shouldDrawButtonAsHighlighted ? kTextPrimary.withAlpha(0.75f) : kTextMuted));
        g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.drawText(title_, static_cast<int>(ledCx + 10.0f), 0, getWidth() - 22, getHeight(),
                   juce::Justification::centredLeft, false);
    }

private:
    juce::String title_;
    juce::Colour accent_;
};

// ============================================================================
// Backlit hardware SLAM / GLUE mode toggle switch
// ============================================================================
class DuneCrusherEditor::SlamToggleButton final : public juce::ToggleButton {
public:
    SlamToggleButton() {
        setClickingTogglesState(true);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void setModuleActive(bool active) {
        if (moduleActive_ != active) {
            moduleActive_ = active;
            repaint();
        }
    }

    void paintButton(juce::Graphics& g, bool highlighted, bool isDown) override {
        const auto area = getLocalBounds().toFloat();
        const auto pill = area.withHeight(36.0f).reduced(1.0f);
        const bool slam = getToggleState();
        const bool lit = slam && moduleActive_;

        // Shadow
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRoundedRectangle(pill.translated(0.0f, 2.0f), 7.0f);

        // Button body
        if (lit) {
            juce::ColourGradient grad(kSlamAccent.brighter(isDown ? -0.1f : 0.05f), pill.getX(), pill.getY(),
                                      kCompAccent.darker(0.35f), pill.getX(), pill.getBottom(), false);
            g.setGradientFill(grad);
        } else {
            juce::ColourGradient grad(juce::Colour(highlighted ? 0xFF312F3C : 0xFF282632), pill.getX(), pill.getY(),
                                      juce::Colour(0xFF191820), pill.getX(), pill.getBottom(), false);
            g.setGradientFill(grad);
        }
        g.fillRoundedRectangle(pill, 7.0f);

        // Border
        g.setColour(lit ? kSlamAccent.brighter(0.35f) : kCardBorder);
        g.drawRoundedRectangle(pill, 7.0f, 1.1f);

        // LED bar inside switch
        const auto ledBar = juce::Rectangle<float>(pill.getCentreX() - 16.0f, pill.getY() + 5.5f, 32.0f, 3.0f);
        g.setColour(lit ? juce::Colour(0xFFFFE4C4) : juce::Colour(0xFF121218));
        g.fillRoundedRectangle(ledBar, 1.5f);

        // Label
        g.setColour(lit ? juce::Colours::white : (moduleActive_ ? kTextPrimary.withAlpha(0.82f) : kTextMuted.withAlpha(0.45f)));
        g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.drawText("SLAM", pill.withTrimmedTop(8.0f).toNearestInt(), juce::Justification::centred, false);

        // Mode caption underneath
        const auto caption = slam ? "MODE: SLAM" : "MODE: GLUE";
        g.setColour(moduleActive_ ? kTextMuted : kTextMuted.withAlpha(0.4f));
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText(caption, area.withTop(pill.getBottom() + 5.0f).toNearestInt(),
                   juce::Justification::centred, false);
    }

private:
    bool moduleActive_ = true;
};

// ============================================================================
// Main DuneCrusherEditor
// ============================================================================
DuneCrusherEditor::DuneCrusherEditor(DuneCrusherProcessor& proc)
    : AudioProcessorEditor(&proc), processor_(proc) {
    auto& apvts = processor_.getAPVTS();

    auto knobReadout = [this](int paramId, float normVal) {
        showTransientReadout(paramId, normVal);
    };

    // Module power switches
    drivePowerBtn_ = std::make_unique<PowerLedButton>("DRIVE", kDriveAccent);
    filterPowerBtn_ = std::make_unique<PowerLedButton>("FILTER", kFilterAccent);
    compPowerBtn_ = std::make_unique<PowerLedButton>("COMPRESSOR", kCompAccent);
    masterPowerBtn_ = std::make_unique<PowerLedButton>("MASTER", kMasterAccent);

    addAndMakeVisible(*drivePowerBtn_);
    addAndMakeVisible(*filterPowerBtn_);
    addAndMakeVisible(*compPowerBtn_);
    addAndMakeVisible(*masterPowerBtn_);

    drivePowerAtt_ = std::make_unique<ButtonAttachment>(apvts, DuneCrusherProcessor::getParamIdString(kDriveOn), *drivePowerBtn_);
    filterPowerAtt_ = std::make_unique<ButtonAttachment>(apvts, DuneCrusherProcessor::getParamIdString(kFilterOn), *filterPowerBtn_);
    compPowerAtt_ = std::make_unique<ButtonAttachment>(apvts, DuneCrusherProcessor::getParamIdString(kCompOn), *compPowerBtn_);
    masterPowerAtt_ = std::make_unique<ButtonAttachment>(apvts, DuneCrusherProcessor::getParamIdString(kMasterOn), *masterPowerBtn_);

    auto hookPowerClick = [this](PowerLedButton& btn, int paramId) {
        btn.onClick = [this, &btn, paramId]() {
            showTransientReadout(paramId, btn.getToggleState() ? 1.0f : 0.0f);
            updateModuleDimming();
        };
    };
    hookPowerClick(*drivePowerBtn_, kDriveOn);
    hookPowerClick(*filterPowerBtn_, kFilterOn);
    hookPowerClick(*compPowerBtn_, kCompOn);
    hookPowerClick(*masterPowerBtn_, kMasterOn);

    // Rotary knobs
    auto makeKnob = [&](std::unique_ptr<RotaryKnob>& knob, std::unique_ptr<SliderAttachment>& att,
                        int paramId, const char* title, juce::Colour accent) {
        knob = std::make_unique<RotaryKnob>(paramId, title, accent, knobReadout);
        addAndMakeVisible(*knob);
        att = std::make_unique<SliderAttachment>(apvts, DuneCrusherProcessor::getParamIdString(paramId), *knob);
        // Set double-click default AFTER attachment so attachment construction cannot overwrite it
        knob->setDoubleClickReturnValue(true, static_cast<double>(DuneCrusherProcessor::getInitDefaultParam(paramId)));
    };

    makeKnob(biteKnob_, biteAtt_, kDriveBite, "BITE", kDriveAccent);
    makeKnob(warmthKnob_, warmthAtt_, kDriveWarmth, "WARMTH", kDriveAccent);
    makeKnob(cutoffKnob_, cutoffAtt_, kFilterCutoff, "CUTOFF", kFilterAccent);
    makeKnob(resKnob_, resAtt_, kFilterRes, "RES", kFilterAccent);
    makeKnob(compAmtKnob_, compAmtAtt_, kCompAmount, "AMOUNT", kCompAccent);
    makeKnob(mixKnob_, mixAtt_, kMasterMix, "MIX", kMasterAccent);
    makeKnob(volumeKnob_, volumeAtt_, kMasterVolume, "VOLUME", kMasterAccent);

    // SLAM mode switch
    slamBtn_ = std::make_unique<SlamToggleButton>();
    addAndMakeVisible(*slamBtn_);
    slamAtt_ = std::make_unique<ButtonAttachment>(apvts, DuneCrusherProcessor::getParamIdString(kCompSlam), *slamBtn_);
    slamBtn_->onClick = [this]() {
        showTransientReadout(kCompSlam, slamBtn_->getToggleState() ? 1.0f : 0.0f);
    };

    // Header preset controls
    auto styleHeaderBtn = [](juce::TextButton& b, juce::Colour bg, juce::Colour textCol) {
        b.setColour(juce::TextButton::buttonColourId, bg);
        b.setColour(juce::TextButton::buttonOnColourId, bg.brighter(0.12f));
        b.setColour(juce::TextButton::textColourOffId, textCol);
        b.setColour(juce::TextButton::textColourOnId, textCol);
        b.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    };

    styleHeaderBtn(prevPresetBtn_, juce::Colour(0xFF24232E), kTextPrimary);
    styleHeaderBtn(nextPresetBtn_, juce::Colour(0xFF24232E), kTextPrimary);
    styleHeaderBtn(presetDisplayBtn_, kOledBg, kOledText);
    styleHeaderBtn(randomBtn_, juce::Colour(0xFF2B2418), kDriveAccent);

    addAndMakeVisible(prevPresetBtn_);
    addAndMakeVisible(presetDisplayBtn_);
    addAndMakeVisible(nextPresetBtn_);
    addAndMakeVisible(randomBtn_);

    prevPresetBtn_.onClick = [this]() {
        const int count = processor_.getNumPrograms();
        if (count > 0) {
            const int prev = (processor_.getCurrentProgram() - 1 + count) % count;
            processor_.applyPreset(prev, true);
            transientText_.clear();
            refreshPresetDisplay();
        }
    };

    nextPresetBtn_.onClick = [this]() {
        const int count = processor_.getNumPrograms();
        if (count > 0) {
            const int next = (processor_.getCurrentProgram() + 1) % count;
            processor_.applyPreset(next, true);
            transientText_.clear();
            refreshPresetDisplay();
        }
    };

    presetDisplayBtn_.onClick = [this]() { openPresetMenu(); };

    randomBtn_.onClick = [this]() {
        processor_.randomizeParameters();
        transientText_ = "RANDOMIZED";
        transientExpiryMs_ = juce::Time::currentTimeMillis() + 1800;
        refreshPresetDisplay();
    };

    lastSeenLabelVersion_ = processor_.getDisplayLabelVersion();
    refreshPresetDisplay();
    updateModuleDimming();

    setResizable(false, false);
    setSize(kEditorWidth, kEditorHeight);
    startTimerHz(30);
}

DuneCrusherEditor::~DuneCrusherEditor() {
    stopTimer();
}

void DuneCrusherEditor::syncVisualStateNow() {
    displayPeakL_ = std::clamp(processor_.getMeterPeakL(), 0.0f, 1.0f);
    displayPeakR_ = std::clamp(processor_.getMeterPeakR(), 0.0f, 1.0f);
    displayGr_ = std::clamp(processor_.getMeterGR(), 0.0f, 1.0f);
    updateModuleDimming();
    refreshPresetDisplay();
    repaint();
}

void DuneCrusherEditor::showTransientReadout(int paramId, float normalizedValue) {
    static constexpr std::array<const char*, kNumParams> kShortNames = {
        "DRIVE", "BITE", "WARMTH",
        "FILTER", "CUTOFF", "RES",
        "COMP", "AMOUNT", "MODE",
        "MASTER", "MIX", "VOLUME"
    };
    const char* name = (paramId >= 0 && paramId < kNumParams) ? kShortNames[static_cast<size_t>(paramId)] : "PARAM";
    transientText_ = juce::String(name) + ": " + juce::String(formatParamDisplay(paramId, normalizedValue));
    transientExpiryMs_ = juce::Time::currentTimeMillis() + 2200;
    refreshPresetDisplay();
}

void DuneCrusherEditor::refreshPresetDisplay() {
    if (transientText_.isNotEmpty()) {
        presetDisplayBtn_.setButtonText(transientText_);
    } else {
        presetDisplayBtn_.setButtonText(processor_.getDisplayPresetLabel());
    }
}

void DuneCrusherEditor::updateModuleDimming() {
    const bool masterOn = masterPowerBtn_->getToggleState();
    const bool driveOn = masterOn && drivePowerBtn_->getToggleState();
    const bool filterOn = masterOn && filterPowerBtn_->getToggleState();
    const bool compOn = masterOn && compPowerBtn_->getToggleState();

    biteKnob_->setModuleActive(driveOn);
    warmthKnob_->setModuleActive(driveOn);
    cutoffKnob_->setModuleActive(filterOn);
    resKnob_->setModuleActive(filterOn);
    compAmtKnob_->setModuleActive(compOn);
    slamBtn_->setModuleActive(compOn);
    mixKnob_->setModuleActive(masterOn);
    volumeKnob_->setModuleActive(masterOn);
    repaint();
}

void DuneCrusherEditor::openPresetMenu() {
    juce::PopupMenu menu;
    const int count = processor_.getNumPrograms();
    const int current = processor_.getCurrentProgram();

    for (int i = 0; i < count; ++i) {
        menu.addItem(i + 1, processor_.getProgramName(i), true, i == current);
    }

    menu.addSeparator();
    constexpr int kInitId = 1001;
    constexpr int kLoadXmlId = 1002;
    constexpr int kSaveXmlId = 1003;
    menu.addItem(kInitId, "Init (Clean)");
    menu.addItem(kLoadXmlId, "Load Preset (.dcpreset)...");
    menu.addItem(kSaveXmlId, "Save Preset (.dcpreset)...");

    juce::Component::SafePointer<DuneCrusherEditor> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetDisplayBtn_),
                       [safeThis, count](int choice) {
                           if (safeThis == nullptr || choice <= 0) return;

                           if (choice >= 1 && choice <= count) {
                               safeThis->processor_.applyPreset(choice - 1, true);
                               safeThis->transientText_.clear();
                               safeThis->refreshPresetDisplay();
                               safeThis->updateModuleDimming();
                           } else if (choice == kInitId) {
                               safeThis->processor_.resetToInitPreset();
                               safeThis->transientText_.clear();
                               safeThis->refreshPresetDisplay();
                               safeThis->updateModuleDimming();
                           } else if (choice == kLoadXmlId) {
                               safeThis->fileChooser_ = std::make_unique<juce::FileChooser>(
                                   "Load DuneCrusher Preset", juce::File(), "*.dcpreset;*.xml");
                               safeThis->fileChooser_->launchAsync(
                                   juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                   [safeThis](const juce::FileChooser& fc) {
                                       if (safeThis == nullptr) return;
                                       auto f = fc.getResult();
                                       if (f.existsAsFile() && safeThis->processor_.loadPresetFromXmlFile(f)) {
                                           safeThis->transientText_.clear();
                                           safeThis->refreshPresetDisplay();
                                           safeThis->updateModuleDimming();
                                       }
                                   });
                           } else if (choice == kSaveXmlId) {
                               safeThis->fileChooser_ = std::make_unique<juce::FileChooser>(
                                   "Save DuneCrusher Preset",
                                   juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                       .getChildFile("MyPreset.dcpreset"),
                                   "*.dcpreset");
                               safeThis->fileChooser_->launchAsync(
                                   juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                                   [safeThis](const juce::FileChooser& fc) {
                                       if (safeThis == nullptr) return;
                                       auto f = fc.getResult();
                                       if (f != juce::File()) {
                                           safeThis->processor_.saveCurrentPresetToXmlFile(f);
                                       }
                                   });
                           }
                       });
}

void DuneCrusherEditor::timerCallback() {
    if (transientText_.isNotEmpty() && juce::Time::currentTimeMillis() >= transientExpiryMs_) {
        transientText_.clear();
        refreshPresetDisplay();
    }

    const uint64_t labelVer = processor_.getDisplayLabelVersion();
    if (labelVer != lastSeenLabelVersion_) {
        lastSeenLabelVersion_ = labelVer;
        transientText_.clear();
        refreshPresetDisplay();
    }

    // Smooth meter ballistics
    const float rawL = std::clamp(processor_.getMeterPeakL(), 0.0f, 1.0f);
    const float rawR = std::clamp(processor_.getMeterPeakR(), 0.0f, 1.0f);
    const float rawGr = std::clamp(processor_.getMeterGR(), 0.0f, 1.0f);

    constexpr float kDecay = 0.84f;
    const float nextL = (rawL > displayPeakL_) ? rawL : (displayPeakL_ * kDecay);
    const float nextR = (rawR > displayPeakR_) ? rawR : (displayPeakR_ * kDecay);
    const float nextGr = (rawGr > displayGr_) ? rawGr : (displayGr_ * 0.80f);

    const bool metersChanged = std::fabs(nextL - displayPeakL_) > 0.002f
                               || std::fabs(nextR - displayPeakR_) > 0.002f
                               || std::fabs(nextGr - displayGr_) > 0.002f;
    displayPeakL_ = nextL < 0.001f ? 0.0f : nextL;
    displayPeakR_ = nextR < 0.001f ? 0.0f : nextR;
    displayGr_ = nextGr < 0.001f ? 0.0f : nextGr;

    if (metersChanged) {
        repaint(meterStripBounds_);
    }

    // Keep module dimming in sync with host automation
    const bool masterOn = masterPowerBtn_->getToggleState();
    const bool driveOn = masterOn && drivePowerBtn_->getToggleState();
    const bool filterOn = masterOn && filterPowerBtn_->getToggleState();
    const bool compOn = masterOn && compPowerBtn_->getToggleState();

    biteKnob_->setModuleActive(driveOn);
    warmthKnob_->setModuleActive(driveOn);
    cutoffKnob_->setModuleActive(filterOn);
    resKnob_->setModuleActive(filterOn);
    compAmtKnob_->setModuleActive(compOn);
    slamBtn_->setModuleActive(compOn);
    mixKnob_->setModuleActive(masterOn);
    volumeKnob_->setModuleActive(masterOn);
}

void DuneCrusherEditor::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();

    // Brushed obsidian outer chassis
    juce::ColourGradient chassisGrad(kBgOuterTop, 0.0f, 0.0f, kBgOuterBot, 0.0f, bounds.getBottom(), false);
    g.setGradientFill(chassisGrad);
    g.fillRect(bounds);

    // Subtle chassis frame
    g.setColour(juce::Colour(0xFF33303D));
    g.drawRect(bounds.reduced(1.0f), 1.0f);

    // Corner hex screws
    drawCornerScrew(g, 10.0f, 10.0f);
    drawCornerScrew(g, bounds.getRight() - 10.0f, 10.0f);
    drawCornerScrew(g, 10.0f, bounds.getBottom() - 10.0f);
    drawCornerScrew(g, bounds.getRight() - 10.0f, bounds.getBottom() - 10.0f);

    // Brand logotype (left header)
    g.setFont(juce::FontOptions(19.0f, juce::Font::bold));
    g.setColour(kTextPrimary);
    g.drawText("DUNE", 22, 14, 56, 24, juce::Justification::centredLeft, false);
    g.setColour(kDriveAccent);
    g.drawText("CRUSHER", 76, 14, 114, 24, juce::Justification::centredLeft, false);

    g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
    g.setColour(kTextMuted);
    g.drawText(juce::String(juce::CharPointer_UTF8("SATURATION  \xe2\x80\xa2  FILTER  \xe2\x80\xa2  DYNAMICS")),
               22, 37, 185, 13, juce::Justification::centredLeft, false);

    // Subtle recessed border around OLED preset display
    const auto oledOutline = presetDisplayBtn_.getBounds().toFloat().expanded(1.0f);
    g.setColour(juce::Colour(0xFF3B352B));
    g.drawRoundedRectangle(oledOutline, 5.0f, 1.0f);

    // Module cards
    const bool masterOn = masterPowerBtn_->getToggleState();
    const bool driveOn = masterOn && drivePowerBtn_->getToggleState();
    const bool filterOn = masterOn && filterPowerBtn_->getToggleState();
    const bool compOn = masterOn && compPowerBtn_->getToggleState();

    drawModuleCard(g, {16, 62, 248, 134}, "DUAL STAGE", kDriveAccent, driveOn);
    drawModuleCard(g, {276, 62, 248, 134}, "12dB RES LP", kFilterAccent, filterOn);
    drawModuleCard(g, {16, 208, 248, 134}, "DYNAMICS", kCompAccent, compOn);
    drawModuleCard(g, {276, 208, 248, 134}, "OUTPUT", kMasterAccent, masterOn);

    // Bottom metering strip
    const auto ms = meterStripBounds_.toFloat();
    g.setColour(kOledBg);
    g.fillRoundedRectangle(ms, 6.0f);
    g.setColour(kCardBorder.darker(0.15f));
    g.drawRoundedRectangle(ms.reduced(0.5f), 6.0f, 1.0f);

    // OUT L/R meter
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.setColour(kTextMuted);
    g.drawText("OUT", meterStripBounds_.getX() + 10, meterStripBounds_.getY(), 28, meterStripBounds_.getHeight(),
               juce::Justification::centredLeft, false);

    const float outX = ms.getX() + 42.0f;
    const float outW = 216.0f;
    drawSegmentedMeter(g, {outX, ms.getY() + 6.0f, outW, 6.5f}, displayPeakL_, false);
    drawSegmentedMeter(g, {outX, ms.getY() + 15.5f, outW, 6.5f}, displayPeakR_, false);

    // Divider
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawVerticalLine(meterStripBounds_.getX() + 272, ms.getY() + 5.0f, ms.getBottom() - 5.0f);

    // GR meter
    g.setColour(kTextMuted);
    g.drawText("GR", meterStripBounds_.getX() + 284, meterStripBounds_.getY(), 22, meterStripBounds_.getHeight(),
               juce::Justification::centredLeft, false);
    drawSegmentedMeter(g, {ms.getX() + 310.0f, ms.getY() + 9.5f, 186.0f, 9.0f}, displayGr_, true);
}

void DuneCrusherEditor::resized() {
    if (!drivePowerBtn_ || !volumeKnob_ || !slamBtn_) return;

    // Header preset strip (right side)
    prevPresetBtn_.setBounds(216, 17, 26, 28);
    presetDisplayBtn_.setBounds(246, 17, 198, 28);
    nextPresetBtn_.setBounds(448, 17, 26, 28);
    randomBtn_.setBounds(480, 17, 44, 28);

    // Card header power buttons
    drivePowerBtn_->setBounds(24, 67, 120, 22);
    filterPowerBtn_->setBounds(284, 67, 120, 22);
    compPowerBtn_->setBounds(24, 213, 130, 22);
    masterPowerBtn_->setBounds(284, 213, 120, 22);

    // Row 1 knobs
    constexpr int kKnobW = 92;
    constexpr int kKnobH = 92;
    biteKnob_->setBounds(38, 99, kKnobW, kKnobH);
    warmthKnob_->setBounds(150, 99, kKnobW, kKnobH);

    cutoffKnob_->setBounds(298, 99, kKnobW, kKnobH);
    resKnob_->setBounds(410, 99, kKnobW, kKnobH);

    // Row 2 controls
    compAmtKnob_->setBounds(38, 245, kKnobW, kKnobH);
    slamBtn_->setBounds(148, 260, 96, 58);

    mixKnob_->setBounds(298, 245, kKnobW, kKnobH);
    volumeKnob_->setBounds(410, 245, kKnobW, kKnobH);

    // Bottom meter bar
    meterStripBounds_ = {16, 352, 508, 28};
}
