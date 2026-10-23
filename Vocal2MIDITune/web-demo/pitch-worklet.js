/* Real-time YIN pitch analysis. All DSP buffers are allocated once at construction. */
class YinPitchProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    const desired = Math.max(2048, Math.floor(sampleRate * 0.08));
    this.size = 2048;
    while (this.size < desired) this.size *= 2;
    this.fftSize = this.size * 2;
    this.hop = Math.max(256, Math.floor(this.size / 8));
    this.ring = new Float32Array(this.size);
    this.analysis = new Float32Array(this.size);
    this.prefix = new Float32Array(this.size + 1);
    this.difference = new Float32Array(this.size / 2 + 2);
    this.cumulative = new Float32Array(this.size / 2 + 2);
    this.fftRe = new Float32Array(this.fftSize);
    this.fftIm = new Float32Array(this.fftSize);
    this.write = 0;
    this.untilAnalysis = 0;
    this.minHz = 50;
    this.maxHz = 1500;
    this.port.onmessage = (event) => {
      const data = event.data;
      if (data.type === 'range') {
        this.minHz = Math.max(40, Math.min(1500, data.min));
        this.maxHz = Math.max(this.minHz + 1, Math.min(1600, data.max));
      } else if (data.type === 'reset') {
        this.ring.fill(0);
        this.write = 0;
        this.untilAnalysis = 0;
      }
    };
  }

  fft(inverse) {
    const re = this.fftRe;
    const im = this.fftIm;
    const n = this.fftSize;
    for (let i = 1, j = 0; i < n; i++) {
      let bit = n >> 1;
      for (; (j & bit) !== 0; bit >>= 1) j ^= bit;
      j ^= bit;
      if (i < j) {
        let temp = re[i]; re[i] = re[j]; re[j] = temp;
        temp = im[i]; im[i] = im[j]; im[j] = temp;
      }
    }
    for (let length = 2; length <= n; length <<= 1) {
      const angle = (inverse ? 2 : -2) * Math.PI / length;
      const stepR = Math.cos(angle);
      const stepI = Math.sin(angle);
      const half = length >> 1;
      for (let start = 0; start < n; start += length) {
        let wr = 1, wi = 0;
        for (let j = 0; j < half; j++) {
          const even = start + j;
          const odd = even + half;
          const oddR = re[odd] * wr - im[odd] * wi;
          const oddI = re[odd] * wi + im[odd] * wr;
          const evenR = re[even], evenI = im[even];
          re[even] = evenR + oddR; im[even] = evenI + oddI;
          re[odd] = evenR - oddR; im[odd] = evenI - oddI;
          const nextR = wr * stepR - wi * stepI;
          wi = wr * stepI + wi * stepR;
          wr = nextR;
        }
      }
    }
    if (inverse) {
      const scale = 1 / n;
      for (let i = 0; i < n; i++) { re[i] *= scale; im[i] *= scale; }
    }
  }

  analyse() {
    let mean = 0;
    for (let i = 0; i < this.size; i++) {
      const value = this.ring[(this.write + i) % this.size];
      this.analysis[i] = value;
      mean += value;
    }
    mean /= this.size;
    let sumSquares = 0, peak = 0;
    this.prefix[0] = 0;
    for (let i = 0; i < this.size; i++) {
      const value = this.analysis[i] - mean;
      this.analysis[i] = value;
      peak = Math.max(peak, Math.abs(value));
      sumSquares += value * value;
      this.prefix[i + 1] = sumSquares;
      this.fftRe[i] = value;
      this.fftIm[i] = 0;
    }
    const rms = Math.sqrt(sumSquares / this.size);
    if (rms < 1e-6 || peak < 1e-5) {
      this.port.postMessage({ type: 'pitch', frequency: 0, confidence: 0, rms, peak, voiceDetected: false, time: currentTime });
      return;
    }
    this.fftRe.fill(0, this.size);
    this.fftIm.fill(0, this.size);
    this.fft(false);
    for (let i = 0; i < this.fftSize; i++) {
      this.fftRe[i] = this.fftRe[i] * this.fftRe[i] + this.fftIm[i] * this.fftIm[i];
      this.fftIm[i] = 0;
    }
    this.fft(true);

    const minTau = Math.max(2, Math.floor(sampleRate / this.maxHz));
    const maxTau = Math.min(this.size / 2 - 2, Math.ceil(sampleRate / this.minHz));
    let running = 0, bestTau = minTau, bestValue = 1e20;
    this.cumulative[0] = 1;
    for (let tau = 1; tau <= maxTau; tau++) {
      const firstEnergy = this.prefix[this.size - tau];
      const secondEnergy = this.prefix[this.size] - this.prefix[tau];
      const d = Math.max(0, firstEnergy + secondEnergy - 2 * this.fftRe[tau]);
      this.difference[tau] = d;
      running += d;
      this.cumulative[tau] = running > 1e-20 ? d * tau / running : 1;
      if (tau >= minTau && this.cumulative[tau] < bestValue) {
        bestValue = this.cumulative[tau]; bestTau = tau;
      }
    }
    let selected = bestTau;
    const threshold = 0.18;
    for (let tau = minTau; tau < maxTau; tau++) {
      if (this.cumulative[tau] >= threshold) continue;
      selected = tau;
      while (selected + 1 <= maxTau && this.cumulative[selected + 1] < this.cumulative[selected]) selected++;
      break;
    }
    let refined = selected;
    if (selected > 1 && selected < maxTau) {
      const left = this.cumulative[selected - 1], centre = this.cumulative[selected], right = this.cumulative[selected + 1];
      const denominator = left - 2 * centre + right;
      if (Math.abs(denominator) > 1e-12) refined += Math.max(-0.5, Math.min(0.5, 0.5 * (left - right) / denominator));
    }
    const frequency = refined > 0 ? sampleRate / refined : 0;
    const confidence = Math.max(0, Math.min(1, 1 - this.cumulative[selected]));
    const valid = frequency >= this.minHz * 0.9 && frequency <= this.maxHz * 1.1 && confidence > 0.05;
    this.port.postMessage({
      type: 'pitch', frequency: valid ? frequency : 0, confidence: valid ? confidence : 0,
      rms, peak, voiceDetected: valid && confidence >= 0.25, time: currentTime
    });
  }

  process(inputs, outputs) {
    const input = inputs[0];
    const output = outputs[0];
    const frames = output[0] ? output[0].length : 128;
    const first = input && input.length > 0 ? input[0] : null;
    const second = input && input.length > 1 ? input[1] : null;

    for (let i = 0; i < frames; i++) {
      const sample = first ? (second ? 0.5 * (first[i] + second[i]) : first[i]) : 0;
      this.ring[this.write] = Number.isFinite(sample) ? sample : 0;
      this.write = (this.write + 1) % this.size;
      if (++this.untilAnalysis >= this.hop) {
        this.untilAnalysis = 0;
        this.analyse();
      }
    }
    for (let channel = 0; channel < output.length; channel++) {
      const out = output[channel];
      const source = input && input.length > 0 ? input[Math.min(channel, input.length - 1)] : null;
      if (source) out.set(source); else out.fill(0);
    }
    return true;
  }
}
registerProcessor('yin-pitch-processor', YinPitchProcessor);
