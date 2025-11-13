#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>

// 表示設定定数の外部宣言（.ino で定義）
extern uint16_t TEXT_FRAME_DELAY_MS;
extern uint8_t TEXT_BRIGHTNESS;

// 8x8/ピン/ピクセルタイプはここで定義（必要なら変更）
#ifndef DISP_W
#define DISP_W 8
#endif

#ifndef DISP_H
#define DISP_H 8
#endif

#ifndef DISP_LED_PIN
#define DISP_LED_PIN 14
#endif

#ifndef DISP_PIXEL_TYPE
#define DISP_PIXEL_TYPE (NEO_GRB + NEO_KHZ800)
#endif

namespace DisplayManager {
  // === 初期化 ===
  void Init(uint8_t global_brightness);

  // === 画像表示 ===
  bool ShowRGBRotCCW(const uint8_t* rgb, size_t n, unsigned long display_ms);
  // 非回転版（互換用ラッパー）。必要に応じて内部で回転描画に委譲
  bool ShowRGB(const uint8_t* rgb, size_t n, unsigned long display_ms);
  void Clear();
  // 全点灯（白）
  void AllOn(uint8_t brightness);

  // === 表示状態管理（ガード） ===
  bool IsActive();              // 表示中ガードが張られているか
  bool EndIfExpired();          // 期限切れなら消灯しtrue
  void BlockFor(unsigned long ms); // 何も描画せず占有だけ張る

  // === テキスト表示 ===
  void SetTextBrightness(uint8_t b);
  void TextInit();  // テキスト用初期設定（setTextWrap等）
  void TextPlayOnce(const char* text, uint16_t frame_delay_ms);
  unsigned long TextEstimateDurationMs(const char* text, uint16_t frame_delay_ms);

  // === Motion等が参照するMatrix（互換維持） ===
  extern Adafruit_NeoMatrix& Matrix;
}
