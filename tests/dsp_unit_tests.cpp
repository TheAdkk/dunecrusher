#include "DuneCrusherDSP.h"
#include "Presets.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

using namespace dunecrusher;

namespace {

int gFailures = 0;

void expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++gFailures;
    }
}

float rms(const std::vector<float>& v) {
    double sum = 0.0;
    for (float x : v) sum += static_cast<double>(x) * static_cast<double>(x);
    return static_cast<float>(std::sqrt(sum / std::max<size_t>(1, v.size())));
}

std::vector<float> makeSine(int n, float freqHz, float sr, float amp = 0.5f) {
    std::vector<float> v(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        v[static_cast<size_t>(i)] = amp * std::sin(2.0f * 3.1415926535f * freqHz * static_cast<float>(i) / sr);
    }
    return v;
}

void testBypassBitExact() {
    DuneCrusherDSP dsp;
    dsp.setSampleRate(48000.0f);
    dsp.setParameter(kMasterOn, 0.0f);
    dsp.setParameter(kDriveOn, 1.0f);
    dsp.setParameter(kDriveBite, 1.0f);
    dsp.reset();

    auto inL = makeSine(512, 440.0f, 48000.0f, 0.6f);
    auto inR = makeSine(512, 880.0f, 48000.0f, 0.4f);
    std::vector<float> outL(512, 0.0f), outR(512, 0.0f);

    dsp.process(inL.data(), inR.data(), outL.data(), outR.data(), 512);
    float maxDiff = 0.0f;
    for (int i = 0; i < 512; ++i) {
        maxDiff = std::max(maxDiff, std::fabs(inL[static_cast<size_t>(i)] - outL[static_cast<size_t>(i)]));
        maxDiff = std::max(maxDiff, std::fabs(inR[static_cast<size_t>(i)] - outR[static_cast<size_t>(i)]));
    }
    expect(maxDiff == 0.0f, "Master bypass should pass clean signal bit-exact");
}

void testDriveStagesShapeAudio() {
    auto in = makeSine(1024, 220.0f, 44100.0f, 0.35f);
    std::vector<float> outBite(1024), outWarmth(1024);

    DuneCrusherDSP d1;
    d1.setSampleRate(44100.0f);
    d1.setParameter(kDriveOn, 1.0f);
    d1.setParameter(kDriveBite, 0.8f);
    d1.reset();
    d1.process(in.data(), nullptr, outBite.data(), nullptr, 1024);

    DuneCrusherDSP d2;
    d2.setSampleRate(44100.0f);
    d2.setParameter(kDriveOn, 1.0f);
    d2.setParameter(kDriveWarmth, 0.8f);
    d2.reset();
    d2.process(in.data(), nullptr, outWarmth.data(), nullptr, 1024);

    expect(rms(outBite) > rms(in) * 1.4f, "Bite stage should increase RMS energy");
    expect(rms(outWarmth) > rms(in) * 1.3f, "Warmth stage should increase RMS energy");
}

void testFilterAttenuatesHighFreq() {
    auto hi = makeSine(2048, 8000.0f, 44100.0f, 0.5f);
    auto lo = makeSine(2048, 120.0f, 44100.0f, 0.5f);
    std::vector<float> outHi(2048), outLo(2048);

    DuneCrusherDSP dHi;
    dHi.setSampleRate(44100.0f);
    dHi.setParameter(kFilterOn, 1.0f);
    dHi.setParameter(kFilterCutoff, 0.08f);
    dHi.setParameter(kFilterRes, 0.0f);
    dHi.reset();
    dHi.process(hi.data(), nullptr, outHi.data(), nullptr, 2048);

    DuneCrusherDSP dLo;
    dLo.setSampleRate(44100.0f);
    dLo.setParameter(kFilterOn, 1.0f);
    dLo.setParameter(kFilterCutoff, 0.08f);
    dLo.setParameter(kFilterRes, 0.0f);
    dLo.reset();
    dLo.process(lo.data(), nullptr, outLo.data(), nullptr, 2048);

    expect(rms(outHi) < rms(hi) * 0.1f, "Low-pass filter should strongly attenuate 8 kHz at low cutoff");
    expect(rms(outLo) > rms(lo) * 0.6f, "Low-pass filter should pass 120 Hz cleanly");
}

