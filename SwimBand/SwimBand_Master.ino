/*
  =============================================================================
  🏊 SwimBand Pro Master - Flagship Biomechanical Swim & Activity Tracker
  =============================================================================
  Target Hardware:
    - NodeMCU / ESP8266 (ESP-12E/F) @ 160 MHz
    - MPU6050 6-DOF IMU (Accelerometer +/-2g, Gyroscope +/-500 dps)
    - Optional: 1.3" / 0.96" I2C OLED (SH1106 / SSD1306) on same I2C bus

  Wiring:
    - MPU6050 VCC -> 3.3V (or 5V if module has onboard 3.3V LDO)
    - MPU6050 GND -> GND
    - MPU6050 SDA -> D2 (GPIO 4)
    - MPU6050 SCL -> D1 (GPIO 5)
    - Optional OLED SDA -> D2, SCL -> D1

  Features:
    - Real-Time 3-Phase Biomechanical Kinematics (Catch, Pull, Recovery, Symmetry)
    - Mahony AHRS Quaternion Sensor Fusion with Fast Inverse Square Root (invSqrt)
    - Cycloid vs Quarter-Arc neural motion discriminator (zero false stroke counts)
    - In-Pool Dynamic Calibration (25m / 50m auto-tuning)
    - Wall Push-Off Turn Detection & Lap Split Timing
    - Dual Interface:
        * Standalone Glassmorphic Web Dashboard at http://192.168.1.4 (or http://swimband.local)
        * Auto-Redirecting Captive Portal (can be enabled/disabled dynamically via Web UI)
        * Optional I2C OLED display with Wrist-Raise Auto-Wake & Sleep Power-Save
    - Flash-safe LittleFS circular CSV session logger (stream-copy rotation)
  =============================================================================
*/

#include <Arduino.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <user_interface.h> // ESP8266 160MHz Overclock API

// Optional OLED Library (included conditionally)
#include <U8g2lib.h>

#include "Config.h"
#include "Biomechanics.h"
#include "WebDashboard.h"

// -----------------------------------------------------------------------------
// Global Instances & Objects
// -----------------------------------------------------------------------------
ESP8266WebServer server(80);
DNSServer dnsServer;
BiomechanicsEngine bioEngine;
SwimmerProfile swimmer;

// Captive Portal Auto-Redirect State (Persisted in LittleFS)
bool captivePortalEnabled = true;

// Optional OLED Display Instance (SH1106 / SSD1306 HW I2C)
U8G2_SH1106_128X64_NONAME_F_HW_I2C oledDisplay(U8G2_R0, U8X8_PIN_NONE);
bool oledDetected = false;
bool oledScreenAwake = true;
uint32_t oledAwakeUntilMs = 0;
uint8_t oledPage = 0;
uint32_t oledPageSwitchMs = 0;

// IMU Raw Reading Registers
static const uint8_t MPU_REG_ACCEL_XOUT_H = 0x3B;
static const uint8_t MPU_REG_PWR_MGMT_1   = 0x6B;
static const uint8_t MPU_REG_GYRO_CONFIG  = 0x1B;
static const uint8_t MPU_REG_ACCEL_CONFIG = 0x1C;
static const uint8_t MPU_REG_CONFIG       = 0x1A;
static const uint8_t MPU_REG_SMPLRT_DIV   = 0x19;

// -----------------------------------------------------------------------------
// Low-Level MPU6050 Communication
// -----------------------------------------------------------------------------
void writeMPUReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU6050_I2C_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

bool readMPURaw(int16_t &ax, int16_t &ay, int16_t &az,
                int16_t &gx, int16_t &gy, int16_t &gz) {
  Wire.beginTransmission(MPU6050_I2C_ADDR);
  Wire.write(MPU_REG_ACCEL_XOUT_H);
  if (Wire.endTransmission(false) != 0) return false;

  size_t count = Wire.requestFrom((uint8_t)MPU6050_I2C_ADDR, (size_t)14, (uint8_t)true);
  if (count < 14) return false;

  ax = (Wire.read() << 8) | Wire.read();
  ay = (Wire.read() << 8) | Wire.read();
  az = (Wire.read() << 8) | Wire.read();
  (void)Wire.read(); (void)Wire.read(); // Skip temperature
  gx = (Wire.read() << 8) | Wire.read();
  gy = (Wire.read() << 8) | Wire.read();
  gz = (Wire.read() << 8) | Wire.read();
  return true;
}

