# Orbit Echo — DSP design notes

This document records how the engine works and every DSP decision that the
project brief left open. Section numbers of the brief are referenced where
relevant.

## 1. Signal flow

```
in L/R ─► Ducker (RMS 5/250 ms) ──────────────────────────────┐
   │                                                          │ duck gain
   └─► × Drive ─► Style routing (+ Feedback × previous heads) ─► Mode record chain
                                                                (saturation, Age, era filters,
                                                                 freq shift / phaser / bits ...)
                                                              ─► Diffusion (bypassed at 0 %)
                                                              ─► Loop EQ (Low/High Cut; not in Crystal)
                                                              ─► DC blocker ─► clamp ±2.5
                                                              ─► delay lines L / R (20 s)
   read heads (Style taps, Mode reader: modulation / pitch / reverse)
   ─► Style wet mix ─► Mode wet stage (Crystal: Loop EQ here) ─► × Drive compensation (−Drive/2 dB)
   ─► × duck gain ─► safety limiter (−0.3 dBFS) ─► Width matrix ─► equal-power Mix ─► out
```

### Where the colour chain sits

The brief's diagram draws the mode processor on the feedback return, but also
requires that *Phaser DDL with delay and feedback at zero works as a stand-alone
phaser* and that every repeat accumulates the colouring. Both hold only if the
colour chain is on the **record path** (input + feedback → chain → delay line),
which is also how tape echoes and BBD units behave (the record amplifier / chip
input colours the first echo as well). The chain is therefore inside the loop,
the first echo is coloured once and each further repeat once more.

### Ordering inside a sample

The delay lines are written **before** they are read, so a 0 ms delay returns
the sample just written (a true zero-delay echo). The feedback uses the head
outputs of the *previous* sample, i.e. the loop is at least one sample long — a
zero-delay feedback loop is not computable.

## 2. Block-size and sample-rate independence

* Everything is processed sample by sample; there are no block-sized scratch
  buffers, so hosts may send blocks of any size, including blocks larger than
  announced in `prepareToPlay` (FL Studio does this when "fixed size buffers" is
  off) and blocks of 0 or 1 samples.
* Control-rate work (filter coefficients, delay-time targets, mode updates) runs
  every 32 samples on an **absolute sample clock** that persists across blocks.
  Output is bit-identical for any block segmentation (test 11).
* Times are defined in milliseconds/Hz and converted with the actual sample rate.

## 3. Delay lines and times

* Two float delay lines (L and R), power-of-two sized for 20 s plus a 150 ms
  modulation / grain margin, allocated in `prepareToPlay` at the actual sample
  rate (reallocated only when the rate changes). Up to 192 kHz is supported
  (~16 MB per line at 192 kHz, ~4 MB at 48 kHz). The constructor allocates
  nothing large, which keeps plugin scans fast.
* Interpolation: 4-point 3rd-order Hermite for modulated reads, 4-point
  3rd-order Lagrange in the digital modes. Both are exact at integer delays.
* **Delay-time changes** glide through a critically damped 2nd-order smoother
  ("motor inertia"), which re-pitches the echoes like a tape transport or a BBD
  clock. Glide time is per mode (Digital 120 ms). An internal option
  (`digitalTimeCrossfade`, off by default) makes the digital modes crossfade
  (40 ms) between the old and new time instead.
* **Group-delay compensation**: the in-loop low-pass filters (era anti-alias
  filters, High Cut) delay the signal by a few samples (more at high sample
  rates, since their cutoffs are fixed in Hz). Each mode reports the DC group
  delay of its filters (for a bilinear 2nd-order low-pass:
  τ = 1 / (2·Q·tan(π·fc/fs)) samples) and the engine reads that much earlier.
  Echoes therefore land on the delay grid (±1 sample) on every repeat and at
  every sample rate.

## 4. Styles

| Style | Heads | Routing | Modulation phase |
|---|---|---|---|
| Single | L = D + Spread, R = D − Spread | each line feeds back into itself | same |
| Dual | Delay L, Delay R | each line into itself, no cross feedback | 0° / 90° |
| Ratio | L = D, R = Ratio × D | feedback mixed through an orthogonal 35° rotation (energy preserving), so echo times combine as m·L + n·R and density grows with time | 0° / 180° |
| PingPong | Delay L, Delay R | (L+R)/2 → L line, L out → R line, R out → L line (Feedback applied on every hop) | 0° / 180° |
| Quad | 4 head pairs A–D | Repeat: feedback from head D only; Swell: from all active heads, normalised by their count | per head, decorrelated |

