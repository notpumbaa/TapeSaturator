# Changelog

All notable changes to TapeSaturator are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project uses [Semantic Versioning](https://semver.org/).

The section of a version is used as its release notes on GitHub automatically.

## [Unreleased]

## [1.2.0] - 2026-11-13

### Added
- **AIR control** (OUTPUT section, 0–100 %, default 0 %): brings back the sheen that tape rounds off.
  Adds subtle 2nd and 3rd harmonics in the high frequencies (generated from the 2.5–7.5 kHz range)
  plus a gentle high shelf (up to about +3 dB at 12–16 kHz). It backs off automatically on loud top end
  (sibilance, cymbals). No extra latency; at 0 % the sound is exactly as before.
- Test option `--response` that prints a frequency-response table.

### Fixed
- Wow & flutter no longer dulls the highs. A new high-quality (windowed-sinc) interpolation keeps the
  top end intact; previously it cost 1–3 dB above 12 kHz.
- The INPUT VU meter now follows the INPUT knob. It shows the level that hits the tape (after INPUT),
  as described in the manual; before, it only reacted to the source level.

### Changed
- The window is slightly wider to make room for the AIR knob.

## [1.1.1] - 2026-11-12

### Added
- Every download ZIP now contains `LICENSE.txt`, a `README.txt` (installation, website and a link to the
  exact source code of that build), `THIRD-PARTY-NOTICES.txt` and the folder `third-party-licenses/`.
- One-click release workflow (Actions → Release → Run workflow).

### Changed
- TapeSaturator is now licensed under the GNU Affero General Public License v3.0. Added a security policy.
- macOS builds are verified to be Universal (Apple Silicon + Intel).
- Builds run on GitHub's Intel macOS runners, which avoids long queue times.

## [1.1.0] - 2026-11-11

### Added
- More realistic interface: consistent top-left lighting with soft shadows, VU meters behind glass,
  walnut side panels, cast name badge, head block with capstan and pinch roller.
- Living mechanics: the reels spin up and down with inertia, the tape pack moves from the left reel to the
  right one, springy tension arms, subtle needle jitter.
- Embedded vintage fonts (Barlow Condensed, Playfair Display SC).
- Resizable window (60–200 %), saved with your project.

### Changed
- The README is now in English and contains a full user guide.

## [1.0.0] - 2026-11-11

### Added
- First release as VST3, AU and Standalone (macOS Universal, Windows).
- Tape saturation with magnetic hysteresis (Jiles-Atherton model, 4x oversampling) and record/playback EQ.
- Tape speed 7.5 / 15 ips with head bump and high-frequency roll-off, bias control.
- Wow & flutter, tape hiss, slap delay with filtered feedback.
- Auto Gain and latency-compensated Dry/Wet.
- Vintage tape machine interface with animated reels and VU meters.

[Unreleased]: https://github.com/notpumbaa/TapeSaturator/compare/v1.2.0...HEAD
[1.2.0]: https://github.com/notpumbaa/TapeSaturator/releases/tag/v1.2.0
[1.1.1]: https://github.com/notpumbaa/TapeSaturator/releases/tag/v1.1.1
[1.1.0]: https://github.com/notpumbaa/TapeSaturator/releases/tag/v1.1.0
[1.0.0]: https://github.com/notpumbaa/TapeSaturator/releases/tag/v1.0.0
