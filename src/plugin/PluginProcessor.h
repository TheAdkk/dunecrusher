#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DuneCrusherDSP.h"
#include "Presets.h"

#include <array>
#include <atomic>
#include <random>
#include <vector>

class DuneCrusherProcessor final : public juce::AudioProcessor {
public:
    DuneCrusherProcessor();
    ~DuneCrusherProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DuneCrusher"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts_; }

    void applyPreset(int index, bool notifyHost = false);
    void randomizeParameters();
    void resetToInitPreset();
    bool saveCurrentPresetToXmlFile(const juce::File& file);
    bool loadPresetFromXmlFile(const juce::File& file);

    juce::String getDisplayPresetLabel() const;
    juce::String getProgramCategory(int index) const;
    juce::String getDisplayPresetCategory() const;
    bool hasCustomDisplayLabel() const;
    uint64_t getDisplayLabelVersion() const noexcept { return displayLabelVersion_.load(std::memory_order_acquire); }

    float getMeterPeakL() const noexcept { return meterPeakL_.load(std::memory_order_relaxed); }
    float getMeterPeakR() const noexcept { return meterPeakR_.load(std::memory_order_relaxed); }
    float getMeterGR() const noexcept { return meterGr_.load(std::memory_order_relaxed); }

    static const char* getParamIdString(int idx) noexcept;
    static float getInitDefaultParam(int idx) noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void syncParametersToDSP() noexcept; // Audio thread / prepareToPlay only
    void setCustomDisplayLabel(const juce::String& label);

    dunecrusher::DuneCrusherDSP dsp_;
    juce::AudioProcessorValueTreeState apvts_;
    std::array<std::atomic<float>*, dunecrusher::kNumParams> paramAtomics_{};

    mutable juce::CriticalSection presetsLock_; // Never acquired on audio thread
    std::vector<dunecrusher::Preset> presets_;
    juce::String customDisplayLabel_;
    std::atomic<int> currentProgram_{0};
    std::atomic<uint64_t> displayLabelVersion_{0};
    std::atomic<bool> pendingReset_{true};

    std::atomic<float> meterPeakL_{0.0f};
    std::atomic<float> meterPeakR_{0.0f};
    std::atomic<float> meterGr_{0.0f};

    std::mt19937 rng_{std::random_device{}()};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuneCrusherProcessor)
};
