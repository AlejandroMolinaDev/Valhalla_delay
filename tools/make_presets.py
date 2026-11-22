#!/usr/bin/env python3
"""Generates the factory presets (source/presets/*.xml).

Each preset only lists the parameters it changes; every other parameter takes
its default value when the preset is loaded. Values are plain parameter values
(percent, ms, Hz, semitones, cents, choice indices).

Run from the repository root:  python3 tools/make_presets.py
"""
import pathlib
from xml.sax.saxutils import quoteattr

MODES = ["Tape", "Tape Clean", "Reel HiFi", "Tape NR", "BBD", "BBD Bright", "Analog+", "Digital", "Clean",
         "Crystal", "LoFi", "Ghost Shift", "Pitch", "Pitch Rich", "Pitch Reverse", "Pitch Duck", "Phaser DDL"]
STYLES = ["Single", "Dual", "Ratio", "PingPong", "Quad"]
ERAS = ["Past", "Present", "Future"]
SYNC = ["ms"] + [f"{b}{v}" for b in ["1/64", "1/32", "1/16", "1/8", "1/4", "1/2"] for v in ("T", "", "D")] \
       + [f"{b}{v}" for b in ["1 bar", "2 bars", "4 bars"] for v in (" T", "", " D")]
assert len(SYNC) == 28 and SYNC[14] == "1/4" and SYNC[12] == "1/8D"


def p(name, category, mode="Tape", style="Single", era="Present", syncL="ms", syncR="ms", **params):
    values = {"mode": MODES.index(mode), "style": STYLES.index(style), "era": ERAS.index(era),
              "syncL": SYNC.index(syncL), "syncR": SYNC.index(syncR)}
    for key, value in params.items():
        if key == "quadMode":
            value = 1 if value == "Swell" else 0
        elif isinstance(value, bool):
            value = 1 if value else 0
        values[key] = value
    return name, category, values


