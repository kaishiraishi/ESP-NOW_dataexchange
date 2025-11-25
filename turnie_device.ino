#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>

#include <LittleFS.h>
#include <FS.h>

#include <ArduinoJson.h>
#include <OneButton.h>

#include "Motion.h"           // Radar/Ripple 用
#include "Display_Manager.h"  // LED表示統合モジュール
#include "Json_Handler.h"     // JSON読み込み・保存管理
#include "BLE_Manager.h"      // BLEを使ったJSON受信
#include "Comm_EspNow.h"      // 通信シーケンス
#include "OTA_Handler.h"      // Wifi経由で書き込み

/***** ========== LED MATRIX ========== *****/
int GLOBAL_BRIGHTNESS = 20;
uint16_t TEXT_FRAME_DELAY_MS = 60;  // スクロール速度(1ステップの遅延)

/***** ========== ボタン ========== *****/
#ifndef BUTTON_PIN
#define BUTTON_PIN 39
#endif


static OneButton g_btn;            
static bool DisplayMode = false;   

// ▼▼▼ 追加 ▼▼▼
String lastRxData = "";          // 最後に受信したデータ
unsigned long lastRxTime = 0;    // 最後に受信した時刻
const unsigned long IGNORE_MS = 10000; // 同じデータを無視する時間(ミリ秒)
// ▲▲▲ 追加 ▲▲▲

/***** ========== 無線・ファイル設定 ========== *****/
static const int WIFI_CH = 6;
static const char* JSON_PATH = "/data.json";

static int RSSI_THRESHOLD_DBM = -20; 

/***** ========== ランタイム状態 ========== *****/
String myJson;

/***** ========== 受信フロー（保存→表示） ========== *****/
static void OnMessageReceived(const uint8_t* data, size_t len) {
  // 先にString化して内容を確認
  String incoming((const char*)data, len);

  // 【判定】データ内容が前回と同じ かつ 指定時間(10秒)以内なら無視して終了
  if (incoming.equals(lastRxData) && (millis() - lastRxTime < IGNORE_MS)) {
    return; 
  }

  // 新しい通信として記録を更新
  lastRxData = incoming;
  lastRxTime = millis();

  // --- 以下、既存の処理 (一部 incoming 変数を利用して効率化) ---
  saveIncomingJson(data, len);  
  DisplayManager::BlockFor(1600);
  Ripple_PlayOnce();

  // 既に incoming に変換済みなので再利用
  if (!loadDisplayFromJsonString(incoming)) {
    Serial.println("[PARSE] 受信JSON解析失敗");
  } else if (!performDisplay()) {
    Serial.println("[DISPLAY] 表示できるデータがありません");
  }
  Serial.println(incoming);  
}

/***** ========== Arduino 標準 ========== *****/
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== ESP-NOW JSON Broadcast ===");

  setupOTA();

  DisplayManager::Init(GLOBAL_BRIGHTNESS);
  DisplayManager::TextInit();
  Ripple_PlayOnce();

/*** ========== ボタン ========== *****/
  g_btn.setup(BUTTON_PIN, INPUT_PULLUP, true);  // 
  g_btn.setClickMs(300);                        // 
  g_btn.attachDoubleClick([]() {
    DisplayMode = !DisplayMode;

    DiagonalWave_PlayOnce();
    
    Serial.printf("[MODE] 受信データ表示モード: %s\n", DisplayMode ? "ON" : "OFF");

    if (DisplayMode) {
      // 表示モード再生 (Play latest)
      size_t n = inboxSize();
      if (n > 0) {
        InboxItem item;
        if (inboxGet(n - 1, item)) {
           if (loadDisplayFromJsonString(item.json)) {
             performDisplay();
           }
        }
      } else {
        Serial.println("[INBOX] データなし");
      }
    } else {
      // 表示モード終了 (End)
      DisplayManager::Clear();
      //Radar_InitIdle();
    }
  });

  g_btn.attachClick([]() {
    if (!DisplayMode) return;
    
    // 受信データの二番目へ (To the 2nd item)
    size_t n = inboxSize();
    if (n >= 2) {
      InboxItem item;
      if (inboxGet(n - 2, item)) {
         if (loadDisplayFromJsonString(item.json)) {
           performDisplay();
         }
      }
    } else {
      Serial.println("[INBOX] 2番目のデータなし");
    }
  });

  myJson = loadJsonFromPath(JSON_PATH, 2048);
  Serial.printf("📄 生データ:\n%s\n", myJson.c_str());
  Serial.printf("📄 %s (%uB)\n", JSON_PATH, (unsigned)myJson.length());
  if (!myJson.isEmpty()) {
    loadDisplayFromLittleFS();
    performDisplay();
  }

  Comm_SetOnMessage(OnMessageReceived);
  Comm_Init(WIFI_CH);
  Comm_SetMinRssiToAccept(RSSI_THRESHOLD_DBM);

  if (!DisplayManager::IsActive()) {
    //Radar_InitIdle();
  } else {
    Serial.println("🔍 起動時に表示中のため、レーダーは有効期限後に開始");
  }

  BLE_Init();
}

void loop() {

  handleOTA();
  
  static unsigned long nextSend = 0;
  unsigned long now = millis();

  if (DisplayManager::EndIfExpired()) {
    if (!myJson.isEmpty()) {
      performDisplay();
    }
  }

  g_btn.tick();

  if (!DisplayManager::IsActive() && !DisplayMode) {
    DisplayManager::TextScroll_Update();
  }
  delay(16);

  BLE_Tick();

  if (!myJson.isEmpty() && now >= nextSend) {
    Comm_SendJsonBroadcast(myJson);
    nextSend = now + 100 + (esp_random() % 50) - 25;  
    
  }
  if (Serial.available() > 0) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.startsWith("save:")) {
      String js = line.substring(5);
      if (!js.isEmpty()) {
        saveJsonToPath("/mydata.json", js);
        saveJsonToPath("/data.json", js);
        loadDisplayFromLittleFS();
        performDisplay();
        Serial.println("Saved JSON to /mydata.json and /data.json and displayed it");
      }
    }
  }
}
