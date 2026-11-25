import React, { useCallback, useEffect, useRef, useState } from "react";
import {
  FACTORY_PRESETS,
  formatParam,
  INIT_DEFAULTS,
  PARAM_LABELS
} from "./dspCurves.js";
import { callNative, createJuceBridge, isJuceEnvironment } from "./juceBridge.js";
import { THEMES } from "./themes.js";
import Knob from "./components/Knob.jsx";
import LiveScope from "./components/LiveScope.jsx";
import MeterBar from "./components/MeterBar.jsx";
import PresetDrawer from "./components/PresetDrawer.jsx";

function getQueryOptions() {
  if (typeof window === "undefined") return {};
  const sp = new URLSearchParams(window.location.search);
  return {
    theme: sp.get("theme"),
    preset: sp.get("preset"),
    browser: sp.get("browser") === "1",
    demoMeters: sp.get("demoMeters") === "1"
  };
}

export default function App() {
  const q = getQueryOptions();
  const initialTheme = q.theme && THEMES[q.theme] ? q.theme : "swiss";
  const initialPreset =
    q.preset !== null && !Number.isNaN(Number(q.preset))
      ? Math.max(0, Math.min(FACTORY_PRESETS.length - 1, Number(q.preset)))
      : 1; // Desert Grit

  const [themeId, setThemeId] = useState(() => {
    if (q.theme && THEMES[q.theme]) return q.theme;
    try {
      const saved = window.localStorage?.getItem("dunecrusher_theme");
      if (saved && THEMES[saved]) return saved;
    } catch {
      // Ignore storage access restrictions inside sandboxed WebViews
    }
    return initialTheme;
  });

  const [params, setParams] = useState(() => [...FACTORY_PRESETS[initialPreset].p]);
  const [presets, setPresets] = useState(() =>
    FACTORY_PRESETS.map((p) => ({ name: p.name, category: p.category }))
  );
  const [currentProgram, setCurrentProgram] = useState(initialPreset);
  const [displayName, setDisplayName] = useState(FACTORY_PRESETS[initialPreset].name);
  const [displayCategory, setDisplayCategory] = useState(FACTORY_PRESETS[initialPreset].category);
  const [isCustomLabel, setIsCustomLabel] = useState(false);
  const [browserOpen, setBrowserOpen] = useState(Boolean(q.browser));
  const [transient, setTransient] = useState(null);

  const bridgeRef = useRef(null);
  const transientTimerRef = useRef(null);
  const metersRef = useRef({
    peakL: q.demoMeters ? 0.92 : 0,
    peakR: q.demoMeters ? 0.78 : 0,
    gr: q.demoMeters ? 0.80 : 0
  });

  const theme = THEMES[themeId] || THEMES.swiss;
  const native = isJuceEnvironment();

  const handleSelectTheme = useCallback((id) => {
    if (!THEMES[id]) return;
    setThemeId(id);
    try {
      window.localStorage?.setItem("dunecrusher_theme", id);
    } catch {
      // Ignore
    }
  }, []);

  const flashTransient = useCallback((paramId, val) => {
    setTransient({
      name: PARAM_LABELS[paramId] || "PARAM",
      val: formatParam(paramId, val),
      norm: val
    });
    clearTimeout(transientTimerRef.current);
    transientTimerRef.current = setTimeout(() => setTransient(null), 1800);
  }, []);

  useEffect(() => {
    const bridge = createJuceBridge({
      onParamChange: (idx, val) => {
        setParams((prev) => {
          if (Math.abs(prev[idx] - val) < 1e-5) return prev;
          const next = [...prev];
          next[idx] = val;
          return next;
        });
      },
      onPresetState: (payload) => {
        if (Array.isArray(payload.presets) && payload.presets.length > 0) {
          setPresets(payload.presets);
        }
        if (typeof payload.currentProgram === "number") {
          setCurrentProgram(payload.currentProgram);
        }
        if (typeof payload.displayName === "string") {
          setDisplayName(payload.displayName);
        }
        if (typeof payload.displayCategory === "string") {
          setDisplayCategory(payload.displayCategory);
        }
        setIsCustomLabel(Boolean(payload.isCustomLabel));
      },
      onMeters: (m) => {
        const cur = metersRef.current;
        const rawL = Math.max(0, Math.min(1, Number(m.peakL) || 0));
        const rawR = Math.max(0, Math.min(1, Number(m.peakR) || 0));
        const rawGr = Math.max(0, Math.min(1, Number(m.gr) || 0));
        cur.peakL = rawL > cur.peakL ? rawL : cur.peakL * 0.84;
        cur.peakR = rawR > cur.peakR ? rawR : cur.peakR * 0.84;
        cur.gr = rawGr > cur.gr ? rawGr : cur.gr * 0.80;
      }
    });

    bridgeRef.current = bridge;
    bridge.requestInitialState();

    const onKeyDown = (e) => {
      if (e.key === "Escape") setBrowserOpen(false);
    };
    window.addEventListener("keydown", onKeyDown);

    return () => {
      window.removeEventListener("keydown", onKeyDown);
      clearTimeout(transientTimerRef.current);
      bridge.dispose();
    };
  }, []);

  const handleKnobChange = useCallback(
    (paramId, norm) => {
      setParams((prev) => {
        const next = [...prev];
        next[paramId] = norm;
        return next;
      });
      bridgeRef.current?.setSliderValue(paramId, norm);
      flashTransient(paramId, norm);
    },
    [flashTransient]
  );

  const handleKnobDragStart = useCallback((paramId) => {
    bridgeRef.current?.beginSliderGesture(paramId);
  }, []);

  const handleKnobDragEnd = useCallback((paramId) => {
    bridgeRef.current?.endSliderGesture(paramId);
  }, []);

  const handleToggle = useCallback(
    (paramId) => {
      setParams((prev) => {
        const nextVal = prev[paramId] >= 0.5 ? 0 : 1;
        const next = [...prev];
        next[paramId] = nextVal;
        bridgeRef.current?.setToggleValue(paramId, nextVal >= 0.5);
        flashTransient(paramId, nextVal);
        return next;
      });
    },
    [flashTransient]
  );

  const selectPreset = useCallback((idx) => {
    setTransient(null);
    setBrowserOpen(false);
    if (bridgeRef.current?.isNative) {
      callNative("selectPreset", idx);
    } else {
      const clamped = (idx + FACTORY_PRESETS.length) % FACTORY_PRESETS.length;
      setCurrentProgram(clamped);
      setParams([...FACTORY_PRESETS[clamped].p]);
      setDisplayName(FACTORY_PRESETS[clamped].name);
      setDisplayCategory(FACTORY_PRESETS[clamped].category);
      setIsCustomLabel(false);
    }
  }, []);

  const stepPreset = useCallback(
    (delta) => {
      setTransient(null);
      if (bridgeRef.current?.isNative) {
        callNative("stepPreset", delta);
      } else {
        selectPreset(currentProgram + delta);
      }
    },
    [currentProgram, selectPreset]
  );

  const handleRandomize = useCallback(() => {
    if (bridgeRef.current?.isNative) {
      callNative("randomize");
      setTransient({ name: "RANDOMIZER", val: "GENERATED", norm: 1 });
      clearTimeout(transientTimerRef.current);
      transientTimerRef.current = setTimeout(() => setTransient(null), 1600);
    } else {
      setParams((prev) => {
        const next = [...prev];
        if (next[0] >= 0.5) {
          next[1] = Math.random() * 0.85;
          next[2] = Math.random();
        }
        if (next[3] >= 0.5) {
          next[4] = Math.random();
          next[5] = Math.random();
        }
        if (next[6] >= 0.5) {
          next[7] = Math.random();
          next[8] = Math.random() < 0.5 ? 1 : 0;
        }
        return next;
      });
      setDisplayName("Randomized");
      setDisplayCategory("CUSTOM");
      setIsCustomLabel(true);
    }
  }, []);

  const handleInit = useCallback(() => {
    setBrowserOpen(false);
    setTransient(null);
    if (bridgeRef.current?.isNative) {
      callNative("initPreset");
    } else {
      setParams([...INIT_DEFAULTS]);
      setCurrentProgram(15);
      setDisplayName("Init (Clean)");
      setDisplayCategory("UTILITY");
      setIsCustomLabel(false);
    }
  }, []);

  const masterOn = params[9] >= 0.5;
  const driveOn = masterOn && params[0] >= 0.5;
  const filterOn = masterOn && params[3] >= 0.5;
  const compOn = masterOn && params[6] >= 0.5;
  const slamOn = params[8] >= 0.5;

  const renderPowerBtn = (paramId, num, title) => {
    const on = params[paramId] >= 0.5;
    const dot = on
      ? theme.dark
        ? "bg-[#FAFAFA] shadow-[0_0_8px_rgba(255,255,255,0.75)]"
        : "bg-[#0B0B0C]"
      : theme.dark
        ? "bg-transparent border border-[#FAFAFA]/40"
        : "bg-transparent border border-[#0B0B0C]/45";
    return (
      <button onClick={() => handleToggle(paramId)} className="flex items-center gap-2 cursor-pointer">
        <span className={`w-2.5 h-2.5 rounded-full inline-block transition ${dot}`} />
        <span className={`text-[10px] font-mono-tech ${theme.mutedText}`}>{num}</span>
        <span className={`text-[11.5px] font-mono-tech font-bold tracking-widest ${on ? "" : "opacity-45 line-through"}`}>
          {title}
        </span>
      </button>
    );
  };

  return (
    <div className={`w-full h-full flex flex-col justify-between relative overflow-hidden ${theme.shell}`}>
      {/* SVG fractal noise filter for Stone Monolith mode */}
      <svg width="0" height="0" className="absolute pointer-events-none">
        <defs>
          <filter id="monolithNoise" x="0%" y="0%" width="100%" height="100%">
            <feTurbulence type="fractalNoise" baseFrequency="0.85" numOctaves="4" stitchTiles="stitch" result="n" />
            <feColorMatrix type="saturate" values="0" in="n" result="m" />
            <feComponentTransfer in="m">
              <feFuncR type="linear" slope="0.18" intercept="0.06" />
              <feFuncG type="linear" slope="0.18" intercept="0.06" />
              <feFuncB type="linear" slope="0.18" intercept="0.06" />
              <feFuncA type="linear" slope="0.40" intercept="0" />
            </feComponentTransfer>
          </filter>
        </defs>
      </svg>

      {theme.noiseOverlay && (
        <div className="absolute inset-0 pointer-events-none opacity-65" style={{ filter: "url(#monolithNoise)" }} />
      )}

      {/* HEADER BAR */}
      <div className={`relative z-10 h-[62px] px-4 flex items-center justify-between gap-3 shrink-0 ${theme.header}`}>
        <div className="flex items-center gap-2.5">
          <div className={`w-2.5 h-8 ${theme.dark ? "bg-white" : "bg-[#0B0B0C]"}`} />
          <div>
            <div className="flex items-baseline gap-1.5 leading-none">
              <span className="text-[19px] font-black tracking-tight">DUNE</span>
              <span className="text-[19px] font-light tracking-widest opacity-80">CRUSHER</span>
            </div>
            <div className={`text-[8.5px] font-mono-tech tracking-[0.22em] mt-1 ${theme.mutedText}`}>
              SATURATION · FILTER · DYNAMICS
            </div>
          </div>
        </div>

        {/* PRESET OLED CLUSTER */}
        <div className="flex items-stretch gap-1.5 w-[356px] h-[42px]">
          <button
            onClick={() => stepPreset(-1)}
            className={`w-8 flex items-center justify-center font-mono-tech text-sm font-bold cursor-pointer transition ${theme.btn}`}
            title="Previous Preset"
          >
            &lt;
          </button>

          <button
            onClick={() => setBrowserOpen((v) => !v)}
            className={`flex-1 px-2.5 py-1 text-left cursor-pointer transition flex flex-col justify-center ${theme.oled}`}
            title="Open Preset Bank"
          >
            {transient ? (
              <>
                <div className="flex items-center justify-between text-[9.5px] font-mono-tech opacity-80">
                  <span>{transient.name}</span>
                  <span className="font-bold opacity-100">{transient.val}</span>
                </div>
                <div
                  className={`mt-1.5 h-1.5 w-full overflow-hidden ${
                    theme.oledLightText ? "bg-white/20" : "bg-black/15"
                  }`}
                >
                  <div
                    className={`h-full ${theme.oledLightText ? "bg-[#F4F3EE]" : "bg-[#0B0B0C]"}`}
                    style={{ width: `${Math.round(transient.norm * 100)}%` }}
                  />
                </div>
              </>
            ) : (
              <>
                <div className="flex items-center justify-between text-[9px] font-mono-tech">
                  <span className="opacity-65">
                    {String(currentProgram + 1).padStart(2, "0")} / {String(presets.length).padStart(2, "0")}
                  </span>
                  <span
                    className={`px-1.5 py-[0.5px] text-[8px] tracking-wider border ${
                      theme.oledLightText ? "border-white/35" : "border-black/35"
                    }`}
                  >
                    {displayCategory}
                  </span>
                </div>
                <div className="flex items-center justify-between mt-0.5">
                  <span className="text-[13px] font-bold tracking-tight truncate">{displayName}</span>
                  <span className="text-[9px] font-mono-tech opacity-70 ml-2">{browserOpen ? "▲" : "▼"}</span>
                </div>
              </>
            )}
          </button>

          <button
            onClick={() => stepPreset(1)}
            className={`w-8 flex items-center justify-center font-mono-tech text-sm font-bold cursor-pointer transition ${theme.btn}`}
            title="Next Preset"
          >
            &gt;
          </button>

          <button
            onClick={handleRandomize}
            className={`px-2.5 font-mono-tech text-[10.5px] font-bold tracking-wider cursor-pointer transition ${theme.btnAccent}`}
            title="Randomize active stages"
          >
            RND
          </button>
        </div>
      </div>

      {/* 2x2 HARDWARE MODULE GRID */}
      <div className="relative z-10 flex-1 p-3.5 grid grid-cols-2 grid-rows-2 gap-3 min-h-0">
        {/* 01 DRIVE */}
        <div className={`p-3 flex flex-col justify-between ${theme.card}`}>
          <div className={`flex items-center justify-between pb-2 ${theme.cardHeader}`}>
            {renderPowerBtn(0, "01", "DRIVE")}
            <div className="flex items-center gap-1.5">
              <LiveScope mode="drive" params={params} active={driveOn} theme={theme} />
              <span className={`text-[8.5px] font-mono-tech px-1.5 py-0.5 ${theme.badge}`}>DUAL</span>
            </div>
          </div>
          <div className="grid grid-cols-2 gap-2 items-center flex-1">
            <Knob
              paramId={1}
              label="BITE"
              value={params[1]}
              active={driveOn}
              theme={theme}
              onChange={handleKnobChange}
              onDragStart={handleKnobDragStart}
              onDragEnd={handleKnobDragEnd}
            />
            <Knob
              paramId={2}
              label="WARMTH"
              value={params[2]}
              active={driveOn}
              theme={theme}
              onChange={handleKnobChange}
              onDragStart={handleKnobDragStart}
              onDragEnd={handleKnobDragEnd}
            />
          </div>
        </div>

        {/* 02 FILTER */}
        <div className={`p-3 flex flex-col justify-between ${theme.card}`}>
          <div className={`flex items-center justify-between pb-2 ${theme.cardHeader}`}>
            {renderPowerBtn(3, "02", "FILTER")}
            <div className="flex items-center gap-1.5">
              <LiveScope mode="filter" params={params} active={filterOn} theme={theme} />
              <span className={`text-[8.5px] font-mono-tech px-1.5 py-0.5 ${theme.badge}`}>12dB</span>
            </div>
          </div>
          <div className="grid grid-cols-2 gap-2 items-center flex-1">
            <Knob
              paramId={4}
              label="CUTOFF"
              value={params[4]}
              active={filterOn}
              theme={theme}
              onChange={handleKnobChange}
              onDragStart={handleKnobDragStart}
              onDragEnd={handleKnobDragEnd}
            />
            <Knob
              paramId={5}
              label="RES"
              value={params[5]}
              active={filterOn}
              theme={theme}
              onChange={handleKnobChange}
              onDragStart={handleKnobDragStart}
              onDragEnd={handleKnobDragEnd}
            />
          </div>
        </div>

        {/* 03 COMPRESSOR */}
        <div className={`p-3 flex flex-col justify-between ${theme.card}`}>
          <div className={`flex items-center justify-between pb-2 ${theme.cardHeader}`}>
            {renderPowerBtn(6, "03", "COMPRESSOR")}
            <span className={`text-[8.5px] font-mono-tech px-1.5 py-0.5 ${theme.badge}`}>DYNAMICS</span>
          </div>
          <div className="grid grid-cols-2 gap-2 items-center flex-1">
            <Knob
              paramId={7}
              label="AMOUNT"
              value={params[7]}
              active={compOn}
              theme={theme}
              onChange={handleKnobChange}
              onDragStart={handleKnobDragStart}
              onDragEnd={handleKnobDragEnd}
            />
            <div className="flex flex-col items-center justify-center px-2">
              <button
                onClick={() => handleToggle(8)}
                className={`w-full h-12 font-mono-tech text-xs font-black tracking-[0.22em] transition cursor-pointer flex items-center justify-center ${
                  slamOn && compOn ? theme.slamOn : theme.slamOff
                } ${compOn ? "" : "opacity-35"}`}
              >
                <span
                  className={
                    slamOn && compOn && themeId === "swiss"
                      ? "bg-[#F4F3EE] px-2.5 py-0.5 border border-[#0B0B0C]"
                      : ""
                  }
                >
                  SLAM
                </span>
              </button>
              <div
                className={`mt-2 text-[9.5px] font-mono-tech font-bold tracking-widest ${theme.mutedText} ${
                  compOn ? "" : "opacity-35"
                }`}
              >
                MODE: {slamOn ? "SLAM" : "GLUE"}
              </div>
            </div>
          </div>
        </div>

        {/* 04 MASTER */}
        <div className={`p-3 flex flex-col justify-between ${theme.card}`}>
          <div className={`flex items-center justify-between pb-2 ${theme.cardHeader}`}>
            {renderPowerBtn(9, "04", "MASTER")}
            <span className={`text-[8.5px] font-mono-tech px-1.5 py-0.5 ${theme.badge}`}>OUTPUT</span>
          </div>
          <div className="grid grid-cols-2 gap-2 items-center flex-1">
            <Knob
              paramId={10}
              label="MIX"
              value={params[10]}
              active={masterOn}
              theme={theme}
              onChange={handleKnobChange}
              onDragStart={handleKnobDragStart}
              onDragEnd={handleKnobDragEnd}
            />
            <Knob
              paramId={11}
              label="VOLUME"
              value={params[11]}
              active={masterOn}
              theme={theme}
              onChange={handleKnobChange}
              onDragStart={handleKnobDragStart}
              onDragEnd={handleKnobDragEnd}
            />
          </div>
        </div>
      </div>

      {/* INTEGRATED EDITORIAL PRESET DRAWER */}
      {browserOpen && (
        <PresetDrawer
          presets={presets}
          currentProgram={currentProgram}
          isCustomLabel={isCustomLabel}
          theme={theme}
          isNative={native}
          onSelectPreset={selectPreset}
          onInit={handleInit}
          onRandomize={handleRandomize}
          onLoadXml={() => callNative("loadPresetFile")}
          onSaveXml={() => callNative("savePresetFile")}
          onClose={() => setBrowserOpen(false)}
        />
      )}

      {/* JET-BLACK BOTTOM METERING BAR */}
      <MeterBar
        metersRef={metersRef}
        theme={theme}
        themeId={themeId}
        onSelectTheme={handleSelectTheme}
      />
    </div>
  );
}
