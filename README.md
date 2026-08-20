# 🏊 SwimBand Pro — Intelligent Biomechanical Swimming Wearable

<p align="center">
  <img src="https://img.shields.io/badge/Hardware-ESP8266%20%2F%20NodeMCU-blue?style=for-the-badge&logo=espressif" alt="ESP8266">
  <img src="https://img.shields.io/badge/Sensor-MPU6050%206--DOF-emerald?style=for-the-badge" alt="MPU6050">
  <img src="https://img.shields.io/badge/Display-SH1106%20%2F%20SSD1306%20OLED-purple?style=for-the-badge" alt="OLED">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17%20%2F%20Arduino-orange?style=for-the-badge&logo=c%2B%2B" alt="C++17">
</p>

---

## 🌟 Overview

**SwimBand Pro** is an open-source, edge-AI wrist wearable and biomechanical swimming analytics system built for competitive swimmers, coaches, and triathletes. Operating entirely on an overclocked **ESP8266 (160 MHz)** coupled with an **MPU6050 6-DOF IMU**, SwimBand Pro computes high-fidelity stroke kinematics, velocity profiles, turn split times, and swimming efficiency (SWOLF) in real time.

SwimBand Pro features a **Zero-App Dual Interface**:
1. **Standalone Glassmorphic Web Dashboard**: Hosted directly by the band via Wi-Fi SoftAP (`http://192.168.1.4` or `http://swimband.local`) with live Canvas telemetry charts, SWOLF gauges, lap splits, pool calibration, and one-click CSV log download.
2. **Optional On-Wrist OLED Screen**: Automatically probes and drives SH1106 / SSD1306 displays with wrist-raise auto-wake, sleep power-saving, and live multi-page metrics.

---

## 🚀 Key Features

- **⚡ 3-Phase Biomechanical Kinematics**: Decomposes every stroke cycle into *Catch*, *Pull*, and *Recovery* phases with real-time phase symmetry scoring and blade propulsion index.
- **🧭 Mahony AHRS Quaternion Sensor Fusion**: Implements the Quake III Fast Inverse Square Root (`invSqrt`) for real-time gravity isolation with 0% CPU latency at 160MHz.
- **🛡️ False-Stroke Rejection (Cycloid vs Quarter-Arc vs Walking)**: Distinguishes between 3D cycloid hand rotations and 1D quarter-arc walking swings, ensuring zero false stroke counts during resting, walking, cheering, or fidgeting.
- **🔄 Wall Push-Off & Lap Split Timing**: Detects high-G wall push-offs to automatically time individual pool lengths, best splits, and lap counts.
- **🏊 In-Pool Dynamic Calibration**: Single-tap 25m/50m calibration button on the Web UI to auto-tune stroke length coefficients to individual swimmer biomechanics.
- **📊 Real-Time SWOLF & Quality Scoring**: Continuously estimates swimming efficiency ($\text{SWOLF} = \text{Time}_{\text{pool}} + \text{Strokes}_{\text{pool}}$) and stroke consistency.
- **💾 Flash-Safe LittleFS Circular Logging**: Stream-copy log rotation protects ESP8266 RAM and prevents flash memory exhaustion.
- **📶 Zero External Dependency**: Operates completely offline without cloud servers, external apps, or active internet connections.

---

## 🔌 Hardware Pinout & Wiring

Connect the MPU6050 (and optional OLED display) to the NodeMCU / ESP8266 board via the shared I2C bus:

| Module Pin | NodeMCU / ESP8266 Pin | GPIO Pin | Description |
| :--- | :--- | :--- | :--- |
| **VCC** | `3.3V` (or `5V` / `VIN`) | — | Power Supply |
| **GND** | `GND` | — | Common Ground |
| **SDA** (MPU6050 + OLED) | `D2` | `GPIO 4` | I2C Serial Data |
| **SCL** (MPU6050 + OLED) | `D1` | `GPIO 5` | I2C Serial Clock |

> [!NOTE]
> The OLED display is **optional**. The system automatically probes address `0x3C`. If no display is attached, it runs smoothly in standalone Web Dashboard mode without bus locking.

---

## 🧮 Mathematical & Biomechanical Foundations

### 1. Mahony AHRS Quaternion Fusion

The estimated gravity vector $\vec{v} = [v_x, v_y, v_z]^T$ is computed from the orientation quaternion $\mathbf{q} = [q_1, q_2, q_3, q_4]^T$:

$$
v_x = 2(q_2 q_4 - q_1 q_3)
$$

$$
v_y = 2(q_1 q_2 + q_3 q_4)
$$

$$
v_z = q_1^2 - q_2^2 - q_3^2 + q_4^2
$$

Linear dynamic acceleration is isolated:

