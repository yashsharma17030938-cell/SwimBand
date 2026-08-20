#ifndef SWIMBAND_BIOMECHANICS_H
#define SWIMBAND_BIOMECHANICS_H

#include <Arduino.h>
#include <math.h>
#include "Config.h"

// =============================================================================
//  Fast Math & Sensor Fusion
// =============================================================================

// Quake III Fast Inverse Square Root (Optimal for 32-bit Tensilica L106 Core)
static inline float invSqrt(float x) {
  float halfx = 0.5f * x;
  float y = x;
  long i = *(long*)&y;
  i = 0x5f3759df - (i >> 1);
  y = *(float*)&i;
  y = y * (1.5f - (halfx * y * y));
  return y;
}

static inline float clampf(float v, float lo, float hi) {
  return (v < lo) ? lo : ((v > hi) ? hi : v);
}

static inline float smooth01(float x) {
  x = clampf(x, 0.0f, 1.0f);
  return x * x * (3.0f - 2.0f * x);
}

struct Vector3 {
  float x, y, z;
  float mag() const {
    float s = x * x + y * y + z * z + 0.00001f;
    return 1.0f / invSqrt(s);
  }
};

struct Quaternion {
  float q1 = 1.0f, q2 = 0.0f, q3 = 0.0f, q4 = 0.0f;
};

// =============================================================================
//  Data Structures & Filtering Utilities
// =============================================================================

struct Ema {
  float y = 0.0f;
  float alpha = 0.15f;
  bool initialized = false;

  float update(float x) {
    if (!initialized) {
      y = x;
      initialized = true;
    } else {
      y = alpha * x + (1.0f - alpha) * y;
    }
    return y;
  }

  void reset(float v = 0.0f) {
    y = v;
    initialized = false;
  }
};

template <size_t N>
struct RingFloat {
  float data[N];
  size_t head = 0;
  size_t count = 0;

  void clear() { head = 0; count = 0; }

  void push(float x) {
    data[head] = x;
    head = (head + 1) % N;
    if (count < N) count++;
  }

  float latest() const {
    if (count == 0) return 0.0f;
    size_t idx = (head + N - 1) % N;
    return data[idx];
  }

  float mean() const {
    if (count == 0) return 0.0f;
    float s = 0.0f;
    for (size_t i = 0; i < count; ++i) s += data[i];
    return s / (float)count;
  }

  float variance() const {
    if (count < 2) return 0.0f;
    float m = mean();
    float s2 = 0.0f;
    for (size_t i = 0; i < count; ++i) {
      float d = data[i] - m;
      s2 += d * d;
    }
    return s2 / (float)(count - 1);
  }

  float minv() const {
    if (count == 0) return 0.0f;
    float mn = data[0];
    for (size_t i = 1; i < count; ++i) if (data[i] < mn) mn = data[i];
    return mn;
  }

  float maxv() const {
    if (count == 0) return 0.0f;
    float mx = data[0];
    for (size_t i = 1; i < count; ++i) if (data[i] > mx) mx = data[i];
    return mx;
  }

  float span() const {
    if (count < 2) return 0.0f;
    return maxv() - minv();
  }
};

template <size_t N>
struct RingU32 {
  uint32_t data[N];
  size_t head = 0;
  size_t count = 0;

  void clear() { head = 0; count = 0; }

  void push(uint32_t x) {
    data[head] = x;
    head = (head + 1) % N;
    if (count < N) count++;
  }

  uint16_t countSince(uint32_t nowMs, uint32_t windowMs) const {
    uint16_t n = 0;
    for (size_t i = 0; i < count; ++i) {
      if (nowMs >= data[i] && (nowMs - data[i]) <= windowMs) n++;
    }
    return n;
  }
};

// =============================================================================
//  Activity & Swim States
// =============================================================================

enum ActivityState : uint8_t {
  STATE_IDLE = 0,
  STATE_WALK = 1,
  STATE_SWIM_EASY = 2,
  STATE_SWIM_RACE = 3,
  STATE_SWIM_REST = 4
};

