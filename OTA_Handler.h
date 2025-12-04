#pragma once
#include <Arduino.h>

// デバッグモードの状態を確認
bool isDebugMode();

// デバッグモードを開始（Bootボタン押下時に呼ぶ）
void startDebugMode();

// デバッグモードのloop処理（OTA + Telnet）
void handleDebugMode();

// デバッグ出力関数（Telnet対応）
void debugPrint(const char* msg);
void debugPrintln(const char* msg);
void debugPrintln(const String& msg);
void debugPrintf(const char* format, ...);

// 従来のOTA関数（互換性のため残す）
void setupOTA();
void handleOTA();