Quad details: with Spacing 0 % the heads sit at D/4, D/2, 3D/4 and D, L = R.
Increasing Spacing moves them towards an irregular, incommensurate layout
(0.237, 0.441, 0.719, 1.0 × D) and pulls the R heads 3.5–9 % earlier than the L
heads. In Repeat mode head D keeps recirculating even if its output switch is
off (a muted playback head still feeds the record head). The wet sum is
normalised by 1/√(active heads).

Style changes dip the heads out over 15 ms, switch, and fade back in over 15 ms;
the delay buffers are kept.

## 5. Feedback, safety and bounding

* Feedback 0–200 %. Every mode has a bounded non-linearity in its record chain
  (saturation curve or limiter), so above 100 % the loop self-oscillates into
  the curve instead of growing.
* In the loop: DC blocker (5 Hz) and a last-resort clamp at ±2.5.
* On the wet: stereo-linked safety limiter (instant attack, 80 ms release,
  threshold 0.93) followed by a hard clip at −0.3 dBFS. The dry signal is never
  limited.
* NaN/Inf anywhere → the sample is replaced (silence in the loop, dry at the
  output) and the whole engine is reset at the end of the block.
* Host bypass: a linear 30 ms ramp fades the wet out while the dry passes; then
  the dry passes untouched and the delay lines are cleared in 32k-sample chunks
  per block (no CPU spike). Leaving bypass restarts from clean buffers.

## 6. Mix, Width, Drive

* Mix: dry = cos θ, wet = sin θ, θ = Mix · π/2 (dry² + wet² = 1).
* Width w ∈ [−1, 1]: L' = ((1+w)/2)·L + ((1−w)/2)·R, R' symmetric.
  +100 % = normal, 0 % = mono sum, −100 % = swapped. The matrix never increases
  the peak, so it sits after the safety limiter.
* Drive (0–24 dB) is applied to the **input** before it is summed with the
  feedback, i.e. it drives the record chain's saturation without changing the
  loop gain (Feedback 100 % stays unity whatever the Drive). The wet output is
  compensated by −Drive/2 dB. In the linear region the echo level therefore
  rises by Drive/2; once the saturation compresses (the normal operating range
  for real program material) the perceived level stays roughly constant —
  test 8 measures this with program-level input.

## 7. Tempo sync

`ms` (free) or a note value from 1/64 to 4 bars, each straight, triplet (×2/3)
and dotted (×1.5). One beat is a quarter note and a bar is 4 beats (4/4). The
tempo comes from the host play head; when the host reports none, the last known
tempo is kept (120 BPM until a tempo is reported). Tempo changes move the delay
target, which glides (no clicks).

## 8. Modes

### Digital (80s floating-point rack)

* Record chain: anti-alias low-pass → converter clipping curve → mantissa
  rounding (floating-point converter: the step scales with the signal, the
  "grain" of early digital delays). Below −90 dBFS the step stops shrinking
  (last gain range).
* Age → effective bits: `bits = round(24 · (1 − Age))`, minimum 3:
  0 % = 24 bits (transparent), **50 % = 12 bits**, 100 % = 3 bits.
* Reads: Lagrange; modulation: smoothed triangle (Mod Rate / Mod Depth, 100 % =
  ±10 ms, limited to 45 % of the head's delay so short delays become chorus /
  flanger).
* Era: Past = 9 kHz 4th-order + hard converter knee; Present = 14 kHz 2nd-order
  + soft (tanh) curve; Future = 19.5 kHz 2nd-order + very clean curve, sine LFO.

### Saturation curves (all modes)

Every in-loop curve is bounded and has a small-signal slope of exactly 1, so
"Feedback 100 %" means unity loop gain for quiet signals in every mode and the
self-oscillation above it settles into the curve. Curves: Clean, Soft (rational
tanh), Converter (hard knee at −1 dBFS), Tape (biased tanh → even harmonics,
normalised by the slope at the bias point), TapeHot (earlier and more
asymmetric), Bbd (soft, x|x| asymmetry), Thick (lower ceiling, denser).

### Tape family (Tape, Tape Clean, Reel HiFi, Tape NR)

