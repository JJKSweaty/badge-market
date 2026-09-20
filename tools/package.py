#!/usr/bin/env python3
"""Validate the stock Share limits and package all required source modules."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
subprocess.run([sys.executable,str(root/'tools/build_copy.py')],check=True)
app = root/'app'/'badge_market'
files = sorted(p for p in app.iterdir() if p.suffix in {'.lua', '.cfg'})
size = sum(p.stat().st_size for p in files)
assert len(files) <= 16 and size <= 48*1024, 'Stock Share bundle cap'
assert all(p.stat().st_size <= 16*1024 for p in files), 'Stock sandbox file cap'
assert size + 2*4096 < 64*1024, 'Leave private A/B save quota'
dest = root/'dist'
dest.mkdir(exist_ok=True)
with ZipFile(dest/'badge_market.zip', 'w', ZIP_DEFLATED) as z:
    for p in files:
        z.write(p, 'badge_market/'+p.name)
report = {"slug":"badge_market", "bytes":size, "files":len(files),
          "sha256":{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in files}}
(dest/'manifest.json').write_text(json.dumps(report, indent=2)+'\n')
print(f'Badge Market: {len(files)} files, {size:,} bytes / 49,152 Share cap')
print(dest/'badge_market.zip')
