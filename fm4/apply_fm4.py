#!/usr/bin/env python3
"""Apply FM4 engine registration, custom badges, and editor integration.

Run from root of a clean Polyend tracker-ce checkout with FM4 files uploaded.
Each edit has an explicit upstream anchor and fails closed on version mismatch.
On failure, discard local changes / reset the branch before retrying.
"""
from pathlib import Path


def update(filename: str, old: str, new: str) -> None:
    path = Path(filename)
    if not path.is_file():
        raise FileNotFoundError(f"Expected upstream file absent: {path}")
    source = path.read_text(encoding="utf-8")
    if new in source:
        print(f"Already patched: {filename}")
        return
    count = source.count(old)
    if count != 1:
        raise RuntimeError(f"Expected exactly one anchor, got {count}: {filename}, {old!r}")
    path.write_text(source.replace(old, new, 1), encoding="utf-8")
    print(f"Patched: {filename}")


assets = [
    Path("core/synth/engines/FM4DSP.h"),
    Path("core/synth/engines/FM4SineTable.h"),
    Path("core/synth/engines/FM4SynthVoice.h"),
    Path("core/synth/engines/FM4SynthVoice.cpp"),
    Path("core/libs/images/fm4_80x28_L2.h"),
    Path("core/libs/images/fm4_108x40_L2.h"),
]
missing = [str(file) for file in assets if not file.is_file()]
if missing:
    raise FileNotFoundError("FM4 source files missing: " + ", ".join(missing))

update("synth/include/PolySynth/config.h",
       "kLastSynth = kChiptuneSynth,",
       "    kFM4Synth,\n    kLastSynth = kFM4Synth,")
update("core/synth/registry.cpp",
       '#include "engines/ChiptuneSynthVoice.h"',
       '#include "engines/ChiptuneSynthVoice.h"\n#include "engines/FM4SynthVoice.h"')
update("core/synth/registry.cpp",
       "createChiptuneSynth());",
       "createChiptuneSynth());\n"
       "    registry.addInstrumentType(static_cast<int>(InstrumentTypeId::kFM4Synth),\n"
       "                                createFM4Synth());")
update("core/synth/Synth.h",
       "polysynth::InstrumentTypeId::kChiptuneSynth };",
       "polysynth::InstrumentTypeId::kChiptuneSynth, polysynth::InstrumentTypeId::kFM4Synth };")

# L2 FT812 assets are registered with distinct bitmap indices and nonoverlapping RAM_G addresses.
# Existing bitmap IDs remain unchanged; append FM4 at the very end of the FEATURE_SYNTH list.
update("core/libs/NewDisplay/displayStructs.h",
       "kBitmapCHIP,",
       "kBitmapCHIP,\n    kBitmapMiniFM4,\n    kBitmapFM4,")
update("core/libs/NewDisplay/display.cpp",
       '#include "images/chip_mini_nomark_80x28_L2.h"',
       '#include "images/chip_mini_nomark_80x28_L2.h"\n'
       '#include "images/fm4_80x28_L2.h"\n'
       '#include "images/fm4_108x40_L2.h"')
update("core/libs/NewDisplay/display.cpp",
       "{ chip_med_108x40_L2, 127700, 1080, 108, 40, L2, 27 },",
       "{ chip_med_108x40_L2, 127700, 1080, 108, 40, L2, 27 },\n"
       "{ fm4_80x28_L2, 128800, 560, 80, 28, L2, 20 },\n"
       "{ fm4_108x40_L2, 129400, 1080, 108, 40, L2, 27 },")

# A separate synth type with its own bitmap in both places.
update("core/interface/instrumentEditor/SynthEditor.cpp",
       '"ACD", "FAT", "VAP", "WTFM", "PERC", "DIRT", "CHIP",',
       '"ACD", "FAT", "VAP", "WTFM", "PERC", "DIRT", "CHIP", "FM4",')
update("core/interface/instrumentEditor/SynthEditor.cpp",
       'enDisplayBitmaps::kBitmapMiniCHIP',
       'enDisplayBitmaps::kBitmapMiniCHIP, enDisplayBitmaps::kBitmapMiniFM4')
update("core/interface/instrumentEditor/SynthPatchEditor.cpp",
       'enDisplayBitmaps::kBitmapCHIP',
       'enDisplayBitmaps::kBitmapCHIP, enDisplayBitmaps::kBitmapFM4')
print("FM4 fully registered. Build firmware and validate on the instrument before flashing widely.")
