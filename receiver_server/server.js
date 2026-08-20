/**
 * SwimBand Pro - Telemetry Ingest & Clock Sync Receiver (Node.js)
 * ----------------------------------------------------------------
 * Listens on:
 *   - UDP Port 9090: Responds to "TIME_REQ" with current Epoch timestamp
 *   - HTTP Port 8080: Ingests JSON telemetry packets and appends to CSV
 */

const http = require('http');
const dgram = require('dgram');
const fs = require('fs');
const path = require('path');

const HTTP_PORT = 8080;
const UDP_PORT = 9090;
const LOG_FILE = path.join(__dirname, 'swim_telemetry_live.csv');

// Initialize CSV header if not present
if (!fs.existsSync(LOG_FILE)) {
  fs.writeFileSync(
    LOG_FILE,
    'timestamp,epoch,device,state,speed_mps,stroke_rate_spm,strokes,distance_m,laps,swolf,quality\n'
  );
}

// -----------------------------------------------------------------------------
// 1. UDP Clock Synchronization Server (:9090)
// -----------------------------------------------------------------------------
const udpServer = dgram.createSocket('udp4');

udpServer.on('message', (msg, rinfo) => {
  const req = msg.toString().trim();
  if (req === 'TIME_REQ') {
    const epochSec = Math.floor(Date.now() / 1000);
    const response = `TIME:${epochSec}`;
    udpServer.send(response, rinfo.port, rinfo.address, (err) => {
      if (err) console.error('[UDP Error]', err);
      else console.log(`[UDP Sync] Sent Epoch ${epochSec} to ${rinfo.address}:${rinfo.port}`);
    });
  }
});

udpServer.bind(UDP_PORT, () => {
  console.log(`[UDP Clock Sync] Listening on 0.0.0.0:${UDP_PORT}`);
});

// -----------------------------------------------------------------------------
// 2. HTTP Ingestion Server (:8080)
// -----------------------------------------------------------------------------
const server = http.createServer((req, res) => {
  if (req.method === 'POST' && (req.url === '/ingest' || req.url === '/session_end')) {
    let body = '';
    req.on('data', chunk => { body += chunk.toString(); });
    req.on('end', () => {
      try {
        const data = JSON.parse(body);
        console.log(`[HTTP ${req.url}] Received from ${data.device || 'ESP8266'}: State=${data.state || 'ACTIVE'} Speed=${data.metrics ? data.metrics.speed_mps : data.speed_mps || 0} m/s`);

        if (data.metrics) {
          const row = `${Date.now()},${data.epoch || Math.floor(Date.now()/1000)},${data.device || 'ESP8266'},${data.state},${data.metrics.speed_mps},${data.metrics.stroke_rate_spm},${data.metrics.stroke_count},${data.metrics.distance_m},${data.metrics.lap_count},${data.metrics.swolf_like || 0},${data.metrics.quality_conf || 0}\n`;
          fs.appendFileSync(LOG_FILE, row);
        }

        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ status: 'ok', received: true }));
      } catch (err) {
        console.error('[HTTP Ingest Parse Error]', err.message);
        res.writeHead(400, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ status: 'error', message: err.message }));
      }
    });
  } else if (req.method === 'GET' && req.url === '/') {
    res.writeHead(200, { 'Content-Type': 'text/html' });
    res.end('<h1>SwimBand Live Receiver Server Running</h1><p>Logging telemetry to <code>swim_telemetry_live.csv</code></p>');
  } else {
    res.writeHead(404);
    res.end();
  }
});

server.listen(HTTP_PORT, () => {
  console.log(`[HTTP Ingestion Server] Listening on http://0.0.0.0:${HTTP_PORT}`);
});
