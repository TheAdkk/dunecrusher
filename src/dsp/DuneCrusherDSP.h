#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace dunecrusher {

enum ParamId : int {
    kDriveOn = 0,
    kDriveBite = 1,
    kDriveWarmth = 2,
    kFilterOn = 3,
    kFilterCutoff = 4,
    kFilterRes = 5,
    kCompOn = 6,
    kCompAmount = 7,
    kCompSlam = 8,
    kMasterOn = 9,
    kMasterMix = 10,
    kMasterVolume = 11,
    kNumParams = 12
};

struct MeterSnapshot {
    float outPeakL = 0.0f;
    float outPeakR = 0.0f;
    float gainReduction = 0.0f; // 0.0 = no reduction, 1.0 = max reduction
};

inline bool bitsEqual(float a, float b) noexcept {
    uint32_t ua = 0, ub = 0;
    std::memcpy(&ua, &a, sizeof(ua));
    std::memcpy(&ub, &b, sizeof(ub));
    return ua == ub;
}

inline float flushDenormal(float v) noexcept {
    uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    if ((bits & 0x7f800000u) == 0u && (bits & 0x7fffffffu) != 0u) {
        return 0.0f;
    }
    return v;
}

inline bool isSubnormal(float v) noexcept {
    uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    return (bits & 0x7f800000u) == 0u && (bits & 0x7fffffffu) != 0u;
}

inline float sanitizeSample(float s) noexcept {
    return std::isfinite(s) ? s : 0.0f;
}

// 10 ms linear parameter smoother (lock-free, allocation-free)
class LinearParamSmoother {
public:
    LinearParamSmoother(float minVal = 0.0f, float maxVal = 1.0f, bool continuous = true) noexcept
        : minVal_(minVal), maxVal_(maxVal), continuous_(continuous) {}

    void setSampleRate(float sr) noexcept {
        sampleRate_ = (std::isfinite(sr) && sr > 0.0f) ? sr : 44100.0f;
        rampSamples_ = std::max(1, static_cast<int>(sampleRate_ * rampSec_));
    }

    void setNormalized(float norm) noexcept {
        if (!std::isfinite(norm)) return;
        float mapped = std::clamp(norm, 0.0f, 1.0f);
        if (!bitsEqual(minVal_, 0.0f) || !bitsEqual(maxVal_, 1.0f)) {
            mapped = minVal_ + mapped * (maxVal_ - minVal_);
        }
        if (!continuous_) {
            mapped = static_cast<float>(static_cast<int>(mapped + 0.5f));
        }
        if (bitsEqual(mapped, target_)) return;
        target_ = mapped;
        elapsed_ = 0;
        start_ = current_;
    }

    float getNormalized() const noexcept {
        if (bitsEqual(minVal_, 0.0f) && bitsEqual(maxVal_, 1.0f)) return target_;
        return (target_ - minVal_) / (maxVal_ - minVal_);
    }

    float getTarget() const noexcept { return target_; }

    void snapToTarget() noexcept {
        current_ = target_;
        start_ = 0.0f;
        elapsed_ = 0;
    }

    float advance(int numSamples) noexcept {
        if (!bitsEqual(current_, target_)) {
            elapsed_ += numSamples;
            const float frac = static_cast<float>(elapsed_) / static_cast<float>(rampSamples_);
            if (frac >= 1.0f) {
                current_ = target_;
            } else {
                current_ = start_ * (1.0f - frac) + target_ * frac;
            }
        }
        return current_;
    }

private:
    float minVal_ = 0.0f;
    float maxVal_ = 1.0f;
    bool continuous_ = true;
    float current_ = 0.0f;
    float target_ = 0.0f;
    float start_ = 0.0f;
    float rampSec_ = 0.01f;
    float sampleRate_ = 44100.0f;
    int rampSamples_ = 441;
    int elapsed_ = 0;
};

// Linear-slew envelope follower used by filter safety limiter and Glue compressor mode
class SlewEnvelopeFollower {
public:
    void setAttackRelease(float attackSec, float releaseSec) noexcept {
        attackSec_ = attackSec;
        releaseSec_ = releaseSec;
        recalc();
    }

