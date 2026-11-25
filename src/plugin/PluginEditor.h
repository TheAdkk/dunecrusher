#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

#include <array>
#include <functional>
#include <memory>

class DuneCrusherEditor final : public juce::AudioProcessorEditor,
                                private juce::Timer {
public:
    explicit DuneCrusherEditor(DuneCrusherProcessor& proc);
    ~DuneCrusherEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Refreshes meter ballistics and module visual states immediately (useful for headless snapshots)
    void syncVisualStateNow();

private:
    class RotaryKnob;
    class PowerLedButton;
    class SlamToggleButton;

    void timerCallback() override;
    void showTransientReadout(int paramId, float normalizedValue);
    void refreshPresetDisplay();
    void openPresetMenu();
    void updateModuleDimming();

    DuneCrusherProcessor& processor_;

    std::unique_ptr<PowerLedButton> drivePowerBtn_;
    std::unique_ptr<PowerLedButton> filterPowerBtn_;
    std::unique_ptr<PowerLedButton> compPowerBtn_;
    std::unique_ptr<PowerLedButton> masterPowerBtn_;

    std::unique_ptr<RotaryKnob> biteKnob_;
    std::unique_ptr<RotaryKnob> warmthKnob_;
    std::unique_ptr<RotaryKnob> cutoffKnob_;
    std::unique_ptr<RotaryKnob> resKnob_;
    std::unique_ptr<RotaryKnob> compAmtKnob_;
    std::unique_ptr<RotaryKnob> mixKnob_;
    std::unique_ptr<RotaryKnob> volumeKnob_;

    std::unique_ptr<SlamToggleButton> slamBtn_;

    juce::TextButton prevPresetBtn_{"<"};
    juce::TextButton nextPresetBtn_{">"};
    juce::TextButton presetDisplayBtn_;
    juce::TextButton randomBtn_{"RND"};

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<ButtonAttachment> drivePowerAtt_;
    std::unique_ptr<ButtonAttachment> filterPowerAtt_;
    std::unique_ptr<ButtonAttachment> compPowerAtt_;
    std::unique_ptr<ButtonAttachment> masterPowerAtt_;
    std::unique_ptr<ButtonAttachment> slamAtt_;

    std::unique_ptr<SliderAttachment> biteAtt_;
    std::unique_ptr<SliderAttachment> warmthAtt_;
    std::unique_ptr<SliderAttachment> cutoffAtt_;
    std::unique_ptr<SliderAttachment> resAtt_;
    std::unique_ptr<SliderAttachment> compAmtAtt_;
    std::unique_ptr<SliderAttachment> mixAtt_;
    std::unique_ptr<SliderAttachment> volumeAtt_;

    std::unique_ptr<juce::FileChooser> fileChooser_;

    juce::String transientText_;
    juce::int64 transientExpiryMs_ = 0;
    uint64_t lastSeenLabelVersion_ = 0;

    float displayPeakL_ = 0.0f;
    float displayPeakR_ = 0.0f;
    float displayGr_ = 0.0f;

    juce::Rectangle<int> meterStripBounds_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuneCrusherEditor)
};