void testCompressorGlueAndSlamAndMeters() {
    auto in = makeSine(4096, 100.0f, 44100.0f, 0.45f);
    std::vector<float> outGlue(4096), outSlam(4096);

    DuneCrusherDSP dGlue;
    dGlue.setSampleRate(44100.0f);
    dGlue.setParameter(kCompOn, 1.0f);
    dGlue.setParameter(kCompAmount, 0.8f);
    dGlue.setParameter(kCompSlam, 0.0f);
    dGlue.reset();
    dGlue.process(in.data(), nullptr, outGlue.data(), nullptr, 4096);
    const auto mGlue = dGlue.getLastBlockMeters();

    DuneCrusherDSP dSlam;
    dSlam.setSampleRate(44100.0f);
    dSlam.setParameter(kCompOn, 1.0f);
    dSlam.setParameter(kCompAmount, 0.8f);
    dSlam.setParameter(kCompSlam, 1.0f);
    dSlam.reset();
    dSlam.process(in.data(), nullptr, outSlam.data(), nullptr, 4096);
    const auto mSlam = dSlam.getLastBlockMeters();

    expect(mGlue.gainReduction > 0.05f, "Glue mode should report measurable gain reduction");
    expect(mSlam.gainReduction > 0.05f, "Slam mode should report measurable gain reduction");
    expect(mSlam.outPeakL > 0.1f && mSlam.outPeakL <= 1.0f, "Output peak meter should stay within valid bounds");

    float diff = 0.0f;
    for (int i = 0; i < 4096; ++i) {
        diff = std::max(diff, std::fabs(outGlue[static_cast<size_t>(i)] - outSlam[static_cast<size_t>(i)]));
    }
    expect(diff > 0.01f, "Slam mode should behave distinctly from Glue mode");
}

void testExtremeSampleRatesAndNanRecovery() {
    const std::array<float, 5> rates = {11025.0f, 22050.0f, 44100.0f, 96000.0f, 192000.0f};
    for (float sr : rates) {
        DuneCrusherDSP dsp;
        dsp.setSampleRate(sr);
        dsp.setParameter(kDriveOn, 1.0f);
        dsp.setParameter(kDriveBite, 0.9f);
        dsp.setParameter(kDriveWarmth, 0.9f);
        dsp.setParameter(kFilterOn, 1.0f);
        dsp.setParameter(kFilterCutoff, 1.0f);
        dsp.setParameter(kFilterRes, 1.0f);
        dsp.setParameter(kCompOn, 1.0f);
        dsp.setParameter(kCompAmount, 0.95f);
        dsp.setParameter(kCompSlam, 1.0f);
        dsp.reset();

        std::vector<float> bufL(512, 0.25f), bufR(512, -0.25f);
        bufL[10] = std::numeric_limits<float>::quiet_NaN();
        bufR[11] = std::numeric_limits<float>::infinity();
        bufL[12] = -std::numeric_limits<float>::infinity();

        dsp.process(bufL.data(), bufR.data(), bufL.data(), bufR.data(), 512);
        bool allFinite = true;
        for (int i = 0; i < 512; ++i) {
            if (!std::isfinite(bufL[static_cast<size_t>(i)]) || !std::isfinite(bufR[static_cast<size_t>(i)])) {
                allFinite = false;
                break;
            }
        }
        expect(allFinite, "DSP must sanitize NaN/Inf and remain finite across 11-192 kHz");

        // Ensure filter did not latch to silence permanently after NaN
        auto recovery = makeSine(512, 400.0f, sr, 0.4f);
        std::vector<float> recOutL(512), recOutR(512);
        dsp.process(recovery.data(), recovery.data(), recOutL.data(), recOutR.data(), 512);
        expect(rms(recOutL) > 0.05f, "DSP must recover cleanly after NaN/Inf input");
    }
}

void testPresetsAndFormatting() {
    const auto presets = getFactoryPresets();
    expect(presets.size() == 16, "Factory bank should contain 16 original presets");
    for (const auto& p : presets) {
        expect(!p.name.empty(), "Preset name must not be empty");
        for (float v : p.params) {
            expect(std::isfinite(v) && v >= 0.0f && v <= 1.0f, "Preset parameter must be within [0, 1]");
        }
    }

    const float parsedCut = parseParamFromString(kFilterCutoff, formatParamDisplay(kFilterCutoff, 0.65f));
    expect(std::fabs(parsedCut - 0.65f) < 0.03f, "Cutoff format/parse round-trip should be close");

    const float parsedVol = parseParamFromString(kMasterVolume, formatParamDisplay(kMasterVolume, 0.75f));
    expect(std::fabs(parsedVol - 0.75f) < 0.03f, "Volume format/parse round-trip should be close");
}

} // namespace

int main() {
    testBypassBitExact();
    testDriveStagesShapeAudio();
    testFilterAttenuatesHighFreq();
    testCompressorGlueAndSlamAndMeters();
    testExtremeSampleRatesAndNanRecovery();
    testPresetsAndFormatting();

    if (gFailures == 0) {
        std::printf("ALL DUNECUSHER DSP UNIT TESTS PASSED (6/6 suites)\n");
        return 0;
    }
    std::fprintf(stderr, "DSP UNIT TESTS FAILED (%d failures)\n", gFailures);
    return 1;
}
