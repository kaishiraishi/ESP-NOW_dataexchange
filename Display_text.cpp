#include "Display_text.h"

// テキスト用の明るさ
uint8_t gTextBrightness = 20;

// テキスト表示用Matrix（Display_imageとは別インスタンス）
#define RGB_Control_PIN 14
Adafruit_NeoMatrix TextMatrix = Adafruit_NeoMatrix(
  8, 8, RGB_Control_PIN,
  NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_PROGRESSIVE,
  NEO_GRB + NEO_KHZ800
);

// カラーパレット
const uint16_t colors[] = {
  TextMatrix.Color(255,255,255),  // 白
  TextMatrix.Color(255,0,0),      // 赤
  TextMatrix.Color(0,255,0),      // 緑
  TextMatrix.Color(0,0,255)       // 青
};

int MatrixWidth = 0;

void Matrix_SetTextBrightness(uint8_t b) {
  gTextBrightness = b;
}

void Matrix_Init() {
  TextMatrix.begin();
  TextMatrix.setTextWrap(false);
  TextMatrix.setBrightness(gTextBrightness);
  TextMatrix.setTextColor(colors[0]);
  MatrixWidth = TextMatrix.width();
}

// 文字列幅の計算（簡易版：6px/文字と仮定）
static int getStringWidth(const char* text) {
  if (!text) return 0;
  return strlen(text) * 6;
}

void Text_Flow(char* Text) {
  TextMatrix.setBrightness(gTextBrightness);
  int textWidth = getStringWidth(Text);
  TextMatrix.fillScreen(0);
  TextMatrix.setCursor(MatrixWidth, 0);
  TextMatrix.print(Text);
  if (--MatrixWidth < -textWidth) MatrixWidth = TextMatrix.width();
  TextMatrix.show();
}

void Text_PlayOnce(const char* text, uint16_t frame_delay_ms) {
  TextMatrix.setBrightness(gTextBrightness);
  int textWidth = getStringWidth(text);
  MatrixWidth = TextMatrix.width();
  while (MatrixWidth >= -textWidth) {
    TextMatrix.fillScreen(0);
    TextMatrix.setCursor(MatrixWidth, 0);
    TextMatrix.print(text);
    TextMatrix.show();
    MatrixWidth--;
    delay(frame_delay_ms);
  }
}

unsigned long Text_EstimateDurationMs(const char* text, uint16_t frame_delay_ms) {
  if (!text) return 0;
  const int textWidth = getStringWidth(text);
  const int steps = TextMatrix.width() + textWidth;  // 左端に抜け切るまでのステップ数
  return (unsigned long)steps * (unsigned long)frame_delay_ms;
}
