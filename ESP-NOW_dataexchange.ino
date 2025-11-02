#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>

#include <LittleFS.h>
#include <FS.h>

#include <ArduinoJson.h>

#include "Motion.h"        // Radar/Ripple 用
#include "Display_image.h" // LED表示モジュール

/***** ========== LED MATRIX ========== *****/
#define GLOBAL_BRIGHTNESS 10

/***** ========== 無線・ファイル設定 ========== *****/
static const int WIFI_CH = 6;
static const uint8_t MAC_BC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

static const char* JSON_PATH = "/my_data_text.json";

/***** ========== 交換プロトコル/制限 ========== *****/
static const size_t MAX_MSG_BYTES  = 2048;
static const size_t CHUNK_DATA_MAX = 160;
static const size_t MAX_CHUNKS     = (MAX_MSG_BYTES + CHUNK_DATA_MAX - 1) / CHUNK_DATA_MAX;

enum MsgType : uint8_t { HELLO=1, META=2, CHUNK=3, NACK=4, ACK=5 };

struct __attribute__((packed)) FrameMeta  { uint8_t type; uint16_t msg_id, total, len_all; uint32_t crc32_all; };
struct __attribute__((packed)) FrameChunk { uint8_t type; uint16_t msg_id, idx, payload_len; uint8_t data[CHUNK_DATA_MAX]; };
struct __attribute__((packed)) FrameHello { uint8_t type; };
struct __attribute__((packed)) FrameAck   { uint8_t type; uint16_t msg_id; };
struct __attribute__((packed)) FrameNack  { uint8_t type; uint16_t msg_id, missing_idx; };

/***** ========== ランデブー/キープアライブ/閾値 ========== *****/
static const unsigned long HELLO_KEEPALIVE_MS = 2000;
static const unsigned long IDLE_RESYNC_MS     = 3000;
static const unsigned long FOUND_LOG_DEBOUNCE = 500;

// RSSIは"開始"の判定にだけ使う
static const int  RSSI_START_THRESHOLD = -20;   // 開始時のみ使用
static bool linkEstablished = false;             // セッション確立後はRSSI無視

// 追加：表示＆HELLOバースト制御
static const unsigned long DISPLAY_MS      = 3000; // 3秒表示
static const unsigned long HELLO_BURST_MS  = 1000; // 表示終了直後のHELLO連打期間
static const unsigned long HELLO_BURST_INT = 150;  // 連打間隔

/***** ========== ランタイム状態 ========== *****/
// ピア検出
volatile bool peerKnown = false;
uint8_t peerMac[6] = {0};
bool broadcastPeerAdded = false;
unsigned long lastHelloMs = 0;

// 自ノード送信データ
String myJson;

// 送信状態
uint16_t current_msg_id = 1;
String   tx_json;
uint32_t tx_crc = 0;
uint16_t tx_total = 0, tx_len_all = 0;
bool     tx_done = false;
unsigned long lastSendMs = 0, lastProgressMs = 0;
uint16_t nextIdx = 0;

// NACK重複抑制
static uint16_t lastNackIdx = 0xFFFF, lastNackMsg = 0xFFFF;
static unsigned long lastNackAt = 0;

// 受信アセンブリ
struct RxState {
  bool     active = false;
  uint16_t msg_id = 0, total = 0, len_all = 0;
  uint32_t crc32_all = 0;
  bool     got[MAX_CHUNKS] = {0};
  uint16_t gotCount = 0;
  uint8_t  buf[MAX_MSG_BYTES];
  unsigned long lastNackTxAt = 0;
  uint16_t      lastNackTxIdx = 0xFFFF;
} rx;

// HELLOバースト制御
unsigned long helloBurstUntil = 0;
unsigned long nextHelloAt     = 0;

// 監視
static unsigned long lastTrafficMs = 0;
static unsigned long lastFoundLogMs = 0;

// RSSI（プロミスキャスから取得）
static volatile int lastRSSI = -100;

/***** ========== 低レベルWiFiヘッダ（RSSI用途） ========== *****/
typedef struct {
  uint16_t frame_ctrl;
  uint16_t duration_id;
  uint8_t  addr1[6];
  uint8_t  addr2[6]; // Tx src
  uint8_t  addr3[6];
  uint16_t seq_ctrl;
} wifi_ieee80211_hdr_t;