    void setSampleRate(float sr) noexcept {
        sampleRate_ = (std::isfinite(sr) && sr > 0.0f) ? sr : 44100.0f;
        recalc();
        env_ = 0.0f;
    }

    void clear() noexcept { env_ = 0.0f; }

    float process(float x) noexcept {
        const float decayed = env_ - releaseStep_;
        float delta = 0.0f;
        if (x > decayed) {
            delta = x - decayed;
        } else if (-decayed > x) {
            delta = (-x) - decayed;
        }
        env_ = std::max(0.0f, decayed + std::min(delta, attackStep_));
        return env_;
    }

private:
    void recalc() noexcept {
        attackStep_ = (attackSec_ > 0.0f && sampleRate_ > 0.0f) ? (1.0f / (attackSec_ * sampleRate_)) : 0.0f;
        releaseStep_ = (releaseSec_ > 0.0f && sampleRate_ > 0.0f) ? (1.0f / (releaseSec_ * sampleRate_)) : 0.0f;
    }

    float sampleRate_ = 44100.0f;
    float attackSec_ = 0.0f;
    float releaseSec_ = 0.0f;
    float env_ = 0.0f;
    float attackStep_ = 0.0f;
    float releaseStep_ = 0.0f;
};

// Dual-stage waveshaping distortion: pre-filter Bite + post-filter Warmth
class DriveStage {
public:
    static void processBite(float* L, float* R, int n, float amount) noexcept {
        const float gain = 1.0f + 10.0f * amount;
        const float slope = 0.25f / (gain - 0.75f);
        auto shape = [gain, slope, n](float* buf) noexcept {
            for (int i = 0; i < n; ++i) {
                float x = buf[i] * gain;
                if (x > 0.75f) {
                    x = (x - 0.75f) * slope + 0.75f;
                } else if (x < -0.75f) {
                    x = -((-0.75f - x) * slope + 0.75f);
                }
                buf[i] = x;
            }
        };
        shape(L);
        shape(R);
    }

    static void processWarmth(float* L, float* R, int n, float amount) noexcept {
        constexpr float kKnee = 0.6f;
        constexpr float kInvSqrt2 = 0.70710701f;
        constexpr float kScale = 0.8f;
        const float gain = 1.0f + 4.0f * amount;
        const float span = gain - kKnee;
        auto shape = [gain, span, n](float* buf) noexcept {
            for (int i = 0; i < n; ++i) {
                float x = buf[i] * gain;
                if (x > kKnee) {
                    const float t = (x - kKnee) / span;
                    const float u = (1.0f - t) * kInvSqrt2;
                    x = (0.5f - u * u) * kScale + kKnee;
                } else if (x < -kKnee) {
                    const float t = (-kKnee - x) / span;
                    const float u = (1.0f - t) * kInvSqrt2;
                    x = -((0.5f - u * u) * kScale + kKnee);
                }
                buf[i] = x;
            }
        };
        shape(L);
        shape(R);
    }
};

// 2-pole RBJ resonant low-pass biquad with denormal & Nyquist stability guards
class ResonantBiquadLP {
public:
    void setSampleRate(float sr) noexcept {
        sampleRate_ = (std::isfinite(sr) && sr > 0.0f) ? sr : 44100.0f;
        dirty_ = true;
    }

    void clearHistory() noexcept {
        dirty_ = true;
        x1_ = x2_ = y1_ = y2_ = 0.0f;
    }

    void setCutoffHz(float hz) noexcept {
        cutoffHz_ = hz;
        dirty_ = true;
    }

    void setResonance(float res) noexcept {
        resonance_ = res;
        dirty_ = true;
    }

