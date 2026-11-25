#include "PluginEditor.h"
#include "DuneCrusherWebUI.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <vector>

using namespace dunecrusher;

namespace {

constexpr int kEditorWidth = 640;
constexpr int kEditorHeight = 492;

juce::RangedAudioParameter& requireParam(DuneCrusherProcessor& proc, int idx) {
    auto* p = proc.getAPVTS().getParameter(DuneCrusherProcessor::getParamIdString(idx));
    jassert(p != nullptr);
    return *p;
}

} // namespace

bool DuneCrusherEditor::SinglePageBrowser::pageAboutToLoad(const juce::String& newURL) {
    return newURL.startsWith(getResourceProviderRoot());
}

DuneCrusherEditor::DuneCrusherEditor(DuneCrusherProcessor& proc)
    : AudioProcessorEditor(&proc),
      processor_(proc),
      webView_(juce::WebBrowserComponent::Options{}
                   .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
                   .withWinWebView2Options(
                       juce::WebBrowserComponent::Options::WinWebView2{}
                           .withUserDataFolder(juce::File::getSpecialLocation(
                               juce::File::SpecialLocationType::tempDirectory)))
                   .withNativeIntegrationEnabled()
                   .withOptionsFrom(driveOnRelay_)
                   .withOptionsFrom(driveBiteRelay_)
                   .withOptionsFrom(driveWarmthRelay_)
                   .withOptionsFrom(filterOnRelay_)
                   .withOptionsFrom(filterCutoffRelay_)
                   .withOptionsFrom(filterResRelay_)
                   .withOptionsFrom(compOnRelay_)
                   .withOptionsFrom(compAmountRelay_)
                   .withOptionsFrom(compSlamRelay_)
                   .withOptionsFrom(masterOnRelay_)
                   .withOptionsFrom(masterMixRelay_)
                   .withOptionsFrom(masterVolumeRelay_)
                   .withNativeFunction("getPresetState",
                                       [this](const juce::Array<juce::var>&,
                                              juce::WebBrowserComponent::NativeFunctionCompletion complete) {
                                           complete(buildPresetStateVar());
                                       })
                   .withNativeFunction("selectPreset",
                                       [this](const juce::Array<juce::var>& args,
                                              juce::WebBrowserComponent::NativeFunctionCompletion complete) {
                                           if (args.size() >= 1) {
                                               processor_.applyPreset(static_cast<int>(args[0]), true);
                                               emitPresetState();
                                           }
                                           complete({});
                                       })
                   .withNativeFunction("stepPreset",
                                       [this](const juce::Array<juce::var>& args,
                                              juce::WebBrowserComponent::NativeFunctionCompletion complete) {
                                           const int count = processor_.getNumPrograms();
                                           if (count > 0 && args.size() >= 1) {
                                               const int delta = static_cast<int>(args[0]);
                                               const int next = (processor_.getCurrentProgram() + delta % count + count) % count;
                                               processor_.applyPreset(next, true);
                                               emitPresetState();
                                           }
                                           complete({});
                                       })
                   .withNativeFunction("randomize",
                                       [this](const juce::Array<juce::var>&,
                                              juce::WebBrowserComponent::NativeFunctionCompletion complete) {
                                           processor_.randomizeParameters();
                                           emitPresetState();
                                           complete({});
                                       })
                   .withNativeFunction("initPreset",
                                       [this](const juce::Array<juce::var>&,
                                              juce::WebBrowserComponent::NativeFunctionCompletion complete) {
                                           processor_.resetToInitPreset();
                                           emitPresetState();
                                           complete({});
                                       })
                   .withNativeFunction("loadPresetFile",
                                       [this](const juce::Array<juce::var>&,
                                              juce::WebBrowserComponent::NativeFunctionCompletion complete) {
                                           launchLoadPresetDialog();
                                           complete({});
                                       })
                   .withNativeFunction("savePresetFile",
                                       [this](const juce::Array<juce::var>&,
                                              juce::WebBrowserComponent::NativeFunctionCompletion complete) {
                                           launchSavePresetDialog();
                                           complete({});
                                       })
                   .withResourceProvider([this](const juce::String& url) { return provideResource(url); })),
      driveOnAtt_(requireParam(proc, kDriveOn), driveOnRelay_, nullptr),
      driveBiteAtt_(requireParam(proc, kDriveBite), driveBiteRelay_, nullptr),
      driveWarmthAtt_(requireParam(proc, kDriveWarmth), driveWarmthRelay_, nullptr),
      filterOnAtt_(requireParam(proc, kFilterOn), filterOnRelay_, nullptr),
      filterCutoffAtt_(requireParam(proc, kFilterCutoff), filterCutoffRelay_, nullptr),
      filterResAtt_(requireParam(proc, kFilterRes), filterResRelay_, nullptr),
      compOnAtt_(requireParam(proc, kCompOn), compOnRelay_, nullptr),
      compAmountAtt_(requireParam(proc, kCompAmount), compAmountRelay_, nullptr),
      compSlamAtt_(requireParam(proc, kCompSlam), compSlamRelay_, nullptr),
      masterOnAtt_(requireParam(proc, kMasterOn), masterOnRelay_, nullptr),
      masterMixAtt_(requireParam(proc, kMasterMix), masterMixRelay_, nullptr),
      masterVolumeAtt_(requireParam(proc, kMasterVolume), masterVolumeRelay_, nullptr) {
    addAndMakeVisible(webView_);
    webView_.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

    lastSeenLabelVersion_ = processor_.getDisplayLabelVersion();
    setResizable(false, false);
    setSize(kEditorWidth, kEditorHeight);
    startTimerHz(30);
}

