import React, { useEffect, useRef } from "react";
import { cutoffToHz, shapeBite, shapeWarmth } from "../dspCurves.js";

export default function LiveScope({ mode, params, active, theme }) {
  const canvasRef = useRef(null);

  useEffect(() => {
    const cv = canvasRef.current;
    if (!cv) return;
    const ctx = cv.getContext("2d");
    const w = cv.width;
    const h = cv.height;

    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = theme.scopeBg;
    ctx.fillRect(0, 0, w, h);

    ctx.strokeStyle = theme.scopeGrid;
    ctx.lineWidth = 1;
    ctx.beginPath();
    if (mode === "drive") {
      ctx.moveTo(0, h / 2);
      ctx.lineTo(w, h / 2);
      ctx.moveTo(w / 2, 0);
      ctx.lineTo(w / 2, h);
    } else {
      ctx.moveTo(0, h * 0.42);
      ctx.lineTo(w, h * 0.42);
    }
    ctx.stroke();

    ctx.strokeStyle = theme.scopeStroke;
    ctx.globalAlpha = active ? 1 : 0.28;
    ctx.lineWidth = 2.2;
    ctx.beginPath();

    const pts = 72;
    if (mode === "drive") {
      const bite = params[1];
      const warmth = params[2];
      for (let i = 0; i < pts; i++) {
        const ph = (i / (pts - 1)) * Math.PI * 2;
        let y = 0.72 * Math.sin(ph);
        if (active) {
          if (bite > 0.001) y = shapeBite(y, bite);
          if (warmth > 0.001) y = shapeWarmth(y, warmth);
        }
        const x = 6 + (i / (pts - 1)) * (w - 12);
        const py = h / 2 - (Math.max(-1.05, Math.min(1.05, y)) / 1.05) * (h * 0.39);
        if (i === 0) ctx.moveTo(x, py);
        else ctx.lineTo(x, py);
      }
    } else {
      const fc = cutoffToHz(params[4]);
      const q = 1 + 6 * params[5];
      for (let i = 0; i < pts; i++) {
        const frac = i / (pts - 1);
        const freq = 50 * Math.pow(20000 / 50, frac);
        const r = freq / Math.max(20, fc);
        const denom = Math.sqrt(Math.pow(1 - r * r, 2) + Math.pow(r / q, 2));
        const mag = 1 / Math.max(1e-5, denom);
        const db = Math.max(-24, Math.min(16, 20 * Math.log10(mag)));
        const yFrac = (db + 24) / 40;
        const x = 6 + frac * (w - 12);
        const y = h - 5 - yFrac * (h - 10);
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
    }
    ctx.stroke();
    ctx.globalAlpha = 1;
  }, [mode, params[1], params[2], params[4], params[5], active, theme]);

  return (
    <canvas
      ref={canvasRef}
      width={136}
      height={44}
      className={`w-[68px] h-[22px] border ${theme.dark ? "border-white/20" : "border-black/25"}`}
    />
  );
}
