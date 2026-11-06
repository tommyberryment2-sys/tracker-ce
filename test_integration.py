"""Host-side integration smoke tests for the FM4 upstream patcher.

Fixtures reproduce expected exact source anchors; actual firmware compile is the
required external integration gate, not simulated by these tests.
"""
from pathlib import Path
import tempfile
import subprocess
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]
files = {
    'synth/include/PolySynth/config.h': 'enum { kChiptuneSynth, kLastSynth = kChiptuneSynth, };\n',
    'core/synth/registry.cpp': '#include "engines/ChiptuneSynthVoice.h"\nvoid registerTypes(){createChiptuneSynth());}\n',
    'core/synth/Synth.h': 'auto types = { polysynth::InstrumentTypeId::kChiptuneSynth };\n',
    'core/libs/NewDisplay/displayStructs.h': 'enum IDs {kBitmapCHIP,};\n',
    'core/libs/NewDisplay/display.cpp': '#include "images/chip_mini_nomark_80x28_L2.h"\n'
        'const strBitmap bitmaps[]={ { chip_med_108x40_L2, 127700, 1080, 108, 40, L2, 27 }, };\n',
    'core/interface/instrumentEditor/SynthEditor.cpp': 'const char *names[]={"ACD", "FAT", "VAP", "WTFM", "PERC", "DIRT", "CHIP",};\n'
        'auto imgs[]={enDisplayBitmaps::kBitmapMiniCHIP};\n',
    'core/interface/instrumentEditor/SynthPatchEditor.cpp':
        'auto imgs[]={enDisplayBitmaps::kBitmapCHIP};\n',
}
with tempfile.TemporaryDirectory() as td:
    path=Path(td)
    for relative,txt in files.items():
        dst=path/relative
        dst.parent.mkdir(parents=True,exist_ok=True)
        dst.write_text(txt)
    for relative in ['core/synth/engines/FM4DSP.h','core/synth/engines/FM4SineTable.h','core/synth/engines/FM4SynthVoice.h',
                     'core/synth/engines/FM4SynthVoice.cpp', 'core/libs/images/fm4_80x28_L2.h',
                     'core/libs/images/fm4_108x40_L2.h']:
        dst=path/relative;dst.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(ROOT/relative,dst)
    script=str(ROOT/'fm4/apply_fm4.py')
    for iteration in range(2):
        subprocess.run([sys.executable,script],cwd=path,check=True,capture_output=True)
    assert 'kBitmapMiniFM4' in (path/'core/libs/NewDisplay/displayStructs.h').read_text()
    assert 'kBitmapFM4' in (path/'core/interface/instrumentEditor/SynthPatchEditor.cpp').read_text()
    assert 'createFM4Synth()' in (path/'core/synth/registry.cpp').read_text()
    assert 'fm4_108x40_L2, 129400' in (path/'core/libs/NewDisplay/display.cpp').read_text()
    assert '"FM4"' in (path/'core/interface/instrumentEditor/SynthEditor.cpp').read_text()
print('FM4 patch integration and idempotence OK (fixture test, not firmware build)')
