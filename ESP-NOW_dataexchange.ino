#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>

#include <LittleFS.h>
#include <FS.h>

#include <ArduinoJson.h>

#include "Motion.h"         // Radar/Ripple 用
#include "Display_Manager.h" // LED表示統合モジュール
#include "Json_Handler.h"   // JSON読み込み・保存管理
#include "Comm_EspNow.h"    // 通信シーケンス外部化

/***** ========== LED MATRIX ========== *****/
#define GLOBAL_BRIGHTNESS 10

// テキストスクロール設定
uint16_t TEXT_FRAME_DELAY_MS = 30;  // スクロール速度(1ステップの遅延)
uint8_t  TEXT_BRIGHTNESS     = 20;  // テキスト時の明るさ

/***** ========== 無線・ファイル設定 ========== *****/
static const int WIFI_CH = 6;
static const char* JSON_PATH = "/data.json";

/***** ========== ランタイム状態 ========== *****/
String myJson;

/***** ========== 受信フロー（保存→表示） ========== *****/
static void OnMessageReceived(const uint8_t* data, size_t len) {
  // 保存→ガード→演出→解析→表示→待機
  saveIncomingJson(data, len); // RAMリングバッファへ保存（直近N件）
  DisplayManager::BlockFor(1600);
  Ripple_PlayOnce();
  loadDisplayDataFromJson();
  performDisplay();
  Radar_InitIdle();
}

/***** ========== Arduino 標準 ========== *****/
void setup(){
  Serial.begin(115200); delay(200);
  Serial.println("\n=== ESP-NOW JSON Broadcast ===");

  // LED
  DisplayManager::Init(GLOBAL_BRIGHTNESS);
  DisplayManager::TextInit();
  Ripple_PlayOnce();

  // LittleFS -> myJson 読み出し＆シリアル表示
  myJson = loadJsonFromPath(JSON_PATH, 2048);
  Serial.printf("📄 %s (%uB)\n", JSON_PATH, (unsigned)myJson.length());
  if (!myJson.isEmpty()) {
    // 起動時にも表示試行
    loadDisplayDataFromJson();
    performDisplay();
  }

  // ESP-NOWコールバック登録
  Comm_SetOnMessage(OnMessageReceived);
  Comm_Init(WIFI_CH);

  // データ待機モード開始 → Radar起動
  Serial.println("🔍 待機中: Radar開始");
  Radar_InitIdle();
}

void loop(){
  static unsigned long nextSend = 0;
  unsigned long now = millis();

  // データ表示/エフェクト中はレーダーを停止
  if (!DisplayManager::IsActive()) {
    Radar_IdleStep(true);
  }
  delay(16);

  if (!myJson.isEmpty() && now >= nextSend) {
    Comm_SendJsonBroadcast(myJson);
    nextSend = now + 2000 + (esp_random() % 200) - 100; // ±100ms ジッター
  }
}
