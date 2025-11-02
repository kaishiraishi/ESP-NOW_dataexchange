#ifndef _MOTION_H_
#define _MOTION_H_

#pragma once
#include <Arduino.h>
#include <Adafruit_NeoMatrix.h>

// Motion.cppで定義されるMatrixへの参照
extern Adafruit_NeoMatrix& Matrix;

// === モーション用の明るさと色相 ===
extern uint8_t gMotionBrightness;    // レーダー/リップル共通
extern uint8_t gMotionHue;           // レーダー/リップル共有色相

void Matrix_SetMotionBrightness(uint8_t b);
void Motion_SetHue(uint8_t h);

// モーション関連の関数
void Ripple_PlayOnce(uint16_t duration_ms = 0);
void Radar_PlayOnce();

// レーダー待機(非ブロッキング)
void Radar_InitIdle();
void Radar_IdleStep(bool doShow = true);

#endif
