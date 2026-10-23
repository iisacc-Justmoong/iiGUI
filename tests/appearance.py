"""LVRS appearance data is a verified design snapshot, not a runtime SDK link."""
from pathlib import Path
import hashlib
import json
import sys

root=Path(sys.argv[1])
catalog=json.loads((root/'resources/lvrs-native-appearance.json').read_text())
for font in catalog['fonts']:
    local=root/'resources/native'/font['file']
    assert hashlib.sha256(local.read_bytes()).hexdigest()==font['sha256'], f'Font changed: {local}'
license=(root/'resources/native/Pretendard-OFL.txt').read_text()
assert 'SIL OPEN FONT LICENSE Version 1.1' in license and 'Kil Hyung-jin' in license
lvrs=root.parent/'LVRS'
if lvrs.is_dir():
    for source in catalog['visual_sources']:
        original=lvrs/source['file']
        assert hashlib.sha256(original.read_bytes()).hexdigest()==source['sha256'], f'LVRS appearance source drift: {original}'
    for font in catalog['fonts']:
        original=root.parent/font['source']
        assert hashlib.sha256(original.read_bytes()).hexdigest()==font['sha256'], f'LVRS font drift: {original}'
print('LVRS appearance/font snapshot verified; Qt and QML are not runtime dependencies')
