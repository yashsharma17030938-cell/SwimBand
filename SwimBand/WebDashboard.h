#ifndef SWIMBAND_WEBDASHBOARD_H
#define SWIMBAND_WEBDASHBOARD_H

#include <Arduino.h>

// =============================================================================
//  SwimBand Pro - Embedded Glassmorphic Responsive Web Application
//  Stored in PROGMEM (Flash) for zero RAM consumption on ESP8266.
// =============================================================================

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
  <title>SwimBand Pro &bull; Telemetry Dashboard</title>
  <style>
    :root {
      --bg: #070d19;
      --card: rgba(17, 29, 51, 0.75);
      --card-border: rgba(255, 255, 255, 0.08);
      --text: #e2e8f0;
      --muted: #94a3b8;
      --primary: #38bdf8;
      --accent: #0284c7;
      --ok: #34d399;
      --warn: #fbbf24;
      --danger: #f87171;
      --glow: rgba(56, 189, 248, 0.15);
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; }
    body {
      background: radial-gradient(1200px 800px at 50% -20%, #172a52 0%, var(--bg) 65%);
      color: var(--text);
      min-height: 100vh;
      padding: 16px;
      overflow-x: hidden;
    }
    .wrap { max-width: 1240px; margin: 0 auto; }
    
    /* Header */
    header {
      display: flex; justify-content: space-between; align-items: center;
      padding: 16px 20px; background: var(--card); backdrop-filter: blur(12px); -webkit-backdrop-filter: blur(12px);
      border: 1px solid var(--card-border); border-radius: 20px;
      box-shadow: 0 12px 32px rgba(0,0,0,0.35); margin-bottom: 16px;
    }
    .brand h1 { font-size: 22px; font-weight: 800; letter-spacing: -0.5px; background: linear-gradient(135deg, #fff 30%, #38bdf8 100%); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
    .brand p { font-size: 12px; color: var(--muted); margin-top: 2px; }
    .state-badge {
      font-size: 13px; font-weight: 700; padding: 8px 16px; border-radius: 999px;
      letter-spacing: 1px; text-transform: uppercase; border: 1px solid rgba(255,255,255,0.15);
      background: rgba(255,255,255,0.05); transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
    }
    .state-SWIM_EASY { background: rgba(52, 211, 153, 0.15); border-color: var(--ok); color: var(--ok); box-shadow: 0 0 20px rgba(52, 211, 153, 0.3); }
    .state-SWIM_RACE { background: rgba(248, 113, 113, 0.2); border-color: var(--danger); color: var(--danger); box-shadow: 0 0 24px rgba(248, 113, 113, 0.4); }
    .state-SWIM_REST { background: rgba(251, 191, 36, 0.15); border-color: var(--warn); color: var(--warn); }
    .state-WALK { background: rgba(251, 191, 36, 0.1); border-color: var(--warn); color: var(--warn); }
    .state-IDLE { background: rgba(148, 163, 184, 0.1); border-color: var(--muted); color: var(--muted); }

    /* Grid Layout */
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(170px, 1fr)); gap: 12px; margin-bottom: 16px; }
    .card {
      background: var(--card); backdrop-filter: blur(10px); -webkit-backdrop-filter: blur(10px);
      border: 1px solid var(--card-border); border-radius: 18px; padding: 14px 16px;
      box-shadow: 0 8px 24px rgba(0,0,0,0.22); position: relative; overflow: hidden;
    }
    .card::before {
      content: ""; position: absolute; top: 0; left: 0; right: 0; height: 3px;
      background: linear-gradient(90deg, transparent, var(--glow), transparent); opacity: 0.5;
    }
    .card-label { font-size: 11px; text-transform: uppercase; letter-spacing: 0.8px; color: var(--muted); font-weight: 600; }
    .card-val { font-size: 28px; font-weight: 800; margin: 4px 0 2px; color: #fff; line-height: 1.1; }
    .card-val span { font-size: 14px; font-weight: 600; color: var(--muted); margin-left: 2px; }
    .card-sub { font-size: 12px; color: var(--muted); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }

    /* Wide Cards */
    .two-col { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-bottom: 16px; }
    @media (max-width: 860px) { .two-col { grid-template-columns: 1fr; } }
    
    /* Progress / Confidence Meter */
    .bar-row { display: flex; align-items: center; justify-content: space-between; margin: 6px 0 3px; font-size: 12px; color: var(--muted); }
    .bar-row b { color: var(--text); }
    .progress-track { height: 8px; background: rgba(255,255,255,0.06); border-radius: 999px; overflow: hidden; margin-bottom: 8px; }
    .progress-fill { height: 100%; border-radius: 999px; transition: width 0.3s ease; }
    .fill-swim { background: linear-gradient(90deg, #38bdf8, #34d399); }
    .fill-walk { background: linear-gradient(90deg, #f59e0b, #fbbf24); }
    .fill-race { background: linear-gradient(90deg, #f87171, #ef4444); }

    /* Canvas Charts */
    .chart-box {
      background: var(--card); border: 1px solid var(--card-border); border-radius: 20px;
      padding: 16px; margin-bottom: 16px; box-shadow: 0 8px 24px rgba(0,0,0,0.22);
    }
    .chart-head { display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; }
    .chart-head h3 { font-size: 14px; font-weight: 700; color: var(--text); }
    .legend { display: flex; gap: 14px; font-size: 11px; color: var(--muted); }
    .legend span { display: flex; align-items: center; gap: 5px; }
    .legend i { display: inline-block; width: 8px; height: 8px; border-radius: 50%; }
    canvas { width: 100%; height: 200px; display: block; background: rgba(5,10,20,0.4); border-radius: 12px; }

    /* Settings & Toggle Switch */
    .toggle-row {
      display: flex; justify-content: space-between; align-items: center;
      padding: 12px 14px; background: rgba(0,0,0,0.25); border-radius: 14px;
      border: 1px solid var(--card-border); margin-top: 8px;
    }
    .toggle-info { display: flex; flex-direction: column; gap: 2px; }
    .toggle-title { font-size: 13px; font-weight: 700; color: var(--text); }
    .toggle-desc { font-size: 11px; color: var(--muted); }
    .switch {
      position: relative; display: inline-block; width: 48px; height: 26px; flex-shrink: 0;
    }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider {
      position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0;
      background-color: #334155; transition: .3s cubic-bezier(0.4, 0, 0.2, 1);
      border-radius: 34px; border: 1px solid rgba(255,255,255,0.1);
    }
    .slider:before {
      position: absolute; content: ""; height: 18px; width: 18px; left: 3px; bottom: 3px;
      background-color: white; transition: .3s cubic-bezier(0.4, 0, 0.2, 1);
      border-radius: 50%; box-shadow: 0 2px 4px rgba(0,0,0,0.4);
    }
    input:checked + .slider { background-color: var(--ok); }
    input:checked + .slider:before { transform: translateX(22px); }

    /* Action Buttons */
    .btn-group { display: flex; gap: 10px; flex-wrap: wrap; margin-top: 10px; }
    button {
      appearance: none; border: 1px solid var(--card-border); cursor: pointer;
      background: rgba(255,255,255,0.06); color: var(--text); padding: 10px 14px;
      border-radius: 12px; font-size: 12px; font-weight: 700; transition: all 0.2s;
      display: inline-flex; align-items: center; gap: 6px;
    }
    button:hover { background: rgba(255,255,255,0.12); border-color: rgba(255,255,255,0.2); }
    button:active { transform: scale(0.98); }
    .btn-primary { background: linear-gradient(180deg, #0284c7, #0369a1); border-color: #38bdf8; color: #fff; }
    .btn-primary:hover { background: linear-gradient(180deg, #0369a1, #075985); }
    .btn-danger { background: rgba(239, 68, 68, 0.15); border-color: rgba(239, 68, 68, 0.4); color: #fca5a5; }
    .btn-danger:hover { background: rgba(239, 68, 68, 0.25); }

    /* Footer */
    footer { text-align: center; color: var(--muted); font-size: 12px; padding: 12px 0 24px; }
  </style>
</head>
<body>
  <div class="wrap">
    <!-- Header -->
    <header>
      <div class="brand">
        <h1>SwimBand Pro</h1>
        <p>Real-Time Biomechanical Kinematics &bull; ESP8266 + MPU6050</p>
      </div>
      <div id="stateBadge" class="state-badge state-IDLE">INITIALIZING</div>
    </header>

    <!-- Top Telemetry Metrics -->
    <div class="grid">
      <div class="card">
        <div class="card-label">Current Speed</div>
        <div class="card-val"><span id="speed">0.00</span><span>m/s</span></div>
        <div class="card-sub">Peak: <b id="peakSpeed">0.00</b> m/s</div>
      </div>

      <div class="card">
        <div class="card-label">Stroke Cadence</div>
        <div class="card-val"><span id="spm">0.0</span><span>SPM</span></div>
        <div class="card-sub">Total Strokes: <b id="strokes">0</b></div>
      </div>

      <div class="card">
        <div class="card-label">Est. Distance</div>
        <div class="card-val"><span id="distance">0.0</span><span>m</span></div>
        <div class="card-sub">Lengths (~25m): <b id="lengths">0</b></div>
      </div>

      <div class="card">
        <div class="card-label">Laps & Turns</div>
        <div class="card-val" id="laps">0</div>
        <div class="card-sub">Push-Off: <b id="pushG">0.0</b> g</div>
      </div>

      <div class="card">
        <div class="card-label">SWOLF Score</div>
        <div class="card-val" id="swolf">0.0</div>
        <div class="card-sub">Quality: <b id="quality">0%</b></div>
      </div>

      <div class="card">
        <div class="card-label">Active Swim Time</div>
        <div class="card-val" id="activeTime">00:00</div>
        <div class="card-sub">Rest Time: <b id="restTime">00:00</b></div>
      </div>
    </div>

    <!-- Middle Analysis Sections -->
    <div class="two-col">
      <!-- Confidence & Motion Classifier Card -->
      <div class="card">
        <div class="card-label" style="margin-bottom:8px">Neural-Kinematic Motion Confidence</div>
        
        <div class="bar-row"><span>Swimming Confidence</span><b id="swimC">0%</b></div>
        <div class="progress-track"><div id="swimBar" class="progress-fill fill-swim" style="width:0%"></div></div>

        <div class="bar-row"><span>Walking Rejection Gate</span><b id="walkC">0%</b></div>
        <div class="progress-track"><div id="walkBar" class="progress-fill fill-walk" style="width:0%"></div></div>

        <div class="bar-row"><span>Sprint / Race Intensity</span><b id="raceC">0%</b></div>
        <div class="progress-track"><div id="raceBar" class="progress-fill fill-race" style="width:0%"></div></div>

        <div class="bar-row" style="margin-top:12px">
          <span>Cycloid Arc: <b id="cyc">0.00</b></span>
          <span>Quarter Arc: <b id="qarc">0.00</b></span>
          <span>Ellipse Ratio: <b id="erat">0.00</b></span>
        </div>
      </div>

      <!-- Stroke 3-Phase Kinematics, Calibration & Portal Settings -->
      <div class="card">
        <div class="card-label" style="margin-bottom:8px">3-Phase Kinematics & Portal Controls</div>
        <div class="bar-row"><span>Phase Symmetry</span><b id="symm">100%</b></div>
        <div class="bar-row"><span>Current Stroke Length</span><b id="strokeLen">1.25 m</b></div>
        <div class="bar-row"><span>Last Split Time</span><b id="lastSplit">0.00 s</b></div>
        <div class="bar-row"><span>Best Split Time</span><b id="bestSplit">0.00 s</b></div>
        <div class="bar-row"><span>Continuous Streak</span><b id="contSwim">00:00</b></div>

        <!-- Captive Portal Auto-Redirect Toggle -->
        <div class="toggle-row">
          <div class="toggle-info">
            <div class="toggle-title">Auto-Open Dashboard (Captive Portal)</div>
            <div class="toggle-desc" id="portalStatus">Redirects browser when connecting to Wi-Fi</div>
          </div>
          <label class="switch">
            <input type="checkbox" id="portalToggle" onchange="togglePortal(this.checked)">
            <span class="slider"></span>
          </label>
        </div>

        <div class="btn-group">
          <button class="btn-primary" onclick="startCal(25)">Calibrate 25m</button>
          <button class="btn-primary" onclick="startCal(50)">Calibrate 50m</button>
          <button onclick="downloadCsv()">Download CSV</button>
          <button class="btn-danger" onclick="resetSession()">Reset Counters</button>
        </div>
      </div>
    </div>

    <!-- Real-time Live Trend Chart -->
    <div class="chart-box">
      <div class="chart-head">
        <h3>Live Real-Time Telemetry Trend</h3>
        <div class="legend">
          <span><i style="background:#38bdf8"></i> Speed (m/s)</span>
          <span><i style="background:#34d399"></i> Stroke Rate (SPM)</span>
          <span><i style="background:#fbbf24"></i> Accel Variance</span>
        </div>
      </div>
      <canvas id="trendChart" width="1150" height="220"></canvas>
    </div>

    <!-- Footer -->
    <footer>
      SwimBand Master Pro &bull; NodeMCU ESP8266 &bull; Web Dashboard Live
    </footer>
  </div>

  <script>
    const MAX_PTS = 200;
    const series = { speed: [], spm: [], avar: [] };
    let portalEnabledState = true;

    function fmtTime(ms) {
      ms = Math.max(0, Math.floor(ms || 0));
      const s = Math.floor(ms / 1000);
      const m = Math.floor(s / 60);
      const r = s % 60;
      return String(m).padStart(2, '0') + ':' + String(r).padStart(2, '0');
    }

    function pushVal(arr, v) {
      arr.push(v);
      if (arr.length > MAX_PTS) arr.shift();
    }

    function drawChart() {
      const c = document.getElementById('trendChart');
      if (!c) return;
      const ctx = c.getContext('2d');
      const w = c.width, h = c.height;
      ctx.clearRect(0, 0, w, h);

      // Grid Lines
      ctx.strokeStyle = 'rgba(255, 255, 255, 0.05)';
      ctx.lineWidth = 1;
      for (let i = 1; i <= 4; i++) {
        let y = (h / 5) * i;
        ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke();
      }

      function renderSeries(data, color, scale, offset) {
        if (data.length < 2) return;
        ctx.beginPath();
        ctx.strokeStyle = color;
        ctx.lineWidth = 2.5;
        for (let i = 0; i < data.length; i++) {
          const x = (i * w) / (MAX_PTS - 1);
          const y = h - (data[i] * scale + offset);
          if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
        }
        ctx.stroke();
      }

      renderSeries(series.speed, '#38bdf8', 65, 15);
      renderSeries(series.spm, '#34d399', 1.8, 15);
      renderSeries(series.avar, '#fbbf24', 160, 15);
    }

    async function fetchTelemetry() {
      try {
        const res = await fetch('/api/state?t=' + Date.now(), { cache: 'no-store' });
        const d = await res.json();

        // Update Badge
        const badge = document.getElementById('stateBadge');
        badge.textContent = d.state;
        badge.className = 'state-badge state-' + d.state;

        // Metrics
        document.getElementById('speed').textContent = d.speed_mps.toFixed(2);
        document.getElementById('peakSpeed').textContent = d.peak_speed_mps.toFixed(2);
        document.getElementById('spm').textContent = d.stroke_rate_spm.toFixed(1);
        document.getElementById('strokes').textContent = d.stroke_count;
        document.getElementById('distance').textContent = d.total_distance_m.toFixed(1);
        document.getElementById('lengths').textContent = Math.floor(d.total_distance_m / 25);
        document.getElementById('laps').textContent = d.lap_count;
        document.getElementById('pushG').textContent = d.last_push_g.toFixed(2);
        document.getElementById('swolf').textContent = d.swolf_score.toFixed(1);
        document.getElementById('quality').textContent = Math.round(d.quality_score * 100) + '%';
        document.getElementById('activeTime').textContent = fmtTime(d.active_swim_ms);
        document.getElementById('restTime').textContent = fmtTime(d.rest_ms);
        document.getElementById('contSwim').textContent = fmtTime(d.continuous_swim_ms);

        // Confidences
        document.getElementById('swimC').textContent = Math.round(d.swim_conf * 100) + '%';
        document.getElementById('walkC').textContent = Math.round(d.walk_conf * 100) + '%';
        document.getElementById('raceC').textContent = Math.round(d.race_conf * 100) + '%';
        document.getElementById('swimBar').style.width = (d.swim_conf * 100) + '%';
        document.getElementById('walkBar').style.width = (d.walk_conf * 100) + '%';
        document.getElementById('raceBar').style.width = (d.race_conf * 100) + '%';

        // Kinematics
        document.getElementById('cyc').textContent = d.cycloid_score.toFixed(2);
        document.getElementById('qarc').textContent = d.quarter_arc_score.toFixed(2);
        document.getElementById('erat').textContent = d.ellipse_ratio.toFixed(2);
        document.getElementById('symm').textContent = Math.round(d.phase_symmetry * 100) + '%';
        document.getElementById('strokeLen').textContent = d.stroke_length_m.toFixed(2) + ' m';
        document.getElementById('lastSplit').textContent = (d.last_split_ms / 1000).toFixed(2) + ' s';
        document.getElementById('bestSplit').textContent = (d.best_split_ms / 1000).toFixed(2) + ' s';

        // Captive Portal State Sync
        if (typeof d.captive_portal === 'boolean' && d.captive_portal !== portalEnabledState) {
          portalEnabledState = d.captive_portal;
          document.getElementById('portalToggle').checked = portalEnabledState;
          document.getElementById('portalStatus').textContent = portalEnabledState
            ? 'Redirects browser automatically on connection'
            : 'Disabled (Standard Wi-Fi mode)';
        }

        // Push to Chart
        pushVal(series.speed, d.speed_mps || 0);
        pushVal(series.spm, d.stroke_rate_spm || 0);
        pushVal(series.avar, d.acc_var || 0);
        drawChart();
      } catch (err) {
        console.warn('Polling error:', err);
      }
    }

    async function togglePortal(enable) {
      portalEnabledState = enable;
      document.getElementById('portalStatus').textContent = enable
        ? 'Redirects browser automatically on connection'
        : 'Disabled (Standard Wi-Fi mode)';
      try {
        await fetch('/api/settings/captive_portal?enabled=' + (enable ? 'true' : 'false'), { method: 'POST' });
      } catch (e) {
        console.error('Portal toggle failed', e);
      }
    }

    async function startCal(dist) {
      await fetch('/api/cal/start?distance=' + dist, { method: 'POST' });
      alert('Started ' + dist + 'm In-Pool Calibration. Swim the distance to complete auto-tuning.');
    }

    async function resetSession() {
      if (confirm('Reset all counters and split times?')) {
        await fetch('/api/reset', { method: 'POST' });
        series.speed = []; series.spm = []; series.avar = [];
        fetchTelemetry();
      }
    }

    function downloadCsv() {
      window.location = '/api/log/download';
    }

    setInterval(fetchTelemetry, 500);
    window.addEventListener('resize', drawChart);
    fetchTelemetry();
  </script>
</body>
</html>
)rawliteral";

#endif // SWIMBAND_WEBDASHBOARD_H
