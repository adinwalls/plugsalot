"""Stand-in for the Gemini API used by CI: lists Flash models; the newest answers 503 (busy),
so the test proves Plugsalot falls back to the next model. Run: python3 plugin/tests/mock_gemini.py"""
import json
from http.server import BaseHTTPRequestHandler, HTTPServer

CHAIN = {"schema_version": 1, "source": "Lead vocal", "stage": "mixing",
         "chain": [{"module": "console_eq", "params": {"hpf_enabled": True, "hpf_hz": 90}},
                   {"module": "multicomp", "params": {"mode": "Opto", "opto_peak_reduction": 45}}],
         "explanation": "Mock chain from the fallback model."}


class H(BaseHTTPRequestHandler):
    def _send(self, code, obj):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("content-type", "application/json")
        self.send_header("content-length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        print("GET", self.path, flush=True)
        if self.headers.get("x-goog-api-key") != "good-key":
            return self._send(400, {"error": {"message": "API key not valid. Please pass a valid API key."}})
        self._send(200, {"models": [
            {"name": "models/gemini-3.8-flash", "displayName": "Gemini 3.8 Flash", "supportedGenerationMethods": ["generateContent"]},
            {"name": "models/gemini-3.5-flash", "displayName": "Gemini 3.5 Flash", "supportedGenerationMethods": ["generateContent"]},
            {"name": "models/gemini-3.5-flash-lite", "displayName": "Gemini 3.5 Flash-Lite", "supportedGenerationMethods": ["generateContent"]},
            {"name": "models/gemini-3.1-pro-preview", "displayName": "Gemini 3.1 Pro Preview", "supportedGenerationMethods": ["generateContent"]},
            {"name": "models/gemini-3.8-flash-image", "displayName": "Image model", "supportedGenerationMethods": ["generateContent"]},
            {"name": "models/text-embedding-004", "displayName": "Embedding", "supportedGenerationMethods": ["embedContent"]},
        ]})

    def do_POST(self):
        self.rfile.read(int(self.headers.get("content-length", 0)))
        print("POST", self.path, flush=True)
        if "gemini-3.8-flash" in self.path:
            return self._send(503, {"error": {"message": "This model is currently experiencing high demand."}})
        text = json.dumps(CHAIN) if "Test" not in self.path else '{"ok": true}'
        self._send(200, {"candidates": [{"content": {"parts": [{"text": text}]}}]})

    def log_message(self, *a):
        pass


HTTPServer(("127.0.0.1", 8765), H).serve_forever()
