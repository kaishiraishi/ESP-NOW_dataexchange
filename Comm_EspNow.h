#pragma once
#include <Arduino.h>

// 完成JSONを通知するコールバック型
using CommOnMessageCB = void (*)(const uint8_t* data, size_t len);

// 初期化（WiFi STA + 指定チャネル + ESP-NOW準備 + ブロードキャストpeer追加）
void Comm_Init(int wifiChannel);

// 完成メッセージのハンドラ登録
void Comm_SetOnMessage(CommOnMessageCB cb);

// JSON文字列をブロードキャスト送信（必要に応じて分割）
void Comm_SendJsonBroadcast(const String& json);

// === 受信後の処理フロー（アプリケーション責務） ===
// 実装例：Json_Handler + DisplayManager + Motion を使った表示フロー
void Comm_DefaultMessageHandler(const uint8_t* data, size_t len);

// 直近で受信したパケットのRSSI(dBm)を返す。
// 取得不可な環境/未受信時は -128 を返す。
int Comm_GetLastRssi();

// 受信許可する最小RSSIしきい値(dBm)。既定は -40。
void Comm_SetMinRssiToAccept(int dbm);
int  Comm_GetMinRssiToAccept();
