#ifndef JSON_HANDLER_H_
#define JSON_HANDLER_H_

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <vector>

// ========== 受信インボックス（RAMリングバッファ） ==========
// 直近N件だけRAMに保持（フラッシュ書き込み寿命に影響しない）
// - i=0 は最古、i=size-1 は最新
// - 保存内容: 受信時刻(millis)、JSON文字列
struct InboxItem {
	unsigned long atMillis;
	String json;
};

// 受信したJSONバイト列をインボックスへ追加（RAMリングバッファ）
void saveIncomingJson(const uint8_t* data, size_t len);

// インボックス件数を取得
size_t inboxSize();

// i番目の要素を取得（0=最古）。成功ならtrue
bool inboxGet(size_t index, InboxItem& out);



// ========== 表示設定定数（.ino で定義） ==========
extern uint16_t TEXT_FRAME_DELAY_MS;  // テキストスクロール速度 [ms]
extern uint8_t TEXT_BRIGHTNESS;       // テキスト表示時の明るさ

// ========== JSON表示データ管理 ==========
extern String displayFlag;
extern String displayText;
extern std::vector<uint8_t> rgbData;

// ========== インターフェース ==========

/**
 * LittleFSから /data.json を読み込み、displayFlag/displayText/rgbData を抽出
 * 内部でJsonDocumentを解放するため、呼び出し後はポインタ経由アクセス不可
 */
// LittleFS から /data.json を読み込み、表示用データをセット
// LittleFS から /data.json を読み込み、表示用データをセット
// path を指定可能にして、デフォルトは "/data.json"
bool loadDisplayFromLittleFS(const char* path = "/data.json");

// LittleFS へ JSON 生文字列を書き込み
// 書き込み成功なら true
bool saveJsonToPath(const char* path, const String& jsonString);

// メモリ上のJSON文字列から displayFlag/displayText/rgbData を抽出してセット
// 成功なら true
// メモリ上のJSON文字列から表示用データをセット
bool loadDisplayFromJsonString(const String& jsonString);

// 受信JSONをRAMリングバッファへ（重複宣言を避けるためコメントのみ）



/**
 * LittleFS からJSONの生文字列を読み込み（ファイルが無ければ空文字列を返す）
 * 読み取りのみで、内部状態（displayFlag/text/rgb）は変更しない
 */
// LittleFS からJSONの生文字列を読み込み（ファイルが無ければ空文字列を返す）
// 読み取りのみで、内部状態（displayFlag/text/rgb）は変更しない
String loadJsonFromPath(const char* path, size_t maxBytes = 2048);

/**
 * 取得したデータ (displayFlag/text/rgb) に基づいて描画を実行
 * DisplayManager を使用して flag に応じた表示分岐（text/image）を行う
 * 呼び出し前に loadDisplayDataFromJson() で状態を更新すること
 * 戻り値：true=描画実行、false=描画せず
 */
bool performDisplay();

#endif // JSON_HANDLER_H_
