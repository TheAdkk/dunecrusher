#pragma once

#include "DuneCrusherDSP.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace dunecrusher {

struct Preset {
    std::string name;
    std::array<float, kNumParams> params{};
    std::string category = "USER";
};

// 16 original factory presets crafted for DuneCrusher
// Order: DriveOn, Bite, Warmth, FilterOn, Cutoff, Res, CompOn, CompAmt, CompSlam, MasterOn, Mix, Vol
inline std::vector<Preset> getFactoryPresets() {
    return {
        {"Warm Console",    {1.0f, 0.05f, 0.30f, 0.0f, 0.85f, 0.10f, 1.0f, 0.35f, 0.0f, 1.0f, 0.85f, 0.88f}, "MIX & MASTER"},
        {"Desert Grit",     {1.0f, 0.38f, 0.68f, 1.0f, 0.72f, 0.32f, 1.0f, 0.62f, 1.0f, 1.0f, 1.00f, 0.68f}, "SATURATION"},
        {"Drum Bus Glue",   {1.0f, 0.08f, 0.24f, 0.0f, 0.90f, 0.05f, 1.0f, 0.58f, 0.0f, 1.0f, 0.55f, 0.82f}, "DRUMS"},
        {"808 Sub Smash",   {1.0f, 0.15f, 0.75f, 1.0f, 0.24f, 0.22f, 1.0f, 0.78f, 1.0f, 1.0f, 1.00f, 0.74f}, "BASS & 808"},
        {"Breakbeat Slam",  {1.0f, 0.25f, 0.52f, 0.0f, 0.65f, 0.12f, 1.0f, 0.86f, 1.0f, 1.0f, 0.80f, 0.66f}, "DRUMS"},
        {"Synth Lead Bite", {1.0f, 0.62f, 0.45f, 1.0f, 0.66f, 0.48f, 1.0f, 0.50f, 0.0f, 1.0f, 1.00f, 0.70f}, "SYNTH & FILTER"},
        {"Tape Heat",       {1.0f, 0.00f, 0.42f, 1.0f, 0.92f, 0.14f, 0.0f, 0.00f, 0.0f, 1.0f, 1.00f, 0.92f}, "SATURATION"},
        {"Parallel Punch",  {1.0f, 0.35f, 0.85f, 0.0f, 0.80f, 0.10f, 1.0f, 0.88f, 1.0f, 1.0f, 0.38f, 0.84f}, "DRUMS"},
        {"Lo-Fi Radio",     {1.0f, 0.48f, 0.35f, 1.0f, 0.32f, 0.64f, 1.0f, 0.72f, 0.0f, 1.0f, 1.00f, 0.72f}, "SYNTH & FILTER"},
        {"Bass Overdrive",  {1.0f, 0.55f, 0.78f, 1.0f, 0.48f, 0.36f, 1.0f, 0.70f, 1.0f, 1.0f, 0.90f, 0.64f}, "BASS & 808"},
        {"Resonant Sweep",  {0.0f, 0.00f, 0.00f, 1.0f, 0.58f, 0.88f, 1.0f, 0.45f, 0.0f, 1.0f, 1.00f, 0.78f}, "SYNTH & FILTER"},
        {"Dark Saturation", {1.0f, 0.72f, 0.20f, 1.0f, 0.38f, 0.18f, 1.0f, 0.68f, 0.0f, 1.0f, 1.00f, 0.68f}, "SATURATION"},
        {"Acid Crunch",     {1.0f, 0.82f, 0.64f, 1.0f, 0.52f, 0.92f, 1.0f, 0.74f, 1.0f, 1.0f, 1.00f, 0.60f}, "SYNTH & FILTER"},
        {"Master Polish",   {1.0f, 0.00f, 0.12f, 0.0f, 1.00f, 0.00f, 1.0f, 0.18f, 0.0f, 1.0f, 0.65f, 0.96f}, "MIX & MASTER"},
        {"Total Meltdown",  {1.0f, 0.96f, 0.95f, 1.0f, 0.68f, 0.85f, 1.0f, 0.94f, 1.0f, 1.0f, 1.00f, 0.56f}, "SATURATION"},
        {"Init (Clean)",    {0.0f, 0.00f, 0.00f, 0.0f, 0.50f, 0.00f, 0.0f, 0.00f, 0.0f, 1.0f, 1.00f, 1.00f}, "UTILITY"},
    };
}

