#!/usr/bin/env python3
"""
SwimBand Pro - Telemetry Ingestion & Clock Sync Server (Python 3)
-----------------------------------------------------------------
Zero-dependency telemetry receiver using only the Python standard library.
Listens for:
  - UDP 9090: Responds to 'TIME_REQ' with Epoch time
  - HTTP 8080: POST /ingest and /session_end with JSON payloads
"""

import http.server
import socketserver
import socket
import threading
import json
import time
import os

HTTP_PORT = 8080
UDP_PORT = 9090
LOG_FILE = os.path.join(os.path.dirname(__file__), "swim_telemetry_live.csv")

# Initialize CSV Header
if not os.path.exists(LOG_FILE):
    with open(LOG_FILE, "w", encoding="utf-8") as f:
        f.write("timestamp,epoch,device,state,speed_mps,stroke_rate_spm,strokes,distance_m,laps,swolf,quality\n")

def run_udp_server():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", UDP_PORT))
    print(f"[UDP Clock Sync] Listening on 0.0.0.0:{UDP_PORT}")
    while True:
        try:
            data, addr = sock.recvfrom(1024)
            msg = data.decode("utf-8", errors="ignore").strip()
            if msg == "TIME_REQ":
                epoch = int(time.time())
                resp = f"TIME:{epoch}".encode("utf-8")
                sock.sendto(resp, addr)
                print(f"[UDP Sync] Sent Epoch {epoch} to {addr[0]}:{addr[1]}")
        except Exception as e:
            print(f"[UDP Error] {e}")

class TelemetryHandler(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path in ("/ingest", "/session_end"):
            content_length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(content_length).decode("utf-8", errors="ignore")
            try:
                data = json.loads(body)
                state = data.get("state", "ACTIVE")
                device = data.get("device", "ESP8266")
                metrics = data.get("metrics", {})
                speed = metrics.get("speed_mps", data.get("speed_mps", 0.0))
                spm = metrics.get("stroke_rate_spm", data.get("stroke_rate_spm", 0.0))
                strokes = metrics.get("stroke_count", data.get("stroke_count", 0))
                dist = metrics.get("distance_m", data.get("total_distance_m", 0.0))
                laps = metrics.get("lap_count", data.get("lap_count", 0))
                swolf = metrics.get("swolf_like", data.get("swolf_score", 0.0))
                qual = metrics.get("quality_conf", data.get("quality_score", 0.0))
                epoch = data.get("epoch", int(time.time()))

                print(f"[HTTP {self.path}] {device} | State: {state} | Speed: {speed:.2f} m/s | SPM: {spm:.1f} | Dist: {dist:.1f} m")

                row = f"{int(time.time()*1000)},{epoch},{device},{state},{speed},{spm},{strokes},{dist},{laps},{swolf},{qual}\n"
                with open(LOG_FILE, "a", encoding="utf-8") as f:
                    f.write(row)

                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(b'{"status":"ok","received":true}')
            except Exception as e:
                self.send_response(400)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"status":"error","message":str(e)}).encode("utf-8"))
        else:
            self.send_response(404)
            self.end_headers()

    def do_GET(self):
        if self.path == "/":
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.end_headers()
            self.wfile.write(b"<h1>SwimBand Python Telemetry Receiver Running</h1><p>Logging to <code>swim_telemetry_live.csv</code></p>")
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        pass  # Suppress default HTTP logging to keep console clean

def run_http_server():
    server = socketserver.TCPServer(("0.0.0.0", HTTP_PORT), TelemetryHandler)
    print(f"[HTTP Ingest Server] Listening on http://0.0.0.0:{HTTP_PORT}")
    server.serve_forever()

if __name__ == "__main__":
    t_udp = threading.Thread(target=run_udp_server, daemon=True)
    t_udp.start()
    run_http_server()