static inline const char *activityStateToString(ActivityState s) {
  switch (s) {
    case STATE_IDLE: return "IDLE";
    case STATE_WALK: return "WALK";
    case STATE_SWIM_EASY: return "SWIM_EASY";
    case STATE_SWIM_RACE: return "SWIM_RACE";
    case STATE_SWIM_REST: return "SWIM_REST";
    default: return "UNKNOWN";
  }
}

// 3-Phase Stroke Kinematics
struct StrokePhases {
  bool inCatch = false;
  bool inPull = false;
  bool inRecovery = false;
  float catchScore = 0.0f;
  float pullScore = 0.0f;
  float recoveryScore = 0.0f;
  float symmetryScore = 1.0f;
};

// Lap and Segment Summary
struct LapSplit {
  uint32_t lapIndex = 0;
  uint32_t durationMs = 0;
  float distanceM = 0.0f;
  float avgSpeedMps = 0.0f;
  float peakSpeedMps = 0.0f;
  uint32_t strokes = 0;
  float pushG = 0.0f;
};

// =============================================================================
//  Comprehensive Telemetry & Biomechanics State
// =============================================================================

struct SwimTelemetry {
  // Raw Sensor Data
  Vector3 rawAcc = {0, 0, 0};       // g
  Vector3 rawGyro = {0, 0, 0};      // deg/s
  Vector3 linAcc = {0, 0, 0};       // m/s^2 (gravity isolated)
  Vector3 euler = {0, 0, 0};        // Roll, Pitch, Yaw in degrees
  float accMag = 1.0f;              // g
  float gyrMag = 0.0f;              // deg/s
  float jerk = 0.0f;                // g/s
  float hpAcc = 0.0f;

  // Signal Features
  float accVar = 0.0f;
  float gyroVar = 0.0f;
  float pitchSpan = 0.0f;
  float rollSpan = 0.0f;
  float arcEllipseRatio = 0.0f;
  float cycloidScore = 0.0f;
  float quarterArcScore = 0.0f;

  // Classification & Confidences
  ActivityState state = STATE_IDLE;
  ActivityState prevState = STATE_IDLE;
  float swimConfidence = 0.0f;
  float walkConfidence = 0.0f;
  float raceConfidence = 0.0f;
  float qualityScore = 0.0f;
  bool swimSessionArmed = false;

  // Stroke & Kinematic Metrics
  uint32_t strokeCount = 0;
  float strokeRateSpm = 0.0f;
  float strokeFreqHz = 0.0f;
  float stepFreqHz = 0.0f;
  float strokeLengthM = 1.25f;
  float speedMps = 0.0f;
  float peakSpeedMps = 0.0f;
  float totalDistanceM = 0.0f;
  float swolfScore = 0.0f;
  float dragFactor = 0.0f;
  float propulsionIndex = 0.0f;
  float fatigueIndex = 0.0f;

  // Hydrodynamic Velocity Integration
  float vX_local = 0.0f;
  float intraStrokeMaxSpd = 0.0f;
  float intraStrokeMinSpd = 99.0f;
  float dragEfficiencyScore = 0.0f;

  // Turns & Splits
  uint32_t lapCount = 0;
  uint32_t turnCount = 0;
  uint32_t lastTurnMs = 0;
  uint32_t lastSplitMs = 0;
  uint32_t bestSplitMs = 0;
  float lastPushG = 0.0f;
  uint32_t lastPushMs = 0;

  // Session Timings
  uint32_t sessionStartMs = 0;
  uint32_t activeSwimMs = 0;
  uint32_t restMs = 0;
  uint32_t continuousSwimMs = 0;
  uint32_t longestContinuousSwimMs = 0;
  uint32_t lastStrokeMs = 0;
  uint32_t lastSwimMotionMs = 0;
  uint32_t lastStateChangeMs = 0;

  // Calibration Engine
  bool calibrationActive = false;
  float calTargetDistanceM = 50.0f;
  uint32_t calStartMs = 0;
  uint32_t calStartStrokeCount = 0;
  float calStartDistanceM = 0.0f;
  float calibrationFactor = 1.0f;
  float strokeLenBaseM = 1.25f;
  float strokeLenSpmCoeff = 0.008f;
  float strokeLenVarCoeff = 0.15f;

  // Phase Decomposition
  StrokePhases phase;
  LapSplit lastSplit;
};

