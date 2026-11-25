#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

using namespace dunecrusher;

namespace {

constexpr std::array<const char*, kNumParams> kParamIds = {
    "DriveOn",
    "DriveBite",
    "DriveWarmth",
    "FilterOn",
    "FilterCutoff",
    "FilterRes",
    "CompOn",
    "CompAmount",
    "CompSlam",
    "MasterOn",
    "MasterMix",
    "MasterVolume"
};

constexpr std::array<const char*, kNumParams> kParamNames = {
    "Drive On",
    "Drive Bite",
    "Drive Warmth",
    "Filter On",
    "Filter Cutoff",
    "Filter Res",
    "Comp On",
    "Comp Amount",
    "Comp Slam",
    "Master On",
    "Master Mix",
    "Master Volume"
};

constexpr std::array<float, kNumParams> kInitDefaults = {
    0.0f, 0.0f, 0.0f,
    0.0f, 0.5f, 0.0f,
    0.0f, 0.0f, 0.0f,
    1.0f, 1.0f, 1.0f
};

} // namespace

const char* DuneCrusherProcessor::getParamIdString(int idx) noexcept {
    if (idx < 0 || idx >= kNumParams) return "";
    return kParamIds[static_cast<size_t>(idx)];
}

float DuneCrusherProcessor::getInitDefaultParam(int idx) noexcept {
    if (idx < 0 || idx >= kNumParams) return 0.0f;
    return kInitDefaults[static_cast<size_t>(idx)];
}

juce::AudioProcessorValueTreeState::ParameterLayout DuneCrusherProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.reserve(kNumParams);

    const auto defaults = getFactoryPresets()[0].params;

    auto addBool = [&](int idx) {
        auto attrs = juce::AudioParameterBoolAttributes{}
                         .withStringFromValueFunction([idx](bool v, int maxLen) {
                             auto s = juce::String(formatParamDisplay(idx, v ? 1.0f : 0.0f));
                             return maxLen > 0 ? s.substring(0, maxLen) : s;
                         })
                         .withValueFromStringFunction([idx](const juce::String& text) {
                             const auto lower = text.trim().toLowerCase();
                             if (idx == kCompSlam) {
                                 return lower == "slam" || lower == "on" || lower == "1" || lower == "true";
                             }
                             return lower == "on" || lower == "1" || lower == "true";
                         });
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{kParamIds[static_cast<size_t>(idx)], 1},
            kParamNames[static_cast<size_t>(idx)],
            defaults[static_cast<size_t>(idx)] >= 0.5f,
            attrs));
    };

    auto addFloat = [&](int idx) {
        auto attrs = juce::AudioParameterFloatAttributes{}
                         .withStringFromValueFunction([idx](float v, int maxLen) {
                             auto s = juce::String(formatParamDisplay(idx, v));
                             return maxLen > 0 ? s.substring(0, maxLen) : s;
                         })
                         .withValueFromStringFunction([idx](const juce::String& text) {
                             return parseParamFromString(idx, text.toStdString());
                         });
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{kParamIds[static_cast<size_t>(idx)], 1},
            kParamNames[static_cast<size_t>(idx)],
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f),
            defaults[static_cast<size_t>(idx)],
            attrs));
    };

    addBool(kDriveOn);
    addFloat(kDriveBite);
    addFloat(kDriveWarmth);
    addBool(kFilterOn);
    addFloat(kFilterCutoff);
    addFloat(kFilterRes);
    addBool(kCompOn);
    addFloat(kCompAmount);
    addBool(kCompSlam);
    addBool(kMasterOn);
    addFloat(kMasterMix);
    addFloat(kMasterVolume);

    return {params.begin(), params.end()};
}

DuneCrusherProcessor::DuneCrusherProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "DuneCrusherState", createParameterLayout()),
      presets_(getFactoryPresets()) {
    for (int i = 0; i < kNumParams; ++i) {
        paramAtomics_[static_cast<size_t>(i)] = apvts_.getRawParameterValue(kParamIds[static_cast<size_t>(i)]);
    }
    currentProgram_.store(0, std::memory_order_relaxed);
    pendingReset_.store(true, std::memory_order_release);
}

void DuneCrusherProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    dsp_.setSampleRate(static_cast<float>(sampleRate));
    syncParametersToDSP();
    dsp_.reset();
    pendingReset_.store(false, std::memory_order_release);
}

void DuneCrusherProcessor::releaseResources() {}

