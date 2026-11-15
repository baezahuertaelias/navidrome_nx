import hashlib
import http.server
import json
import subprocess
import threading
import urllib.parse


class Server(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass  # Do not print authenticated request URLs.

    def do_GET(self):
        url = urllib.parse.urlsplit(self.path)
        query = urllib.parse.parse_qs(url.query)
        expected = hashlib.md5(b"s3cretfixed-salt").hexdigest()
        if query.get("u") != ["a user+&"] or query.get("t") != [expected]:
            self.send_error(401)
            return
        endpoint = url.path.removeprefix("/rest/").removesuffix(".view")
        status = 200
        body = {"subsonic-response": {"status": "ok"}}
        if endpoint == "getAlbumList2":
            body["subsonic-response"]["albumList2"] = {"album": [{"id": "id /&", "name": "Album", "artist": "Artist", "coverArt": "cover /&"}]}
        elif endpoint == "getAlbum":
            if query.get("id") != ["id /&"]:
                self.send_error(400)
                return
            body["subsonic-response"]["album"] = {"song": [{"id": "track", "title": "Track", "duration": 185}]}
        elif endpoint == "denied":
            body = {"subsonic-response": {"status": "failed", "error": {"code": 40}}}
        elif endpoint == "getStarred2":
            body["subsonic-response"]["starred2"] = {"album": [{"id": "favorite-album", "name": "Favorite album"}], "song": [{"id": "favorite-track", "title": "Favorite track", "coverArt": "cover /&"}]}
        elif endpoint == "getLyricsBySongId":
            if query.get("id") == ["empty"]:
                body["subsonic-response"]["lyricsList"] = {}
            else:
                body["subsonic-response"]["lyricsList"] = {"structuredLyrics": [{"synced": True, "line": [{"start": 0, "value": "First test line"}, {"start": 2000, "value": "Second test line"}]}, {"line": [{"value": "Another language"}]}]}
        elif endpoint == "getLyrics":
            body["subsonic-response"]["lyrics"] = {"value": "First test line\r\n\nLast test line"}
        elif endpoint == "getCoverArt" and (query.get("id") != ["cover /&"] or query.get("size") != ["128"]):
            self.send_error(400)
            return
        elif endpoint == "missingStatus":
            body = {"subsonic-response": {}}
        elif endpoint == "httpError":
            status = 503
        elif endpoint == "redirect":
            status = 302
        raw = json.dumps(body).encode()
        if endpoint == "stream":
            raw = b"fake mp3"
        elif endpoint == "getCoverArt":
            import base64
            raw = base64.b64decode("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aZV8AAAAASUVORK5CYII=")
        elif endpoint == "malformed":
            raw = b"not json"
        self.send_response(status)
        if status == 302:
            self.send_header("Location", "/must-not-follow")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)


with http.server.ThreadingHTTPServer(("127.0.0.1", 0), Server) as server:
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        subprocess.run(["/tmp/api_test", f"http://127.0.0.1:{server.server_port}"], check=True)
    finally:
        server.shutdown()