    void process(float* buf, int n) noexcept {
        if (dirty_) {
            dirty_ = false;
            updateCoeffs();
        }
        for (int i = 0; i < n; ++i) {
            const float x0 = buf[i];
            const float y0 = b0_ * x0 + b1_ * x1_ + b2_ * x2_ - a1_ * y1_ - a2_ * y2_;
            y2_ = y1_;
            y1_ = y0;
            x2_ = x1_;
            x1_ = x0;
            buf[i] = y0;
        }
        if (!std::isfinite(y1_) || !std::isfinite(y2_) || !std::isfinite(x1_) || !std::isfinite(x2_)
            || isSubnormal(y1_) || isSubnormal(y2_)
            || std::fabs(y1_) > 1000.0f || std::fabs(y2_) > 1000.0f) {
            x1_ = x2_ = y1_ = y2_ = 0.0f;
        }
    }

private:
    void updateCoeffs() noexcept {
        constexpr double kTwoPi = 6.28318530717958647692;
        const float safeHz = std::clamp(cutoffHz_, 1.0f, sampleRate_ * 0.495f);
        const float q = 1.0f + 6.0f * std::clamp(resonance_, 0.0f, 1.0f);
        const double w0 = (static_cast<double>(safeHz) / static_cast<double>(sampleRate_)) * kTwoPi;
        const double sn = std::sin(w0);
        const double cs = std::cos(w0);
        const double alpha = sn / (2.0 * static_cast<double>(q));
        const double a0 = 1.0 + alpha;
        const double oneMinusCos = 1.0 - cs;

        b0_ = flushDenormal(static_cast<float>((oneMinusCos * 0.5) / a0));
        b1_ = flushDenormal(static_cast<float>(oneMinusCos / a0));
        b2_ = b0_;
        a1_ = flushDenormal(static_cast<float>((-2.0 * cs) / a0));
        a2_ = flushDenormal(static_cast<float>((1.0 - alpha) / a0));
    }

    float sampleRate_ = 44100.0f;
    bool dirty_ = true;
    float cutoffHz_ = 16000.0f;
    float resonance_ = 0.0f;
    float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f, a1_ = 0.0f, a2_ = 0.0f;
    float x1_ = 0.0f, x2_ = 0.0f, y1_ = 0.0f, y2_ = 0.0f;
};

// Resonant low-pass filter stage with smoothed cutoff modulation and peak safety limiter
class FilterStage {
public:
    FilterStage() noexcept {
        limEnv_.setAttackRelease(0.0075f, 0.05f);
        reset(44100.0f);
    }

    static float normalizedToHz(float p) noexcept {
        if (p >= 1.0f) return 16000.0f;
        if (p <= 0.0f) return 80.0f;
        const double t = -0.25 * std::log10(static_cast<double>(1.0f - p));
        if (t > 1.0) return 16000.0f;
        if (t < 1e-7) return 80.0f;
        return static_cast<float>(t * 15920.0 + 80.0);
    }

    void reset(float sr) noexcept {
        sampleRate_ = (std::isfinite(sr) && sr > 0.0f) ? sr : 44100.0f;
        limGain_ = 1.0f;
        limStep_ = 0.0f;
        limEnv_.setSampleRate(sampleRate_);
        limEnv_.clear();
        filterL_.setSampleRate(sampleRate_);
        filterR_.setSampleRate(sampleRate_);
        filterL_.clearHistory();
        filterR_.clearHistory();
        initialized_ = false;
        prevCutoff_ = -1.0f;
        prevRes_ = -1.0f;
    }

