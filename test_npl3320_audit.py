import importlib.util
import tempfile
from pathlib import Path
spec=importlib.util.spec_from_file_location('audit',Path(__file__).resolve().parents[1]/'tools/audit_noise_sources.py')
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
assert sum(map(len,module.BANKS))==30
with tempfile.TemporaryDirectory() as td:
    p=Path(td)
    for bank in module.BANKS:
        for name in bank:
            (p/f'P_{name}.hpp').write_text('// Licensed under GPL-3.0-or-later\n#include "Plugin.hpp"\nclass Example { AudioEffectGranular granular1; };\n')
    rows=module.analyze(p)
    assert len(rows)==30 and all(r['present'] for r in rows)
    assert all('GPL-3.0-or-later' in r['notices'] for r in rows)
    assert all('AudioEffectGranular' in r['audio_objects'] for r in rows)
print('PASS source audit parser: all 30 entries, license notices, audio dependencies')
