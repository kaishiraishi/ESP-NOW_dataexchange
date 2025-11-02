#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>

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
// 例: NEO_GRB + NEO_KHZ800
#define DISP_PIXEL_TYPE (NEO_GRB + NEO_KHZ800)
#endif

// Motion.cpp から参照できるように公開する Matrix 参照（互換維持）
extern Adafruit_NeoMatrix& Matrix;

// 初期化（グローバル輝度）
void Display_Init(uint8_t global_brightness);

// 画面消去（即時）
void Display_Clear();

// JSONバッファから表示（rgb[] または records[0].rgb[] を想定）
// 表示時間(ms)を指定（例: 3000）
bool Display_ShowFromJson(const uint8_t* buf, size_t len, unsigned long display_ms);

// すでにRGBの一次元配列(8*8*3)を持っている場合の表示
bool Display_ShowRGBRotCCW(const uint8_t* rgb, size_t n, unsigned long display_ms);

// 現在、表示中か？（タイマー中）
bool Display_IsActive();

// 期限切れなら内部で消灯し true を返す（エッジ検出用途）
// 期限内/すでに消えているなら false
bool Display_EndIfExpired();

// 何も描画せずに「表示中ガード」だけ張る（ms）
void Display_BlockFor(unsigned long ms);