$$
\vec{a}_{\text{linear}} = (\vec{a}_{\text{raw}} - \vec{v}) \cdot 9.80665 \text{ m/s}^2
$$

### 2. Hydrodynamic Drag & Velocity Decay

During the glide and recovery phase, deceleration is modeled via fluid drag equations:

$$
F_d = \frac{1}{2} \rho C_d A v^2 \implies a_{\text{drag}} = \frac{F_d}{m_{\text{swimmer}}}
$$

where $\rho = 997 \text{ kg/m}^3$, $C_d \approx 0.60$, and $A \approx 0.09 \text{ m}^2$.

### 3. Cycloid vs Quarter-Arc Motion Metric

The wrist trajectory ratio is derived from pitch and roll angular spans:

$$
r_{\text{ellipse}} = \frac{\min(\Delta\theta_{\text{pitch}}, \Delta\theta_{\text{roll}})}{\max(\Delta\theta_{\text{pitch}}, \Delta\theta_{\text{roll}})}
$$

- **Swimming**: Closed-loop 3D cycloid path ($r_{\text{ellipse}} \in [0.4, 0.8]$)
- **Walking**: Planar 1D pendulum swing ($r_{\text{ellipse}} < 0.25$)

---

## 🛠️ Installation & Flashing

### Option A: Arduino IDE
1. Open Arduino IDE and install the **ESP8266 Board Package** (`Tools > Board > Boards Manager > ESP8266`).
2. Install required libraries via Library Manager:
   - `U8g2` (by olikraus) — *for OLED support*
   - `LittleFS` (built-in for ESP8266 core)
3. Select board: **NodeMCU 1.0 (ESP-12E Module)** or **Generic ESP8266 Module**.
4. Set CPU Frequency: **160 MHz**.
5. Set Flash Size: **4MB (FS: 1MB / OTA:~1019KB)**.
6. Open `SwimBand_Master/SwimBand_Master.ino` and click **Upload**.

### Option B: PlatformIO (VS Code)
Add to your `platformio.ini`:
```ini
[env:nodemcuv2]
platform = espressif8266
board = nodemcuv2
framework = arduino
board_build.f_cpu = 160000000L
board_build.filesystem = littlefs
monitor_speed = 115200
lib_deps =
    olikraus/U8g2@^2.35.9
```

---

## 📱 How to Use the Web Dashboard

1. Power on the SwimBand device.
2. On your smartphone or laptop, connect to the Wi-Fi Access Point:
   - **SSID**: `SwimBand-Pro`
   - **Password**: `swim1234`
3. **Auto-Redirect (Captive Portal)**:
   - Your device will automatically pop up or redirect to the dashboard at **`http://192.168.1.4`** (or **`http://swimband.local`**).
   - **Disable Auto-Redirect**: If you prefer standard Wi-Fi mode without automatic popups, simply flip the **"Auto-Open Dashboard (Captive Portal)"** toggle switch inside the Web Dashboard settings. Your preference is automatically saved to flash memory.
4. View real-time speed, stroke cadence, SWOLF efficiency, and live trend graphs.
5. Click **"Download CSV"** to save your session data directly to your device.

---

## 🌐 REST API Reference

| Endpoint | Method | Description |
| :--- | :--- | :--- |
| `/` | `GET` | Serves the standalone Glassmorphic Web App. |
| `/api/state` | `GET` | Returns full real-time telemetry JSON payload. |
| `/api/reset` | `POST` | Resets all active session counters and split times. |
| `/api/cal/start?distance=50` | `POST` | Initiates in-pool distance auto-calibration (25m or 50m). |
| `/api/cal/stop` | `POST` | Finalizes calibration and saves calibrated factor. |
| `/api/settings/captive_portal?enabled=true` | `POST` | Enables or disables Captive Portal auto-redirection on the Fly. |
| `/api/log/download` | `GET` | Streams the CSV session log file directly. |
| `/api/log/clear` | `POST` | Clears stored flash logs and recreates CSV header. |

---

## 📁 Repository Structure

```
SwimBand/
├── SwimBand_Master/              # 🚀 Flagship Firmware Codebase
│   ├── SwimBand_Master.ino       # Core orchestration, setup, loop & server
│   ├── Biomechanics.h            # Mahony AHRS, Hydrodynamics & Stroke Phase Engine
│   ├── Config.h                  # Hardware pins, network & swimmer profiles
│   └── WebDashboard.h            # HTML5/CSS3/JS Web Application in PROGMEM
├── receiver_server/              # 📡 Companion Telemetry Receivers (Optional)
│   ├── server.js                 # Node.js UDP clock sync & HTTP ingestion server
│   ├── server.py                 # Python 3 zero-dependency companion receiver
│   └── package.json
└── README.md                     # Documentation & Getting Started Guide
```
