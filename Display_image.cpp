#include "Display_image.h"
#include <ArduinoJson.h>

// ---- 内部状態 ----
static unsigned long s_until_ms = 0;

// NeoMatrix 本体（本モジュール内で生成）
static Adafruit_NeoMatrix s_matrix(
  DISP_W, DISP_H, DISP_LED_PIN,
  NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_PROGRESSIVE,
  DISP_PIXEL_TYPE
);

// Motion.cpp から見える参照（互換維持）
Adafruit_NeoMatrix& Matrix = s_matrix;

// ---- 内部ユーティリティ ----
static void drawRGBArrayRotCCW(const uint8_t* rgb, size_t n) {
  if (!rgb) return;
  if (n < (size_t)(DISP_W * DISP_H * 3)) return;

  s_matrix.fillScreen(0);
  for (int sy = 0; sy < DISP_H; ++sy) {
    for (int sx = 0; sx < DISP_W; ++sx) {
      size_t i = (size_t)(sy * DISP_W + sx) * 3;
      int dx = sy;
      int dy = DISP_W - 1 - sx; // 90°CCW
      s_matrix.drawPixel(dx, dy, s_matrix.Color(rgb[i], rgb[i+1], rgb[i+2]));
    }
  }
  s_matrix.show();
}

// ---- 公開API ----
void Display_Init(uint8_t global_brightness) {
  s_matrix.begin();
  s_matrix.setBrightness(global_brightness);
  s_matrix.fillScreen(0);
  s_matrix.show();
  s_until_ms = 0;
}

void Display_Clear() {
  s_matrix.fillScreen(0);
  s_matrix.show();
  s_until_ms = 0;
}

bool Display_ShowRGBRotCCW(const uint8_t* rgb, size_t n, unsigned long display_ms) {
  if (!rgb) return false;
  if (n < (size_t)(DISP_W * DISP_H * 3)) return false;
  drawRGBArrayRotCCW(rgb, n);
  s_until_ms = millis() + display_ms;
  return true;
}

bool Display_ShowFromJson(const uint8_t* buf, size_t len, unsigned long display_ms) {
  if (!buf || len == 0) return false;

  // JSON parse
  DynamicJsonDocument doc(8192);
  if (deserializeJson(doc, buf, len)) {
    Serial.println("❌ JSON parse");
    return false;
  }

  // 可変輝度（任意）
  if (doc.containsKey("brightness")) {
    int br = constrain(doc["brightness"].as<int>(), 0, 255);
    s_matrix.setBrightness(br);
  }

  JsonArray rgbA;
  if (doc["rgb"].is<JsonArray>()) {
    rgbA = doc["rgb"].as<JsonArray>();
  } else if (doc["records"].is<JsonArray>() && doc["records"][0]["rgb"].is<JsonArray>()) {
    rgbA = doc["records"][0]["rgb"].as<JsonArray>();
  } else {
    Serial.println("❌ no rgb[]");
    return false;
  }

  const size_t need = (size_t)DISP_W * DISP_H * 3;
  if (rgbA.size() < need) {
    Serial.printf("❌ rgb too short %u<%u\n", (unsigned)rgbA.size(), (unsigned)need);
    return false;
  }

  static uint8_t rgbBuf[DISP_W * DISP_H * 3];
  for (size_t i = 0; i < need; ++i) rgbBuf[i] = (uint8_t)rgbA[i].as<int>();

  drawRGBArrayRotCCW(rgbBuf, need);
  Serial.println("✅ 表示完了");
  s_until_ms = millis() + display_ms;
  return true;
}

bool Display_IsActive() {
  return (s_until_ms != 0) && (millis() < s_until_ms);
}

bool Display_EndIfExpired() {
  if (s_until_ms && millis() >= s_until_ms) {
    Display_Clear();
    return true;
  }
  return false;
}

void Display_BlockFor(unsigned long ms) {
  if (ms == 0) { return; }
  // 何も描画しないが、受信抑止フラグとして until を張る
  s_until_ms = millis() + ms;
}