Record chain: (NR encode) → +6 dB HF pre-emphasis (3 kHz shelf) → tape curve
with soft hysteresis (the "magnetisation" follows the drive faster when it grows
— 22 kHz — than when it relaxes — 14 kHz, giving level-dependent HF
compression) → matching de-emphasis → gap-loss low-pass → head bump → asperity
noise → splice. Playback: wow/flutter-modulated Hermite read → (NR decode).

* **Tape speed** = 150 ms / delay. The gap-loss cutoff tracks it
  (`7.5 kHz × speed^0.6`, clamped 2.2 kHz … 0.45·fs) and the head bump moves
  with it (`70 Hz × √speed`, 35–200 Hz): longer delays are darker, as on a
  variable-speed tape echo. This is the "global EQ that follows the delay time",
  in addition to Low/High Cut.
* **Wow / flutter** (delay-linked): rate = factor / delay (wow 0.4/s, flutter
  3/s, i.e. 1 Hz and 7.5 Hz at 400 ms; clamped 0.1–4 Hz and 2–25 Hz), depth =
  fraction of the delay (wow 0.4 %, flutter 0.12 % at 100 %) plus a periodic
  capstan component. The pitch wobble therefore has the same character at any
  delay time while its rate follows the delay, like the hardware.
* **Normalisation**: the chain is scaled so its most resonant frequency (head
  bump, gap-loss peak) has unity gain; self-oscillation starts near 100 %
  Feedback in every era and mid frequencies lose 1–3 dB per pass.