typedef struct {
  wifi_ieee80211_hdr_t hdr;
  uint8_t payload[0];
} wifi_ieee80211_packet_t;

/***** ========== ユーティリティ ========== *****/
static uint32_t crc32_simple(const uint8_t* p, size_t n){
  uint32_t c=0xFFFFFFFF;
  for(size_t i=0;i<n;i++){ c ^= p[i]; for(int k=0;k<8;k++) c = (c>>1) ^ (0xEDB88320 & (-(int)(c & 1))); }
  return ~c;
}
static void macToStr(const uint8_t mac[6], char out[18]){
  sprintf(out,"%02X:%02X:%02X:%02X:%02X:%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
}
bool ensurePeer(const uint8_t mac[6], bool encrypt=false){
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, mac, 6);
  peer.ifidx = WIFI_IF_STA;
  peer.channel = WIFI_CH;
  peer.encrypt = encrypt;
  if (esp_now_is_peer_exist(mac)) return true;
  if (esp_now_add_peer(&peer) != ESP_OK){
    char m[18]; macToStr(mac,m);
    Serial.printf("[ERR] add_peer %s\n", m);
    return false;
  }
  return true;
}

/***** ========== プロミスキャス（RSSI 計測） ========== *****/
static void IRAM_ATTR promiscuous_rx_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT && type != WIFI_PKT_DATA) return;
  const wifi_promiscuous_pkt_t* ppkt = (const wifi_promiscuous_pkt_t*)buf;
  lastRSSI = (int)ppkt->rx_ctrl.rssi; // dBm
}

/***** ========== ファイル ========== *****/
String loadJsonFromLittleFS(const char* path, size_t maxBytes){
  if (!LittleFS.begin(true)) Serial.println("[LittleFS] mount failed");
  if (!LittleFS.exists(path)) {
    Serial.printf("[LittleFS] %s not found. using default\n", path);
    return String(R"({"id":"393235","flag":"photo","note":"default"})");
  }
  File f = LittleFS.open(path, "r");
  if (!f || f.isDirectory()) return String(R"({"error":"open failed"})");
  size_t n = f.size(); if (n > maxBytes) n = maxBytes;
  String s; s.reserve(n);
  while (f.available() && s.length() < (int)n) s += (char)f.read();
  f.close();
  Serial.printf("[LittleFS] loaded %u bytes\n", (unsigned)s.length());
  return s;
}

/***** ========== 送信関連 ========== *****/
void sendHELLO(){
  if (!broadcastPeerAdded) { ensurePeer(MAC_BC,false); broadcastPeerAdded = true; }
  FrameHello f{HELLO}; esp_now_send(MAC_BC, (uint8_t*)&f, sizeof(f));
}
void sendMETA(const uint8_t mac[6]){
  FrameMeta m{META, current_msg_id, tx_total, tx_len_all, tx_crc};
  ensurePeer(mac,false); esp_now_send(mac, (uint8_t*)&m, sizeof(m)); lastProgressMs = millis();
}
void sendCHUNK(const uint8_t mac[6], uint16_t idx){
  FrameChunk c{}; c.type=CHUNK; c.msg_id=current_msg_id; c.idx=idx;
  size_t off = (size_t)idx * CHUNK_DATA_MAX;
  size_t remain = (tx_len_all > off) ? (tx_len_all - off) : 0;
  size_t n = remain > CHUNK_DATA_MAX ? CHUNK_DATA_MAX : remain;
  memcpy(c.data, tx_json.c_str() + off, n); c.payload_len = (uint16_t)n;
  esp_now_send(mac, (uint8_t*)&c, sizeof(c.type)+2+2+2+n); lastProgressMs = millis();
}
void sendACK (const uint8_t mac[6], uint16_t msg_id){ FrameAck a{ACK, msg_id};  esp_now_send(mac, (uint8_t*)&a, sizeof(a)); }
void sendNACK(const uint8_t mac[6], uint16_t msg_id, uint16_t missing_idx){
  FrameNack n{NACK, msg_id, missing_idx}; esp_now_send(mac, (uint8_t*)&n, sizeof(n)); lastProgressMs = millis();
}