    void process(float cutoffNorm, float resNorm, float* bufL, float* bufR, int n) noexcept {
        if (n <= 0) return;

        bool rampCutoff = false;
        float cutoffStep = 0.0f;

        if (!initialized_) {
            initialized_ = true;
            prevCutoff_ = cutoffNorm;
            prevRes_ = resNorm;
            const float hz = normalizedToHz(cutoffNorm);
            filterL_.setCutoffHz(hz);
            filterR_.setCutoffHz(hz);
            filterL_.setResonance(resNorm);
            filterR_.setResonance(resNorm);
            filterL_.clearHistory();
            filterR_.clearHistory();
            limGain_ = 1.0f;
            limStep_ = 0.0f;
            limEnv_.clear();
        } else {
            if (!bitsEqual(prevCutoff_, cutoffNorm)) {
                cutoffStep = (cutoffNorm - prevCutoff_) / static_cast<float>(n);
                rampCutoff = true;
            }
            if (!bitsEqual(prevRes_, resNorm)) {
                prevRes_ = resNorm;
                const float hz = normalizedToHz(prevCutoff_);
                filterL_.setResonance(resNorm);
                filterR_.setResonance(resNorm);
                filterL_.setCutoffHz(hz);
                filterR_.setCutoffHz(hz);
            }
        }

        int offset = 0;
        while (offset < n) {
            const int chunk = std::min(16, n - offset);
            filterL_.process(bufL + offset, chunk);
            filterR_.process(bufR + offset, chunk);
            if (rampCutoff) {
                prevCutoff_ += cutoffStep * static_cast<float>(chunk);
                const float hz = normalizedToHz(prevCutoff_);
                filterL_.setCutoffHz(hz);
                filterR_.setCutoffHz(hz);
            }
            offset += chunk;
        }
        prevCutoff_ = cutoffNorm;
        prevRes_ = resNorm;

        const float relDenom = 7.5f * sampleRate_;
        for (int i = 0; i < n; ++i) {
            const float peak = std::max(std::fabs(bufL[i]), std::fabs(bufR[i]));
            const float env = limEnv_.process(peak);
            if (env > 1.0f) {
                const float targetGain = 1.0f / env;
                if (targetGain < limGain_) {
                    limGain_ = targetGain;
                    limStep_ = (1.0f - targetGain) / relDenom;
                }
            }
            if (limGain_ < 1.0f) {
                bufL[i] *= limGain_;
                bufR[i] *= limGain_;
                limGain_ += limStep_;
            }
        }
    }

private:
    float sampleRate_ = 44100.0f;
    bool initialized_ = false;
    float prevCutoff_ = -1.0f;
    float prevRes_ = -1.0f;
    float limGain_ = 1.0f;
    float limStep_ = 0.0f;
    SlewEnvelopeFollower limEnv_;
    ResonantBiquadLP filterL_;
    ResonantBiquadLP filterR_;
};

// One-knob dynamics compressor with Glue (30/80 ms envelope) and Slam (instant peak) modes
class CompressorStage {
public:
    CompressorStage() noexcept {
        env_.setAttackRelease(0.03f, 0.08f);
        clear();
    }

    void setSampleRate(float sr) noexcept {
        sampleRate_ = (std::isfinite(sr) && sr > 0.0f) ? sr : 44100.0f;
        env_.setSampleRate(sampleRate_);
    }

    void setSlamMode(bool slam) noexcept { slamMode_ = slam; }

    void clear() noexcept {
        gain_ = 1.0f;
        releaseStep_ = 0.0f;
        prevAmount_ = 0.0f;
        lastGr_ = 0.0f;
        env_.clear();
    }

    void syncAmount(float amount) noexcept {
        prevAmount_ = std::isfinite(amount) ? std::clamp(amount, 0.0f, 0.99f) : 0.0f;
    }

    float getLastGainReduction() const noexcept { return lastGr_; }

    void process(float* L, float* R, int n, float amount) noexcept {
        if (n <= 0) return;
        constexpr float kKnee = 0.31f;
        constexpr float kInvSqrt2 = 0.70710701f;
        constexpr float kCurveScale = 1.38f;
        constexpr float kFastReleaseSec = 0.00025f;

        const float clampedAmt = std::isfinite(amount) ? std::clamp(amount, 0.0f, 0.99f) : 0.0f;
        const float makeup = 1.0f / (1.0f - prevAmount_);
        const float relSamples = kFastReleaseSec * sampleRate_;
        const float kneeSpan = makeup - kKnee;
        float maxReduction = 0.0f;

        for (int i = 0; i < n; ++i) {
            const float inL = L[i];
            const float inR = R[i];
            const float peak = std::max(std::fabs(inL), std::fabs(inR));
            const float det = slamMode_ ? peak : env_.process(peak);

            float outL = inL * makeup;
            float outR = inR * makeup;
            const float scaledDet = det * makeup;

            if (scaledDet > kKnee) {
                const float t = (scaledDet - kKnee) / kneeSpan;
                const float u = (1.0f - t) * kInvSqrt2;
                const float targetLevel = (0.5f - u * u) * kCurveScale + kKnee;
                const float targetGain = targetLevel / scaledDet;
                if (std::isfinite(targetGain) && targetGain < gain_) {
                    gain_ = targetGain;
                    releaseStep_ = (relSamples > 0.0f) ? ((1.0f - targetGain) / relSamples) : (1.0f - targetGain);
                }
            }
            if (gain_ < 1.0f) {
                outL *= gain_;
                outR *= gain_;
                maxReduction = std::max(maxReduction, std::clamp(1.0f - gain_, 0.0f, 1.0f));
                gain_ += releaseStep_;
            }
            L[i] = outL;
            R[i] = outR;
        }
        prevAmount_ = clampedAmt;
        lastGr_ = maxReduction;
    }

private:
    float prevAmount_ = 0.0f;
    bool slamMode_ = false;
    float sampleRate_ = 44100.0f;
    float gain_ = 1.0f;
    float releaseStep_ = 0.0f;
    float lastGr_ = 0.0f;
    SlewEnvelopeFollower env_;
};

