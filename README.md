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

## Was das Plugin macht

TapeSaturator simuliert, was mit einem Signal passiert, wenn es auf eine Studio-Bandmaschine aufgenommen
und wieder abgespielt wird:

1. **Bandsättigung:** Das Herzstück. Je lauter das Signal aufs Band geht, desto mehr wird es weich komprimiert
   und angedickt, und es entstehen warme Obertöne. Spitzen werden abgerundet statt hart abgeschnitten.
2. **Bandgeschwindigkeits-EQ:** Leichte Bass-Anhebung („Head Bump“), darüber eine kleine Senke,
   sanfter Höhenabfall.
3. **Wow & Flutter:** Kleine Gleichlaufschwankungen des Bandes (minimale Tonhöhen-Bewegung).
4. **Bandrauschen:** Leises Rauschen (Hiss), optional.
5. **Slap-Delay:** Bandecho im Stil der 50er/60er.
6. **Output und Dry/Wet:** Ausgangspegel und Parallel-Mischung mit dem Originalsignal.

## Die Regler

### TAPE
| Regler | Bereich | Was er tut | Tipp |
|---|---|---|---|
| **INPUT** | ±18 dB | Wie laut das Signal aufs Band geht. **Der wichtigste Regler für die Sättigung.** | Mehr Input = mehr Farbe und Kompression. |
| **SATURATION** | 0–100 % | Wie früh das Band in die Sättigung geht. | 30–50 % dezent, 70–100 % deutlich. |
| **BIAS** | 0–100 % | Unter 50 %: heller, rauer, mehr Verzerrung. Über 50 %: dunkler, weicher, sauberer. | Bei 50 % beginnen. |
| **SPEED** | 7.5 / 15 ips | **15 ips:** offener, präziser, Bass-Anhebung um 90 Hz. **7.5 ips:** wärmer, Bass-Anhebung um 50 Hz, Höhen ab ca. 13 kHz gedämpft, etwas mehr Rauschen. | Master meist 15 ips, Vintage-Charakter 7.5 ips. |

### TRANSPORT
| Regler | Bereich | Was er tut | Tipp |
|---|---|---|---|
| **WOW/FLUTTER** | 0–100 % | Tonhöhenschwankungen (bis 0,25 % Wow, 0,08 % Flutter). | **Auf dem Master 0–10 %**, sonst schwimmen Bass und Gesang. Standard (20 %) ist für den Master eher zu viel. |
| **NOISE** | aus … -50 dBFS | Bandrauschen. | Auf dem Master meist aus. |

### SLAP DELAY
| Regler | Bereich | Was er tut | Tipp |
|---|---|---|---|
| **TIME** | 30–800 ms | Echozeit. Beim Drehen verändert sich kurz die Tonhöhe, wie bei echtem Band. | Klassischer Slapback: 80–140 ms. |
| **FEEDBACK** | 0–90 % | Anzahl der Wiederholungen, jede wird dumpfer. | 0–30 % Slapback, mehr für Dub-Echos. |
| **MIX** | 0–100 % | Lautstärke des Echos; 0 % = Delay aus. | **Auf dem Master immer 0 %.** |

### OUTPUT
| Regler | Bereich | Was er tut | Tipp |
|---|---|---|---|
| **OUTPUT** | ±18 dB | Ausgangspegel. | Zum Pegelangleich für fairen A/B-Vergleich. |
| **DRY/WET** | 0–100 % | Mischung mit dem unbearbeiteten Signal (latenzkompensiert). | 50–80 % = Parallel-Sättigung, Transienten bleiben erhalten. |
| **AUTO GAIN** | an/aus | Gleicht den Input-Regler am Ausgang automatisch aus. | **Anlassen**, dann hörst du die echte Klangänderung statt nur „lauter“. |

**Bedienung:** Knopf ziehen = Wert ändern, **Doppelklick = Standardwert**. Der genaue Wert erscheint beim
Ziehen oder Darüberfahren.

## Die VU-Meter

- **0 VU entspricht -18 dBFS** (Durchschnittspegel, nicht Spitzenpegel).
- Die Nadel ist absichtlich träge wie bei einem echten VU-Meter und zeigt die „gefühlte“ Lautstärke.
- **Input um 0 VU:** sauberer Arbeitspunkt mit leichter Färbung.
- **Input im roten Bereich (+1 bis +3):** deutlich hörbare Sättigung und Kompression.

## So stellst du es ein

1. **Neutral starten:** Wow/Flutter 0 %, Noise 0 %, Delay Mix 0 %, Auto Gain an.
2. **Input einpegeln:** INPUT so einstellen, dass das linke VU-Meter in lauten Passagen um **0 VU** pendelt.
3. **Farbe dosieren:** SATURATION langsam erhöhen, bis es hörbar „zusammenklebt“, dann ein Stück zurück.
4. **Charakter wählen:** SPEED und BIAS nach Geschmack: wärmer (7.5 ips, Bias höher) oder offener (15 ips, Bias 50 %).
5. **Fair vergleichen:** Mit dem Bypass der DAW an/aus vergleichen und bei Bedarf mit OUTPUT auf gleiche
   Lautheit bringen. Lauter klingt immer „besser“ und täuscht.
6. **Zu viel Sättigung?** Lieber DRY/WET etwas runter, statt die Sättigung komplett zu reduzieren.

## Startpunkte

| Einsatz | Input | Saturation | Bias | Speed | Wow/Flutter | Noise | Delay Mix | Dry/Wet |
|---|---|---|---|---|---|---|---|---|
| **Master (dezent)** | 0 dB | 30 % | 50 % | 15 ips | 0–5 % | 0 % | 0 % | 100 % |
| **Mix-Bus / Gruppe** | +3 dB | 50 % | 50 % | 15 ips | 5 % | 0 % | 0 % | 100 % |
| **Drums (Punch)** | +6 dB | 70 % | 40 % | 15 ips | 0 % | 0 % | 0 % | 60 % |
| **Bass (Wärme)** | +3 dB | 60 % | 60 % | 7.5 ips | 0 % | 0 % | 0 % | 100 % |
| **Vocals (Slapback)** | +2 dB | 40 % | 50 % | 15 ips | 10 % | 0 % | 25–40 % (Time 110 ms, Feedback 15 %) | 100 % |
| **Lo-Fi / Vintage** | +9 dB | 90 % | 30 % | 7.5 ips | 50–70 % | 40–60 % | nach Geschmack | 100 % |

Die Werte sind Startpunkte, bitte nach Gehör nachjustieren.

### Auf dem Master

- **Nach dem EQ, vor dem Limiter:** TapeSaturator gehört **vor** den finalen Limiter.
- **Wenig ist mehr:** Oft reichen 1–2 dB „Kompression“ durch das Band. Der Effekt soll eher gefühlt als gehört werden.
- **Latenz:** ca. 78 Samples @ 48 kHz (~1,6 ms). Das Plugin meldet sie der DAW, die gleicht sie automatisch aus.

## Builds herunterladen

Fertige Versionen: **[Releases](https://github.com/notpumbaa/TapeSaturator/releases)** (ZIPs werden beim Veröffentlichen eines Releases automatisch angehängt).

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