void beginExchangeIfReady(){
  if (!peerKnown || tx_done) return;
  tx_json = myJson;
  if ((size_t)tx_json.length() > MAX_MSG_BYTES) tx_json = tx_json.substring(0, MAX_MSG_BYTES);
  tx_len_all = tx_json.length();
  tx_crc     = crc32_simple((const uint8_t*)tx_json.c_str(), tx_len_all);
  tx_total   = (tx_len_all + CHUNK_DATA_MAX - 1) / CHUNK_DATA_MAX;
  nextIdx    = 0;

  Serial.println("📡 送信開始");
  sendMETA(peerMac); lastSendMs = millis();
  uint16_t burst = min<uint16_t>(tx_total, 6);
  for(uint16_t i=0;i<burst;i++){ sendCHUNK(peerMac, i); delay(3); }
}

void startNewExchange() {
  current_msg_id++; if (current_msg_id == 0) current_msg_id = 1;
  tx_done = false; nextIdx = 0; beginExchangeIfReady();
}

/***** ========== コールバック ========== *****/
void onSend(const uint8_t* /*mac*/, esp_now_send_status_t /*s*/){
  lastTrafficMs = millis();
}

void onRecv(const uint8_t* mac, const uint8_t* data, int len){
  if (len <= 0) return;

  // 画像表示中は受信を無視（LED表示3秒の間）
  if (Display_IsActive()) {
    // Serial.println("⏸ 表示中のため受信無視");
    return;
  }

  uint8_t t = data[0];
  int rssi = lastRSSI;

  lastTrafficMs = millis();

  if (t == HELLO){
    // まだリンク未確立のときだけ、開始判定としてRSSIを使う
    if (!linkEstablished && rssi < RSSI_START_THRESHOLD) {
      // 弱電界ならHELLOは無視（開始しない）
      static unsigned long lastWarn = 0;
      if (millis() - lastWarn > 5000) {
        char m[18]; macToStr(mac, m);
        Serial.printf("⚠️ 信号弱: %d<%d 無視(%s)\n", rssi, RSSI_START_THRESHOLD, m);
        lastWarn = millis();
      }
      return;
    }

    unsigned long nowms = millis();
    if (nowms - lastFoundLogMs > FOUND_LOG_DEBOUNCE) {
      const char* distance = (rssi>-50)?"非常に近い":(rssi>-60)?"近い":(rssi>-70)?"中距離":(rssi>-80)?"遠い":"非常に遠い";
      char m[18]; macToStr(mac,m);
      Serial.printf("🔍 相手発見: %s | RSSI: %d dBm (%s)\n", m, rssi, distance);
      lastFoundLogMs = nowms;
    }
    if (!peerKnown) { memcpy(peerMac, mac, 6); peerKnown = true; }
    bool idle = peerKnown && !rx.active && tx_done;
    if (idle) { Serial.println("🔄 再交換開始"); Display_Clear(); startNewExchange(); }
    else beginExchangeIfReady();
    return;
  }

  if (t == META && len >= (int)sizeof(FrameMeta)){
    // 受信セッション未開始なら、開始時のみRSSIチェック
    if (!linkEstablished && rssi < RSSI_START_THRESHOLD) {
      // まだ開始しない
      return;
    }
    // すでに受信中なら新METAで初期化しない（やり直し防止）
    if (rx.active) {
      // Serial.println("[RX] META ignored (already receiving)");
      return;
    }

    const FrameMeta* m = (const FrameMeta*)data;
    if (!peerKnown) { memcpy(peerMac, mac, 6); peerKnown = true; }

    Serial.printf("📥 受信開始... (RSSI:%d)\n", rssi);
    Display_Clear();

    memset(&rx, 0, sizeof(rx));
    rx.active = true; rx.msg_id = m->msg_id; rx.total = m->total; rx.len_all = m->len_all; rx.crc32_all = m->crc32_all;
    lastProgressMs = millis();

    linkEstablished = true; // ★ 一度確立したら以降はRSSI無視で走り切る

    beginExchangeIfReady();
    return;
  }

  if (t == CHUNK && len >= (int)(sizeof(uint8_t)+2+2+2)){
    const FrameChunk* c = (const FrameChunk*)data;

    if (rx.active && c->msg_id == rx.msg_id && c->idx < MAX_CHUNKS && rx.got[c->idx]) { lastProgressMs = millis(); return; }
    if (!rx.active || c->msg_id != rx.msg_id) { char m[18]; macToStr(mac,m); Serial.printf("[RX] CHUNK ignored (no active/msg mismatch) %s\n", m); return; }
    if (c->idx >= rx.total || c->payload_len > CHUNK_DATA_MAX) { char m[18]; macToStr(mac,m); Serial.printf("[RX] CHUNK invalid idx=%u len=%u %s\n", c->idx, c->payload_len, m); return; }

    size_t off = (size_t)c->idx * CHUNK_DATA_MAX, can = rx.len_all - off;
    if (c->payload_len > can) { char m[18]; macToStr(mac,m); Serial.printf("[RX] CHUNK overflow idx=%u len=%u can=%u %s\n", c->idx, c->payload_len, (unsigned)can, m); return; }

    memcpy(rx.buf + off, c->data, c->payload_len);
    if (!rx.got[c->idx]) { rx.got[c->idx] = true; rx.gotCount++; }

    static uint16_t lastPct = 0; uint16_t pct = (rx.gotCount * 100) / rx.total;
    if (pct >= lastPct + 10 || rx.gotCount == rx.total) { Serial.printf("📦 受信中: %u%%\n", pct); lastPct = pct; }
    lastProgressMs = millis();

    if (rx.gotCount < rx.total){
      for(uint16_t i=0;i<rx.total;i++){
        if (!rx.got[i]){
          unsigned long nowms = millis();
          if (!(rx.lastNackTxIdx == i && (nowms - rx.lastNackTxAt) < 30)) { sendNACK(mac, rx.msg_id, i); rx.lastNackTxIdx=i; rx.lastNackTxAt=nowms; }
          break;
        }
      }
    } else {
      uint32_t csum = crc32_simple(rx.buf, rx.len_all);
      if (csum == rx.crc32_all){
        sendACK(mac, rx.msg_id); Serial.println("✅ 受信完了");
        Ripple_PlayOnce();                 // 完了エフェクト
        Display_ShowFromJson(rx.buf, rx.len_all, DISPLAY_MS);// LED表示
        rx.active = false; lastPct = 0;
      } else {
        Serial.println("❌ CRC - NACK(0)");
        unsigned long nowms = millis();
        if (!((rx.lastNackTxIdx == 0) && (nowms - rx.lastNackTxAt) < 30)) { sendNACK(mac, rx.msg_id, 0); rx.lastNackTxIdx=0; rx.lastNackTxAt=nowms; }
      }
    }
    return;
  }

  if (t == NACK && len >= (int)sizeof(FrameNack)){
    const FrameNack* n = (const FrameNack*)data;
    unsigned long nowms = millis();
    if (n->msg_id == lastNackMsg && n->missing_idx == lastNackIdx && (nowms - lastNackAt) < 50) return;
    lastNackMsg = n->msg_id; lastNackIdx = n->missing_idx; lastNackAt = nowms;

    if (n->msg_id == current_msg_id && n->missing_idx < tx_total){
      sendCHUNK(mac, n->missing_idx);
      if (n->missing_idx + 1 < tx_total) sendCHUNK(mac, n->missing_idx + 1);
    }
    lastProgressMs = millis();
    return;
  }

  if (t == ACK && len >= (int)sizeof(FrameAck)){
    const FrameAck* a = (const FrameAck*)data;
    if (a->msg_id == current_msg_id){ tx_done = true; Serial.println("✅ 送信完了"); }
    lastProgressMs = millis();
    return;
  }
}

