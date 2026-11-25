import React, { useState } from "react";

const CATEGORIES = [
  ["ALL", "ALL"],
  ["SATURATION", "GRIT"],
  ["DRUMS", "DRUMS"],
  ["BASS & 808", "BASS"],
  ["SYNTH & FILTER", "SYNTH"],
  ["MIX & MASTER", "MASTER"]
];

export default function PresetDrawer({
  presets,
  currentProgram,
  isCustomLabel,
  theme,
  isNative,
  onSelectPreset,
  onInit,
  onRandomize,
  onLoadXml,
  onSaveXml,
  onClose
}) {
  const [filterCat, setFilterCat] = useState("ALL");

  const filtered = presets
    .map((pr, idx) => ({ ...pr, idx }))
    .filter((pr) => filterCat === "ALL" || pr.category === filterCat);

  return (
    <div
      className={`absolute inset-x-3.5 top-[70px] bottom-[50px] z-20 p-3 flex flex-col justify-between overflow-hidden ${theme.browserBg}`}
    >
      {/* Top category filter strip */}
      <div className={`flex items-center justify-between gap-2 pb-2 shrink-0 ${theme.cardHeader}`}>
        <span className="text-[10.5px] font-mono-tech font-bold tracking-widest">
          PRESET INDEX · {String(presets.length).padStart(2, "0")}
        </span>
        <div className="flex items-center gap-1">
          {CATEGORIES.map(([key, lbl]) => (
            <button
              key={key}
              onClick={() => setFilterCat(key)}
              className={`px-2 py-0.5 text-[9.5px] font-mono-tech font-bold cursor-pointer transition ${
                filterCat === key ? theme.btnAccent : theme.btn
              }`}
            >
              {lbl}
            </button>
          ))}
        </div>
      </div>

      {/* 8x2 editorial preset grid — all 16 presets fit cleanly without scroll */}
      <div className="grid grid-cols-2 gap-1.5 my-2 flex-1 content-start min-h-0">
        {filtered.map((pr) => {
          const active = pr.idx === currentProgram && !isCustomLabel;
          return (
            <button
              key={pr.idx}
              onClick={() => onSelectPreset(pr.idx)}
              className={`flex items-center justify-between px-2.5 h-[28px] text-left text-[11px] transition cursor-pointer ${
                active ? theme.btnAccent : theme.btn
              }`}
            >
              <span className="flex items-center gap-2 truncate">
                <span className="font-mono-tech text-[9.5px] opacity-65">
                  {String(pr.idx + 1).padStart(2, "0")}
                </span>
                <span className="font-semibold truncate">{pr.name}</span>
              </span>
              <span className="font-mono-tech text-[8.5px] tracking-wider opacity-65 ml-2 shrink-0">
                {pr.category.split(" ")[0]}
              </span>
            </button>
          );
        })}
      </div>

      {/* Bottom utility actions */}
      <div className={`flex items-center justify-between gap-2 pt-2 shrink-0 border-t ${theme.dark ? "border-white/15" : "border-black/20"}`}>
        <div className="flex items-center gap-1.5">
          <button
            onClick={onInit}
            className={`px-2.5 py-1 text-[9.5px] font-mono-tech font-bold cursor-pointer transition ${theme.btn}`}
          >
            INIT (CLEAN)
          </button>
          {isNative ? (
            <>
              <button
                onClick={onLoadXml}
                className={`px-2.5 py-1 text-[9.5px] font-mono-tech font-bold cursor-pointer transition ${theme.btn}`}
              >
                LOAD .dcpreset
              </button>
              <button
                onClick={onSaveXml}
                className={`px-2.5 py-1 text-[9.5px] font-mono-tech font-bold cursor-pointer transition ${theme.btn}`}
              >
                SAVE .dcpreset
              </button>
            </>
          ) : (
            <button
              onClick={onRandomize}
              className={`px-2.5 py-1 text-[9.5px] font-mono-tech font-bold cursor-pointer transition ${theme.btn}`}
            >
              RANDOMIZE
            </button>
          )}
        </div>
        <button
          onClick={onClose}
          className={`px-3 py-1 text-[9.5px] font-mono-tech font-bold cursor-pointer transition ${theme.btnAccent}`}
        >
          CLOSE [ESC]
        </button>
      </div>
    </div>
  );
}