// =============================================================================
//  Biomechanics & Sensor Fusion Engine
// =============================================================================

class BiomechanicsEngine {
private:
  Quaternion q;
  float eInt[3] = {0.0f, 0.0f, 0.0f};
  const float Kp = 2.0f;
  const float Ki = 0.005f;

  // Feature Extraction Buffers
  RingFloat<200> bufAccHp;
  RingFloat<200> bufGyro;
  RingFloat<80> bufPitch;
  RingFloat<80> bufRoll;
  RingFloat<80> bufStrokePeriod;
  RingFloat<80> bufStepPeriod;
  RingU32<80> bufStrokeTime;
  RingU32<80> bufStepTime;

  // Exponential Filters
  Ema emaAcc{.alpha = 0.20f};
  Ema emaGyro{.alpha = 0.20f};
  Ema emaSpeed{.alpha = 0.16f};
  Ema emaSwimConf{.alpha = 0.12f};
  Ema emaWalkConf{.alpha = 0.12f};
  Ema emaRaceConf{.alpha = 0.12f};
  Ema emaQuality{.alpha = 0.10f};
  Ema emaDrag{.alpha = 0.08f};
  Ema emaProp{.alpha = 0.10f};

  // Calibration offsets
  float accBiasX = 0.0f, accBiasY = 0.0f, accBiasZ = 0.0f;
  float gyroBiasX = 0.0f, gyroBiasY = 0.0f, gyroBiasZ = 0.0f;

  // Biomechanical Profile
  SwimmerProfile profile;

public:
  SwimTelemetry tele;

  void init(const SwimmerProfile &prof) {
    profile = prof;
    tele = SwimTelemetry();
    tele.sessionStartMs = millis();
    tele.lastStateChangeMs = tele.sessionStartMs;
    tele.lastSwimMotionMs = tele.sessionStartMs;
    tele.lastStrokeMs = tele.sessionStartMs;
    q = Quaternion();
  }

  void setBiases(float axB, float ayB, float azB, float gxB, float gyB, float gzB) {
    accBiasX = axB; accBiasY = ayB; accBiasZ = azB;
    gyroBiasX = gxB; gyroBiasY = gyB; gyroBiasZ = gzB;
  }

  // --- Mahony AHRS Algorithm ---
  void updateAHRS(float gx, float gy, float gz, float ax, float ay, float az, float dt) {
    float norm = ax * ax + ay * ay + az * az;
    if (norm == 0.0f) return;
    norm = invSqrt(norm);
    ax *= norm; ay *= norm; az *= norm;

    // Estimated direction of gravity
    float vx = 2.0f * (q.q2 * q.q4 - q.q1 * q.q3);
    float vy = 2.0f * (q.q1 * q.q2 + q.q3 * q.q4);
    float vz = q.q1 * q.q1 - q.q2 * q.q2 - q.q3 * q.q3 + q.q4 * q.q4;

    // Error is cross product between estimated and measured gravity
    float ex = (ay * vz - az * vy);
    float ey = (az * vx - ax * vz);
    float ez = (ax * vy - ay * vx);

    if (Ki > 0.0f) {
      eInt[0] += ex * dt;
      eInt[1] += ey * dt;
      eInt[2] += ez * dt;
    }

    gx += Kp * ex + Ki * eInt[0];
    gy += Kp * ey + Ki * eInt[1];
    gz += Kp * ez + Ki * eInt[2];

    // Integrate quaternion rate
    float pa = q.q2, pb = q.q3, pc = q.q4;
    q.q1 += (-pa * gx - pb * gy - pc * gz) * (0.5f * dt);
    q.q2 += ( q.q1 * gx + pb * gz - pc * gy) * (0.5f * dt);
    q.q3 += ( q.q1 * gy - pa * gz + pc * gx) * (0.5f * dt);
    q.q4 += ( q.q1 * gz + pa * gy - pb * gx) * (0.5f * dt);

    norm = invSqrt(q.q1 * q.q1 + q.q2 * q.q2 + q.q3 * q.q3 + q.q4 * q.q4);
    q.q1 *= norm; q.q2 *= norm; q.q3 *= norm; q.q4 *= norm;

    // Euler angles (Roll, Pitch, Yaw) in degrees
    tele.euler.x = atan2f(q.q1 * q.q2 + q.q3 * q.q4, 0.5f - q.q2 * q.q2 - q.q3 * q.q3) * 57.2957795f;
    tele.euler.y = asinf(clampf(-2.0f * (q.q1 * q.q3 - q.q2 * q.q4), -1.0f, 1.0f)) * 57.2957795f;
    tele.euler.z = atan2f(q.q1 * q.q4 + q.q2 * q.q3, 0.5f - q.q3 * q.q3 - q.q4 * q.q4) * 57.2957795f;

    // Gravity isolation: LinAcc = RawAcc - Gravity
    tele.linAcc.x = (tele.rawAcc.x - vx) * 9.80665f;
    tele.linAcc.y = (tele.rawAcc.y - vy) * 9.80665f;
    tele.linAcc.z = (tele.rawAcc.z - vz) * 9.80665f;
  }

