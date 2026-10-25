"""Loopback-only preview of the THREE actual generated assets, with device CSP.

Development tool only: no firmware endpoint, device request or runtime dependency.
"""
from pathlib import Path
from http.server import BaseHTTPRequestHandler, HTTPServer
import argparse
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from web_asset_pipeline import verify

manifest = verify(ROOT)
routes = {entry["path"]: entry for entry in manifest["assets"]}


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        entry = routes.get(self.path)
        if not entry:
            self.send_error(404); return
        data = (ROOT / "web/generated" / entry["file"]).read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", entry["content_type"])
        self.send_header("Content-Encoding", "gzip")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Content-Security-Policy", "default-src 'self'; connect-src 'self' ws://192.168.4.1; frame-ancestors 'none'; base-uri 'none'")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Cache-Control", "no-store" if self.path == "/" else "public, max-age=31536000, immutable")
        self.send_header("Connection", "close")
        self.end_headers(); self.wfile.write(data); self.close_connection = True


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    print("Generated-asset preview at http://127.0.0.1:" + str(args.port), flush=True)
    HTTPServer(("127.0.0.1", args.port), Handler).serve_forever()