PRESETS = [
    p("Spool Echo", "Tape", delayL=320, feedback=40, wow=30, flutter=20, age=35, highCut=6000, mix=30),
    p("Brass Capstan", "Tape", era="Past", delayL=450, feedback=55, drive=6, wow=40, flutter=25, age=50,
      highCut=5000, mix=30),
    p("Chrome Ribbon", "Tape", mode="Tape Clean", syncL="1/8D", feedback=35, wow=15, flutter=10, age=15,
      highCut=11000, mix=28),
    p("Studio Reel", "Tape", mode="Reel HiFi", era="Future", delayL=250, feedback=30, modRate=0.8, modDepth=15,
      highCut=16000, mix=25),
    p("Controlled Howl", "Tape", delayL=380, feedback=120, drive=4, wow=25, flutter=15, age=30, lowCut=120,
      highCut=4500, mix=30),
    p("Head Rhythm", "Multi-Head", style="Quad", delayL=600, feedback=45, spacing=0, tapA=False, tapB=True,
      tapC=False, tapD=True, quadMode="Repeat", wow=20, flutter=15, age=25, highCut=6500, mix=32),
    p("Head Choir", "Multi-Head", style="Quad", delayL=700, feedback=70, spacing=85, quadMode="Swell",
      diffAmount=20, diffSize=40, wow=25, age=20, highCut=5500, mix=35),
    p("Murky Chorus", "Analog", mode="BBD", era="Past", delayL=18, feedback=10, modRate=0.7, modDepth=45,
      lowCut=60, highCut=9000, mix=50),
    p("Bucket Slap", "Analog", mode="Analog+", delayL=95, feedback=8, modDepth=5, age=10, mix=35),
    p("Bright Brigade", "Analog", mode="BBD Bright", syncL="1/8", feedback=45, duck=25, modRate=0.4,
      modDepth=12, mix=30),
    p("Ping Dotted", "Rhythmic", mode="Digital", style="PingPong", syncL="1/8D", syncR="1/8D", feedback=45,
      age=10, highCut=9000, mix=30),
    p("Rack Delay '84", "Rhythmic", mode="Digital", era="Past", syncL="1/4D", feedback=40, age=50, modDepth=8,
      mix=30),
    p("Wide Dual", "Rhythmic", mode="Digital", style="Dual", era="Future", syncL="1/8", syncR="1/8D",
      feedback=30, age=0, modDepth=5, highCut=14000, mix=28),
    p("Clean Machine", "Rhythmic", mode="Clean", syncL="1/4", drive=12, feedback=110, age=30, modDepth=10,
      mix=25),
    p("Golden Bloom", "Ambient", mode="Clean", style="Ratio", delayL=280, ratio=61.8, feedback=75,
      diffAmount=30, diffSize=50, age=20, mix=35),
    p("Diffuse Hall", "Ambient", mode="Crystal", delayL=180, feedback=0, diffAmount=91, diffSize=70,
      lowCut=80, highCut=9000, mix=35),
    p("Glass Shimmer", "Ambient", mode="Pitch Rich", delayL=350, pitchShift=12, pitchDetune=8, pitchMix=60,
      diffAmount=85, diffSize=80, feedback=60, highCut=12000, mix=40),
    p("Endless Climb", "Pitch", mode="Pitch", delayL=300, pitchShift=12, pitchDetune=6, feedback=80,
      highCut=9000, mix=35),
    p("Micro Doubler", "Pitch", mode="Pitch", era="Future", delayL=22, pitchShift=0, pitchDetune=12,
      feedback=0, age=0, mix=45),
    p("Backwards Dream", "Pitch", mode="Pitch Reverse", delayL=600, pitchShift=0, pitchDetune=10, feedback=40,
      diffAmount=30, diffSize=40, mix=40),
    p("Duck & Spiral", "Pitch", mode="Pitch Duck", syncL="1/4", pitchShift=-12, pitchDetune=10, feedback=70,
      duck=50, highCut=8000, mix=35),
    p("Phantom Voices", "Modulation", mode="Ghost Shift", era="Future", delayL=420, freqShift=8, freqDetune=3,
      diffAmount=70, diffSize=45, feedback=65, mix=38),
    p("Psych Sweep", "Modulation", mode="Phaser DDL", delayL=520, feedback=55, modRate=0.3, modDepth=70,
      res=55, mix=35),
    p("Phase Only", "Modulation", mode="Phaser DDL", era="Future", delayL=0, feedback=0, modRate=0.5,
      modDepth=80, res=40, mix=50),
    p("Broken Radio", "Lo-Fi", mode="LoFi", era="Past", delayL=260, feedback=45, age=75, modDepth=20,
      highCut=5000, mix=35),
    p("Vocal Throw", "Vocal", mode="Tape NR", syncL="1/4", feedback=35, duck=60, wow=15, flutter=10,
      highCut=7000, lowCut=150, mix=30),
    p("Clean Loop", "Utility", mode="Crystal", delayL=2000, feedback=100, age=0, modDepth=0, lowCut=30,
      highCut=16000, mix=50),
    p("Init", "Utility", syncL="1/4"),
]


def main():
    root = pathlib.Path(__file__).resolve().parent.parent
    out = root / "source" / "presets"
    out.mkdir(parents=True, exist_ok=True)
    for old in out.glob("*.xml"):
        old.unlink()
    for index, (name, category, values) in enumerate(PRESETS, start=1):
        slug = "".join(ch for ch in name.title() if ch.isalnum())
        lines = [f'<OrbitEchoPreset name={quoteattr(name)} category={quoteattr(category)} version="1">']
        for key, value in values.items():
            lines.append(f'  <PARAM id="{key}" value="{value}"/>')
        lines.append("</OrbitEchoPreset>")
        (out / f"{index:02d}_{slug}.xml").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {len(PRESETS)} presets to {out}")


if __name__ == "__main__":
    main()
