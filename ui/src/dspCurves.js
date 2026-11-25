// Exact parameter metadata and DSP math matching src/dsp/DuneCrusherDSP.h & Presets.h

export const PARAM_IDS = [
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
];

export const PARAM_LABELS = [
  "DRIVE",
  "BITE",
  "WARMTH",
  "FILTER",
  "CUTOFF",
  "RESONANCE",
  "COMPRESSOR",
  "AMOUNT",
  "COMP MODE",
  "MASTER",
  "MIX",
  "VOLUME"
];

export const TOGGLE_INDICES = [0, 3, 6, 8, 9];
export const SLIDER_INDICES = [1, 2, 4, 5, 7, 10, 11];

export const INIT_DEFAULTS = [0, 0, 0, 0, 0.5, 0, 0, 0, 0, 1, 1, 1];

export const FACTORY_PRESETS = [
  { name: "Warm Console",    category: "MIX & MASTER",   p: [1, 0.05, 0.30, 0, 0.85, 0.10, 1, 0.35, 0, 1, 0.85, 0.88] },
  { name: "Desert Grit",     category: "SATURATION",     p: [1, 0.38, 0.68, 1, 0.72, 0.32, 1, 0.62, 1, 1, 1.00, 0.68] },
  { name: "Drum Bus Glue",   category: "DRUMS",          p: [1, 0.08, 0.24, 0, 0.90, 0.05, 1, 0.58, 0, 1, 0.55, 0.82] },
  { name: "808 Sub Smash",   category: "BASS & 808",     p: [1, 0.15, 0.75, 1, 0.24, 0.22, 1, 0.78, 1, 1, 1.00, 0.74] },
  { name: "Breakbeat Slam",  category: "DRUMS",          p: [1, 0.25, 0.52, 0, 0.65, 0.12, 1, 0.86, 1, 1, 0.80, 0.66] },
  { name: "Synth Lead Bite", category: "SYNTH & FILTER", p: [1, 0.62, 0.45, 1, 0.66, 0.48, 1, 0.50, 0, 1, 1.00, 0.70] },
  { name: "Tape Heat",       category: "SATURATION",     p: [1, 0.00, 0.42, 1, 0.92, 0.14, 0, 0.00, 0, 1, 1.00, 0.92] },
  { name: "Parallel Punch",  category: "DRUMS",          p: [1, 0.35, 0.85, 0, 0.80, 0.10, 1, 0.88, 1, 1, 0.38, 0.84] },
  { name: "Lo-Fi Radio",     category: "SYNTH & FILTER", p: [1, 0.48, 0.35, 1, 0.32, 0.64, 1, 0.72, 0, 1, 1.00, 0.72] },
  { name: "Bass Overdrive",  category: "BASS & 808",     p: [1, 0.55, 0.78, 1, 0.48, 0.36, 1, 0.70, 1, 1, 0.90, 0.64] },
  { name: "Resonant Sweep",  category: "SYNTH & FILTER", p: [0, 0.00, 0.00, 1, 0.58, 0.88, 1, 0.45, 0, 1, 1.00, 0.78] },
  { name: "Dark Saturation", category: "SATURATION",     p: [1, 0.72, 0.20, 1, 0.38, 0.18, 1, 0.68, 0, 1, 1.00, 0.68] },
  { name: "Acid Crunch",     category: "SYNTH & FILTER", p: [1, 0.82, 0.64, 1, 0.52, 0.92, 1, 0.74, 1, 1, 1.00, 0.60] },
  { name: "Master Polish",   category: "MIX & MASTER",   p: [1, 0.00, 0.12, 0, 1.00, 0.00, 1, 0.18, 0, 1, 0.65, 0.96] },
  { name: "Total Meltdown",  category: "SATURATION",     p: [1, 0.96, 0.95, 1, 0.68, 0.85, 1, 0.94, 1, 1, 1.00, 0.56] },
  { name: "Init (Clean)",    category: "UTILITY",        p: [0, 0.00, 0.00, 0, 0.50, 0.00, 0, 0.00, 0, 1, 1.00, 1.00] }
];

export function cutoffToHz(p) {
  if (p >= 1) return 16000;
  if (p <= 0) return 80;
  const t = -0.25 * Math.log10(1 - p);
  if (t > 1) return 16000;
  if (t < 1e-7) return 80;
  return t * 15920 + 80;
}

export function formatParam(id, v) {
  if (id === 0 || id === 3 || id === 6 || id === 9) return v >= 0.5 ? "ON" : "OFF";
  if (id === 8) return v >= 0.5 ? "SLAM" : "GLUE";
  if (id === 1 || id === 2 || id === 5 || id === 10) return `${Math.round(v * 100)}%`;
  if (id === 4) {
    const hz = Math.round(cutoffToHz(v));
    return hz >= 1000 ? `${(hz / 1000).toFixed(2)} kHz` : `${hz} Hz`;
  }
  if (id === 7) {
    const db = 20 * Math.log10(Math.max(1e-6, 1 - v * 0.99));
    return `${Math.abs(db) < 0.5 ? 0 : Math.round(db)} dB`;
  }
  if (id === 11) {
    if (v < 0.001) return "-inf dB";
    const db = 20 * Math.log10(Math.pow(v, 2.737));
    return `${Math.abs(db) < 0.05 ? "0.0" : db.toFixed(1)} dB`;
  }
  return "";
}

export function shapeBite(x, amt) {
  const gain = 1 + 10 * amt;
  const slope = 0.25 / (gain - 0.75);
  let y = x * gain;
  if (y > 0.75) y = (y - 0.75) * slope + 0.75;
  else if (y < -0.75) y = -((-0.75 - y) * slope + 0.75);
  return y;
}

export function shapeWarmth(x, amt) {
  const kKnee = 0.6;
  const kInvSqrt2 = 0.70710701;
  const kScale = 0.8;
  const gain = 1 + 4 * amt;
  const span = gain - kKnee;
  let y = x * gain;
  if (y > kKnee) {
    const t = (y - kKnee) / span;
    const u = (1 - t) * kInvSqrt2;
    y = (0.5 - u * u) * kScale + kKnee;
  } else if (y < -kKnee) {
    const t = (-kKnee - y) / span;
    const u = (1 - t) * kInvSqrt2;
    y = -((0.5 - u * u) * kScale + kKnee);
  }
  return y;
}