  // --- Process Raw IMU Sample ---
  void processSample(int16_t rax, int16_t ray, int16_t raz,
                     int16_t rgx, int16_t rgy, int16_t rgz,
                     uint32_t nowMs) {
    // 1. Scale Conversion & Bias Removal
    float ax = (float)rax / ACCEL_SCALE_FACTOR - accBiasX;
    float ay = (float)ray / ACCEL_SCALE_FACTOR - accBiasY;
    float az = (float)raz / ACCEL_SCALE_FACTOR - accBiasZ;

    float gx = (float)rgx / GYRO_SCALE_FACTOR - gyroBiasX;
    float gy = (float)rgy / GYRO_SCALE_FACTOR - gyroBiasY;
    float gz = (float)rgz / GYRO_SCALE_FACTOR - gyroBiasZ;

    tele.rawAcc = {ax, ay, az};
    tele.rawGyro = {gx, gy, gz};
    tele.accMag = sqrtf(ax * ax + ay * ay + az * az);
    tele.gyrMag = sqrtf(gx * gx + gy * gy + gz * gz);

    static float prevAccMag = 1.0f;
    tele.jerk = fabsf(tele.accMag - prevAccMag) / DT;
    prevAccMag = tele.accMag;

    // 2. High-Pass Filter for dynamic stroke impulses
    const float hpAlpha = 0.94f;
    static float prevHp = 0.0f, prev2Hp = 0.0f;
    tele.hpAcc = hpAlpha * (tele.hpAcc + tele.accMag - prevAccMag);

    // 3. Mahony AHRS Update (convert deg/s to rad/s for integration)
    updateAHRS(gx * 0.0174532925f, gy * 0.0174532925f, gz * 0.0174532925f, ax, ay, az, DT);

    // 4. Feature Windows
    bufAccHp.push(tele.hpAcc);
    bufGyro.push(tele.gyrMag);
    bufPitch.push(tele.euler.y);
    bufRoll.push(tele.euler.x);

    tele.accVar = bufAccHp.variance();
    tele.gyroVar = bufGyro.variance();
    tele.pitchSpan = bufPitch.span();
    tele.rollSpan = bufRoll.span();

    // 5. Arc-Shape & Cycloid Motion Classification
    float smallSpan = min(tele.pitchSpan, tele.rollSpan);
    float largeSpan = max(tele.pitchSpan, tele.rollSpan) + 0.001f;
    tele.arcEllipseRatio = smallSpan / largeSpan;
    float spanScore = smooth01((largeSpan - 18.0f) / 65.0f);
    float ratioCenter = 1.0f - fabsf(tele.arcEllipseRatio - 0.62f) / 0.62f;
    tele.cycloidScore = 0.62f * spanScore + 0.38f * smooth01(ratioCenter);
    tele.quarterArcScore = smooth01((1.0f - tele.arcEllipseRatio - 0.18f) / 0.70f) * smooth01((largeSpan - 8.0f) / 40.0f);

    // 6. 3-Phase Stroke Decomposition
    float rollNorm = clampf((fabsf(tele.euler.x) - 8.0f) / 70.0f, 0.0f, 1.0f);
    float pitchNorm = clampf((fabsf(tele.euler.y) - 8.0f) / 65.0f, 0.0f, 1.0f);
    float gxNorm = clampf((fabsf(tele.rawGyro.x) - 25.0f) / 160.0f, 0.0f, 1.0f);
    float axNorm = clampf((fabsf(tele.rawAcc.x) - 0.08f) / 1.5f, 0.0f, 1.0f);

    tele.phase.catchScore = 0.45f * pitchNorm + 0.35f * axNorm + 0.20f * (1.0f - gxNorm);
    tele.phase.pullScore = 0.50f * gxNorm + 0.30f * rollNorm + 0.20f * axNorm;
    tele.phase.recoveryScore = 0.55f * rollNorm + 0.45f * (1.0f - axNorm);
    tele.phase.inCatch = tele.phase.catchScore > 0.55f;
    tele.phase.inPull = tele.phase.pullScore > 0.52f;
    tele.phase.inRecovery = tele.phase.recoveryScore > 0.55f;
    float sym = 1.0f - fabsf((tele.phase.catchScore + tele.phase.recoveryScore) - 2.0f * tele.phase.pullScore);
    tele.phase.symmetryScore = clampf(sym, 0.0f, 1.0f);

    // 7. Peak Detection & Cadence Calculation
    bool isPeak = (prevHp > prev2Hp) && (prevHp > tele.hpAcc);
    float dynThresh = 0.15f + 0.75f * sqrtf(clampf(tele.accVar, 0.0f, 4.0f));
    bool strongPeak = isPeak && (prevHp > dynThresh);

    if (strongPeak) {
      uint32_t dtStroke = nowMs - tele.lastStrokeMs;
      uint32_t dtStep = nowMs - bufStepTime.latest();

      bool horizArm = fabsf(tele.euler.x) > 15.0f || fabsf(tele.euler.y) > 15.0f;
      bool cycloidGate = (tele.cycloidScore > 0.32f) && (tele.arcEllipseRatio > 0.20f);
      bool strokeGate = horizArm && cycloidGate && tele.gyrMag > 40.0f && dtStroke > 280 && dtStroke < 2500;
      bool stepGate = !horizArm && tele.gyrMag > 16.0f && dtStep > 240 && dtStep < 1300;

      if (strokeGate) {
        bufStrokePeriod.push((float)dtStroke / 1000.0f);
        bufStrokeTime.push(nowMs);
        tele.lastStrokeMs = nowMs;
        tele.lastSwimMotionMs = nowMs;
        tele.strokeCount++;

        // Drag Efficiency drop inside stroke
        tele.dragEfficiencyScore = tele.intraStrokeMaxSpd - tele.intraStrokeMinSpd;
        tele.intraStrokeMaxSpd = 0.0f;
        tele.intraStrokeMinSpd = 99.0f;
        tele.vX_local = 0.5f; // ZUPT boundary reset
      } else if (stepGate) {
        bufStepPeriod.push((float)dtStep / 1000.0f);
        bufStepTime.push(nowMs);
      }
    }

    prev2Hp = prevHp;
    prevHp = tele.hpAcc;

    float strokeP = bufStrokePeriod.mean();
    float stepP = bufStepPeriod.mean();
    tele.strokeFreqHz = (strokeP > 0.001f) ? (1.0f / strokeP) : 0.0f;
    tele.stepFreqHz = (stepP > 0.001f) ? (1.0f / stepP) : 0.0f;
    tele.strokeRateSpm = tele.strokeFreqHz * 60.0f;

    // 8. State Classifier & Confidence Model
    uint16_t strokes10s = bufStrokeTime.countSince(nowMs, 10000);
    float varScore = smooth01((tele.accVar - 0.01f) / 0.40f);
    float strokeScore = smooth01((tele.strokeFreqHz - 0.20f) / 1.0f);
    float stepScore = smooth01((tele.stepFreqHz - 0.60f) / 1.8f);
    float gyroScore = smooth01((tele.gyrMag - 20.0f) / 120.0f);
    float armHorizScore = smooth01((fabsf(tele.euler.x) - 15.0f) / 45.0f);

    float swimC = 0.28f * varScore + 0.26f * strokeScore + 0.16f * gyroScore + 0.14f * armHorizScore + 0.16f * tele.cycloidScore;
    float walkC = 0.42f * stepScore + 0.22f * varScore + 0.16f * (1.0f - armHorizScore) + 0.20f * tele.quarterArcScore;
    float raceC = swimC * smooth01((tele.strokeFreqHz - 0.90f) / 0.75f) * smooth01((tele.accVar - 0.11f) / 0.35f);

    tele.swimConfidence = emaSwimConf.update(swimC);
    tele.walkConfidence = emaWalkConf.update(walkC);
    tele.raceConfidence = emaRaceConf.update(raceC);

    bool recentlyStroke = (nowMs - tele.lastStrokeMs) < 4500;
    bool veryLowMotion = tele.accVar < 0.006f && fabsf(tele.hpAcc) < 0.07f;
    bool strongWalk = (tele.walkConfidence > 0.46f && tele.quarterArcScore > 0.40f && tele.stepFreqHz > 0.75f);
    bool clearSwim = (tele.swimConfidence > 0.52f && tele.cycloidScore > 0.38f && strokes10s >= 2);

    if (!tele.swimSessionArmed && clearSwim) tele.swimSessionArmed = true;
    if (tele.swimSessionArmed && (nowMs - tele.lastStrokeMs) > 15000 && strokes10s < 1) tele.swimSessionArmed = false;

    ActivityState newState = tele.state;
    if (strongWalk && !recentlyStroke) {
      newState = STATE_WALK;
    } else if (tele.swimSessionArmed && tele.swimConfidence > 0.48f && recentlyStroke) {
      newState = (tele.raceConfidence > 0.58f) ? STATE_SWIM_RACE : STATE_SWIM_EASY;
    } else if (tele.swimSessionArmed && tele.swimConfidence > 0.30f && !recentlyStroke && !veryLowMotion) {
      newState = STATE_SWIM_REST;
    } else if (tele.walkConfidence > 0.38f && tele.swimConfidence < 0.45f) {
      newState = STATE_WALK;
    } else {
      newState = veryLowMotion ? STATE_IDLE : tele.state;
    }

    // Hysteresis debouncing
    if (newState != tele.state) {
      if ((nowMs - tele.lastStateChangeMs) > 1500 || (newState == STATE_IDLE && veryLowMotion)) {
        tele.prevState = tele.state;
        tele.state = newState;
        tele.lastStateChangeMs = nowMs;
      }
    }

    // 9. Turn & Wall Push-Off Detection
    bool turnCooldown = (nowMs - tele.lastTurnMs) < 4000;
    bool turnGate = tele.swimSessionArmed && !turnCooldown && (tele.accMag > 2.20f) && (nowMs - tele.lastStrokeMs < 2500);
    if (turnGate) {
      tele.lastPushG = tele.accMag;
      tele.lastPushMs = nowMs;
      tele.lastTurnMs = nowMs;
      tele.turnCount++;
      tele.lapCount++;

      if (tele.lastSplit.durationMs > 0 || tele.lapCount > 1) {
        tele.lastSplitMs = nowMs - (tele.lastSplit.durationMs ? tele.lastSplit.durationMs : tele.sessionStartMs);
        if (tele.bestSplitMs == 0 || tele.lastSplitMs < tele.bestSplitMs) tele.bestSplitMs = tele.lastSplitMs;
      }
      tele.lastSplit.lapIndex = tele.lapCount;
      tele.lastSplit.durationMs = tele.lastSplitMs;
      tele.lastSplit.distanceM = profile.poolLengthM;
      tele.lastSplit.pushG = tele.lastPushG;
    }

    // 10. Hydrodynamic Speed & Distance Integration
    float dragFactor = 0.5f * profile.waterDensity * profile.glideDragCoeff * profile.frontalAreaM2;
    tele.vX_local += tele.linAcc.x * DT;
    float dragDecel = (dragFactor * tele.vX_local * fabsf(tele.vX_local)) / profile.bodyMassKg;
    tele.vX_local -= dragDecel * DT;
    if (tele.vX_local > tele.intraStrokeMaxSpd) tele.intraStrokeMaxSpd = tele.vX_local;
    if (tele.vX_local < tele.intraStrokeMinSpd && tele.vX_local > 0.1f) tele.intraStrokeMinSpd = tele.vX_local;

    // Stroke-length distance model
    tele.strokeLengthM = tele.strokeLenBaseM
                       + tele.strokeLenSpmCoeff * clampf(tele.strokeRateSpm, 0.0f, 70.0f)
                       + tele.strokeLenVarCoeff * clampf((tele.accVar - 0.03f) * 8.0f, 0.0f, 1.5f);

    float rawSpeed = (tele.strokeFreqHz > 0.05f) ? (tele.strokeLengthM * tele.strokeFreqHz) : 0.0f;
    uint32_t msSinceStroke = nowMs - tele.lastStrokeMs;
    if (msSinceStroke > 1800) rawSpeed *= 0.35f;
    if (msSinceStroke > 2800) rawSpeed = 0.0f;
    if (tele.state == STATE_SWIM_REST || tele.state == STATE_IDLE || tele.state == STATE_WALK) rawSpeed = 0.0f;

    tele.speedMps = emaSpeed.update(rawSpeed);
    if (tele.speedMps < 0.03f) tele.speedMps = 0.0f;
    if (tele.speedMps > tele.peakSpeedMps) tele.peakSpeedMps = tele.speedMps;

    tele.totalDistanceM += tele.speedMps * DT;

    // 11. SWOLF Score & Quality Metrics
    if (tele.totalDistanceM > 5.0f && tele.speedMps > 0.1f) {
      float secPerPool = profile.poolLengthM / tele.speedMps;
      float strokesPerPool = (tele.strokeRateSpm / 60.0f) * secPerPool;
      tele.swolfScore = secPerPool + strokesPerPool;
    }

    float qualRaw = 0.35f * tele.phase.symmetryScore + 0.35f * smooth01(tele.strokeLengthM / 1.6f) + 0.30f * smooth01(tele.speedMps / 2.0f);
    tele.qualityScore = emaQuality.update(qualRaw);

    // 12. Session Timers
    if (tele.state == STATE_SWIM_EASY || tele.state == STATE_SWIM_RACE) {
      tele.activeSwimMs += SAMPLE_INTERVAL_MS;
      tele.continuousSwimMs += SAMPLE_INTERVAL_MS;
      if (tele.continuousSwimMs > tele.longestContinuousSwimMs) {
        tele.longestContinuousSwimMs = tele.continuousSwimMs;
      }
    } else if (tele.state == STATE_SWIM_REST) {
      tele.restMs += SAMPLE_INTERVAL_MS;
    }

    if ((nowMs - tele.lastSwimMotionMs) > 10000) {
      tele.continuousSwimMs = 0;
    }

    // 13. Dynamic In-Pool Calibration
    if (tele.calibrationActive && (tele.totalDistanceM - tele.calStartDistanceM) >= tele.calTargetDistanceM) {
      finalizeCalibration();
    }
  }

  void startCalibration(float targetDistanceM) {
    tele.calibrationActive = true;
    tele.calTargetDistanceM = clampf(targetDistanceM, 10.0f, 500.0f);
    tele.calStartMs = millis();
    tele.calStartStrokeCount = tele.strokeCount;
    tele.calStartDistanceM = tele.totalDistanceM;
  }

  void finalizeCalibration() {
    if (!tele.calibrationActive) return;
    uint32_t durMs = millis() - tele.calStartMs;
    uint32_t strokes = tele.strokeCount - tele.calStartStrokeCount;
    float measuredDist = tele.totalDistanceM - tele.calStartDistanceM;

    if (durMs >= 6000 && strokes >= 3 && measuredDist >= 5.0f && tele.calTargetDistanceM >= 5.0f) {
      float factor = tele.calTargetDistanceM / measuredDist;
      factor = clampf(factor, 0.60f, 1.60f);
      tele.calibrationFactor = factor;
      tele.strokeLenBaseM *= factor;
      tele.strokeLenSpmCoeff *= factor;
      tele.strokeLenVarCoeff *= factor;
    }
    tele.calibrationActive = false;
  }
};

#endif // SWIMBAND_BIOMECHANICS_H
