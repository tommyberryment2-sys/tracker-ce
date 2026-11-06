# FM4 — final 292-inspired LPG / LPF fix (v1.1.2)

This is a source-only GitHub Actions update for the *existing*, tested FM4 v1.1
Tracker Mini fork. **Do not flash any files from this ZIP**. It must be compiled
by the new GitHub Actions workflow first and the generated .ptf inspected.

## Changes (no new parameter IDs)

- Replaces the previous LPG and LPF single-pole filters with a switched
  **two-pole passive Buchla 292-inspired audio network**.
- Both modes use one shared nonlinear vactrol optical state that drives **both
  matched photoresistances**; no independent, arbitrary output gain taper.
- **LPG** (Buchla 'Both') uses the two-pole network with no C3 feedback capacitor.
- **LPF** switches in a 4.7 nF feedback capacitor, giving a different pole/Q
  configuration and a steeper high-frequency response with a mild peak.
- Preserves the clean **VCA** path in v1.1 as a deliberate compatibility choice.
  It already follows the independent operator ADSRs; it is **not** a claim that
  this clean bypass exactly emulates the Buchla's separate resistive VCA mode.
- Vactrol opening is rapid, closing slower, with state-dependent rate.
- Audio circuit is solved via **trapezoidal capacitor companions**, keeping
  meaningful circuit states as the optical resistance changes.
- Voice-local light and capacitor states; no shared gating between notes.
- Brightness, Env to Cutoff, LPG Response, Output Mode, and all other existing
  FM4 parameters remain natively assignable to Tracker macros.
- Preserves original FM4 parameter IDs 0–53, patch format, operator editors,
  16 waveforms, 8 algorithms, feedback, fixed Hz, wavefolder, saturation and logo.

## Important modeling limitation

This is an **original 292-inspired model**, not a laboratory-calibrated replica:
its LED-control mapping, optical memory, and resistance curve are simplified
heuristics chosen for inexpensive per-voice real-time operation on the Mini.
The LPF / LPG *audio network topology*, capacitor values and mode switch are
informed by Parker & D'Angelo (2013), A Digital Model of the Buchla Lowpass-Gate:
https://dangelo.audio/dafx13-lpg

The LPF circuit is **not** a perfect constant-gain filter: like the original
292, its resistor network can also attenuate low-frequency output when closed.
This is intentional and different from the old simplistic gain-taper approach.

## GitHub installation (browser only)

1. Save a copy of the working FM4 v1.1 firmware and your projects/presets.
2. Extract the ZIP.
3. From your GitHub fork's Code tab select **Add file > Upload files**.
4. Drag the **fm4_final_292** folder into GitHub's upload area (not the main page).
5. Drag the **.github** folder separately into the same upload area.
6. Verify the paths below appear, then commit directly to main:
   - fm4_final_292/FM4292.h
   - fm4_final_292/FM4DSP.h
   - fm4_final_292/FM4SynthVoice.cpp
   - fm4_final_292/test_292.cpp
   - .github/workflows/fm4-final-292-mini.yml
7. From Actions, select **Build FM4 Final 292 Tracker Mini**, choose **Run workflow**.
8. If green, download the **FM4-Final-292-TrackerMini-ptf** artifact ZIP.
9. Inspect/check the .ptf first; then back up SD and install as a firmware update.
   Keep the known-good v1.1 firmware for rollback.

## Verify on hardware

- In VCA mode, old Init and saved sounds should retain the original amplitude
  behavior and the post-output gain should not modify fold intensity.
- In LPF / LPG, use a bright saw or square, Env to Cutoff 100%, Brightness
  40–60%, and a long decay with Sustain 0–25%. LPF should close with a more
  pronounced spectral filtering character; LPG should darken *and* lose
  amplitude more gradually, with an optical tail.
- Check all six voices, preset load/save, and macro control of Output Mode,
  Brightness and LPG Response; do not rely only on one static sustain tone.
- Switch modes at a moderate volume. Discrete mode switches may still click
  during a loud sustained note, as topology actually changes.

## Host verification

- `tests/test_dsp.cpp`: original algorithm and waveform regression
- `tests/test_v11.cpp`: 16 waves, four feedback paths, fixed frequency, fold
- `tests/test_output_modes.cpp`: stage differentiation and gain independence
- `tests/test_292.cpp`: optical memory, mode-specific spectral response,
  distinct rendered outputs and finite audio during rapid mode sweeps

**Status:** host regression tests pass. The full Polyend/ARM cross-compile and
actual Tracker Mini hardware test must be completed separately.
