(() => {
  'use strict';

  const $ = (selector) => document.querySelector(selector);
  const $$ = (selector) => [...document.querySelectorAll(selector)];
  const clamp = (value, low, high) => Math.max(low, Math.min(high, value));
  const noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
  const scaleNames = ['Chromatic', 'Major', 'Natural Minor', 'Harmonic Minor', 'Melodic Minor', 'Major Pentatonic', 'Minor Pentatonic', 'Blues', 'Dorian', 'Phrygian', 'Lydian', 'Mixolydian', 'Locrian'];
  const scales = [
    [0,1,2,3,4,5,6,7,8,9,10,11], [0,2,4,5,7,9,11], [0,2,3,5,7,8,10],
    [0,2,3,5,7,8,11], [0,2,3,5,7,9,11], [0,2,4,7,9], [0,3,5,7,10],
    [0,3,5,6,7,10], [0,2,3,5,7,9,10], [0,1,3,5,7,8,10],
    [0,2,4,6,7,9,11], [0,2,4,5,7,9,10], [0,1,3,5,6,8,10]
  ];

  const defaults = {
    rootNote: 0, scaleType: 0, scaleLock: false, transpose: 0, octaveShift: 0,
    inputThresholdDb: -60, confidenceThreshold: 0.65, minFrequency: 50, maxFrequency: 1500,
    noteStabilityMs: 35, noteReleaseMs: 100, minMidiNote: 0, maxMidiNote: 127,
    midiChannel: 1, velocityMode: 0, fixedVelocity: 100, minVelocity: 20, maxVelocity: 127,
    velocityCurve: 0, legato: false, midiOutputEnabled: true,
    glideEnabled: false, glideMode: 0, glideTimeMs: 120, glideCurve: 0, glideMinInterval: 0,
    pitchBendEnabled: true, pitchBendRange: 0, pitchBendAmount: 0.8,
    pitchBendSmoothingMs: 35, pitchBendDeadZoneCents: 3
  };
  const state = { ...defaults };
  const bendRanges = [2, 3, 5, 12, 24];

  const controlSets = {
    '#scale-controls': [
      { id: 'rootNote', type: 'choice', label: 'Root Note', options: noteNames },
      { id: 'scaleType', type: 'choice', label: 'Scale', options: scaleNames },
      { id: 'scaleLock', type: 'toggle', label: 'Scale Lock' },
      { id: 'transpose', type: 'range', label: 'Transpose', min: -24, max: 24, step: 1, unit: ' st' },
      { id: 'octaveShift', type: 'range', label: 'Octave Shift', min: -2, max: 2, step: 1, unit: ' oct' }
    ],
    '#detection-controls': [
      { id: 'inputThresholdDb', type: 'range', label: 'Input Threshold', min: -80, max: 0, step: 0.1, unit: ' dB', decimals: 1 },
      { id: 'confidenceThreshold', type: 'range', label: 'Confidence Threshold', min: 0, max: 1, step: 0.01, factor: 100, unit: ' %' },
      { id: 'minFrequency', type: 'range', label: 'Min Frequency', min: 50, max: 1500, step: 1, unit: ' Hz' },
      { id: 'maxFrequency', type: 'range', label: 'Max Frequency', min: 50, max: 1500, step: 1, unit: ' Hz' },
      { id: 'noteStabilityMs', type: 'range', label: 'Stability Time', min: 0, max: 500, step: 1, unit: ' ms' },
      { id: 'noteReleaseMs', type: 'range', label: 'Release Time', min: 0, max: 1000, step: 1, unit: ' ms' }
    ],
    '#midi-controls': [
      { id: 'midiChannel', type: 'range', label: 'MIDI Channel', min: 1, max: 16, step: 1 },
      { id: 'velocityMode', type: 'choice', label: 'Velocity Mode', options: ['Dynamic', 'Fixed'] },
      { id: 'fixedVelocity', type: 'range', label: 'Fixed Velocity', min: 1, max: 127, step: 1 },
      { id: 'velocityCurve', type: 'choice', label: 'Velocity Curve', options: ['Linear', 'Soft', 'Hard'] },
      { id: 'minVelocity', type: 'range', label: 'Min Velocity', min: 1, max: 127, step: 1 },
      { id: 'maxVelocity', type: 'range', label: 'Max Velocity', min: 1, max: 127, step: 1 },
      { id: 'minMidiNote', type: 'range', label: 'Min MIDI Note', min: 0, max: 127, step: 1 },
      { id: 'maxMidiNote', type: 'range', label: 'Max MIDI Note', min: 0, max: 127, step: 1 },
      { id: 'legato', type: 'toggle', label: 'Legato Mode' },
      { id: 'midiOutputEnabled', type: 'toggle', label: 'MIDI Output Enabled' }
    ],
    '#glide-controls': [
      { id: 'glideEnabled', type: 'toggle', label: 'Glide Enabled' },
      { id: 'glideMode', type: 'choice', label: 'Glide Mode', options: ['Off', 'Legato Only', 'Always'] },
      { id: 'glideTimeMs', type: 'range', label: 'Glide Time', min: 0, max: 1000, step: 1, unit: ' ms' },
      { id: 'glideCurve', type: 'choice', label: 'Glide Curve', options: ['Linear', 'Exponential', 'Smooth'] },
      { id: 'glideMinInterval', type: 'range', label: 'Minimum Interval', min: 0, max: 24, step: 1, unit: ' st' }
    ],
    '#bend-controls': [
      { id: 'pitchBendEnabled', type: 'toggle', label: 'Pitch Bend Enabled' },
      { id: 'pitchBendRange', type: 'choice', label: 'Pitch Bend Range', options: ['±2 semitones', '±3 semitones', '±5 semitones', '±12 semitones', '±24 semitones'] },
      { id: 'pitchBendAmount', type: 'range', label: 'Bend Amount', min: 0, max: 1, step: 0.01, factor: 100, unit: ' %' },
      { id: 'pitchBendSmoothingMs', type: 'range', label: 'Bend Smoothing', min: 0, max: 300, step: 1, unit: ' ms' },
      { id: 'pitchBendDeadZoneCents', type: 'range', label: 'Bend Dead Zone', min: 0, max: 100, step: 0.1, unit: ' cents', decimals: 1 }
    ]
  };

  const controlElements = new Map();
  let toastTimer = 0;
  function toast(message, error = false) {
    const element = $('#toast');
    element.textContent = message;
    element.classList.toggle('error', error);
    element.classList.add('show');
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => element.classList.remove('show'), 4200);
  }

  function formatControl(descriptor, value) {
    const display = value * (descriptor.factor || 1);
    return `${display.toFixed(descriptor.decimals || 0)}${descriptor.unit || ''}`;
  }

  function parameterChanged(id) {
    if (id === 'minFrequency' && state.minFrequency >= state.maxFrequency) {
      state.maxFrequency = Math.min(1500, state.minFrequency + 1);
      syncControl('maxFrequency');
    }
    if (id === 'maxFrequency' && state.maxFrequency <= state.minFrequency) {
      state.minFrequency = Math.max(50, state.maxFrequency - 1);
      syncControl('minFrequency');
    }
    if ((id === 'minFrequency' || id === 'maxFrequency') && workletNode) {
      workletNode.port.postMessage({ type: 'range', min: state.minFrequency, max: state.maxFrequency });
    }
  }

  function renderControls() {
    Object.entries(controlSets).forEach(([containerSelector, descriptors]) => {
      const container = $(containerSelector);
      descriptors.forEach((descriptor) => {
        const wrapper = document.createElement('div');
        wrapper.className = `control ${descriptor.type === 'toggle' ? 'toggle-control' : ''}`;
        const id = `control-${descriptor.id}`;
        if (descriptor.type === 'toggle') {
          const label = document.createElement('label');
          label.htmlFor = id;
          const input = document.createElement('input');
          input.type = 'checkbox'; input.id = id;
          const text = document.createElement('span'); text.textContent = descriptor.label;
          label.append(input, text); wrapper.append(label);
          input.addEventListener('change', () => { state[descriptor.id] = input.checked; parameterChanged(descriptor.id); });
          controlElements.set(descriptor.id, { input, descriptor });
        } else if (descriptor.type === 'choice') {
          const label = document.createElement('label'); label.htmlFor = id; label.textContent = descriptor.label;
          const select = document.createElement('select'); select.id = id;
          descriptor.options.forEach((option, index) => select.add(new Option(option, index)));
          select.addEventListener('change', () => { state[descriptor.id] = Number(select.value); parameterChanged(descriptor.id); });
          wrapper.append(label, select);
          controlElements.set(descriptor.id, { input: select, descriptor });
        } else {
          const label = document.createElement('label'); label.htmlFor = id;
          const text = document.createElement('span'); text.textContent = descriptor.label;
          const output = document.createElement('output'); output.htmlFor = id;
          label.append(text, output);
          const input = document.createElement('input');
          Object.assign(input, { type: 'range', id, min: descriptor.min, max: descriptor.max, step: descriptor.step });
          input.addEventListener('input', () => {
            state[descriptor.id] = Number(input.value);
            output.textContent = formatControl(descriptor, state[descriptor.id]);
            parameterChanged(descriptor.id);
          });
          wrapper.append(label, input);
          controlElements.set(descriptor.id, { input, output, descriptor });
        }
        container.append(wrapper);
      });
    });
    syncControls();
  }

  function syncControl(id) {
    const control = controlElements.get(id);
    if (!control) return;
    const { input, output, descriptor } = control;
    if (descriptor.type === 'toggle') input.checked = Boolean(state[id]);
    else input.value = state[id];
    if (output) output.textContent = formatControl(descriptor, state[id]);
  }
  function syncControls() { controlElements.forEach((_, id) => syncControl(id)); }

  function midiToFrequency(note) { return 440 * Math.pow(2, (note - 69) / 12); }
  function frequencyToMidi(frequency) { return frequency > 0 ? 69 + 12 * Math.log2(frequency / 440) : 0; }
  function midiNoteName(note) {
    if (!Number.isFinite(note) || note < 0 || note > 127) return '—';
    const rounded = Math.round(note);
    return `${noteNames[rounded % 12]}${Math.floor(rounded / 12) - 1}`;
  }
  function quantize(midi, root, scale, locked) {
    const nearest = Math.round(clamp(midi, 0, 127));
    if (!locked || scale === 0) return nearest;
    let best = nearest, bestDistance = 1000;
    for (let note = Math.max(0, nearest - 12); note <= Math.min(127, nearest + 12); note++) {
      const pitchClass = ((note - root) % 12 + 12) % 12;
      if (!scales[scale].includes(pitchClass)) continue;
      const distance = Math.abs(midi - note);
      if (distance < bestDistance - 0.0001 || (Math.abs(distance - bestDistance) < 0.0001 && note > best)) {
        best = note; bestDistance = distance;
      }
    }
    return best;
  }
  function pitchBend14Bit(semitones) {
    const range = bendRanges[state.pitchBendRange];
    const normalized = clamp(semitones / range, -1, 1);
    return clamp(Math.round(8192 + normalized * (normalized >= 0 ? 8191 : 8192)), 0, 16383);
  }

  class SynthEngine {
    constructor(context) {
      this.context = context;
      this.master = context.createGain();
      this.compressor = context.createDynamicsCompressor();
      this.compressor.threshold.value = -14;
      this.compressor.ratio.value = 5;
      this.master.connect(this.compressor).connect(context.destination);
      this.voices = new Map();
      this.volume = 0.25;
      this.enabled = true;
      this.updateMaster();
    }
    updateMaster() { this.master.gain.setTargetAtTime(this.enabled ? this.volume : 0, this.context.currentTime, 0.015); }
    setVolume(value) { this.volume = value; this.updateMaster(); }
    setEnabled(value) { this.enabled = value; this.updateMaster(); }
    noteOn(note, velocity) {
      this.noteOff(note, 0.025);
      const now = this.context.currentTime;
      const oscillator = this.context.createOscillator();
      const filter = this.context.createBiquadFilter();
      const gain = this.context.createGain();
      oscillator.type = 'sawtooth';
      oscillator.frequency.value = midiToFrequency(note);
      filter.type = 'lowpass'; filter.frequency.value = 1500; filter.Q.value = 0.7;
      gain.gain.setValueAtTime(0, now);
      gain.gain.linearRampToValueAtTime(0.32 * velocity / 127, now + 0.015);
      oscillator.connect(filter).connect(gain).connect(this.master);
      oscillator.start(now);
      oscillator.onended = () => { oscillator.disconnect(); filter.disconnect(); gain.disconnect(); };
      this.voices.set(note, { oscillator, gain, filter });
    }
    noteOff(note, release = 0.08) {
      const voice = this.voices.get(note);
      if (!voice) return;
      this.voices.delete(note);
      const now = this.context.currentTime;
      voice.gain.gain.cancelScheduledValues(now);
      voice.gain.gain.setValueAtTime(voice.gain.gain.value, now);
      voice.gain.gain.linearRampToValueAtTime(0, now + release);
      voice.oscillator.stop(now + release + 0.02);
    }
    bend(semitones) {
      const now = this.context.currentTime;
      this.voices.forEach((voice, note) => voice.oscillator.frequency.setTargetAtTime(midiToFrequency(note + semitones), now, 0.008));
    }
    panic() { [...this.voices.keys()].forEach((note) => this.noteOff(note, 0.025)); this.bend(0); }
  }

  let audioContext = null;
  let workletNode = null;
  let monitorGain = null;
  let sourceNode = null;
  let mediaStream = null;
  let synth = null;
  let demo = null;
  let demoTimer = 0;
  let running = false;
  let startPending = false;
  let currentSource = 'demo';
  let fileData = null;
  let midiAccess = null;
  let midiOutput = null;

  let activeNote = -1;
  let activeChannel = 1;
  let candidateNote = -1;
  let candidateMs = 0;
  let unvoicedMs = 0;
  let lastNote = -1;
  let vocalBend = 0;
  let lastSentBend = 8192;
  let bendAccumulator = 1000;
  let glideActive = false;
  let glideElapsed = 0;
  let glideDuration = 0;
  let glideStart = 0;
  let glideCurrent = 0;
  let glideCurve = 0;
  let lastPitchTime = 0;
  let latestPitch = { frequency: 0, confidence: 0, rms: 0, voiceDetected: false };
  let latestDeviation = 0;

  function emitNoteOn(note, velocity, channel) {
    if (synth) synth.noteOn(note, velocity);
    if (midiOutput) midiOutput.send([0x90 | ((channel - 1) & 15), note, velocity]);
  }
  function emitNoteOff(note, channel) {
    if (synth) synth.noteOff(note);
    if (midiOutput) midiOutput.send([0x80 | ((channel - 1) & 15), note, 0]);
  }
  function emitBendValue(value, semitones, channel, force = false) {
    if (!force && Math.abs(value - lastSentBend) < 4) return;
    if (synth) synth.bend(semitones);
    if (midiOutput) midiOutput.send([0xE0 | ((channel - 1) & 15), value & 127, (value >> 7) & 127]);
    lastSentBend = value;
  }
  function emitBend(semitones, channel, force = false) { emitBendValue(pitchBend14Bit(semitones), semitones, channel, force); }
  function centreBend(channel, force = false) {
    if (!force && lastSentBend === 8192) return;
    emitBendValue(8192, 0, channel, force);
  }

  function calculateVelocity(pitch) {
    const low = Math.max(1, Math.min(state.minVelocity, state.maxVelocity));
    const high = Math.min(127, Math.max(state.minVelocity, state.maxVelocity));
    if (state.velocityMode === 1) return clamp(state.fixedVelocity, low, high);
    const db = 20 * Math.log10(Math.max(pitch.rms, 1e-12));
    let amount = clamp((db - state.inputThresholdDb) / Math.max(1, -state.inputThresholdDb), 0, 1);
    if (state.velocityCurve === 1) amount = Math.sqrt(amount);
    else if (state.velocityCurve === 2) amount *= amount;
    return clamp(Math.round(low + amount * (high - low)), 1, 127);
  }

  function startNote(note, pitch) {
    const oldNote = activeNote;
    const oldChannel = activeChannel;
    const sourceNote = oldNote >= 0 ? oldNote : lastNote;
    const modeAllows = state.glideMode === 2 || (state.glideMode === 1 && oldNote >= 0);
    const interval = sourceNote >= 0 ? Math.abs(sourceNote - note) : 0;
    glideActive = state.glideEnabled && state.glideMode !== 0 && modeAllows && sourceNote >= 0 && state.glideTimeMs > 0 && interval >= state.glideMinInterval && interval > 0;
    glideElapsed = 0; glideDuration = state.glideTimeMs; glideCurve = state.glideCurve;
    const range = bendRanges[state.pitchBendRange];
    glideStart = glideActive ? clamp(sourceNote - note, -0.95 * range, 0.95 * range) : 0;
    glideCurrent = glideStart; vocalBend = 0;
    if (glideActive) emitBend(glideCurrent, state.midiChannel, true);
    else centreBend(state.midiChannel);

    const velocity = calculateVelocity(pitch);
    if (oldNote >= 0 && state.legato) {
      emitNoteOn(note, velocity, state.midiChannel);
      emitNoteOff(oldNote, oldChannel);
    } else {
      if (oldNote >= 0) emitNoteOff(oldNote, oldChannel);
      emitNoteOn(note, velocity, state.midiChannel);
    }
    activeNote = note; activeChannel = state.midiChannel; lastNote = note;
    candidateNote = -1; candidateMs = 0; unvoicedMs = 0; bendAccumulator = 1000;
  }

  function stopActiveNote() {
    if (activeNote >= 0) {
      const channel = activeChannel;
      emitNoteOff(activeNote, channel);
      lastNote = activeNote;
      activeNote = -1;
      centreBend(channel);
    }
    candidateNote = -1; candidateMs = 0; vocalBend = 0;
    glideActive = false; glideCurrent = 0; latestDeviation = 0;
  }

  function updatePitchBend(elapsedMs) {
    if (glideActive) {
      glideElapsed += elapsedMs;
      const progress = clamp(glideElapsed / Math.max(1, glideDuration), 0, 1);
      let remaining = 1 - progress;
      if (glideCurve === 1) {
        const end = Math.exp(-5);
        remaining = clamp((Math.exp(-5 * progress) - end) / (1 - end), 0, 1);
      } else if (glideCurve === 2) remaining = 1 - progress * progress * (3 - 2 * progress);
      glideCurrent = glideStart * remaining;
      if (progress >= 1) { glideActive = false; glideCurrent = 0; }
    }
    let desired = 0;
    if (state.pitchBendEnabled && activeNote >= 0 && candidateNote < 0) {
      const deviation = latestRawMidi - activeNote;
      const deadZone = state.pitchBendDeadZoneCents / 100;
      const magnitude = Math.max(0, Math.abs(deviation) - deadZone);
      desired = Math.sign(deviation) * magnitude * state.pitchBendAmount;
      desired = clamp(desired, -bendRanges[state.pitchBendRange], bendRanges[state.pitchBendRange]);
    }
    const smoothing = Math.max(0, state.pitchBendSmoothingMs);
    const coefficient = smoothing <= 0 ? 1 : 1 - Math.exp(-elapsedMs / smoothing);
    vocalBend += (desired - vocalBend) * coefficient;
    bendAccumulator += elapsedMs;
    if (bendAccumulator >= 10) {
      emitBend(vocalBend + glideCurrent, activeChannel);
      bendAccumulator = 0;
    }
  }

  let latestRawMidi = 0;
  const rawHistory = [];
  const targetHistory = [];
  function pushHistory(raw, target) {
    rawHistory.push(raw); targetHistory.push(target);
    if (rawHistory.length > 260) { rawHistory.shift(); targetHistory.shift(); }
  }

  function handlePitch(pitch) {
    if (!running) return;
    const now = performance.now();
    const elapsedMs = lastPitchTime ? clamp(now - lastPitchTime, 0, 100) : 10;
    lastPitchTime = now;
    latestPitch = pitch;

    if (!state.midiOutputEnabled) {
      if (activeNote >= 0) stopActiveNote();
      candidateNote = -1; candidateMs = 0;
      pushHistory(null, null);
      return;
    }
    const inputDb = 20 * Math.log10(Math.max(pitch.rms, 1e-12));
    const voiced = pitch.voiceDetected && pitch.frequency > 0 && pitch.confidence >= state.confidenceThreshold && inputDb >= state.inputThresholdDb;
    if (!voiced) {
      candidateNote = -1; candidateMs = 0; unvoicedMs += elapsedMs;
      if (activeNote >= 0 && unvoicedMs >= state.noteReleaseMs) stopActiveNote();
      else if (activeNote < 0) centreBend(state.midiChannel);
      latestRawMidi = 0;
      pushHistory(null, activeNote >= 0 ? activeNote : null);
      return;
    }

    unvoicedMs = 0;
    latestRawMidi = frequencyToMidi(pitch.frequency) + state.transpose + 12 * state.octaveShift;
    const low = Math.min(state.minMidiNote, state.maxMidiNote);
    const high = Math.max(state.minMidiNote, state.maxMidiNote);
    const target = clamp(quantize(latestRawMidi, state.rootNote, state.scaleType, state.scaleLock), low, high);
    latestDeviation = (latestRawMidi - target) * 100;

    if (target !== activeNote) {
      if (candidateNote !== target) { candidateNote = target; candidateMs = 0; }
      else candidateMs += elapsedMs;
      if (candidateMs + 0.001 >= state.noteStabilityMs) startNote(target, pitch);
    } else {
      candidateNote = -1; candidateMs = 0;
    }
    if (activeNote >= 0) updatePitchBend(elapsedMs);
    pushHistory(frequencyToMidi(pitch.frequency), activeNote >= 0 ? activeNote : null);
  }

  function panicNotes() {
    if (activeNote >= 0) emitNoteOff(activeNote, activeChannel);
    if (synth) synth.panic();
    centreBend(activeChannel, true);
    activeNote = -1; candidateNote = -1; candidateMs = 0; unvoicedMs = 0;
    lastNote = -1; vocalBend = 0; glideActive = false; glideCurrent = 0; latestDeviation = 0;
  }

  function createDemoSource() {
    const context = audioContext;
    const oscillator = context.createOscillator();
    const real = new Float32Array([0, 0, 0, 0, 0, 0]);
    const imaginary = new Float32Array([0, 1, 0.32, 0.16, 0.08, 0.035]);
    oscillator.setPeriodicWave(context.createPeriodicWave(real, imaginary));
    const gain = context.createGain(); gain.gain.value = 0;
    const lfo = context.createOscillator(); const lfoGain = context.createGain();
    lfo.frequency.value = 5.4; lfoGain.gain.value = 9;
    lfo.connect(lfoGain).connect(oscillator.detune);
    oscillator.connect(gain).connect(workletNode);
    oscillator.start(); lfo.start();
    sourceNode = gain;
    demo = { oscillator, gain, lfo, nextTime: context.currentTime + 0.12, step: -1 };
    const sequence = [57, 60, 62, 64, 67, 64, 62, 60, 55, 57, 62, 64, 60, 57];
    const schedule = () => {
      if (!demo || !audioContext) return;
      while (demo.nextTime < audioContext.currentTime + 0.16) {
        demo.step = (demo.step + 1) % sequence.length;
        const time = demo.nextTime;
        const note = sequence[demo.step];
        const duration = demo.step % 4 === 3 ? 0.52 : 0.64;
        const gap = demo.step % 3 === 2 ? 0.15 : 0.035;
        oscillator.frequency.cancelScheduledValues(time);
        oscillator.frequency.setTargetAtTime(midiToFrequency(note), time, 0.025);
        gain.gain.cancelScheduledValues(time);
        gain.gain.setValueAtTime(0, time);
        gain.gain.linearRampToValueAtTime(0.42, time + 0.045);
        gain.gain.setValueAtTime(0.42, Math.max(time + 0.045, time + duration - 0.07));
        gain.gain.linearRampToValueAtTime(0, time + duration);
        demo.nextTime = time + duration + gap;
      }
    };
    schedule();
    demoTimer = setInterval(schedule, 50);
  }

  async function startAudio() {
    if (startPending || running) return;
    startPending = true;
    const button = $('#start-button'); button.disabled = true;
    try {
      const Context = window.AudioContext || window.webkitAudioContext;
      if (!Context) throw new Error('Web Audio n’est pas disponible dans ce navigateur.');
      audioContext = new Context({ latencyHint: 'interactive' });
      await audioContext.resume();
      await audioContext.audioWorklet.addModule('./pitch-worklet.js');
      workletNode = new AudioWorkletNode(audioContext, 'yin-pitch-processor', {
        numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [1], channelCount: 2, channelCountMode: 'max'
      });
      workletNode.port.onmessage = (event) => { if (event.data.type === 'pitch') handlePitch(event.data); };
      workletNode.port.postMessage({ type: 'range', min: state.minFrequency, max: state.maxFrequency });
      monitorGain = audioContext.createGain();
      monitorGain.gain.value = $('#monitor-enabled').checked ? 0.55 : 0;
      workletNode.connect(monitorGain).connect(audioContext.destination);
      synth = new SynthEngine(audioContext);
      synth.setVolume(Number($('#synth-volume').value) / 100);
      synth.setEnabled($('#synth-enabled').checked);
      running = true;

      if (currentSource === 'demo') createDemoSource();
      else if (currentSource === 'mic') {
        if (!navigator.mediaDevices?.getUserMedia) throw new Error('Le microphone nécessite un navigateur récent et une page HTTPS.');
        mediaStream = await navigator.mediaDevices.getUserMedia({ audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false } });
        sourceNode = audioContext.createMediaStreamSource(mediaStream);
        sourceNode.connect(workletNode);
      } else {
        if (!fileData) throw new Error('Choisis d’abord un fichier audio monophonique.');
        const buffer = await audioContext.decodeAudioData(fileData.slice(0));
        sourceNode = audioContext.createBufferSource();
        sourceNode.buffer = buffer; sourceNode.loop = true; sourceNode.connect(workletNode); sourceNode.start();
      }

      button.disabled = false; button.classList.add('stop');
      button.innerHTML = '<svg viewBox="0 0 20 20" aria-hidden="true"><path d="M6 6h8v8H6z" fill="currentColor"/></svg><span>Arrêter le test</span>';
      $('#engine-status').textContent = currentSource === 'demo' ? 'VOIX DE TEST ACTIVE' : currentSource === 'mic' ? 'MICROPHONE ACTIF' : 'FICHIER EN LECTURE';
      $('#status-dot').classList.add('running');
      $('#canvas-empty').classList.add('hidden');
      toast(currentSource === 'demo' ? 'Mélodie de test lancée. Essaie maintenant le preset 808 Glide ou Hard Tune MIDI.' : 'Analyse démarrée. Chante une note stable et monophonique.');
    } catch (error) {
      console.error(error);
      toast(error.message || 'Impossible de démarrer l’audio.', true);
      await stopAudio();
    } finally {
      startPending = false; button.disabled = false;
    }
  }

  async function stopAudio() {
    running = false;
    clearInterval(demoTimer); demoTimer = 0;
    panicNotes();
    if (demo) {
      try { demo.oscillator.stop(); } catch (_) {}
      try { demo.lfo.stop(); } catch (_) {}
      demo = null;
    }
    if (mediaStream) { mediaStream.getTracks().forEach((track) => track.stop()); mediaStream = null; }
    if (sourceNode) {
      try { if (currentSource === 'file') sourceNode.stop(); } catch (_) {}
      try { sourceNode.disconnect(); } catch (_) {}
      sourceNode = null;
    }
    try { workletNode?.disconnect(); monitorGain?.disconnect(); } catch (_) {}
    if (audioContext) { try { await audioContext.close(); } catch (_) {} }
    audioContext = null; workletNode = null; monitorGain = null; synth = null;
    lastPitchTime = 0; latestPitch = { frequency: 0, confidence: 0, rms: 0, voiceDetected: false };
    latestRawMidi = 0; latestDeviation = 0;
    const button = $('#start-button');
    button.classList.remove('stop'); button.disabled = false;
    button.innerHTML = '<svg viewBox="0 0 20 20" aria-hidden="true"><path d="M7 5.5v9l7-4.5z" fill="currentColor"/></svg><span>Démarrer le test</span>';
    $('#engine-status').textContent = 'PRÊT À TESTER';
    $('#status-dot').classList.remove('running');
  }

  function updateSourceUI() {
    $$('.segment').forEach((button) => button.classList.toggle('active', button.dataset.source === currentSource));
    $('#file-button').hidden = currentSource !== 'file';
  }

  $$('.segment').forEach((button) => button.addEventListener('click', async () => {
    const next = button.dataset.source;
    if (next === currentSource) return;
    currentSource = next; updateSourceUI();
    if (next === 'file' && !fileData) $('#audio-file').click();
    if (running) { await stopAudio(); if (next !== 'file' || fileData) await startAudio(); }
  }));
  $('#file-button').addEventListener('click', () => $('#audio-file').click());
  $('#audio-file').addEventListener('change', async (event) => {
    const file = event.target.files[0];
    if (!file) return;
    fileData = await file.arrayBuffer();
    $('#file-name').hidden = false; $('#file-name').textContent = `${file.name} · ${(file.size / 1048576).toFixed(1)} Mo`;
    currentSource = 'file'; updateSourceUI();
    if (running) { await stopAudio(); await startAudio(); }
  });
  $('#start-button').addEventListener('click', () => running ? stopAudio() : startAudio());
  $('#panic-button').addEventListener('click', () => { panicNotes(); toast('Notes MIDI fermées et pitch bend recentré.'); });
  $('#monitor-enabled').addEventListener('change', (event) => {
    if (monitorGain && audioContext) monitorGain.gain.setTargetAtTime(event.target.checked ? 0.55 : 0, audioContext.currentTime, 0.02);
    if (event.target.checked && currentSource === 'mic') toast('Utilise un casque pour éviter le Larsen avec le microphone.');
  });
  $('#synth-enabled').addEventListener('change', (event) => synth?.setEnabled(event.target.checked));
  $('#synth-volume').addEventListener('input', (event) => {
    $('#volume-output').textContent = `${event.target.value} %`;
    synth?.setVolume(Number(event.target.value) / 100);
  });

  const presetOverrides = {
    'Natural Vocal MIDI': {},
    'Hard Tune MIDI': { scaleType: 1, scaleLock: true, noteStabilityMs: 0, noteReleaseMs: 50, pitchBendEnabled: false, pitchBendSmoothingMs: 5 },
    '808 Glide': { rootNote: 9, scaleType: 6, scaleLock: true, octaveShift: -1, inputThresholdDb: -55, confidenceThreshold: 0.60, noteStabilityMs: 15, noteReleaseMs: 80, legato: true, glideEnabled: true, glideMode: 2, glideTimeMs: 280, glideCurve: 2, pitchBendRange: 3, pitchBendAmount: 0.4, pitchBendSmoothingMs: 25 },
    'Expressive Synth Lead': { noteStabilityMs: 20, noteReleaseMs: 80, legato: true, glideEnabled: true, glideMode: 1, glideTimeMs: 90, glideCurve: 2, pitchBendRange: 2, pitchBendAmount: 1, pitchBendSmoothingMs: 15, pitchBendDeadZoneCents: 1 },
    'Piano Notes': { noteStabilityMs: 55, noteReleaseMs: 140, velocityCurve: 1, pitchBendEnabled: false },
    'Bass Vocal': { rootNote: 9, scaleType: 6, scaleLock: true, octaveShift: -1, minFrequency: 50, maxFrequency: 350, noteStabilityMs: 40, noteReleaseMs: 130, pitchBendAmount: 0.5, pitchBendSmoothingMs: 45 }
  };
  $('#apply-preset').addEventListener('click', () => {
    const name = $('#preset-select').value;
    panicNotes(); Object.assign(state, defaults, presetOverrides[name]); syncControls();
    if (workletNode) workletNode.port.postMessage({ type: 'range', min: state.minFrequency, max: state.maxFrequency });
    toast(`Preset « ${name} » appliqué.`);
  });

  async function populateMidiOutputs() {
    const select = $('#midi-outputs');
    select.innerHTML = '';
    if (!midiAccess || midiAccess.outputs.size === 0) {
      select.add(new Option('Aucune sortie MIDI détectée')); select.disabled = true; midiOutput = null; return;
    }
    midiAccess.outputs.forEach((output) => select.add(new Option(output.name || `MIDI ${output.id}`, output.id)));
    select.disabled = false;
    midiOutput = midiAccess.outputs.get(select.value) || [...midiAccess.outputs.values()][0];
    select.value = midiOutput.id;
  }
  $('#midi-outputs').addEventListener('change', (event) => { midiOutput = midiAccess?.outputs.get(event.target.value) || null; });
  $('#web-midi-button').addEventListener('click', async () => {
    if (!navigator.requestMIDIAccess) {
      $('#midi-support').textContent = 'Web MIDI n’est pas disponible ici. Utilise Chrome ou Edge, ou teste simplement le synthé interne.';
      toast('Web MIDI non disponible dans ce navigateur ou cette prévisualisation.', true); return;
    }
    try {
      midiAccess = await navigator.requestMIDIAccess({ sysex: false });
      midiAccess.onstatechange = populateMidiOutputs;
      await populateMidiOutputs();
      $('#web-midi-button').textContent = 'Web MIDI activé';
      $('#midi-support').textContent = 'Sélectionne une sortie. Sous Windows, loopMIDI peut relier cette démo à Ableton Live.';
      toast(midiOutput ? `Sortie MIDI : ${midiOutput.name}` : 'Aucune sortie MIDI physique ou virtuelle détectée.');
    } catch (error) { toast(`Accès Web MIDI refusé : ${error.message}`, true); }
  });

  const canvas = $('#pitch-canvas');
  const context2d = canvas.getContext('2d');
  let lastFrame = 0;
  function drawHistory() {
    const ratio = window.devicePixelRatio || 1;
    const width = canvas.clientWidth, height = canvas.clientHeight;
    if (canvas.width !== Math.round(width * ratio) || canvas.height !== Math.round(height * ratio)) {
      canvas.width = Math.round(width * ratio); canvas.height = Math.round(height * ratio);
    }
    context2d.setTransform(ratio, 0, 0, ratio, 0, 0);
    context2d.clearRect(0, 0, width, height);
    const left = 27, right = 8, top = 5, bottom = 5;
    const graphWidth = Math.max(1, width - left - right), graphHeight = Math.max(1, height - top - bottom);
    const yForNote = (note) => top + graphHeight - clamp((note - 36) / 48, 0, 1) * graphHeight;
    context2d.font = '8px ui-sans-serif, system-ui';
    context2d.textAlign = 'left'; context2d.textBaseline = 'middle';
    for (let note = 36; note <= 84; note += 12) {
      const y = yForNote(note);
      context2d.strokeStyle = 'rgba(70, 91, 113, .19)'; context2d.lineWidth = 1;
      context2d.beginPath(); context2d.moveTo(left, y); context2d.lineTo(width - right, y); context2d.stroke();
      context2d.fillStyle = '#52667a'; context2d.fillText(midiNoteName(note), 1, y);
    }
    const drawLine = (history, colour, lineWidth, stepped) => {
      if (history.length < 2) return;
      context2d.strokeStyle = colour; context2d.lineWidth = lineWidth; context2d.lineJoin = 'round'; context2d.lineCap = 'round';
      context2d.beginPath();
      let started = false, previousY = 0;
      const offset = 260 - history.length;
      for (let i = 0; i < history.length; i++) {
        const note = history[i];
        if (note === null || note === undefined || note < 20) { started = false; continue; }
        const x = left + (offset + i) / 259 * graphWidth;
        const y = yForNote(note);
        if (!started) { context2d.moveTo(x, y); started = true; }
        else {
          if (stepped) context2d.lineTo(x, previousY);
          context2d.lineTo(x, y);
        }
        previousY = y;
      }
      context2d.stroke();
    };
    drawLine(rawHistory, '#8fb7ff', 1.25, false);
    drawLine(targetHistory, '#68e0c2', 2, true);
  }

  function updateDisplay(now) {
    requestAnimationFrame(updateDisplay);
    if (now - lastFrame < 33) return;
    lastFrame = now;
    const pitch = latestPitch;
    $('#frequency-value').innerHTML = pitch.frequency > 0 ? `${pitch.frequency.toFixed(1)} <small>Hz</small>` : '— <small>Hz</small>';
    const inputMidi = pitch.frequency > 0 ? frequencyToMidi(pitch.frequency) : null;
    $('#input-note-value').textContent = inputMidi === null ? '—' : midiNoteName(inputMidi);
    $('#raw-midi-value').textContent = inputMidi === null ? 'En attente d’un signal' : `MIDI flottant ${inputMidi.toFixed(2)}`;
    $('#target-note-value').textContent = activeNote >= 0 ? midiNoteName(activeNote) : '—';
    $('#target-midi-value').textContent = activeNote >= 0 ? `Note On · canal ${activeChannel}` : 'Aucune note active';
    $('#confidence-value').innerHTML = `${Math.round(pitch.confidence * 100)} <small>%</small>`;
    $('#confidence-bar').style.width = `${pitch.confidence * 100}%`;
    const db = 20 * Math.log10(Math.max(pitch.rms, 1e-12));
    $('#db-value').textContent = pitch.rms > 1e-6 ? `${db.toFixed(1)} dB` : '−∞ dB';
    $('#level-bar').style.width = `${clamp((db + 60) / 60, 0, 1) * 100}%`;
    const cents = activeNote >= 0 ? latestDeviation : 0;
    $('#cents-value').textContent = `${cents >= 0 ? '+' : ''}${cents.toFixed(1)} cents`;
    $('#cents-marker').style.left = `${50 + clamp(cents, -50, 50)}%`;
    drawHistory();
  }

  renderControls();
  updateSourceUI();
  requestAnimationFrame(updateDisplay);
  window.addEventListener('beforeunload', () => { if (activeNote >= 0) emitNoteOff(activeNote, activeChannel); });
})();