// Pure C++20 DuneCrusher DSP engine (audio-thread safe, zero allocations, zero locks)
class DuneCrusherDSP {
public:
    DuneCrusherDSP() noexcept {
        params_[kDriveOn]      = LinearParamSmoother(0.0f, 1.0f, false);
        params_[kDriveBite]    = LinearParamSmoother(0.0f, 1.0f, true);
        params_[kDriveWarmth]  = LinearParamSmoother(0.0f, 1.0f, true);
        params_[kFilterOn]     = LinearParamSmoother(0.0f, 1.0f, false);
        params_[kFilterCutoff] = LinearParamSmoother(0.0f, 1.0f, true);
        params_[kFilterRes]    = LinearParamSmoother(0.0f, 1.0f, true);
        params_[kCompOn]       = LinearParamSmoother(0.0f, 1.0f, false);
        params_[kCompAmount]   = LinearParamSmoother(0.0f, 0.99f, true);
        params_[kCompSlam]     = LinearParamSmoother(0.0f, 1.0f, false);
        params_[kMasterOn]     = LinearParamSmoother(0.0f, 1.0f, false);
        params_[kMasterMix]    = LinearParamSmoother(0.0f, 1.0f, true);
        params_[kMasterVolume] = LinearParamSmoother(0.0f, 1.0f, true);
        setParameter(kMasterOn, 1.0f);
        setParameter(kMasterMix, 1.0f);
        setParameter(kMasterVolume, 1.0f);
        setSampleRate(44100.0f);
    }

    void setSampleRate(float sr) noexcept {
        sampleRate_ = (std::isfinite(sr) && sr > 0.0f) ? sr : 44100.0f;
        filter_.reset(sampleRate_);
        comp_.setSampleRate(sampleRate_);
        for (auto& p : params_) p.setSampleRate(sampleRate_);
    }

    void reset() noexcept {
        for (auto& p : params_) p.snapToTarget();
        filter_.reset(sampleRate_);
        comp_.clear();
        comp_.syncAmount(params_[kCompAmount].getTarget());
        curMix_ = params_[kMasterMix].getTarget();
        curVol_ = static_cast<float>(std::pow(static_cast<double>(params_[kMasterVolume].getTarget()), 2.737));
        masterSynced_ = true;
        meters_ = {};
    }

    void setParameter(int idx, float normValue) noexcept {
        if (idx < 0 || idx >= kNumParams || !std::isfinite(normValue)) return;
        const float clamped = std::clamp(normValue, 0.0f, 1.0f);
        params_[static_cast<size_t>(idx)].setNormalized(clamped);
        if (idx == kCompSlam) {
            comp_.setSlamMode(clamped >= 0.5f);
        } else if (idx == kMasterOn) {
            bypass_ = (clamped < 0.5f);
        }
    }

    float getParameter(int idx) const noexcept {
        if (idx < 0 || idx >= kNumParams) return 0.0f;
        return params_[static_cast<size_t>(idx)].getNormalized();
    }

    MeterSnapshot getLastBlockMeters() const noexcept { return meters_; }