* **Age**: asperity (modulation) noise — high-passed white noise multiplied by
  the signal envelope, so it exists only under the echoes and the plugin is
  silent in silence (FL Studio's Smart Disable can sleep). Tape adds **splices**:
  once per loop revolution (8 × delay, 0.5–20 s) an 18 ms level dip (up to
  −6 dB) and a short click proportional to the signal.
* Glide ("motor inertia"): 350 ms (Reel HiFi 200 ms).

| | Tape | Tape Clean | Reel HiFi | Tape NR |
|---|---|---|---|---|
| Gap loss @150 ms | 7.5 kHz, tracks speed | 10 kHz, tracks | 16 kHz fixed | 11.5 kHz, tracks |
| Wow / flutter | delay-linked | delay-linked | free (Mod Rate / Depth) | delay-linked |
| Asperity noise scale | 1 | 0.25 | 0.4 | 0.12 |
| Splices | yes | no | no | no |
| Extra | | finer, brighter curve (bias 0.05) | | 2:1 / 1:2 compander around the tape |

Era (tape): Past = HF × 0.8, gap-loss Q 1.1 at high speed (the highs ring at
short delays), head bump +2 dB, TapeHot curve; Present = Q 0.85, +1.5 dB, Tape
curve; Future = HF × 1.3, Q 0.707, +0.8 dB, gentler drive (Reel HiFi: Clean
curve).

### BBD family (BBD, BBD Bright, Analog+)

Record chain: input op-amp curve → anti-alias low-pass → 2:1 compressor (4 /
60 ms) → chip clipping (in the compressed domain) → chip hiss + clock leakage
tone (clock = 4096 stages / (2·delay)), gated by the signal. Playback, per head:
reconstruction low-pass (same cutoff/order) → 1:2 expander (6 / 80 ms — slower
than the encoder, so the pair breathes). The noise sits between compressor and
expander, so it follows the signal dynamics; with high Drive the compressor
(which sees input + feedback) pulls the recirculating echoes down — the BBD
ducking. Modulation: LFO (Past triangle, Present smoothed triangle, Future
sine), Mod Rate / Mod Depth. Glide 250 ms.

| | Bandwidth (AA = reconstruction) | Noise | Curves |
|---|---|---|---|
| BBD | Past 2.8 kHz / Present 3.8 kHz (4th order), Future 7 kHz (2nd order, the "bright model") | 1 (Past 1.3) | Soft → Bbd |
| BBD Bright | `2.6 kHz × (300 ms / delay)^0.6`, 1.6–15 kHz, × era (0.8 / 1 / 1.3): short delays bright, long ones dark | 0.8 | Bbd → Thick (Future: Soft) |
| Analog+ | ~2× BBD: 5.6 / 7.6 / 12 kHz | 0.34 (≈ −6 dB at the output, i.e. half) | Thick → Thick |

Measured at Age 100 % under a −10 dBFS tone: Tape −49 dBFS, BBD −54 dBFS,
Reel HiFi −57 dBFS, Analog+ −60 dBFS of noise; 0 after the input stops.

### Clean

Wide-band digital delay (Lagrange reads) whose loop element is a
**program-dependent limiter** instead of a saturation curve: threshold −1.5 dB,
1 ms attack, release that moves between 40 ms (transient material, high crest
factor) and 600 ms (dense material). Drive and Feedback above 100 % can be pushed
without distortion (THD < 3 % at Drive +24 dB, vs. > 10× more in Digital/Tape).
**Age = bandwidth**: an in-loop 2nd-order low-pass from 20 kHz (0 %) to 1.5 kHz
(100 %), log-spaced. **Era = deep, unusual modulation shapes** with twice the
usual excursion (±20 ms at 100 %): Past = smoothed square (alternating
up/down pitch), Present = ramp (slow rise, quick return), Future = asymmetric
"hump". Duck in the main section.

### Crystal

Low Cut / High Cut are applied to the **wet output**, not in the loop, and the
loop contains only a nearly colourless limiter (−1 dB threshold, 5 ms attack,
300 ms – 1.5 s release) and the Age-controlled floating-point converter (as
Digital). Every repeat keeps its brightness (measured: identical HF ratio on the
1st and 5th repeat, whereas Digital loses > 50 %), and Feedback 100 % sustains a
loop (level after 20 repeats within ±20 %). Sine modulation.

### LoFi

Converter clipping → (anti-alias filter, per Era) → fractional sample-and-hold
→ linear bit crusher → 6.5 kHz band limit. **Age = rate reduction**: hold rate
= host rate × (1/32)^Age (0 % none, 100 % 1/32 ≈ 1.5 kHz at 48 kHz). **Era =
anti-alias filter and word length**: Past = none (raw aliasing), 8 bits;
Present = 1-pole at the new Nyquist, 10 bits; Future = 4th-order Butterworth at
0.45 × the new rate, 12 bits. The hold's average delay (half a hold period) is
compensated in the echo time. Duck in the main section.

### Phaser DDL

Digital delay with an analog-style phaser on the record path (in the loop):
first-order all-pass stages, break frequency swept exponentially between 180 Hz
and 5.2 kHz, output = ½(input + all-pass chain) → notches, resonance feedback
around the chain (**Res**, replaces Age, up to 0.85), soft curve after it. The
two channels sweep in quadrature. **Mod Rate / Mod Depth drive the phaser LFO**
(the read heads are not modulated); the depth shrinks as the rate rises:
depth_eff = depth / (1 + rate / 1.5 Hz). **Era = number of stages** (design
decision): Past 4 (2 notches), Present 6 (3 notches), Future 12 (6 notches) —
verified by sweeping a sine through the plugin. With Delay 0 and Feedback 0 the
wet signal is the phaser alone.

### Ghost Shift

Record chain (in the loop): hi-fi tape curve (bias 0.08) → fixed gap loss
(Past 12 kHz, Present 16 kHz, Future 19 kHz) → asperity noise (Age, signal
gated) → **single-sideband frequency shifter** → Era dispersion. Reads: Hermite
with a subtle fixed hi-fi transport drift (0.35 Hz, ±0.15 ms).

* Frequency shifter: a Hilbert transformer (two chains of four z⁻² all-pass
  sections of the classic 90° phase-difference network; measured in tests:
  I/Q correlation < 0.02 and equal power at 100 Hz, 1 kHz and 8 kHz) and a
  double-precision quadrature oscillator: y = I·cos φ + Q·sin φ (upper
  sideband for positive Hz). The shift is linear in Hz (1 kHz + 150 Hz =
  1150 Hz, not a musical interval), so harmonics become inharmonic and each
  repeat moves further. **Freq Shift** ±1000 Hz (cubic knob curve, most travel
  near 0); **Freq Detune** 0–50 Hz: L shifts by shift + detune/2, R by
  shift − detune/2 (small = subtle width, large = fast auto-pan).
* **Era = diffusion character**: Past = standard (only the global Diffusion);
  Present = frequency-dependent diffusion: 12 first-order all-passes with
  negative coefficients (−0.55 … −0.85 with the Diffusion amount) delay the lows
  more than the highs — the highs arrive first (measured +1.6 ms low-vs-high
  spread); Future = strongly frequency dependent: 32 stages (−0.70 … −0.92;
  measured +7.8 ms) — chirping, ghostly voices.
* Compensation keeps the 1 kHz region on the delay grid (gap loss + Hilbert
  path + dispersion group delay at 1 kHz).

### Pitch, Pitch Duck

Digital delay (DigitalColor record chain: Era filters/curves, Age = converter
bits) whose heads read the delay line through a **granular pitch shifter**:
two grains per head, half a window apart, smooth windows (smoothstep of a triangle) that sum to exactly 1;
each grain's extra delay ramps at (1 − ratio) samples/sample and wraps every
window W = clamp(2 × delay, 20 ms, 60 ms). Because the grains read the delay
line itself the shifter adds **no latency**: the average extra delay (W/2) is
subtracted from the echo time (measured: echo centroid within 2 ms of the set
time). The first echo is shifted and every repeat is shifted again: spirals.
Pitch Shift ±24 st (chromatic steps in the UI, fine with Shift-drag); Pitch
Detune: L = +detune/2, R = −detune/2 cents (micro-shift doubling). Pitch Duck
is the same algorithm with Duck in the main section.

### Pitch Rich

Two grain shifters per head sharing the delay time: a pitch-shifted path
(Pitch Shift ± Detune/2) and a "lush" path (only ± Detune/2, plus a slow random
±1.5 ms chorus drift). **Pitch Mix** (replaces Age) crossfades lush (0 %) →
shifted (100 %) with an equal-power law. Converter at full resolution. With
high Diffusion: shimmer reverb.

### Pitch Reverse

The heads play the delay line backwards in splices whose length is the head's
**delay time**. A grain starting at delay d₀ = 2 samples moves as
d(t) = d₀ + (1 + r)·t — backwards through the recording at r × speed, which is
the pitch shift (r = 2^(semitones/12) ± detune). Two grains half a splice
apart with the same smooth windows crossfade every splice without clicks; the splice is
shortened if (1 + r)·T would exceed the 20 s buffer. In the loop every repeat is
reversed (and shifted) again. Record chain: DigitalColor (Age = bits).

## 9. Era summary (what Era changes in each mode)

| Mode | Past | Present | Future |
|---|---|---|---|
| Tape / Tape Clean / Tape NR | HF × 0.8, gap-loss Q 1.1 at short delays, bump +2 dB, hot curve | Q 0.85, +1.5 dB, tape curve | HF × 1.3, Q 0.707, +0.8 dB, gentler drive |
| Reel HiFi | as tape (fixed 16 kHz × era) | | Clean curve |
| BBD | dark 2.8 kHz 4th-order, noise × 1.3, triangle LFO | dark 3.8 kHz 4th-order, smoothed triangle | bright 7 kHz 2nd-order, sine LFO |
| BBD Bright | × 0.8 bandwidth | × 1 | × 1.3, 2nd-order, softer chip curve |
| Analog+ | 5.6 kHz | 7.6 kHz | 12 kHz, 2nd-order |
| Digital / Pitch / Pitch Duck / Pitch Reverse / Pitch Rich | 9 kHz 4th-order + converter knee | 14 kHz 2nd-order + tanh | 19.5 kHz 2nd-order + clean curve (Digital: sine LFO) |
| Clean | smoothed-square LFO | ramp LFO | "hump" LFO |
| Crystal | — (Era not used) | — | — |
| LoFi | no anti-alias filter, 8 bits | 1-pole AA, 10 bits | 4th-order AA, 12 bits |
| Ghost Shift | standard diffusion, 12 kHz | frequency-dependent dispersion (12 stages), 16 kHz | strongly frequency-dependent (32 stages), 19 kHz |
| Phaser DDL | 4 stages | 6 stages | 12 stages |

## 10. Diffusion (all modes)

Eight series Schroeder all-passes per channel on the record path (after the
mode chain, before the Loop EQ). Stage delays are fixed fractions of the total
(0.043 … 0.219, sum 1; the right channel detuned by ±1–3 %) and each is
modulated by a slow random LFO (±0.2 ms + 0.2 %) so it never rings metallically.

* **Amount → all-pass gain**: g = 0.866 · Amount^1.53, calibrated by measuring
  the envelope of a diffused echo: at **68 %** (g ≈ 0.48) attack time ≈ decay
  time (ratio 1.04–1.16 for Size 30–80 %: a bell); at **91 %** (g ≈ 0.75) the
  echo builds up immediately and decays over hundreds of ms with no gaps
  (continuity > 0.9, 8–10 reflections/ms) — a dense reverb with Feedback 0 %.
* **Amount 0 %** skips the network entirely (no CPU; test 10 counts calls).
  Turning it on/off crossfades over 30 ms.
* **Size** sets the total dispersion relative to the shortest head delay:
  Σd = (0.01 + 0.99·Size²) × delay, 1 ms … 20 s. Low values blur transients
  but stay a delay; high values become reverberant.
* **Centring**: the energy centroid of a Schroeder all-pass is exactly its
  delay d, whatever g, so the centroid of the chain is Σd. The heads are read
  Σd earlier (scaled by the on/off crossfade): diffused echoes stay centred on
  the delay grid (measured within 6 %; linear interpolation slightly damps the
  very late tail) and "barely on" (a pure delay of Σd) sounds identical to
  "off".
* Memory: 20 s of all-pass buffer per channel (exact sizes, no power-of-two
  padding), allocated in `prepareToPlay`.

## 11. Duck (all modes)

A single global knob (the brief's superset): RMS detector on the dry input
(5 ms window/attack, 250 ms release on the level, 15 ms gain smoothing). Wet
reduction = Duck × clamp(0.75 × (level_dBFS + 40), 0, 24) dB; the feedback is
reduced by a third of that. Below −40 dBFS RMS nothing is ducked. Design
decision: ducking the feedback by the full amount would empty the loop while
playing, so nothing could "swell back" in the gaps; a third keeps the repeats
from piling up yet leaves material to come back (measured: −22 dB while
playing, +6.7 dB swell once the input stops, 400 ms delay, Duck 100 %).
Duck = 0 is an exact bypass. The knob sits in the main section for Tape NR,
BBD Bright, Clean, LoFi and Pitch Duck, in the "Extra" panel otherwise.

## 12. Drive compensation, measured (test 8)

+12 dB of Drive with program material at −9 dBFS RMS changes the wet RMS by
−1.3 … +2.4 dB depending on the mode (all within ±3 dB). In the linear region
(quiet material) the −Drive/2 rule gives +6 dB, as specified.

## 13. Performance

Measured with `oe_render --bench` (48 kHz stereo, 256-sample blocks, Feedback 60 %, Duck 30 %;
percent of one core of the CI-class x86-64 container used for development):

| | Single | Quad | PingPong | Single + Diffusion 60 % |
|---|---|---|---|---|
| Digital / Clean / LoFi | ~0.7 % | ~1.3 % | ~0.7 % | ~1.2 % |
| Pitch / Pitch Reverse / Pitch Duck | ~0.9 % | ~1.7 % | ~0.9 % | ~1.4 % |
| BBD family | ~1.2 % | ~2.4 % | ~1.3 % | ~1.8 % |
| Tape family | ~1.4 % | ~3.5 % | ~1.5 % | ~2.2 % |
| Pitch Rich (worst case) | ~1.5 % | ~3.9 % | ~1.6 % | ~2.3 % |

Optimisations that do not change the sound: diffusion modulation evaluated every 16 samples
with linear ramps (it moves at 0.2–0.8 Hz); the Duck gain law (log/pow) evaluated every 16
samples while the detector and the gain smoother run per sample; random-LFO sequence values
cached per head (they change only when the read point crosses a step); grain windows are a
smoothstep of a triangle (C¹, overlap-add to exactly 1) instead of sin²; filter coefficients
only recomputed when their control value changes; Diffusion 0 % skips the network entirely.
All of them are keyed to the engine's absolute sample clock, so the output stays bit-identical
for any block segmentation.

## 14. Real-time rules

* No allocation, lock or I/O on the audio thread (verified by a test that
  replaces the global `operator new` and counts allocations during
  `processBlock` while modes, styles, eras, times, tempo and bypass change).
* Denormals: `juce::ScopedNoDenormals` in the plugin, FTZ/DAZ helper in tests.
* Noise sources: xorshift / hashed sequences, seeded per channel and per head.
* No mutable static state: several instances can run on different threads
  (FL Studio's multithreaded mixer).