void DuneCrusherProcessor::reset() {
    pendingReset_.store(true, std::memory_order_release);
}

bool DuneCrusherProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto& out = layouts.getMainOutputChannelSet();
    const auto& in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) {
        return false;
    }
    return in == out;
}

void DuneCrusherProcessor::syncParametersToDSP() noexcept {
    for (int i = 0; i < kNumParams; ++i) {
        if (auto* p = paramAtomics_[static_cast<size_t>(i)]) {
            dsp_.setParameter(i, p->load(std::memory_order_relaxed));
        }
    }
}

void DuneCrusherProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int bufCh = buffer.getNumChannels();
    const int numInputCh = getTotalNumInputChannels();
    const int numOutputCh = std::min(getTotalNumOutputChannels(), bufCh);

    for (int ch = numInputCh; ch < numOutputCh; ++ch) {
        buffer.clear(ch, 0, numSamples);
    }
    if (numSamples <= 0 || bufCh <= 0) return;

    syncParametersToDSP();
    if (pendingReset_.exchange(false, std::memory_order_acq_rel)) {
        dsp_.reset();
    }

    if (bufCh == 1) {
        float* chL = buffer.getWritePointer(0);
        dsp_.process(chL, nullptr, chL, nullptr, numSamples);
    } else {
        float* chL = buffer.getWritePointer(0);
        float* chR = buffer.getWritePointer(1);
        dsp_.process(chL, chR, chL, chR, numSamples);
    }

    const auto m = dsp_.getLastBlockMeters();
    meterPeakL_.store(m.outPeakL, std::memory_order_relaxed);
    meterPeakR_.store(m.outPeakR, std::memory_order_relaxed);
    meterGr_.store(m.gainReduction, std::memory_order_relaxed);
}

void DuneCrusherProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    pendingReset_.store(true, std::memory_order_release);
    meterPeakL_.store(0.0f, std::memory_order_relaxed);
    meterPeakR_.store(0.0f, std::memory_order_relaxed);
    meterGr_.store(0.0f, std::memory_order_relaxed);
    AudioProcessor::processBlockBypassed(buffer, midiMessages);
}

juce::AudioProcessorEditor* DuneCrusherProcessor::createEditor() {
    return new DuneCrusherEditor(*this);
}

int DuneCrusherProcessor::getNumPrograms() {
    const juce::ScopedLock sl(presetsLock_);
    return static_cast<int>(presets_.size());
}

int DuneCrusherProcessor::getCurrentProgram() {
    return currentProgram_.load(std::memory_order_relaxed);
}

void DuneCrusherProcessor::setCurrentProgram(int index) {
    applyPreset(index, false);
}

const juce::String DuneCrusherProcessor::getProgramName(int index) {
    const juce::ScopedLock sl(presetsLock_);
    if (index >= 0 && index < static_cast<int>(presets_.size())) {
        return juce::String(presets_[static_cast<size_t>(index)].name);
    }
    return "Init (Clean)";
}

void DuneCrusherProcessor::changeProgramName(int index, const juce::String& newName) {
    const juce::ScopedLock sl(presetsLock_);
    if (index >= 0 && index < static_cast<int>(presets_.size())) {
        presets_[static_cast<size_t>(index)].name = newName.toStdString();
        displayLabelVersion_.fetch_add(1, std::memory_order_release);
    }
}

juce::String DuneCrusherProcessor::getDisplayPresetLabel() const {
    const juce::ScopedLock sl(presetsLock_);
    if (customDisplayLabel_.isNotEmpty()) return customDisplayLabel_;
    const int idx = currentProgram_.load(std::memory_order_relaxed);
    if (idx >= 0 && idx < static_cast<int>(presets_.size())) {
        return juce::String(presets_[static_cast<size_t>(idx)].name);
    }
    return "Init (Clean)";
}

void DuneCrusherProcessor::setCustomDisplayLabel(const juce::String& label) {
    {
        const juce::ScopedLock sl(presetsLock_);
        customDisplayLabel_ = label;
    }
    displayLabelVersion_.fetch_add(1, std::memory_order_release);
}

