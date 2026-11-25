#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

#include <memory>
#include <optional>

class DuneCrusherEditor final : public juce::AudioProcessorEditor,
                                private juce::Timer {
public:
    explicit DuneCrusherEditor(DuneCrusherProcessor& proc);
    ~DuneCrusherEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct SinglePageBrowser final : juce::WebBrowserComponent {
        using juce::WebBrowserComponent::WebBrowserComponent;
        bool pageAboutToLoad(const juce::String& newURL) override;
    };

    void timerCallback() override;
    std::optional<juce::WebBrowserComponent::Resource> provideResource(const juce::String& url);
    juce::var buildPresetStateVar() const;
    void emitPresetState();
    void launchLoadPresetDialog();
    void launchSavePresetDialog();

    DuneCrusherProcessor& processor_;

    // Relays MUST be declared before webView_ and parameter attachments
    juce::WebToggleButtonRelay driveOnRelay_{"DriveOn"};
    juce::WebSliderRelay driveBiteRelay_{"DriveBite"};
    juce::WebSliderRelay driveWarmthRelay_{"DriveWarmth"};
    juce::WebToggleButtonRelay filterOnRelay_{"FilterOn"};
    juce::WebSliderRelay filterCutoffRelay_{"FilterCutoff"};
    juce::WebSliderRelay filterResRelay_{"FilterRes"};
    juce::WebToggleButtonRelay compOnRelay_{"CompOn"};
    juce::WebSliderRelay compAmountRelay_{"CompAmount"};
    juce::WebToggleButtonRelay compSlamRelay_{"CompSlam"};
    juce::WebToggleButtonRelay masterOnRelay_{"MasterOn"};
    juce::WebSliderRelay masterMixRelay_{"MasterMix"};
    juce::WebSliderRelay masterVolumeRelay_{"MasterVolume"};

    SinglePageBrowser webView_;

    // Attachments MUST be declared after relays & webView_ so they detach first on destruction
    juce::WebToggleButtonParameterAttachment driveOnAtt_;
    juce::WebSliderParameterAttachment driveBiteAtt_;
    juce::WebSliderParameterAttachment driveWarmthAtt_;
    juce::WebToggleButtonParameterAttachment filterOnAtt_;
    juce::WebSliderParameterAttachment filterCutoffAtt_;
    juce::WebSliderParameterAttachment filterResAtt_;
    juce::WebToggleButtonParameterAttachment compOnAtt_;
    juce::WebSliderParameterAttachment compAmountAtt_;
    juce::WebToggleButtonParameterAttachment compSlamAtt_;
    juce::WebToggleButtonParameterAttachment masterOnAtt_;
    juce::WebSliderParameterAttachment masterMixAtt_;
    juce::WebSliderParameterAttachment masterVolumeAtt_;

    std::unique_ptr<juce::FileChooser> fileChooser_;
    uint64_t lastSeenLabelVersion_ = 0;
    bool lastMetersNonZero_ = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuneCrusherEditor)
};
