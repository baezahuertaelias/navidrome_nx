"""Create the static bold lyrics font; needs fonttools only when regenerating.

Input: docs/font-files/InterVariable.ttf from rsms/inter v4.1.
Usage: python scripts/prepare_lyrics_font.py /path/to/InterVariable.ttf
"""
from pathlib import Path
import sys
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont

font = instantiateVariableFont(TTFont(sys.argv[1]), {'wght': 700, 'opsz': 14}, inplace=False)
assert font['OS/2'].usWeightClass == 700 and 'fvar' not in font
names = {1: 'Navidrome Lyrics', 2: 'Bold', 3: 'NavidromeLyrics-Bold-1.0',
         4: 'Navidrome Lyrics Bold', 6: 'NavidromeLyrics-Bold', 16: 'Navidrome Lyrics', 17: 'Bold'}
for record in font['name'].names:
    if record.nameID in names:
        record.string = names[record.nameID].encode(record.getEncoding())
font.save(Path(__file__).resolve().parents[1] / 'resources/inter/NavidromeLyrics-Bold.ttf')
