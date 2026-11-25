import React, { useEffect, useRef } from "react";
import { THEMES, THEME_ORDER } from "../themes.js";

const SIGNAL_YELLOW = "#F5C518";
const SIGNAL_RED = "#E6392E";

function getSegmentColour(frac, isGainReduction, baseWhite) {
  if (isGainReduction) {
    if (frac > 0.68) return SIGNAL_RED;
    if (frac > 0.32) return SIGNAL_YELLOW;
    return baseWhite;
  }
  if (frac > 0.85) return SIGNAL_RED;
  if (frac > 0.62) return SIGNAL_YELLOW;
  return baseWhite;
}

function drawSegBar(ctx, w, level, segs, reverse, baseWhite, offCol, y0, bh) {
  const gap = 3;
  const sw = (w - gap * (segs - 1)) / segs;
  for (let i = 0; i < segs; i++) {
    const frac = (i + 1) / segs;
    const idx = reverse ? segs - 1 - i : i;
    const sx = idx * (sw + gap);
    const lit = level >= (i + 0.35) / segs;
    ctx.fillStyle = lit ? getSegmentColour(frac, reverse, baseWhite) : offCol;
    ctx.fillRect(sx, y0, sw, bh);
  }
}

export default function MeterBar({ metersRef, theme, themeId, onSelectTheme }) {
  const outRef = useRef(null);
  const grRef = useRef(null);

  useEffect(() => {
    let rafId = 0;
    const drawFrame = () => {
      const m = metersRef.current;
      const outCv = outRef.current;
      if (outCv) {
        const ctx = outCv.getContext("2d");
        ctx.clearRect(0, 0, outCv.width, outCv.height);
        drawSegBar(ctx, outCv.width, m.peakL, 18, false, theme.meterOn, theme.meterOff, 2, 10);
        drawSegBar(ctx, outCv.width, m.peakR, 18, false, theme.meterOn, theme.meterOff, 16, 10);
      }
      const grCv = grRef.current;
      if (grCv) {
        const ctx = grCv.getContext("2d");
        ctx.clearRect(0, 0, grCv.width, grCv.height);
        drawSegBar(ctx, grCv.width, m.gr, 14, true, theme.meterOn, theme.meterOff, 7, 14);
      }
    };

    // Draw synchronously on mount so frame 0 is immediately populated
    drawFrame();

    const loop = () => {
      drawFrame();
      rafId = requestAnimationFrame(loop);
    };
    rafId = requestAnimationFrame(loop);
    return () => cancelAnimationFrame(rafId);
  }, [metersRef, theme]);

  return (
    <div className={`relative z-10 h-[42px] px-4 flex items-center justify-between gap-3 shrink-0 ${theme.meterBg}`}>
      {/* OUT L/R stereo meter (White -> Yellow -> Red) */}
      <div className="flex items-center gap-2.5 flex-1 min-w-0">
        <span className="text-[10px] font-mono-tech font-bold tracking-widest text-white/80 shrink-0">OUT</span>
        <canvas ref={outRef} width={220} height={28} className="flex-1 h-[22px] block min-w-0" />
      </div>

      <div className="w-px h-5 bg-white/20 shrink-0" />

      {/* Gain Reduction meter (White -> Yellow -> Red) */}
      <div className="flex items-center gap-2.5 w-[170px] shrink-0">
        <span className="text-[10px] font-mono-tech font-bold tracking-widest text-white/80 shrink-0">GR</span>
        <canvas ref={grRef} width={160} height={28} className="flex-1 h-[22px] block min-w-0" />
      </div>

      {/* Discrete Monochrome Theme Switcher anchored cleanly in the black bottom strip */}
      <div className="flex items-center gap-1 shrink-0 pl-2.5 border-l border-white/20">
        {THEME_ORDER.map((id) => {
          const active = id === themeId;
          return (
            <button
              key={id}
              onClick={() => onSelectTheme(id)}
              className={`px-1.5 py-0.5 text-[8.5px] font-mono-tech font-bold tracking-wider transition cursor-pointer ${
                active
                  ? "bg-[#F4F3EE] text-[#0B0B0C]"
                  : "text-white/45 hover:text-white/90"
              }`}
              title={`Theme: ${THEMES[id].short}`}
            >
              {THEMES[id].short}
            </button>
          );
        })}
      </div>
    </div>
  );
}
