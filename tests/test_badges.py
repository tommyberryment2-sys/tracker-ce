"""Verify packed 2-bit FM4 badge format: byte count and four possible shades."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]/'core/libs/images'
for dimensions in ['80x28','108x40']:
    w,h=map(int,dimensions.split('x'))
    src=(root/('fm4_'+dimensions+'_L2.h')).read_text()
    body=src[src.index('{')+1:src.index('};')]
    data=[int(x,16) for x in re.findall(r'0x([A-Fa-f0-9]{2})',body)]
    assert len(data)==w*h//4, (dimensions,len(data))
    pixels=[(v >> shift)&3 for v in data for shift in (6,4,2,0)]
    assert len(set(pixels)) >= 3
    assert min(pixels)==0 and max(pixels)==3
    print('FM4 badge',dimensions,len(data),'bytes, 2-bit L2 OK')
