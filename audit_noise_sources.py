#!/usr/bin/env python3
"""File-level inventory only. Never copies third-party code into build outputs.

Usage: python3 tools/audit_noise_sources.py /path/to/Noise_plethora /path/to/out
Reports are metadata; license flags require human review.
"""
from pathlib import Path
import csv, json, re, sys

BANKS = [
["radioOhNo", "Rwalk_SineFMFlange", "xModRingSqr", "XModRingSine", "CrossModRing", "resonoise", "grainGlitch", "grainGlitchII", "grainGlitchIII", "basurilla"],
["clusterSaw", "pwCluster", "crCluster2", "sineFMcluster", "TriFMcluster", "PrimeCluster", "PrimeCnoise", "FibonacciCluster", "partialCluster", "phasingCluster"],
["BasuraTotal", "Atari", "WalkingFilomena", "S_H", "arrayOnTheRocks", "existencelsPain", "whoKnows", "satanWorkout", "Rwalk_BitCrushPW", "Rwalk_LFree"]
]

def analyze(root):
    rows=[]
    for bank, names in enumerate(BANKS,1):
        for slot, name in enumerate(names,1):
            fn=root/("P_"+name+".hpp")
            row={"bank":bank,"slot":slot,"program":name,"file":fn.name,"present":fn.is_file()}
            if fn.is_file():
                body=fn.read_text(encoding="utf-8",errors="replace")
                header="\n".join(body.splitlines()[:18])
                notices=[l.strip() for l in header.splitlines() if any(w in l.lower() for w in ("license","copyright","creative commons","gpl"))]
                row.update({"notices":" | ".join(notices),
                    "includes":"; ".join(re.findall(r'^\s*#include\s*[<\"]([^>\"]+)',body,re.M)),
                    "audio_objects":"; ".join(sorted(set(re.findall(r'\b(Audio(?:Synth|Effect|Mixer|Filter|Analyze|Stream)[A-Za-z0-9_]*)\b',body)))),
                    "uses_micros":bool(re.search(r'\bmicros\s*\(',body)),
                    "uses_random":bool(re.search(r'\b(?:random|rand|lfsr|noise)\b',body,re.I)),
                    "static_array_decls":"; ".join(re.findall(r'\b(?:int16_t|float|uint8_t|int32_t)\s+\w+\[\s*\w+\s*\]',body)),
                    "bytes_of_source":fn.stat().st_size,
                    "review_status":"NEEDS HUMAN LICENSING AND DSP DEPENDENCY REVIEW"})
            else:
                row["review_status"]="MISSING SOURCE FILE"
            rows.append(row)
    return rows

def main():
    if len(sys.argv)!=3:
        raise SystemExit("Usage: audit_noise_sources.py SOURCE_REPO OUTPUT_DIR")
    src=Path(sys.argv[1]); out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
    rows=analyze(src)
    (out/"program_inventory.json").write_text(json.dumps(rows,indent=2),encoding="utf-8")
    with (out/"program_inventory.csv").open("w",newline="",encoding="utf-8") as f:
        writer=csv.DictWriter(f,fieldnames=list(rows[0].keys())+list(dict.fromkeys(k for r in rows for k in r.keys() if k not in rows[0])))
        writer.writeheader();writer.writerows(rows)
    lines=["# Befaco Noise Plethora source audit (metadata only)","", "**No DSP was copied. This is not a license clearance.**", "", "| Bank | Slot | Program | File exists | License notice | Audio objects |", "|---:|---:|---|---|---|---|"]
    for r in rows:
        lines.append(f'| {r["bank"]} | {r["slot"]} | {r["program"]} | {"yes" if r["present"] else "NO"} | {(r.get("notices") or "unidentified")[:85].replace("|", " · ")} | {(r.get("audio_objects") or "—")[:90].replace("|", " · ")} |')
    lines.extend(["","Do not integrate third-party DSP into the Polyend firmware without addressing the applicable license obligations and dependencies."])
    (out/"AUDIT.md").write_text("\n".join(lines)+"\n",encoding="utf-8")
    print(f'Audited {len(rows)} listed programs; {sum(int(r["present"]) for r in rows)} source files found. Output: {out}')
    if any(not r["present"] for r in rows):sys.exit(2)
if __name__=="__main__":main()