    void process(const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept {
        if (numSamples <= 0 || inL == nullptr || outL == nullptr) return;
        const bool isStereo = (inR != nullptr && outR != nullptr);

        float peakL = 0.0f;
        float peakR = 0.0f;
        float blockGr = 0.0f;

        if (bypass_) {
            for (int i = 0; i < numSamples; ++i) {
                const float sL = sanitizeSample(inL[i]);
                outL[i] = sL;
                peakL = std::max(peakL, std::fabs(sL));
                if (isStereo) {
                    const float sR = sanitizeSample(inR[i]);
                    outR[i] = sR;
                    peakR = std::max(peakR, std::fabs(sR));
                }
            }
            meters_ = {peakL, isStereo ? peakR : peakL, 0.0f};
            return;
        }

        if (!masterSynced_) {
            curMix_ = params_[kMasterMix].getTarget();
            curVol_ = static_cast<float>(std::pow(static_cast<double>(params_[kMasterVolume].getTarget()), 2.737));
            comp_.syncAmount(params_[kCompAmount].getTarget());
            masterSynced_ = true;
        }

        const float driveOn  = params_[kDriveOn].advance(numSamples);
        const bool doBite    = (driveOn > 0.5f) && (params_[kDriveBite].advance(numSamples) > 0.0f);
        const bool doWarmth  = (driveOn > 0.5f) && (params_[kDriveWarmth].advance(numSamples) > 0.0f);
        const float filterOn = params_[kFilterOn].advance(numSamples);
        const float compOn   = params_[kCompOn].advance(numSamples);

        int offset = 0;
        while (offset < numSamples) {
            const int n = std::min(64, numSamples - offset);
            const float invN = 1.0f / static_cast<float>(n);

            for (int i = 0; i < n; ++i) {
                const float sL = sanitizeSample(inL[offset + i]);
                const float sR = isStereo ? sanitizeSample(inR[offset + i]) : sL;
                dryL_[i] = wetL_[i] = sL;
                dryR_[i] = wetR_[i] = sR;
            }

            if (doBite) {
                DriveStage::processBite(wetL_, wetR_, n, params_[kDriveBite].advance(n));
            }
            if (filterOn > 0.5f) {
                const float cutoff = params_[kFilterCutoff].advance(n);
                const float res    = params_[kFilterRes].advance(n);
                filter_.process(cutoff, res, wetL_, wetR_, n);
            }
            if (doWarmth) {
                DriveStage::processWarmth(wetL_, wetR_, n, params_[kDriveWarmth].advance(n));
            }
            if (compOn > 0.5f) {
                const float amt = std::min(params_[kCompAmount].advance(n), 0.99f);
                comp_.process(wetL_, wetR_, n, amt);
                blockGr = std::max(blockGr, comp_.getLastGainReduction());
            }

            const float targetMix = params_[kMasterMix].advance(n);
            const float volParam  = params_[kMasterVolume].advance(n);
            const float targetVol = static_cast<float>(std::pow(static_cast<double>(volParam), 2.737));
            const float mixStep   = (targetMix - curMix_) * invN;
            const float volStep   = (targetVol - curVol_) * invN;

            constexpr float kClip = 0.9999f;
            for (int i = 0; i < n; ++i) {
                const float dryW = 1.0f - curMix_;
                float sL = (wetL_[i] * curMix_ + dryL_[i] * dryW) * curVol_;
                float sR = (wetR_[i] * curMix_ + dryR_[i] * dryW) * curVol_;
                curVol_ += volStep;
                curMix_ += mixStep;

                sL = std::isfinite(sL) ? std::clamp(sL, -kClip, kClip) : 0.0f;
                sR = std::isfinite(sR) ? std::clamp(sR, -kClip, kClip) : 0.0f;
                outL[offset + i] = sL;
                peakL = std::max(peakL, std::fabs(sL));
                if (isStereo) {
                    outR[offset + i] = sR;
                    peakR = std::max(peakR, std::fabs(sR));
                }
            }
            offset += n;
        }

        meters_ = {peakL, isStereo ? peakR : peakL, blockGr};
    }

private:
    float sampleRate_ = 44100.0f;
    bool bypass_ = false;
    bool masterSynced_ = false;
    float curVol_ = 1.0f;
    float curMix_ = 1.0f;
    MeterSnapshot meters_{};
    std::array<LinearParamSmoother, kNumParams> params_;
    FilterStage filter_;
    CompressorStage comp_;
    float dryL_[64]{};
    float dryR_[64]{};
    float wetL_[64]{};
    float wetR_[64]{};
};

} // namespace dunecrusher
