#!/usr/bin/env python3
"""Register original Noise Companion DEV engine after FM4, fail closed on upstream drift.

Run ONLY in an ephemeral GitHub Actions checkout AFTER apply_fm4.py, never in
an installed working firmware tree. Preserves existing type IDs and bitmaps.
"""
from pathlib import Path

def update(path,old,new):
    path=Path(path)
    if not path.is_file():raise RuntimeError(f'Missing upstream file: {path}')
    txt=path.read_text()
    if new in txt:
        print('Already patched',path)
        return
    if txt.count(old)!=1:
        raise RuntimeError(f'Upstream anchor count {txt.count(old)} !=1 in {path}: {old[:90]!r}')
    path.write_text(txt.replace(old,new,1))
    print('Patched',path)

needed=[
 'core/synth/engines/NoiseCompanionCore.h',
 'core/synth/engines/NoiseCompanionDevDSP.h',
 'core/synth/engines/NoiseCompanionVoice.h',
 'core/synth/engines/NoiseCompanionVoice.cpp',
 'core/libs/images/noise_lab_80x28_L2.h',
 'core/libs/images/noise_lab_108x40_L2.h',
]
assert all(Path(p).is_file() for p in needed),[p for p in needed if not Path(p).is_file()]

update('synth/include/PolySynth/config.h',
       '    kFM4Synth,\n    kLastSynth = kFM4Synth,',
       '    kFM4Synth,\n    kNoiseCompanionDevSynth,\n    kLastSynth = kNoiseCompanionDevSynth,')
update('core/synth/registry.cpp',
       '#include "engines/FM4SynthVoice.h"',
       '#include "engines/FM4SynthVoice.h"\n#include "engines/NoiseCompanionVoice.h"')
update('core/synth/registry.cpp',
       '                                createFM4Synth());',
       '                                createFM4Synth());\n'
       '    registry.addInstrumentType(static_cast<int>(InstrumentTypeId::kNoiseCompanionDevSynth),\n'
       '                                createNoiseCompanionDevSynth());')
update('core/synth/Synth.h',
       'polysynth::InstrumentTypeId::kFM4Synth };',
       'polysynth::InstrumentTypeId::kFM4Synth, polysynth::InstrumentTypeId::kNoiseCompanionDevSynth };')
update('core/libs/NewDisplay/displayStructs.h',
       '    kBitmapMiniFM4,\n    kBitmapFM4,',
       '    kBitmapMiniFM4,\n    kBitmapFM4,\n    kBitmapMiniNPL,\n    kBitmapNPL,')
update('core/libs/NewDisplay/display.cpp',
       '#include "images/fm4_108x40_L2.h"',
       '#include "images/fm4_108x40_L2.h"\n'
       '#include "images/noise_lab_80x28_L2.h"\n'
       '#include "images/noise_lab_108x40_L2.h"')
update('core/libs/NewDisplay/display.cpp',
       '{ fm4_108x40_L2, 129400, 1080, 108, 40, L2, 27 },',
       '{ fm4_108x40_L2, 129400, 1080, 108, 40, L2, 27 },\n'
       '{ noise_lab_80x28_L2, 130520, 560, 80, 28, L2, 20 },\n'
       '{ noise_lab_108x40_L2, 131120, 1080, 108, 40, L2, 27 },')
update('core/interface/instrumentEditor/SynthEditor.cpp',
       '"CHIP", "FM4",',
       '"CHIP", "FM4", "NPL DEV",')
update('core/interface/instrumentEditor/SynthEditor.cpp',
       'enDisplayBitmaps::kBitmapMiniFM4',
       'enDisplayBitmaps::kBitmapMiniFM4, enDisplayBitmaps::kBitmapMiniNPL')
update('core/interface/instrumentEditor/SynthPatchEditor.cpp',
       'enDisplayBitmaps::kBitmapFM4',
       'enDisplayBitmaps::kBitmapFM4, enDisplayBitmaps::kBitmapNPL')
print('Noise Companion DEV added after FM4, with separate type ID and display assets')
