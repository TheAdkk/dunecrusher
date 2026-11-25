#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"
#include "PluginEditor.h"

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

    // Verify hosted editor instantiation & teardown
    if (std::unique_ptr<juce::AudioProcessorEditor> ed(instance->createEditorAndMakeActive()); ed != nullptr) {
        std::printf("Hosted editor bounds: %dx%d\n", ed->getWidth(), ed->getHeight());
    } else {
        std::fprintf(stderr, "FAIL: Hosted editor returned null\n");
        return false;
    }

    return true;
}

bool renderScreenshot(const juce::String& outPath) {
    DuneCrusherProcessor proc;
    proc.prepareToPlay(44100.0, 512);
    proc.applyPreset(1, false); // "Desert Grit" — all stages + SLAM active

    // Drive real audio through DSP so meters show authentic signal & gain reduction
    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;
    for (int blk = 0; blk < 12; ++blk) {
        for (int s = 0; s < 512; ++s) {
            const float t = static_cast<float>(blk * 512 + s) / 44100.0f;
            const float sigL = 0.42f * std::sin(2.0f * 3.14159265f * 110.0f * t)
                             + 0.18f * std::sin(2.0f * 3.14159265f * 330.0f * t);
            const float sigR = 0.36f * std::sin(2.0f * 3.14159265f * 110.0f * t + 0.3f)
                             + 0.22f * std::sin(2.0f * 3.14159265f * 440.0f * t);
            buf.setSample(0, s, sigL);
            buf.setSample(1, s, sigR);
        }
        proc.processBlock(buf, midi);
    }

    std::unique_ptr<DuneCrusherEditor> editor(static_cast<DuneCrusherEditor*>(proc.createEditor()));
    if (!editor) return false;
    editor->syncVisualStateNow();

    juce::Image img(juce::Image::ARGB, editor->getWidth() * 2, editor->getHeight() * 2, true);
    {
        juce::Graphics g(img);
        g.addTransform(juce::AffineTransform::scale(2.0f));
        editor->paintEntireComponent(g, true);
    }
    juce::File outFile(outPath);
    outFile.getParentDirectory().createDirectory();
    outFile.deleteFile();
    juce::FileOutputStream fos(outFile);
    if (!fos.openedOk()) {
        std::fprintf(stderr, "FAIL: Could not open screenshot path: %s\n", outPath.toRawUTF8());
        return false;
    }

    juce::PNGImageFormat png;
    if (!png.writeImageToStream(img, fos)) {
        std::fprintf(stderr, "FAIL: PNG write failed\n");
        return false;
    }

    std::printf("Saved 2x Retina UI screenshot (%dx%d) to %s\n",
                img.getWidth(), img.getHeight(), outPath.toRawUTF8());
    return true;
}

} // namespace

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI guiInit;

    if (argc < 2) {
        std::fprintf(stderr, "Usage: dunecrusher_verify <path/to/DuneCrusher.vst3> [screenshot.png]\n");
        return 2;
    }

    if (!verifyHostedVST3(argv[1])) {
        return 1;
    }

    if (argc >= 3) {
        if (!renderScreenshot(argv[2])) {
            return 1;
        }
    }

    std::printf("DUNECUSHER VST3 RUNTIME VERIFICATION PASSED\n");
    return 0;
}