void initMPU6050() {
  writeMPUReg(MPU_REG_PWR_MGMT_1, 0x00); // Wake up device
  delay(30);
  writeMPUReg(MPU_REG_GYRO_CONFIG, 0x08);  // +/- 500 dps
  writeMPUReg(MPU_REG_ACCEL_CONFIG, 0x00); // +/- 2g
  writeMPUReg(MPU_REG_CONFIG, 0x03);       // DLPF ~44Hz
  writeMPUReg(MPU_REG_SMPLRT_DIV, 0x00);   // Sample rate divisor = 0
}

void calibrateStationaryIMU(uint16_t samples = 500) {
  long sax = 0, say = 0, saz = 0;
  long sgx = 0, sgy = 0, sgz = 0;
  uint16_t good = 0;

  for (uint16_t i = 0; i < samples; ++i) {
    int16_t ax, ay, az, gx, gy, gz;
    if (readMPURaw(ax, ay, az, gx, gy, gz)) {
      sax += ax; say += ay; saz += az;
      sgx += gx; sgy += gy; sgz += gz;
      good++;
    }
    delay(3);
    yield();
  }

  if (good == 0) return;

  float axB = (float)sax / (float)good / ACCEL_SCALE_FACTOR;
  float ayB = (float)say / (float)good / ACCEL_SCALE_FACTOR;
  float azB = ((float)saz / (float)good / ACCEL_SCALE_FACTOR) - 1.0f; // Gravity upright assumption

  float gxB = (float)sgx / (float)good / GYRO_SCALE_FACTOR;
  float gyB = (float)sgy / (float)good / GYRO_SCALE_FACTOR;
  float gzB = (float)sgz / (float)good / GYRO_SCALE_FACTOR;

  bioEngine.setBiases(axB, ayB, azB, gxB, gyB, gzB);
}

// -----------------------------------------------------------------------------
// LittleFS Memory-Safe Streamed Circular Logging & Settings
// -----------------------------------------------------------------------------
void ensureLogFileHeader() {
  if (!LittleFS.exists(LOG_FILE_PATH)) {
    File f = LittleFS.open(LOG_FILE_PATH, "w");
    if (f) {
      f.println("ts_ms,state,speed_mps,stroke_rate_spm,strokes,distance_m,laps,swolf,quality,symmetry");
      f.close();
    }
  }
}

void loadPortalSettings() {
  if (LittleFS.exists(PORTAL_CONFIG_PATH)) {
    File f = LittleFS.open(PORTAL_CONFIG_PATH, "r");
    if (f) {
      String val = f.readStringUntil('\n');
      val.trim();
      captivePortalEnabled = (val == "1" || val == "true");
      f.close();
    }
  }
}

void savePortalSettings(bool enable) {
  File f = LittleFS.open(PORTAL_CONFIG_PATH, "w");
  if (f) {
    f.println(enable ? "1" : "0");
    f.close();
  }
}

void rotateLogStreaming(uint16_t keepLines) {
  File in = LittleFS.open(LOG_FILE_PATH, "r");
  if (!in || in.size() < 15000) { // Rotate only when file grows > 15KB
    if (in) in.close();
    return;
  }

  uint16_t total = 0;
  while (in.available()) {
    in.readStringUntil('\n');
    total++;
  }
  in.seek(0, SeekSet);

  File out = LittleFS.open(TEMP_FILE_PATH, "w");
  if (!out) { in.close(); return; }

  uint16_t skip = (total > keepLines) ? (total - keepLines) : 0;
  uint16_t idx = 0;

  while (in.available()) {
    String line = in.readStringUntil('\n');
    if (idx++ >= skip) out.println(line);
  }

  in.close();
  out.close();

  LittleFS.remove(LOG_FILE_PATH);
  LittleFS.rename(TEMP_FILE_PATH, LOG_FILE_PATH);
}

