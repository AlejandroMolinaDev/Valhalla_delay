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

*(The remaining modes are documented as they are implemented in phases 2–4.)*

## 9. Real-time rules

* No allocation, lock or I/O on the audio thread (verified by a test that
  replaces the global `operator new` and counts allocations during
  `processBlock` while modes, styles, eras, times, tempo and bypass change).
* Denormals: `juce::ScopedNoDenormals` in the plugin, FTZ/DAZ helper in tests.
* Noise sources: xorshift / hashed sequences, seeded per channel and per head.
* No mutable static state: several instances can run on different threads
  (FL Studio's multithreaded mixer).
