#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>

namespace {

bool verifyHostedVST3(const juce::String& vst3Path) {
    juce::AudioPluginFormatManager manager;
    manager.addFormat(std::make_unique<juce::VST3PluginFormat>());

    juce::OwnedArray<juce::PluginDescription> descs;
    for (int i = 0; i < manager.getNumFormats(); ++i) {
        auto* fmt = manager.getFormat(i);
        fmt->findAllTypesForFile(descs, vst3Path);
    }

    if (descs.isEmpty()) {
        std::fprintf(stderr, "FAIL: No VST3 plugin discovered at %s\n", vst3Path.toRawUTF8());
        return false;
    }

    const auto& desc = *descs[0];
    std::printf("Discovered VST3: name='%s' manufacturer='%s' inputs=%d outputs=%d\n",
                desc.name.toRawUTF8(), desc.manufacturerName.toRawUTF8(),
                desc.numInputChannels, desc.numOutputChannels);

    juce::String err;
    auto instance = manager.createPluginInstance(desc, 44100.0, 512, err);
    if (!instance) {
        std::fprintf(stderr, "FAIL: Could not instantiate VST3: %s\n", err.toRawUTF8());
        return false;
    }

    const int numParams = static_cast<int>(instance->getParameters().size());
    std::printf("Hosted VST3 params=%d programs=%d\n", numParams, instance->getNumPrograms());
    if (numParams < 12 || numParams > 20) {
        std::fprintf(stderr, "FAIL: Unexpected hosted VST3 parameter count: %d\n", numParams);
        return false;
    }

    const double sampleRates[] = {11025.0, 44100.0, 48000.0, 96000.0};
    for (double sr : sampleRates) {
        instance->prepareToPlay(sr, 256);
        juce::AudioBuffer<float> buf(2, 256);
        juce::MidiBuffer midi;
        for (int ch = 0; ch < 2; ++ch) {
            for (int s = 0; s < 256; ++s) {
                buf.setSample(ch, s, 0.35f * std::sin(0.05f * static_cast<float>(s)));
            }
        }
        buf.setSample(0, 12, std::numeric_limits<float>::quiet_NaN());
        buf.setSample(1, 13, std::numeric_limits<float>::infinity());

        instance->processBlock(buf, midi);
        for (int ch = 0; ch < 2; ++ch) {
            for (int s = 0; s < 256; ++s) {
                if (!std::isfinite(buf.getSample(ch, s))) {
                    std::fprintf(stderr, "FAIL: Non-finite output at %.0f Hz\n", sr);
                    return false;
                }
            }
        }
        instance->releaseResources();
    }

    // State save & restore round-trip
    juce::MemoryBlock saved;
    instance->getStateInformation(saved);
    if (saved.getSize() == 0) {
        std::fprintf(stderr, "FAIL: Empty state block\n");
        return false;
    }
    instance->setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));

    // Verify hosted JUCE 8 WebView editor instantiation & teardown
    if (std::unique_ptr<juce::AudioProcessorEditor> ed(instance->createEditorAndMakeActive()); ed != nullptr) {
        std::printf("Hosted WebView editor bounds: %dx%d\n", ed->getWidth(), ed->getHeight());
        if (ed->getWidth() != 640 || ed->getHeight() != 492) {
            std::fprintf(stderr, "FAIL: Unexpected editor bounds\n");
            return false;
        }
    } else {
        std::fprintf(stderr, "FAIL: Hosted editor returned null\n");
        return false;
    }

    return true;
}

} // namespace

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI guiInit;

    if (argc < 2) {
        std::fprintf(stderr, "Usage: dunecrusher_verify <path/to/DuneCrusher.vst3>\n");
        return 2;
    }

    if (!verifyHostedVST3(argv[1])) {
        return 1;
    }

    std::printf("DUNECUSHER VST3 + WEBVIEW RUNTIME VERIFICATION PASSED\n");
    return 0;
}
