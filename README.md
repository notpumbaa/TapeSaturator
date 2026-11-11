# TapeSaturator

Eigenständige Bandmaschinen-Emulation als **VST3 / AU / Standalone** (JUCE 8, CMake).
Für den Einsatz auf Bus und Master: Bandsättigung mit echter Hysterese, Head Bump,
Wow & Flutter, Bandrauschen und Slap-Delay – im Look einer klassischen Studio-Bandmaschine
(animierte Spulen, VU-Meter, Chicken-Head-Knöpfe, Kippschalter).

Bedienung: Knöpfe ziehen (hoch/runter), **Doppelklick** = Standardwert, Wert wird beim Ziehen/Überfahren angezeigt.
VU-Meter: 0 VU = -18 dBFS RMS.

![Oberfläche](docs/screenshot.png)

## Signalkette

```
Input ─► Record-EQ ─► [4x Oversampling: Hysterese (Jiles-Atherton)] ─► Playback-EQ
      ─► Speed-EQ (Head Bump, HF-Rolloff, Bias-Tilt) ─► Wow & Flutter ─► Slap-Delay
      ─► Bandrauschen ─► Output / Auto Gain ─► Dry/Wet (latenzkompensiert)
```

## Parameter

| Sektion | Parameter | Bereich | Wirkung |
|---|---|---|---|
| Tape | Input | ±18 dB | Pegel ins Band, mehr Pegel = mehr Sättigung |
| | Saturation | 0–100 % | Wie stark das Band in die Sättigung geht |
| | Bias | 0–100 % | <50 %: Unterbias (heller, mehr Klirr), >50 %: Überbias (dunkler, sauberer) |
| | Speed | 7.5 / 15 ips | 7.5: mehr/tieferer Head Bump, früherer Höhenabfall, mehr Rauschen |
| Transport | Wow/Flutter | 0–100 % | Tonhöhenschwankungen (bis 0,25 % Wow, 0,08 % Flutter) |
| | Noise | 0–100 % | Bandrauschen (aus bis -50 dBFS) |
| Slap Delay | Time | 30–800 ms | Echozeit (Änderungen gleiten wie bei Band) |
| | Feedback | 0–90 % | Wiederholungen, im Loop gefiltert und gesättigt |
| | Delay Mix | 0–100 % | Anteil des Echos |
| Output | Output | ±18 dB | Ausgangspegel |
| | Dry/Wet | 0–100 % | Parallel-Mischung, phasenrichtig |
| | Auto Gain | an/aus | Gleicht den Input-Gain am Ausgang wieder aus |

Latenz: ~78 Samples @ 48 kHz (wird dem Host gemeldet).

## Builds herunterladen

Jeder Push baut automatisch über **GitHub Actions** (Reiter *Actions* → letzter Lauf → *Artifacts*):

- `TapeSaturator-macOS`: AU, VST3, Standalone (Universal: Apple Silicon + Intel)
- `TapeSaturator-Windows`: VST3, Standalone

### Installation macOS

1. ZIPs entpacken.
2. `TapeSaturator.component` → `~/Library/Audio/Plug-Ins/Components/`
   `TapeSaturator.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
3. Da die Builds nicht notarisiert sind, einmal im Terminal die Quarantäne entfernen:
   ```bash
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/TapeSaturator.component ~/Library/Audio/Plug-Ins/VST3/TapeSaturator.vst3
   ```
4. DAW neu starten (Logic: ggf. Plug-in-Manager → *Neu scannen*).

### Installation Windows

`TapeSaturator.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren, DAW neu scannen lassen.

## Selbst bauen

Voraussetzungen: CMake ≥ 3.22, C++20-Compiler (Xcode / Visual Studio 2022 / GCC/Clang).
JUCE wird automatisch heruntergeladen.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Option `-DTAPESAT_COPY_AFTER_BUILD=ON` kopiert die Plugins nach dem Build direkt in die Systemordner.

### Tests & Hörbeispiele

`TapeSaturatorTests` prüft Pegelstabilität, Klirrverhalten (Saturation/Bias), Stabilität im Worst Case,
latenzkompensiertes Dry/Wet und State-Speichern. Zusätzlich:

```bash
./build/TapeSaturatorTests_artefacts/Release/TapeSaturatorTests --render render   # WAV-Hörbeispiele
./build/TapeSaturatorTests_artefacts/Release/TapeSaturatorTests --snapshot ui.png  # Screenshot der UI
```

## Projektstruktur

```
Source/
  PluginProcessor.*     Plugin-Hülle, Parameter → DSP
  PluginEditor.*        Oberfläche
  Parameters.*          Parameter-Definitionen (IDs, Bereiche)
  dsp/Hysteresis.h      Jiles-Atherton-Bandmodell (RK4)
  dsp/TapeEngine.*      komplette Signalkette
  ui/TapeLookAndFeel.*  Optik (Chicken-Head-Knöpfe, Kippschalter, gebürstetes Metall)
  ui/VUMeter.*          analoges VU-Meter mit Ballistik
  ui/TapeReel.h         animierte Tonbandspule
tests/TestMain.cpp      DSP-Tests, Offline-Renderer, UI-Snapshot
.github/workflows/      CI für macOS + Windows inkl. auval / pluginval
```

## Lizenz-Hinweis

JUCE ist unter AGPLv3 bzw. kommerzieller Lizenz verfügbar. Für privaten Gebrauch unkritisch;
vor einer Weitergabe/einem Verkauf die passende JUCE-Lizenz prüfen.
