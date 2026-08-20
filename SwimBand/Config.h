#ifndef SWIMBAND_CONFIG_H
#define SWIMBAND_CONFIG_H

#include <Arduino.h>

// =============================================================================
//  SwimBand Pro - Hardware & Firmware Configuration
// =============================================================================

// --- I2C & Hardware Pins (NodeMCU / ESP8266) ---
#ifndef D1
#define D1 5   // GPIO5 = NodeMCU D1 (SCL)
#endif

#ifndef D2
#define D2 4   // GPIO4 = NodeMCU D2 (SDA)
#endif

static const uint8_t PIN_I2C_SDA = D2;
static const uint8_t PIN_I2C_SCL = D1;
static const uint32_t I2C_BUS_SPEED_HZ = 100000; // 100kHz for stable shared bus (MPU6050 + OLED)
static const uint8_t MPU6050_I2C_ADDR = 0x68;
static const uint8_t OLED_I2C_ADDR = 0x3C;       // Standard SH1106 / SSD1306 address

// --- Sampling & Loop Timing ---
static const float SAMPLE_HZ = 50.0f;             // 50Hz IMU sampling rate
static const uint32_t SAMPLE_INTERVAL_MS = 20;   // 20ms period (1000 / 50)
static const float DT = 1.0f / SAMPLE_HZ;
static const uint32_t UI_REFRESH_MS = 100;       // 10 FPS OLED / UI refresh rate
static const uint32_t TELEMETRY_PUSH_MS = 500;   // Web API poll / push cadence

// --- Wi-Fi SoftAP Configuration (Standalone Zero-App Mode) ---
static const char *DEFAULT_AP_SSID = "SwimBand-Pro";
static const char *DEFAULT_AP_PASS = "swim1234";
static const IPAddress AP_LOCAL_IP(192, 168, 1, 4);
static const IPAddress AP_GATEWAY(192, 168, 1, 4);
static const IPAddress AP_SUBNET(255, 255, 255, 0);

// --- Captive Portal Configuration ---
static const uint16_t DNS_PORT = 53;
static const char *PORTAL_CONFIG_PATH = "/portal.cfg";

// --- Optional Station (STA) Sync Mode (For telemetry ingestion servers) ---
static const char *DEFAULT_STA_SSID = "vivo";
static const char *DEFAULT_STA_PASS = "12345678";
static const char *DEFAULT_SERVER_HOST = "192.168.43.1";
static const uint16_t SERVER_HTTP_PORT = 8080;
static const uint16_t SERVER_UDP_PORT = 9090;
static const uint16_t LOCAL_UDP_PORT = 9091;

// --- File Storage & Logging (LittleFS) ---
static const char *LOG_FILE_PATH = "/swim_log.csv";
static const char *TEMP_FILE_PATH = "/temp.csv";
static const char *CONFIG_FILE_PATH = "/config.json";
static const uint16_t MAX_LOG_LINES_RETAIN = 800; // Low-RAM circular stream rotation

// --- Default Swimmer Anthropometric Profile ---
struct SwimmerProfile {
  float armLengthM = 0.7874f;       // ~31.0 inches (arm span segment)
  float shoulderToBandM = 0.5842f;  // ~23.0 inches (wrist to shoulder lever)
  float palmAreaM2 = 0.0085f;       // ~85 cm^2 effective blade surface
  float bodyMassKg = 75.0f;         // Swimmer mass in kg
  float poolLengthM = 25.0f;        // Standard 25m short-course pool (or 50m Olympic)
  
  // Fluid Dynamics Constants
  float waterDensity = 997.0f;      // kg/m^3 (fresh water at 25°C)
  float glideDragCoeff = 0.60f;     // Streamlined Cd
  float frontalAreaM2 = 0.090f;     // Frontal area in glide streamline
};

// --- IMU Full-Scale Scales ---
// Accel: +/- 2g => 16384 LSB/g
// Gyro:  +/- 500 dps => 65.5 LSB/(deg/s)
static const float ACCEL_SCALE_FACTOR = 16384.0f;
static const float GYRO_SCALE_FACTOR  = 65.5f;

#endif // SWIMBAND_CONFIG_H