/***** ========== Arduino 標準 ========== *****/
void setup(){
  Serial.begin(115200); delay(200);
  Serial.println("\n=== ESP-NOW データ交換 & LED表示 ===\n");

  // LED
  Display_Init(GLOBAL_BRIGHTNESS);
  Serial.println("✅ LED初期化");

  // 起動演出 → 自分のJSON表示
  Serial.println("💫 起動Ripple"); Ripple_PlayOnce();
  myJson = loadJsonFromLittleFS(JSON_PATH, MAX_MSG_BYTES);
  Serial.printf("📄 JSON %u bytes\n", (unsigned)myJson.length());
  Serial.println("💡 起動表示"); Display_ShowFromJson((const uint8_t*)myJson.c_str(), myJson.length(), DISPLAY_MS);

  // WiFi/ESP-NOW
  WiFi.mode(WIFI_STA);
  wifi_country_t ctry = { .cc="JP", .schan=1, .nchan=13, .max_tx_power=20, .policy=WIFI_COUNTRY_POLICY_MANUAL };
  esp_wifi_set_country(&ctry);

  // RSSI取得のためプロミスキャスON（フィルタ設定）
  wifi_promiscuous_filter_t filt; filt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&promiscuous_rx_cb);

  // 固定チャネル
  esp_wifi_set_channel(WIFI_CH, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) { Serial.println("❌ ESP-NOW init"); while(true){ delay(1000);} }
  Serial.println("✅ ESP-NOW準備OK");
  esp_now_register_send_cb(onSend);
  esp_now_register_recv_cb(onRecv);

  uint8_t ch; wifi_second_chan_t sc; esp_wifi_get_channel(&ch, &sc);
  Serial.printf("📡 MAC:%s CH:%u\n\n", WiFi.macAddress().c_str(), ch);
  Serial.println("🔍 探索開始...\n");

  // レーダー待機
  Radar_InitIdle();

  // 初期HELLO
  lastHelloMs = millis() - 2000;
  lastProgressMs = lastTrafficMs = millis();
}

