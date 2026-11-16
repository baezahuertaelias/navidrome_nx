"""Local Navidrome fixture for the actual Aurore app screens; no real account."""
import hashlib
import http.server
import io
import json
import math
import os
import struct
import subprocess
import threading
import urllib.parse
import wave
import zlib

ARTISTS = [{'id': f'artist-{i}', 'name': f'Artist {i}'} for i in range(3)]
ALBUMS = [{'id': f'album-{i}', 'name': f'Album {i:02}', 'artist': f'Artist {i % 3}', 'coverArt': f'cover-{i}', 'created': '2026-11-01'} for i in range(56)]
SONGS = [{'id': f'song-{i}', 'title': f'Track {i:02}', 'artist': f'Artist {i % 3}', 'album': f'Album {i // 2:02}', 'coverArt': f'cover-{i // 2}', 'duration': 20, 'suffix': 'flac', 'bitRate': 900, 'samplingRate': 44100, 'bitDepth': 16, 'channelCount': 2} for i in range(12)]

def png(seed):
    color = hashlib.sha256(seed.encode()).digest()
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    rows = b''.join(b'\0' + bytes(channel for x in range(64) for channel in (30 + (color[0] + x * 2) % 170, 25 + (color[1] + y * 2) % 170, 50 + color[2] % 150)) for y in range(64))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 64, 64, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b'')

wav = io.BytesIO()
with wave.open(wav, 'wb') as output:
    output.setparams((2, 2, 22050, 0, 'NONE', 'not compressed'))
    frame = b''.join(struct.pack('<hh', int(6000 * math.sin(i * math.tau * 440 / 22050)), int(3000 * math.sin(i * math.tau * 220 / 22050))) for i in range(22050))
    output.writeframes(frame * 20)

class Server(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass
    def do_GET(self):
        query = urllib.parse.parse_qs(urllib.parse.urlsplit(self.path).query)
        expected = hashlib.md5(('test-secret' + query.get('s', [''])[0]).encode()).hexdigest()
        if query.get('u') != ['test-user'] or query.get('t') != [expected]:
            self.send_error(401); return
        endpoint = urllib.parse.urlsplit(self.path).path.split('/')[-1].removesuffix('.view')
        body = {'status': 'ok'}
        if endpoint == 'getAlbumList2':
            offset = int(query.get('offset', ['0'])[0]); size = int(query.get('size', ['50'])[0])
            body['albumList2'] = {'album': ALBUMS[offset:offset+size]}
        elif endpoint == 'search3':
            term = query.get('query', [''])[0].lower().strip('"')
            result = {}
            for kind, values in [('artist', ARTISTS), ('song', SONGS), ('album', ALBUMS)]:
                matches = [item for item in values if term in item.get('title', item.get('name', '')).lower()]
                offset = int(query.get(kind+'Offset', ['0'])[0]); count = int(query.get(kind+'Count', ['0'])[0])
                result[kind] = matches[offset:offset+count]
            body['searchResult3'] = result
        elif endpoint == 'getStarred2': body['starred2'] = {'song': SONGS[:6], 'album': ALBUMS[:3]}
        elif endpoint == 'getRandomSongs': body['randomSongs'] = {'song': SONGS[:int(query.get('size', ['12'])[0])]}
        elif endpoint == 'getAlbum': body['album'] = {'song': SONGS[:6]}
        elif endpoint == 'getArtist': body['artist'] = {'album': ALBUMS[:4]}
        elif endpoint == 'getPlaylists': body['playlists'] = {'playlist': [{'id': 'playlist-0', 'name': 'Evening playlist', 'songCount': 6, 'coverArt': 'playlist-cover'}]}
        elif endpoint == 'getPlaylist': body['playlist'] = {'entry': SONGS[:6]}
        elif endpoint == 'getLyricsBySongId': body['lyricsList'] = {'structuredLyrics': [{'synced': True, 'line': [{'start': i * 1000, 'value': f'Lyrics line {i+1}'} for i in range(24)]}]}
        raw = json.dumps({'subsonic-response': body}).encode()
        if endpoint == 'getCoverArt': raw = png(query.get('id', ['cover'])[0])
        elif endpoint == 'stream': raw = wav.getvalue()
        self.send_response(200); self.send_header('Content-Length', str(len(raw))); self.end_headers()
        try: self.wfile.write(raw)
        except (BrokenPipeError, ConnectionResetError): pass

with http.server.ThreadingHTTPServer(('127.0.0.1', 0), Server) as server:
    threading.Thread(target=server.serve_forever, daemon=True).start()
    env = dict(os.environ, NAVIDROME_UI_SERVER=f'http://127.0.0.1:{server.server_port}')
    binary = os.path.abspath('build.ui/navidrome_ui_tests')
    try:
        for locale in ['en-US', 'es-419']:
            subprocess.run([binary], env=dict(env, NAVIDROME_UI_LANG=locale), check=True)
    finally:
        server.shutdown()
