"""Check saved selection compatibility against the 0.5.424 fork checkpoint."""
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[1]

def array(source, name):
    match = re.search(r'\b' + name + r'\[\]\s*=\s*\{(.*?)\};', source, re.S)
    assert match, name
    return re.findall(r'"([^"\n]*)"', match[1])

for file, names in {
    'mp_fixes.cpp': ['CustomWeapons'],
    'class_editor.cpp': ['Weapons', 'Sidearms'],
    'overlay.cpp': ['weapons', 'sidearms'],
}.items():
    relative = 'CoD3Host/' + file
    before = subprocess.check_output(['git', 'show', 'b22188d5067f4ec8e52bcc8e65540bb173da8eaf:' + relative], cwd=root, text=True)
    after = (root / relative).read_text()
    for name in names:
        old, new = array(before, name), array(after, name)
        assert new[:len(old)] == old, f'{file}:{name}: existing saved IDs changed'
        assert len(new) == (28 if 'sidearm' in name.lower() else 26), (file, name)

settings = (root / 'CoD3Host/settings.cpp').read_text()
assert 'v.classPrimary[i] > 25' in settings
assert 'v.classSidearm[i] > 27' in settings
print('PASS: saved weapon IDs are unchanged and all five selection tables expose the new slots')