inline std::string formatParamDisplay(int paramId, float normVal) {
    const float v = std::isfinite(normVal) ? std::clamp(normVal, 0.0f, 1.0f) : 0.0f;
    char buf[64] = {};
    switch (paramId) {
        case kDriveOn:
        case kFilterOn:
        case kCompOn:
        case kMasterOn:
            return v >= 0.5f ? "On" : "Off";
        case kCompSlam:
            return v >= 0.5f ? "Slam" : "Glue";
        case kDriveBite:
        case kDriveWarmth:
        case kFilterRes:
        case kMasterMix:
            std::snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(v * 100.0f + 0.5f));
            return buf;
        case kFilterCutoff: {
            const int hz = static_cast<int>(FilterStage::normalizedToHz(v));
            if (hz >= 1000) {
                std::snprintf(buf, sizeof(buf), "%.2f kHz", static_cast<double>(hz) * 0.001);
            } else {
                std::snprintf(buf, sizeof(buf), "%d Hz", hz);
            }
            return buf;
        }
        case kCompAmount: {
            const float scaled = v * 0.99f;
            double db = 20.0 * std::log10(std::max(1e-6, static_cast<double>(1.0f - scaled)));
            if (std::fabs(db) < 0.5) db = 0.0;
            std::snprintf(buf, sizeof(buf), "%.0f dB", db);
            return buf;
        }
        case kMasterVolume: {
            if (v < 0.001f) return "-inf dB";
            const double lin = std::pow(static_cast<double>(v), 2.737);
            double db = 20.0 * std::log10(lin);
            if (std::fabs(db) < 0.05) db = 0.0;
            std::snprintf(buf, sizeof(buf), "%.1f dB", db);
            return buf;
        }
        default:
            return "";
    }
}

inline float parseParamFromString(int paramId, const std::string& text) {
    if (text.empty()) return 0.0f;
    if (text.find("-inf") != std::string::npos || text.find("-INF") != std::string::npos) return 0.0f;
    char* endPtr = nullptr;
    double val = std::strtod(text.c_str(), &endPtr);
    if (endPtr == text.c_str() || !std::isfinite(val)) return 0.0f;

    switch (paramId) {
        case kDriveBite:
        case kDriveWarmth:
        case kFilterRes:
        case kMasterMix:
            return std::clamp(static_cast<float>(val > 1.0 ? val * 0.01 : val), 0.0f, 1.0f);
        case kFilterCutoff: {
            if (text.find("kHz") != std::string::npos || text.find("khz") != std::string::npos) {
                val *= 1000.0;
            }
            if (val <= 1.0) return std::clamp(static_cast<float>(val), 0.0f, 1.0f);
            const double hz = std::clamp(val, 80.0, 16000.0);
            const double t = (hz - 80.0) / 15920.0;
            return std::clamp(static_cast<float>(1.0 - std::pow(10.0, -4.0 * t)), 0.0f, 1.0f);
        }
        case kCompAmount: {
            if (val >= 0.0 && val <= 1.0) return static_cast<float>(val);
            const double lin = std::pow(10.0, std::min(0.0, val) / 20.0);
            return std::clamp(static_cast<float>((1.0 - lin) / 0.99), 0.0f, 1.0f);
        }
        case kMasterVolume: {
            if (val > 0.0 && val <= 1.0) return static_cast<float>(val);
            const double lin = std::pow(10.0, std::min(0.0, val) / 20.0);
            return std::clamp(static_cast<float>(std::pow(lin, 1.0 / 2.737)), 0.0f, 1.0f);
        }
        default:
            return std::clamp(static_cast<float>(val), 0.0f, 1.0f);
    }
}

} // namespace dunecrusher