DuneCrusherEditor::~DuneCrusherEditor() {
    stopTimer();
}

std::optional<juce::WebBrowserComponent::Resource> DuneCrusherEditor::provideResource(const juce::String& url) {
    const auto path = (url == "/" || url.isEmpty()) ? juce::String("index.html")
                                                    : url.fromFirstOccurrenceOf("/", false, false)
                                                          .upToFirstOccurrenceOf("?", false, false);
    if (path == "index.html") {
        const auto size = static_cast<size_t>(dunecrusher_webui::index_htmlSize);
        std::vector<std::byte> bytes(size);
        std::memcpy(bytes.data(), dunecrusher_webui::index_html, size);
        return juce::WebBrowserComponent::Resource{std::move(bytes), "text/html; charset=utf-8"};
    }
    return std::nullopt;
}

juce::var DuneCrusherEditor::buildPresetStateVar() const {
    auto* root = new juce::DynamicObject();
    root->setProperty("currentProgram", processor_.getCurrentProgram());
    root->setProperty("displayName", processor_.getDisplayPresetLabel());
    root->setProperty("displayCategory", processor_.getDisplayPresetCategory());
    root->setProperty("isCustomLabel", processor_.hasCustomDisplayLabel());

    juce::Array<juce::var> list;
    const int count = processor_.getNumPrograms();
    for (int i = 0; i < count; ++i) {
        auto* item = new juce::DynamicObject();
        item->setProperty("name", processor_.getProgramName(i));
        item->setProperty("category", processor_.getProgramCategory(i));
        list.add(juce::var(item));
    }
    root->setProperty("presets", list);
    return juce::var(root);
}

void DuneCrusherEditor::emitPresetState() {
    lastSeenLabelVersion_ = processor_.getDisplayLabelVersion();
    webView_.emitEventIfBrowserIsVisible("presetState", buildPresetStateVar());
}

void DuneCrusherEditor::launchLoadPresetDialog() {
    juce::Component::SafePointer<DuneCrusherEditor> safeThis(this);
    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Load DuneCrusher Preset", juce::File(), "*.dcpreset;*.xml");
    fileChooser_->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis](const juce::FileChooser& fc) {
            if (safeThis == nullptr) return;
            auto f = fc.getResult();
            if (f.existsAsFile() && safeThis->processor_.loadPresetFromXmlFile(f)) {
                safeThis->emitPresetState();
            }
        });
}

void DuneCrusherEditor::launchSavePresetDialog() {
    juce::Component::SafePointer<DuneCrusherEditor> safeThis(this);
    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Save DuneCrusher Preset",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("MyPreset.dcpreset"),
        "*.dcpreset");
    fileChooser_->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis](const juce::FileChooser& fc) {
            if (safeThis == nullptr) return;
            auto f = fc.getResult();
            if (f != juce::File()) {
                safeThis->processor_.saveCurrentPresetToXmlFile(f);
            }
        });
}

void DuneCrusherEditor::timerCallback() {
    const uint64_t ver = processor_.getDisplayLabelVersion();
    if (ver != lastSeenLabelVersion_) {
        emitPresetState();
    }

    const float pL = std::clamp(processor_.getMeterPeakL(), 0.0f, 1.0f);
    const float pR = std::clamp(processor_.getMeterPeakR(), 0.0f, 1.0f);
    const float gr = std::clamp(processor_.getMeterGR(), 0.0f, 1.0f);
    const bool nonZero = (pL > 1e-4f || pR > 1e-4f || gr > 1e-4f);

    if (nonZero || lastMetersNonZero_) {
        lastMetersNonZero_ = nonZero;
        auto* m = new juce::DynamicObject();
        m->setProperty("peakL", pL);
        m->setProperty("peakR", pR);
        m->setProperty("gr", gr);
        webView_.emitEventIfBrowserIsVisible("meters", juce::var(m));
    }
}

void DuneCrusherEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFF0B0B0C));
}

void DuneCrusherEditor::resized() {
    webView_.setBounds(getLocalBounds());
}
