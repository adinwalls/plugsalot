"""Stand-in for the TONE3000 API used by CI and local tests. Implements the device authorisation flow
(approves on the 2nd poll, once slow_down has been exercised), the trending/latest/favorited lists,
a tone's models, and the model download (Bearer required). Run: python3 plugin/tests/mock_tone3000.py [port]"""
import json
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import parse_qs, urlparse

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8766
ACCESS = "mock-access-1"
REFRESH = "mock-refresh-1"
polls = 0
refreshes = 0
BASE = f"http://127.0.0.1:{PORT}"
NAM = open(sys.argv[2], "rb").read() if len(sys.argv) > 2 else b'{"version":"0.5.4","architecture":"WaveNet","config":{},"weights":[0]}'

TONES = [
    {"id": 101, "title": "Plexi Crunch", "gear": "amp", "format": "nam", "license": "cc-by",
     "user": {"id": 1, "username": "tonesmith", "display_name": None, "avatar_url": BASE + "/avatar.png", "is_verified": False},
     "images": [BASE + "/tone.png"], "url": "https://www.tone3000.com/tones/101", "models_count": 2},
    {"id": 102, "title": "Tube Screamer", "gear": "pedal", "format": "nam", "license": "t3k",
     "user": {"id": 2, "username": "pedalhead", "display_name": "Pedal Head", "avatar_url": None, "is_verified": True},
     "images": None, "url": "https://www.tone3000.com/tones/102", "models_count": 1},
]
MODELS = {"101": [{"id": 1001, "name": "Plexi Crunch (standard)", "size": 1, "tone_id": 101, "architecture_version": "2", "model_url": BASE + "/files/1001.nam"},
                  {"id": 1002, "name": "Plexi Crunch (lite)", "size": 1, "tone_id": 101, "architecture_version": "1", "model_url": BASE + "/files/1002.nam"}],
          "102": [{"id": 1003, "name": "TS808", "size": 1, "tone_id": 102, "architecture_version": "1", "model_url": BASE + "/files/1003.nam"}]}


class H(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def _send(self, code, obj, raw=None):
        body = raw if raw is not None else json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("content-type", "application/octet-stream" if raw is not None else "application/json")
        self.send_header("content-length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _authed(self):
        return self.headers.get("authorization") == f"Bearer {ACCESS}"

    def do_POST(self):
        global polls, refreshes
        form = parse_qs(self.rfile.read(int(self.headers.get("content-length", 0))).decode())
        path = urlparse(self.path).path
        print("POST", path, flush=True)
        if path == "/oauth/device_authorization":
            if form.get("client_id") != ["pk_test"]:
                return self._send(401, {"error": "invalid_client"})
            return self._send(200, {"device_code": "dev123", "user_code": "ABCD-1234", "verification_uri": "https://www.tone3000.com/activate",
                                    "verification_uri_complete": "https://www.tone3000.com/activate?user_code=ABCD-1234", "expires_in": 600, "interval": 1})
        if path == "/oauth/token":
            grant = form.get("grant_type", [""])[0]
            if grant.endswith("device_code"):
                polls += 1
                if polls == 1:
                    return self._send(400, {"error": "authorization_pending"})
                return self._send(200, {"access_token": ACCESS, "refresh_token": REFRESH, "token_type": "bearer", "expires_in": 3600, "scope": "read"})
            if grant == "refresh_token" and form.get("refresh_token") == [REFRESH]:
                refreshes += 1
                return self._send(200, {"access_token": ACCESS, "refresh_token": REFRESH, "token_type": "bearer", "expires_in": 3600, "scope": "read"})
            return self._send(400, {"error": "invalid_grant"})
        self._send(404, {"error": "not found"})

    def do_GET(self):
        u = urlparse(self.path)
        q = parse_qs(u.query)
        print("GET", self.path, flush=True)
        if u.path in ("/tones/trending", "/tones/latest", "/tones/favorited", "/tones/downloaded", "/tones/created"):
            if not self._authed():
                return self._send(401, {"error": "unauthorized"})
            data = TONES
            if q.get("gear"):
                data = [t for t in data if t["gear"] == q["gear"][0]]
            if q.get("query"):
                data = [t for t in data if q["query"][0].lower() in t["title"].lower()]
            out = {"data": data}
            if u.path not in ("/tones/trending", "/tones/latest"):
                out.update({"page": 1, "page_size": 25, "total": len(data), "total_pages": 1})
            return self._send(200, out)
        if u.path == "/models":
            if not self._authed():
                return self._send(401, {"error": "unauthorized"})
            return self._send(200, {"data": MODELS.get(q.get("tone_id", [""])[0], []), "page": 1, "page_size": 100, "total": 1, "total_pages": 1})
        if u.path.startswith("/files/"):
            if not self._authed():
                return self._send(401, {"error": "unauthorized"})
            return self._send(200, None, raw=NAM)
        self._send(404, {"error": "not found"})


HTTPServer(("127.0.0.1", PORT), H).serve_forever()