void loop(){
  unsigned long now = millis();

  // 表示の自動消灯→レーダー再開＋HELLOバースト開始
  if (Display_EndIfExpired()){
    Serial.println("💤 LED消灯");

    // 次の接続は再び"開始判定"から入る
    linkEstablished = false;

    // レーダー開始
    Radar_InitIdle();

    // 直後HELLO連打を開始
    helloBurstUntil = now + HELLO_BURST_MS;
    nextHelloAt     = now; // すぐ1発目を送る
  }

  // レーダー（受信中/表示中以外）
  if (!rx.active && !Display_IsActive()){ Radar_IdleStep(true); delay(16); }

  // 表示直後のHELLOバースト（peerKnownかどうかに関係なく打つ）
  if (helloBurstUntil){
    if (now >= nextHelloAt){
      sendHELLO();
      nextHelloAt = now + HELLO_BURST_INT;
    }
    if (now >= helloBurstUntil){
      helloBurstUntil = 0; // バースト終了
    }
  }

  // 未発見 → HELLO送信
  if (!peerKnown && now - lastHelloMs > 500){ lastHelloMs = now + (random(0,200)); sendHELLO(); }

  // 送信進行（ドロップ抑制のため35ms）
  if (peerKnown && !tx_done && (now - lastSendMs > 35)){
    if (nextIdx < tx_total){ sendCHUNK(peerMac, nextIdx++); lastSendMs = now; }
  }

  // 進捗停止 → META再送＋先頭バースト
  if (peerKnown && !tx_done && (now - lastProgressMs > 1500)){
    Serial.println("⚠️ 進捗停止 - 再送"); sendMETA(peerMac);
    uint16_t burst = min<uint16_t>(tx_total, 6); for(uint16_t i=0;i<burst;i++){ sendCHUNK(peerMac,i); delay(3); }
    lastProgressMs = now;
  }

  // アイドルKeepalive HELLO
  static unsigned long lastHelloKeepalive = 0;
  bool idle = peerKnown && !rx.active && tx_done;
  if (idle && (now - lastHelloKeepalive > HELLO_KEEPALIVE_MS)){
    lastHelloKeepalive = now + (random(0,150)); sendHELLO();
  }

  // 長時間アイドル → 新セッションで再交換
  if (idle && (now - lastTrafficMs > IDLE_RESYNC_MS)){
    Serial.println("🔄 無通信 - 再交換"); startNewExchange(); lastTrafficMs = now;
  }

  // 保険：セッション確立後に長時間無進捗ならリセット
  if (linkEstablished && (now - lastProgressMs > 4000)){
    Serial.println("⏹ 無進捗タイムアウト - 再探索へ");
    rx.active = false;
    linkEstablished = false;
    peerKnown = false;
  }
}
