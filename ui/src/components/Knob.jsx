import React, { useRef } from "react";
import { formatParam, INIT_DEFAULTS } from "../dspCurves.js";

const START_DEG = -135;
const END_DEG = 135;
const CX = 44;
const CY = 44;

function arcPath(r, a0Deg, a1Deg) {
  const a0 = ((a0Deg - 90) * Math.PI) / 180;
  const a1 = ((a1Deg - 90) * Math.PI) / 180;
  const x0 = CX + r * Math.cos(a0);
  const y0 = CY + r * Math.sin(a0);
  const x1 = CX + r * Math.cos(a1);
  const y1 = CY + r * Math.sin(a1);
  const large = a1Deg - a0Deg > 180 ? 1 : 0;
  return `M ${x0.toFixed(2)} ${y0.toFixed(2)} A ${r} ${r} 0 ${large} 1 ${x1.toFixed(2)} ${y1.toFixed(2)}`;
}

export default function Knob({
  paramId,
  label,
  value,
  active,
  theme,
  onChange,
  onDragStart,
  onDragEnd
}) {
  const dragRef = useRef(null);
  const deg = START_DEG + value * (END_DEG - START_DEG);
  const rad = ((deg - 90) * Math.PI) / 180;

  const handlePointerDown = (e) => {
    e.preventDefault();
    e.currentTarget.setPointerCapture(e.pointerId);
    dragRef.current = { startY: e.clientY, startVal: value };
    onDragStart?.(paramId);
  };

  const handlePointerMove = (e) => {
    if (!dragRef.current) return;
    const scale = e.shiftKey ? 520 : 155;
    const next = Math.max(0, Math.min(1, dragRef.current.startVal + (dragRef.current.startY - e.clientY) / scale));
    onChange(paramId, next);
  };

  const handlePointerUp = () => {
    if (!dragRef.current) return;
    dragRef.current = null;
    onDragEnd?.(paramId);
  };

  const handleWheel = (e) => {
    e.preventDefault();
    const step = e.shiftKey ? 0.008 : 0.03;
    const next = Math.max(0, Math.min(1, value - Math.sign(e.deltaY) * step));
    onDragStart?.(paramId);
    onChange(paramId, next);
    onDragEnd?.(paramId);
  };

  const handleDoubleClick = () => {
    const def = INIT_DEFAULTS[paramId] ?? 0;
    onDragStart?.(paramId);
    onChange(paramId, def);
    onDragEnd?.(paramId);
  };

  const renderDial = () => {
    const op = active ? 1 : 0.34;
    if (theme.knobStyle === "swiss") {
      const ticks = [];
      for (let i = 0; i <= 20; i++) {
        const td = START_DEG + (i / 20) * (END_DEG - START_DEG);
        const tr = ((td - 90) * Math.PI) / 180;
        const major = i % 5 === 0;
        const r1 = major ? 33 : 35;
        const r2 = 39;
        const lit = i / 20 <= value + 0.001 && active;
        ticks.push(
          <line
            key={i}
            x1={(CX + r1 * Math.cos(tr)).toFixed(1)}
            y1={(CY + r1 * Math.sin(tr)).toFixed(1)}
            x2={(CX + r2 * Math.cos(tr)).toFixed(1)}
            y2={(CY + r2 * Math.sin(tr)).toFixed(1)}
            stroke="#0B0B0C"
            strokeWidth={major ? 1.6 : 1}
            strokeOpacity={lit ? 1 : 0.25}
          />
        );
      }
      return (
        <svg viewBox="0 0 88 88" className="w-[78px] h-[78px] block mx-auto" style={{ opacity: op }}>
          {ticks}
          <circle cx={CX} cy={CY + 2} r="28" fill="rgba(11,11,12,0.16)" />
          <circle cx={CX} cy={CY} r="27" fill="#0B0B0C" />
          <circle cx={CX} cy={CY} r="22.5" fill="none" stroke="rgba(244,243,238,0.16)" strokeWidth="1" />
          <line
            x1={(CX + 8 * Math.cos(rad)).toFixed(1)}
            y1={(CY + 8 * Math.sin(rad)).toFixed(1)}
            x2={(CX + 25 * Math.cos(rad)).toFixed(1)}
            y2={(CY + 25 * Math.sin(rad)).toFixed(1)}
            stroke="#F4F3EE"
            strokeWidth="2.6"
            strokeLinecap="square"
          />
        </svg>
      );
    }

    if (theme.knobStyle === "dotmatrix") {
      const dots = [];
      for (let i = 0; i <= 20; i++) {
        const td = START_DEG + (i / 20) * (END_DEG - START_DEG);
        const tr = ((td - 90) * Math.PI) / 180;
        const lit = i / 20 <= value + 0.001 && value > 0.005 && active;
        dots.push(
          <circle
            key={i}
            cx={(CX + 37 * Math.cos(tr)).toFixed(1)}
            cy={(CY + 37 * Math.sin(tr)).toFixed(1)}
            r={lit ? 2.0 : 1.3}
            fill="#FAFAFA"
            fillOpacity={lit ? 1 : 0.16}
          />
        );
      }
      return (
        <svg viewBox="0 0 88 88" className="w-[78px] h-[78px] block mx-auto" style={{ opacity: op }}>
          {dots}
          <circle cx={CX} cy={CY} r="27" fill="#0F0F12" stroke="#FAFAFA" strokeWidth="1.2" strokeOpacity="0.55" />
          <circle cx={CX} cy={CY} r="22" fill="#070708" stroke="#FAFAFA" strokeWidth="0.6" strokeOpacity="0.18" />
          <line
            x1={(CX + 10 * Math.cos(rad)).toFixed(1)}
            y1={(CY + 10 * Math.sin(rad)).toFixed(1)}
            x2={(CX + 26 * Math.cos(rad)).toFixed(1)}
            y2={(CY + 26 * Math.sin(rad)).toFixed(1)}
            stroke="#FAFAFA"
            strokeWidth="2.2"
            strokeLinecap="round"
          />
        </svg>
      );
    }

    if (theme.knobStyle === "monolith") {
      const knurl = [];
      for (let i = 0; i < 36; i++) {
        const a = ((i * 10 + deg) * Math.PI) / 180;
        knurl.push(
          <line
            key={i}
            x1={(CX + 25.5 * Math.cos(a)).toFixed(1)}
            y1={(CY + 25.5 * Math.sin(a)).toFixed(1)}
            x2={(CX + 28.5 * Math.cos(a)).toFixed(1)}
            y2={(CY + 28.5 * Math.sin(a)).toFixed(1)}
            stroke="rgba(242,241,236,0.28)"
            strokeWidth="1.1"
          />
        );
      }
      return (
        <svg viewBox="0 0 88 88" className="w-[78px] h-[78px] block mx-auto" style={{ opacity: op }}>
          <path d={arcPath(36, START_DEG, END_DEG)} fill="none" stroke="rgba(242,241,236,0.12)" strokeWidth="2.4" strokeLinecap="round" />
          {value > 0.005 && (
            <path d={arcPath(36, START_DEG, deg)} fill="none" stroke="#F2F1EC" strokeWidth="2.4" strokeLinecap="round" />
          )}
          <circle cx={CX} cy={CY + 3} r="29" fill="rgba(0,0,0,0.65)" />
          <circle cx={CX} cy={CY} r="28" fill="#18181C" stroke="rgba(242,241,236,0.22)" strokeWidth="1" />
          {knurl}
          <circle cx={CX} cy={CY} r="23.5" fill="#121215" stroke="rgba(242,241,236,0.14)" strokeWidth="0.9" />
          <line
            x1={(CX + 9 * Math.cos(rad)).toFixed(1)}
            y1={(CY + 9 * Math.sin(rad)).toFixed(1)}
            x2={(CX + 23 * Math.cos(rad)).toFixed(1)}
            y2={(CY + 23 * Math.sin(rad)).toFixed(1)}
            stroke="#FFFFFF"
            strokeWidth="2.2"
            strokeLinecap="round"
          />
        </svg>
      );
    }

    // E-Ink
    return (
      <svg viewBox="0 0 88 88" className="w-[78px] h-[78px] block mx-auto" style={{ opacity: op }}>
        <path d={arcPath(35, START_DEG, END_DEG)} fill="none" stroke="rgba(17,17,17,0.12)" strokeWidth="2.6" strokeLinecap="round" />
        {value > 0.005 && (
          <path d={arcPath(35, START_DEG, deg)} fill="none" stroke="#111111" strokeWidth="2.6" strokeLinecap="round" />
        )}
        <circle cx={CX} cy={CY} r="26" fill="#FFFFFF" stroke="#111111" strokeWidth="1.5" />
        <circle cx={CX} cy={CY} r="3" fill="#111111" />
        <line
          x1={(CX + 10 * Math.cos(rad)).toFixed(1)}
          y1={(CY + 10 * Math.sin(rad)).toFixed(1)}
          x2={(CX + 24 * Math.cos(rad)).toFixed(1)}
          y2={(CY + 24 * Math.sin(rad)).toFixed(1)}
          stroke="#111111"
          strokeWidth="2.2"
          strokeLinecap="round"
        />
      </svg>
    );
  };

  return (
    <div
      className="knob-hit flex flex-col items-center py-0.5"
      onPointerDown={handlePointerDown}
      onPointerMove={handlePointerMove}
      onPointerUp={handlePointerUp}
      onPointerCancel={handlePointerUp}
      onWheel={handleWheel}
      onDoubleClick={handleDoubleClick}
      title={`${label} (Shift+Drag: fine, Double-click: reset)`}
    >
      {renderDial()}
      <div className={`mt-1 text-[10.5px] font-mono-tech font-bold tracking-wider ${active ? "" : "opacity-35"}`}>
        {label}
      </div>
      <div className={`text-[10.5px] font-mono-tech ${theme.mutedText} ${active ? "" : "opacity-40"}`}>
        {formatParam(paramId, value)}
      </div>
    </div>
  );
}
