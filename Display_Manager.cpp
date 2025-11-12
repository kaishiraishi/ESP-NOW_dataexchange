#include "Display_Manager.h"

namespace DisplayManager {

// ---- 内部状態 ----
static unsigned long s_until_ms = 0;

// NeoMatrix 本体（ここで1つだけ生成）
static Adafruit_NeoMatrix s_matrix(
  DISP_W, DISP_H, DISP_LED_PIN,
  NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_PROGRESSIVE,
  DISP_PIXEL_TYPE
);

// Motion等が参照するMatrix（互換維持）
Adafruit_NeoMatrix& Matrix = s_matrix;

// テキスト表示用の明るさと状態
static uint8_t gTextBrightness = 20;  // 初期値は TEXT_BRIGHTNESS と同期
static int MatrixWidth = 0;

// テキストカラーパレット
static const uint16_t colors[] = {
  s_matrix.Color(255,255,255),  // 白
  s_matrix.Color(255,0,0),      // 赤
  s_matrix.Color(0,255,0),      // 緑
  s_matrix.Color(0,0,255)       // 青
};

// ---- 内部ユーティリティ（画像表示用） ----
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

// ---- 内部ユーティリティ（テキスト表示用） ----
static int getStringWidth(const char* text) {
  if (!text) return 0;
  return strlen(text) * 6;
}

// ========== 公開API：初期化 ==========
void Init(uint8_t global_brightness) {
  s_matrix.begin();
  s_matrix.setBrightness(global_brightness);
  s_matrix.fillScreen(0);
  s_matrix.show();
  s_until_ms = 0;
}

// ========== 公開API：画像表示 ==========
void Clear() {
  s_matrix.fillScreen(0);
  s_matrix.show();
  s_until_ms = 0;
}

bool ShowRGBRotCCW(const uint8_t* rgb, size_t n, unsigned long display_ms) {
  if (!rgb) return false;
  if (n < (size_t)(DISP_W * DISP_H * 3)) return false;
  drawRGBArrayRotCCW(rgb, n);
  s_until_ms = millis() + display_ms;
  return true;
}

// ========== 公開API：表示状態管理 ==========
bool IsActive() {
  return (s_until_ms != 0) && (millis() < s_until_ms);
}

bool EndIfExpired() {
  if (s_until_ms && millis() >= s_until_ms) {
    Clear();
    return true;
  }
  return false;
}

void BlockFor(unsigned long ms) {
  if (ms == 0) return;
  s_until_ms = millis() + ms;
}

// ========== 公開API：テキスト表示 ==========
void SetTextBrightness(uint8_t b) {
  gTextBrightness = b;
}

void TextInit() {
  // Matrix初期化はInit()で済んでいる想定
  s_matrix.setTextWrap(false);
  s_matrix.setBrightness(TEXT_BRIGHTNESS);
  s_matrix.setTextColor(colors[0]);
  MatrixWidth = s_matrix.width();
}

void TextPlayOnce(const char* text, uint16_t frame_delay_ms) {
  s_matrix.setBrightness(TEXT_BRIGHTNESS);
  int textWidth = getStringWidth(text);
  MatrixWidth = s_matrix.width();
  while (MatrixWidth >= -textWidth) {
    s_matrix.fillScreen(0);
    s_matrix.setCursor(MatrixWidth, 0);
    s_matrix.print(text);
    s_matrix.show();
    MatrixWidth--;
    delay(frame_delay_ms);
  }
}

unsigned long TextEstimateDurationMs(const char* text, uint16_t frame_delay_ms) {
  if (!text) return 0;
  const int textWidth = getStringWidth(text);
  const int steps = s_matrix.width() + textWidth;
  return (unsigned long)steps * (unsigned long)frame_delay_ms;
}

} // namespace DisplayManager