void logSessionSnapshot() {
  File f = LittleFS.open(LOG_FILE_PATH, "a");
  if (!f) return;

  const auto &t = bioEngine.tele;
  f.printf("%lu,%s,%.2f,%.1f,%lu,%.1f,%lu,%.1f,%.2f,%.2f\n",
           millis(),
           activityStateToString(t.state),
           t.speedMps,
           t.strokeRateSpm,
           t.strokeCount,
           t.totalDistanceM,
           t.lapCount,
           t.swolfScore,
           t.qualityScore,
           t.phase.symmetryScore);
  f.close();
  rotateLogStreaming(MAX_LOG_LINES_RETAIN);
}

// -----------------------------------------------------------------------------
// Optional I2C OLED Display Engine
// -----------------------------------------------------------------------------
bool probeI2CDevice(uint8_t addr) {
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

void setOledAwake(bool awake) {
  if (!oledDetected) return;
  if (awake) {
    oledDisplay.setPowerSave(0);
    oledScreenAwake = true;
    oledAwakeUntilMs = millis() + 8000;
  } else {
    oledDisplay.setPowerSave(1);
    oledScreenAwake = false;
  }
}

void drawOledUI(uint32_t nowMs) {
  if (!oledDetected || !oledScreenAwake) return;

  if (nowMs - oledPageSwitchMs > 2500) {
    oledPageSwitchMs = nowMs;
    oledPage = (oledPage + 1) % 3;
  }

  const auto &t = bioEngine.tele;
  oledDisplay.clearBuffer();

  char buf[32];
  if (t.state == STATE_SWIM_EASY || t.state == STATE_SWIM_RACE) {
    if (oledPage == 0) {
      oledDisplay.setFont(u8g2_font_6x12_tr);
      oledDisplay.drawStr(0, 10, "SWIM ACTIVE");
      oledDisplay.setFont(u8g2_font_logisoso18_tf);
      snprintf(buf, sizeof(buf), "%.2f", t.speedMps);
      oledDisplay.drawStr(0, 38, buf);
      oledDisplay.setFont(u8g2_font_6x12_tr);
      oledDisplay.drawStr(74, 38, "m/s");
      snprintf(buf, sizeof(buf), "SPM %.0f  Stk %lu", t.strokeRateSpm, t.strokeCount);
      oledDisplay.drawStr(0, 58, buf);
    } else if (oledPage == 1) {
      oledDisplay.setFont(u8g2_font_6x12_tr);
      oledDisplay.drawStr(0, 10, "DISTANCE & LAPS");
      oledDisplay.setFont(u8g2_font_logisoso20_tf);
      snprintf(buf, sizeof(buf), "%.0f", t.totalDistanceM);
      oledDisplay.drawStr(0, 42, buf);
      oledDisplay.setFont(u8g2_font_6x12_tr);
      oledDisplay.drawStr(72, 42, "m");
      snprintf(buf, sizeof(buf), "Laps %lu  Spl %.1fs", t.lapCount, t.lastSplitMs / 1000.0f);
      oledDisplay.drawStr(0, 58, buf);
    } else {
      oledDisplay.setFont(u8g2_font_6x12_tr);
      oledDisplay.drawStr(0, 10, "SWOLF & QUALITY");
      snprintf(buf, sizeof(buf), "SWOLF: %.1f", t.swolfScore);
      oledDisplay.drawStr(0, 26, buf);
      snprintf(buf, sizeof(buf), "Symm:  %d%%", (int)(t.phase.symmetryScore * 100));
      oledDisplay.drawStr(0, 42, buf);
      snprintf(buf, sizeof(buf), "Length: %.2fm", t.strokeLengthM);
      oledDisplay.drawStr(0, 58, buf);
    }
  } else if (t.state == STATE_SWIM_REST) {
    oledDisplay.setFont(u8g2_font_6x12_tr);
    oledDisplay.drawStr(0, 12, "REST INTERVAL");
    snprintf(buf, sizeof(buf), "Rest: %02lu:%02lu", (t.restMs / 60000), ((t.restMs / 1000) % 60));
    oledDisplay.drawStr(0, 28, buf);
    snprintf(buf, sizeof(buf), "Last Split: %.2fs", t.lastSplitMs / 1000.0f);
    oledDisplay.drawStr(0, 44, buf);
    snprintf(buf, sizeof(buf), "Tot Dist:   %.0fm", t.totalDistanceM);
    oledDisplay.drawStr(0, 58, buf);
  } else {
    oledDisplay.setFont(u8g2_font_6x12_tr);
    oledDisplay.drawStr(0, 12, "IDLE / READY");
    oledDisplay.drawStr(0, 28, "Web: 192.168.1.4");
    snprintf(buf, sizeof(buf), "Strokes: %lu", t.strokeCount);
    oledDisplay.drawStr(0, 44, buf);
    snprintf(buf, sizeof(buf), "Dist:    %.0fm", t.totalDistanceM);
    oledDisplay.drawStr(0, 58, buf);
  }

  oledDisplay.sendBuffer();
}

// -----------------------------------------------------------------------------
// Web Server Request Handlers & Captive Portal Routing
// -----------------------------------------------------------------------------
bool isCaptivePortalRequest() {
  if (!captivePortalEnabled) return false;
  String host = server.hostHeader();
  if (host.indexOf("192.168.1.4") >= 0 || host.indexOf("swimband.local") >= 0) {
    return false;
  }
  return true;
}

void handleCaptivePortalRedirect() {
  if (captivePortalEnabled) {
    server.sendHeader("Location", "http://192.168.1.4/", true);
    server.send(302, "text/plain", "");
  } else {
    server.send(204, "text/plain", "");
  }
}

void handleNotFoundOrCaptive() {
  if (isCaptivePortalRequest()) {
    server.sendHeader("Location", "http://192.168.1.4/", true);
    server.send(302, "text/plain", "");
    return;
  }
  server.send(404, "text/plain", "404 Not Found");
}

void handleRoot() {
  if (isCaptivePortalRequest()) {
    server.sendHeader("Location", "http://192.168.1.4/", true);
    server.send(302, "text/plain", "");
    return;
  }
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleApiState() {
  const auto &t = bioEngine.tele;
  String json;
  json.reserve(1024);

  json += '{';
  json += "\"uptime_s\":" + String(millis() / 1000.0, 1) + ',';
  json += "\"state\":\"" + String(activityStateToString(t.state)) + "\",";
  json += "\"speed_mps\":" + String(t.speedMps, 2) + ',';
  json += "\"peak_speed_mps\":" + String(t.peakSpeedMps, 2) + ',';
  json += "\"stroke_rate_spm\":" + String(t.strokeRateSpm, 1) + ',';
  json += "\"stroke_count\":" + String(t.strokeCount) + ',';
  json += "\"total_distance_m\":" + String(t.totalDistanceM, 1) + ',';
  json += "\"lap_count\":" + String(t.lapCount) + ',';
  json += "\"turn_count\":" + String(t.turnCount) + ',';
  json += "\"last_push_g\":" + String(t.lastPushG, 2) + ',';
  json += "\"last_split_ms\":" + String(t.lastSplitMs) + ',';
  json += "\"best_split_ms\":" + String(t.bestSplitMs) + ',';
  json += "\"swolf_score\":" + String(t.swolfScore, 1) + ',';
  json += "\"quality_score\":" + String(t.qualityScore, 3) + ',';
  json += "\"phase_symmetry\":" + String(t.phase.symmetryScore, 3) + ',';
  json += "\"stroke_length_m\":" + String(t.strokeLengthM, 2) + ',';
  json += "\"active_swim_ms\":" + String(t.activeSwimMs) + ',';
  json += "\"rest_ms\":" + String(t.restMs) + ',';
  json += "\"continuous_swim_ms\":" + String(t.continuousSwimMs) + ',';
  json += "\"swim_conf\":" + String(t.swimConfidence, 3) + ',';
  json += "\"walk_conf\":" + String(t.walkConfidence, 3) + ',';
  json += "\"race_conf\":" + String(t.raceConfidence, 3) + ',';
  json += "\"cycloid_score\":" + String(t.cycloidScore, 3) + ',';
  json += "\"quarter_arc_score\":" + String(t.quarterArcScore, 3) + ',';
  json += "\"ellipse_ratio\":" + String(t.arcEllipseRatio, 3) + ',';
  json += "\"acc_var\":" + String(t.accVar, 4) + ',';
  json += "\"captive_portal\":" + String(captivePortalEnabled ? "true" : "false") + ',';
  json += "\"free_heap\":" + String(ESP.getFreeHeap());
  json += '}';

  server.send(200, "application/json", json);
}

void handleApiReset() {
  bioEngine.init(swimmer);
  server.send(200, "application/json", "{\"ok\":true,\"msg\":\"session_reset\"}");
}

void handleApiCalStart() {
  float dist = 50.0f;
  if (server.hasArg("distance")) {
    dist = server.arg("distance").toFloat();
  }
  bioEngine.startCalibration(dist);
  server.send(200, "application/json", "{\"ok\":true,\"msg\":\"calibration_started\"}");
}

void handleApiCalStop() {
  bioEngine.finalizeCalibration();
  server.send(200, "application/json", "{\"ok\":true,\"factor\":" + String(bioEngine.tele.calibrationFactor, 3) + "}");
}

void handleApiPortalToggle() {
  if (server.hasArg("enabled")) {
    String val = server.arg("enabled");
    captivePortalEnabled = (val == "true" || val == "1");
    savePortalSettings(captivePortalEnabled);

    if (captivePortalEnabled) {
      dnsServer.stop();
      dnsServer.start(DNS_PORT, "*", AP_LOCAL_IP);
      Serial.println(F("[DNS] Captive portal DNS server started."));
    } else {
      dnsServer.stop();
      Serial.println(F("[DNS] Captive portal DNS server stopped."));
    }
  }
  server.send(200, "application/json", "{\"ok\":true,\"captive_portal\":" + String(captivePortalEnabled ? "true" : "false") + "}");
}

void handleApiLogDownload() {
  File f = LittleFS.open(LOG_FILE_PATH, "r");
  if (!f) {
    server.send(404, "text/plain", "No session log found.");
    return;
  }
  server.streamFile(f, "text/csv");
  f.close();
}

void handleApiLogClear() {
  LittleFS.remove(LOG_FILE_PATH);
  ensureLogFileHeader();
  server.send(200, "application/json", "{\"ok\":true,\"msg\":\"log_cleared\"}");
}

// -----------------------------------------------------------------------------
// Arduino Setup
// -----------------------------------------------------------------------------
void setup() {
  // Overclock ESP8266 to 160MHz for zero-latency math and web serving
  system_update_cpu_freq(160);

  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println(F("========================================"));
  Serial.println(F("  SwimBand Pro Master - ESP8266"));
  Serial.println(F("========================================"));

  // Initialize LittleFS & Load Settings
  if (LittleFS.begin()) {
    Serial.println(F("[FS] LittleFS mounted successfully."));
    ensureLogFileHeader();
    loadPortalSettings();
  } else {
    Serial.println(F("[FS] Warning: LittleFS mount failed!"));
  }

  // Initialize Shared I2C Bus (D2=SDA, D1=SCL)
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(I2C_BUS_SPEED_HZ);

  // Probe and initialize OLED if present
  oledDetected = probeI2CDevice(OLED_I2C_ADDR);
  if (oledDetected) {
    Serial.println(F("[OLED] SH1106/SSD1306 Display detected. Initializing..."));
    oledDisplay.begin();
    oledDisplay.clearBuffer();
    oledDisplay.setFont(u8g2_font_6x12_tr);
    oledDisplay.drawStr(0, 20, "SwimBand Pro");
    oledDisplay.drawStr(0, 36, "Calibrating IMU...");
    oledDisplay.sendBuffer();
  } else {
    Serial.println(F("[OLED] No display found. Running in Web Dashboard mode."));
  }

  // Initialize MPU6050
  initMPU6050();
  Serial.println(F("[IMU] Calibrating gyro & rest noise... keep still."));
  calibrateStationaryIMU(600);
  Serial.println(F("[IMU] Calibration complete."));

  // Initialize Biomechanics Engine
  bioEngine.init(swimmer);

  // Start Wi-Fi SoftAP
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_LOCAL_IP, AP_GATEWAY, AP_SUBNET);
  WiFi.softAP(DEFAULT_AP_SSID, DEFAULT_AP_PASS);
  Serial.print(F("[WiFi] SoftAP Started: SSID="));
  Serial.print(DEFAULT_AP_SSID);
  Serial.print(F(" | IP="));
  Serial.println(WiFi.softAPIP());

  // Start Captive Portal DNS Server if enabled
  if (captivePortalEnabled) {
    dnsServer.start(DNS_PORT, "*", AP_LOCAL_IP);
    Serial.println(F("[DNS] Captive portal DNS active (auto-redirect enabled)."));
  }

  // Setup mDNS responder (http://swimband.local)
  if (MDNS.begin("swimband")) {
    Serial.println(F("[mDNS] Responder started at http://swimband.local"));
    MDNS.addService("http", "tcp", 80);
  }

  // Setup Web Server Endpoints & Captive Portal Probes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/hotspot-detect.html", handleCaptivePortalRedirect);
  server.on("/generate_204", handleCaptivePortalRedirect);
  server.on("/gen_204", handleCaptivePortalRedirect);
  server.on("/ncsi.txt", handleCaptivePortalRedirect);
  server.on("/connecttest.txt", handleCaptivePortalRedirect);

  server.on("/api/state", HTTP_GET, handleApiState);
  server.on("/api/reset", HTTP_POST, handleApiReset);
  server.on("/api/cal/start", HTTP_POST, handleApiCalStart);
  server.on("/api/cal/stop", HTTP_POST, handleApiCalStop);
  server.on("/api/settings/captive_portal", HTTP_POST, handleApiPortalToggle);
  server.on("/api/log/download", HTTP_GET, handleApiLogDownload);
  server.on("/api/log/clear", HTTP_POST, handleApiLogClear);

  server.onNotFound(handleNotFoundOrCaptive);
  server.begin();
  Serial.println(F("[HTTP] Web server started on port 80."));

  if (oledDetected) {
    oledDisplay.clearBuffer();
    oledDisplay.drawStr(0, 20, "SwimBand Ready");
    oledDisplay.drawStr(0, 36, "AP: SwimBand-Pro");
    oledDisplay.drawStr(0, 52, "IP: 192.168.1.4");
    oledDisplay.sendBuffer();
    delay(600);
    setOledAwake(true);
  }
}

// -----------------------------------------------------------------------------
// Arduino Main Loop (50 Hz Sample Cadence + Web, DNS & UI)
// -----------------------------------------------------------------------------
void loop() {
  uint32_t nowMs = millis();
  static uint32_t lastSampleMs = 0;
  static uint32_t lastUiMs = 0;
  static uint32_t lastLogMs = 0;

  // 1. 50Hz IMU Acquisition & Biomechanical Processing
  if (nowMs - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = nowMs;
    int16_t ax, ay, az, gx, gy, gz;
    if (readMPURaw(ax, ay, az, gx, gy, gz)) {
      bioEngine.processSample(ax, ay, az, gx, gy, gz, nowMs);
    }
  }

  // 2. Captive Portal DNS Processing
  if (captivePortalEnabled) {
    dnsServer.processNextRequest();
  }

  // 3. Web Server & mDNS Handlers
  server.handleClient();
  MDNS.update();

  // 4. UI / OLED Refresh Loop
  if (nowMs - lastUiMs >= UI_REFRESH_MS) {
    lastUiMs = nowMs;
    // Wrist-raise auto-wake detection
    if (fabsf(bioEngine.tele.euler.x) > 35.0f && bioEngine.tele.euler.y > -20.0f && bioEngine.tele.euler.y < 80.0f) {
      setOledAwake(true);
    }
    // Auto-sleep if idle
    if (oledScreenAwake && nowMs > oledAwakeUntilMs && (bioEngine.tele.state == STATE_IDLE || bioEngine.tele.state == STATE_SWIM_REST)) {
      setOledAwake(false);
    }
    drawOledUI(nowMs);
  }

  // 5. Session Periodic Log (every 2.5 seconds during active swimming)
  if ((bioEngine.tele.state == STATE_SWIM_EASY || bioEngine.tele.state == STATE_SWIM_RACE) && (nowMs - lastLogMs >= 2500)) {
    lastLogMs = nowMs;
    logSessionSnapshot();
  }

  yield();
}