void DuneCrusherProcessor::applyPreset(int index, bool notifyHost) {
    Preset pr;
    int clamped = 0;
    {
        const juce::ScopedLock sl(presetsLock_);
        if (presets_.empty()) return;
        clamped = std::clamp(index, 0, static_cast<int>(presets_.size()) - 1);
        pr = presets_[static_cast<size_t>(clamped)];
        customDisplayLabel_.clear();
    }

    currentProgram_.store(clamped, std::memory_order_relaxed);
    displayLabelVersion_.fetch_add(1, std::memory_order_release);

    for (int i = 0; i < kNumParams; ++i) {
        if (auto* param = apvts_.getParameter(kParamIds[static_cast<size_t>(i)])) {
            if (notifyHost) param->beginChangeGesture();
            param->setValueNotifyingHost(pr.params[static_cast<size_t>(i)]);
            if (notifyHost) param->endChangeGesture();
        }
    }

    if (notifyHost) {
        updateHostDisplay(ChangeDetails{}.withProgramChanged(true));
    }
}

void DuneCrusherProcessor::resetToInitPreset() {
    for (int i = 0; i < kNumParams; ++i) {
        if (auto* param = apvts_.getParameter(kParamIds[static_cast<size_t>(i)])) {
            param->beginChangeGesture();
            param->setValueNotifyingHost(kInitDefaults[static_cast<size_t>(i)]);
            param->endChangeGesture();
        }
    }
    setCustomDisplayLabel("Init (Clean)");
}

void DuneCrusherProcessor::randomizeParameters() {
    std::uniform_real_distribution<float> uni(0.0f, 1.0f);
    auto randGauss = [&](float mean, float sigma) {
        std::normal_distribution<float> dist(mean, sigma);
        return std::clamp(dist(rng_), 0.0f, 1.0f);
    };
    auto setP = [&](int idx, float val) {
        if (auto* param = apvts_.getParameter(kParamIds[static_cast<size_t>(idx)])) {
            param->beginChangeGesture();
            param->setValueNotifyingHost(std::clamp(val, 0.0f, 1.0f));
            param->endChangeGesture();
        }
    };
    auto isEnabled = [&](int idx) {
        auto* p = paramAtomics_[static_cast<size_t>(idx)];
        return p != nullptr && p->load(std::memory_order_relaxed) > 0.5f;
    };

    if (isEnabled(kDriveOn)) {
        setP(kDriveBite, randGauss(0.0f, 0.6f));
        setP(kDriveWarmth, randGauss(0.0f, 1.0f));
    }
    if (isEnabled(kFilterOn)) {
        setP(kFilterCutoff, uni(rng_));
        setP(kFilterRes, uni(rng_));
    }
    if (isEnabled(kCompOn)) {
        setP(kCompAmount, uni(rng_));
        setP(kCompSlam, uni(rng_) < 0.5f ? 1.0f : 0.0f);
    }
}

bool DuneCrusherProcessor::saveCurrentPresetToXmlFile(const juce::File& file) {
    auto state = apvts_.copyState();
    state.setProperty("presetName", file.getFileNameWithoutExtension(), nullptr);
    if (auto xml = state.createXml()) {
        return xml->writeTo(file);
    }
    return false;
}

bool DuneCrusherProcessor::loadPresetFromXmlFile(const juce::File& file) {
    if (auto xml = juce::XmlDocument::parse(file)) {
        if (xml->hasTagName(apvts_.state.getType())) {
            apvts_.replaceState(juce::ValueTree::fromXml(*xml));
            setCustomDisplayLabel(file.getFileNameWithoutExtension());
            return true;
        }
    }
    return false;
}

void DuneCrusherProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts_.copyState();
    state.setProperty("currentProgram", currentProgram_.load(std::memory_order_relaxed), nullptr);
    {
        const juce::ScopedLock sl(presetsLock_);
        if (customDisplayLabel_.isNotEmpty()) {
            state.setProperty("customDisplayLabel", customDisplayLabel_, nullptr);
        }
    }
    if (auto xml = state.createXml()) {
        copyXmlToBinary(*xml, destData);
    }
}

void DuneCrusherProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes)) {
        if (xml->hasTagName(apvts_.state.getType())) {
            auto tree = juce::ValueTree::fromXml(*xml);
            const int prog = static_cast<int>(tree.getProperty("currentProgram", 0));
            const juce::String customLabel = tree.getProperty("customDisplayLabel", "").toString();
            {
                const juce::ScopedLock sl(presetsLock_);
                const int maxProg = std::max(0, static_cast<int>(presets_.size()) - 1);
                currentProgram_.store(std::clamp(prog, 0, maxProg), std::memory_order_relaxed);
                customDisplayLabel_ = customLabel;
            }
            apvts_.replaceState(tree);
            displayLabelVersion_.fetch_add(1, std::memory_order_release);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new DuneCrusherProcessor();
}
