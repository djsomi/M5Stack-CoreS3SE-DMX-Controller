"""Loopback-only, finite WebSocket fixture for real-browser projection QA.

Not firmware/a device simulator: no mutations or persistent data. Uses the
actual three gzip assets/CSP. Mock lifecycle/ordering faults live in Node tests.
"""
import argparse
import base64
import hashlib
import json
from http.server import ThreadingHTTPServer
import select
import struct
import time
from preview import Handler


class Fixture(Handler):
    protocol_version = "HTTP/1.1"
    quiet = False
    stall_after = None
    def do_GET(self):
        if self.path != "/ws":
            return super().do_GET()
        if self.headers.get("Upgrade", "").lower() != "websocket":
            self.send_error(400); return
        accept = base64.b64encode(hashlib.sha1((self.headers.get("Sec-WebSocket-Key", "") +
            "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()).decode()
        self.send_response(101)
        self.send_header("Upgrade", "websocket")
        self.send_header("Connection", "Upgrade")
        self.send_header("Sec-WebSocket-Accept", accept)
        self.end_headers()
        sequence = 0
        values = [index % 256 for index in range(512)]
        blackout = False

        def send(kind, **fields):
            nonlocal sequence
            sequence += 1
            message = dict(v=1, type=kind, id=0, revision=str(9007199254740993 + sequence), seq=str(sequence))
            message.update(fields)
            payload = json.dumps(message, separators=(",", ":")).encode()
            frame = b"\x81" + (bytes([len(payload)]) if len(payload) < 126 else b"\x7e" + struct.pack("!H", len(payload)))
            self.connection.sendall(frame + payload)

        def snapshot(request_id=0):
            send("state.snapshot", id=request_id, values=values, presets=[0, 1, 2, 3, 0, 1, 0, 1], blackout=blackout, sender=True, loaded=0, modified=False)

        try:
            send("hello", channels=512, max_message=256, max_clients=4, max_outstanding=4)
            snapshot()
            started = time.monotonic()
            deadline = started + 180
            tick = time.monotonic()
            self.connection.settimeout(1)
            while time.monotonic() < deadline:
                if select.select([self.connection], [], [], .05)[0]:
                    header = self.rfile.read(2)
                    if len(header) != 2 or header[0] & 15 == 8:
                        break
                    length = header[1] & 127
                    if length == 127:
                        break
                    if length == 126:
                        length = struct.unpack("!H", self.rfile.read(2))[0]
                    if length > 256 or not header[1] & 128 or header[0] != 129:
                        break
                    mask = self.rfile.read(4)
                    payload = self.rfile.read(length)
                    message = json.loads(bytes(byte ^ mask[i % 4] for i, byte in enumerate(payload)))
                    if set(message) != {"v", "id", "type"} or message["v"] != 1 or message["type"] != "state.request":
                        raise ValueError("Fixture rejected a mutation/unsupported request")
                    # Test-only application black hole: TCP remains OPEN but
                    # stops providing updates or snapshot replies after N seconds.
                    if self.stall_after is None or time.monotonic() - started < self.stall_after:
                        snapshot(message["id"])
                if not self.quiet and (self.stall_after is None or time.monotonic() - started < self.stall_after) and time.monotonic() - tick >= .1:
                    tick = time.monotonic(); values[0] = (values[0] + 1) % 256
                    blackout = values[0] // 20 % 2 == 1
                    send("state.update", channel=1, value=values[0], blackout=blackout, sender=True, loaded=0, modified=False)
        except (OSError, ValueError, IndexError):
            pass
        finally:
            self.close_connection = True


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8766)
    parser.add_argument("--quiet", action="store_true", help="No incremental updates; answer idle health snapshots")
    parser.add_argument("--stall-after", type=float, help="Keep TCP open but stop updates/snapshot replies after N seconds")
    args = parser.parse_args()
    if args.stall_after is not None and not 0 <= args.stall_after < 180:
        parser.error("--stall-after must be within 0..180 seconds")
    Fixture.quiet = args.quiet
    Fixture.stall_after = args.stall_after
    print("TEST FIXTURE ONLY http://127.0.0.1:" + str(args.port), flush=True)
    ThreadingHTTPServer(("127.0.0.1", args.port), Fixture).serve_forever()
