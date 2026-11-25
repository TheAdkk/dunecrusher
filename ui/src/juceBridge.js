import { PARAM_IDS, SLIDER_INDICES, TOGGLE_INDICES } from "./dspCurves.js";

// Native JUCE 8 WebBrowserComponent relay bridge with graceful browser preview fallback
export function isJuceEnvironment() {
  return typeof window !== "undefined" && Boolean(window.__JUCE__ && window.__JUCE__.backend);
}

let nextPromiseId = 0;
const pendingPromises = new Map();
let promiseListenerInstalled = false;

function ensurePromiseListener() {
  if (promiseListenerInstalled || !isJuceEnvironment()) return;
  promiseListenerInstalled = true;
  window.__JUCE__.backend.addEventListener("__juce__complete", ({ promiseId, result }) => {
    const entry = pendingPromises.get(promiseId);
    if (entry) {
      pendingPromises.delete(promiseId);
      entry.resolve(result);
    }
  });
}

export function callNative(name, ...args) {
  if (!isJuceEnvironment()) return Promise.resolve(null);
  ensurePromiseListener();
  const promiseId = nextPromiseId++;
  return new Promise((resolve) => {
    pendingPromises.set(promiseId, { resolve });
    window.__JUCE__.backend.emitEvent("__juce__invoke", {
      name,
      params: args,
      resultId: promiseId
    });
  });
}

export function createJuceBridge({ onParamChange, onPresetState, onMeters }) {
  if (!isJuceEnvironment()) {
    return {
      isNative: false,
      setSliderValue: () => {},
      beginSliderGesture: () => {},
      endSliderGesture: () => {},
      setToggleValue: () => {},
      requestInitialState: () => {},
      dispose: () => {}
    };
  }

  const backend = window.__JUCE__.backend;
  const cleanups = [];

  for (const idx of SLIDER_INDICES) {
    const id = "__juce__slider" + PARAM_IDS[idx];
    const token = backend.addEventListener(id, (ev) => {
      if (ev && ev.eventType === "valueChanged" && typeof ev.value === "number") {
        onParamChange(idx, Math.max(0, Math.min(1, ev.value)));
      }
    });
    cleanups.push(() => backend.removeEventListener(token));
    backend.emitEvent(id, { eventType: "requestInitialUpdate" });
  }

  for (const idx of TOGGLE_INDICES) {
    const id = "__juce__toggle" + PARAM_IDS[idx];
    const token = backend.addEventListener(id, (ev) => {
      if (ev && ev.eventType === "valueChanged") {
        onParamChange(idx, ev.value ? 1 : 0);
      }
    });
    cleanups.push(() => backend.removeEventListener(token));
    backend.emitEvent(id, { eventType: "requestInitialUpdate" });
  }

  const presetToken = backend.addEventListener("presetState", (payload) => {
    if (payload && onPresetState) onPresetState(payload);
  });
  cleanups.push(() => backend.removeEventListener(presetToken));

  const meterToken = backend.addEventListener("meters", (payload) => {
    if (payload && onMeters) onMeters(payload);
  });
  cleanups.push(() => backend.removeEventListener(meterToken));

  return {
    isNative: true,
    beginSliderGesture(idx) {
      backend.emitEvent("__juce__slider" + PARAM_IDS[idx], { eventType: "sliderDragStarted" });
    },
    setSliderValue(idx, norm) {
      const clamped = Math.max(0, Math.min(1, norm));
      backend.emitEvent("__juce__slider" + PARAM_IDS[idx], {
        eventType: "valueChanged",
        value: clamped
      });
    },
    endSliderGesture(idx) {
      backend.emitEvent("__juce__slider" + PARAM_IDS[idx], { eventType: "sliderDragEnded" });
    },
    setToggleValue(idx, boolVal) {
      backend.emitEvent("__juce__toggle" + PARAM_IDS[idx], {
        eventType: "valueChanged",
        value: Boolean(boolVal)
      });
    },
    requestInitialState() {
      callNative("getPresetState").then((res) => {
        if (res && onPresetState) onPresetState(res);
      });
    },
    dispose() {
      for (const fn of cleanups) {
        try {
          fn();
        } catch {
          // Ignore teardown listener removal errors
        }
      }
    }
  };
}
