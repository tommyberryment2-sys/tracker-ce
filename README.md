# FM4 v1.1 development overlay — Tracker Mini

Upload the **contents of this package**, preserving the directories, to the existing
`tracker-ce` fork's `main` branch. It adds an independent Actions workflow named
**Build FM4 v1.1 Tracker Mini**. It does not replace committed v1 source files.

The Actions workflow overlays v1.1 files on its temporary build runner and then
runs host tests, applies the existing CE registration patch, compiles the firmware,
and uploads a firmware artifact if and only if compilation succeeds.

## Changes
- 16 independent operator waveforms; indices 0–7 retained from v1.
- OP1, OP2, OP3 individual feedback; OP4 keeps v1 parameter ID 34.
- Per-operator ratio/fixed-frequency modes and frequency setting.
- Dynamic wavefolding and saturation after carrier mix, driven by the
  envelope-shaped signal. Fold Input is static input trim, not another VCA.
- VCA, LPG, LPF output modes. VCA preserves original operator envelope
  response. LPF follows a carrier-envelope-controlled cutoff; LPG uses a
  slower, asymmetrical cutoff response. Both intentionally avoid applying
  a second full-strength envelope VCA, which would double-attenuate sound.
- Post-stage master Output gain, independent of folding intensity.
- Logo rebuilt for 80x28 and 108x40 two-bit grayscale display assets.
- Four operator editor sections use native Oscillator section graphics.
- New parameters are native synth parameters and should be visible to the
  standard Tracker macro-assignment interface. Macro automation via FX slots
  is inherited from CE, not modified here.

## Important validation limits
Host DSP tests and source patch fixture tests passed locally. This package
has **not** been fully cross-compiled against Polyend CE in this session,
and has **not** been tested on a Tracker Mini. Do not flash until the GitHub
Actions build passes, and keep the confirmed v1 .ptf installer backed up.

Saved-patch parameter IDs 0-48 remain unchanged; new IDs 49-53 are appended.
This is intended to preserve v1 parameter mappings, but real-world patch
compatibility remains to be tested on the device.
