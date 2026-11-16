"""Regenerate the app text font with icon codepoints reserved for Material Icons.

Run with Python fonttools installed; normal builds use the committed font asset.
"""
from pathlib import Path
from fontTools.ttLib import TTFont

root = Path(__file__).resolve().parents[1]
font = TTFont(root / 'resources/inter/Inter-Switch.ttf')
for table in font['cmap'].tables:
    if table.isUnicode():
        table.cmap = {code: glyph for code, glyph in table.cmap.items() if not 0xE000 <= code <= 0xF8FF}
for name in font['name'].names:
    names = {1: 'Navidrome Music', 2: 'Regular', 3: 'NavidromeMusic-Regular-1.0',
             4: 'Navidrome Music Regular', 6: 'NavidromeMusic-Regular',
             16: 'Navidrome Music', 17: 'Regular'}
    if name.nameID in names:
        name.string = names[name.nameID].encode(name.getEncoding())
font.save(root / 'resources/inter/NavidromeMusic.ttf')
