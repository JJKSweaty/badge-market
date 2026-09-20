#!/usr/bin/env python3
"""Package the three native flash segments, preserving their IDF offsets."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json
import shutil

root = Path(__file__).resolve().parents[1]
build = root / 'firmware/build'
config = json.loads((build / 'flasher_args.json').read_text())
dest = root / 'dist/native'
dest.mkdir(parents=True, exist_ok=True)
files = []
for offset, filename in config['flash_files'].items():
    source = build / filename
    target = dest / source.name
    shutil.copyfile(source, target)
    files.append({'offset': offset, 'file': target.name, 'bytes': target.stat().st_size,
                  'sha256': hashlib.sha256(target.read_bytes()).hexdigest()})
manifest = {'name': 'Badge Market native', 'version': '0.3.1', 'chip': 'esp32c3',
            'idf': '5.5.3', 'flash_settings': config['flash_settings'], 'files': files}
(dest / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
readme = (root / 'firmware/README.md').read_text().replace('(../MARKET-README.md)', '(MARKET-README.md)')
readme = readme.replace('[implementation/design contract](../docs/architecture.md) and\n', '')
(dest / 'README.md').write_text(readme)
shutil.copyfile(root / 'firmware/VERIFICATION.md', dest / 'VERIFICATION.md')
guide = (root / 'MARKET-README.md').read_text().replace('(firmware/README.md)', '(README.md)')
(dest / 'MARKET-README.md').write_text(guide)
shutil.copyfile(root / 'DEMO-PLAN.md', dest / 'DEMO-PLAN.md')
with ZipFile(root / 'dist/badge_market_firmware.zip', 'w', ZIP_DEFLATED) as archive:
    for p in sorted(dest.iterdir()):
        archive.write(p, 'badge_market_firmware/' + p.name)
print(root / 'dist/badge_market_firmware.zip')
