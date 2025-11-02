# 1401デバイスへのJSONアップロード手順

## 🎯 目的
ESP32 (1401) にのみ `my_data_text.json` をアップロードする

## 📝 前提条件
- ✅ 1401が `/dev/cu.usbmodem1401` に接続されている
- ✅ PlatformIOがインストール済み
- ✅ `data/my_data_text.json` が準備済み

## 🚀 アップロード手順

### 方法1: スクリプト使用（推奨）

```bash
cd /Users/kaishiraishi/Desktop/ESP-NOW_dataexchange
./upload_1401.sh
```

### 方法2: 手動コマンド

```bash
cd /Users/kaishiraishi/Desktop/ESP-NOW_dataexchange

# 1401専用環境でLittleFSイメージをビルド
pio run -e esp32_1401 --target buildfs

# 1401にアップロード
pio run -e esp32_1401 --target uploadfs
```

### 方法3: VS Code

1. VS Codeの下部ステータスバーで環境を選択
2. **Switch PlatformIO Project Environment** をクリック
3. **esp32_1401** を選択
4. **PlatformIO** → **Upload File System image** をクリック

## 📊 動作確認

```bash
# 1401のシリアルモニタを起動
pio device monitor -e esp32_1401

# 以下のメッセージが表示されればOK
# [LittleFS] loaded XXX bytes
```

## 🔧 トラブルシューティング

### デバイスが見つからない場合

```bash
# 接続デバイスを確認
pio device list

# 1401が表示されることを確認
# /dev/cu.usbmodem1401
```

### アップロードエラー

1. 他のプログラム（Arduino IDE等）が1401のポートを使用していないか確認
2. USBケーブルを再接続
3. ESP32をリセット

## 📂 ファイル構造

```
ESP-NOW_dataexchange/
├── data/
│   └── my_data_text.json    ← このファイルがアップロードされる
├── platformio.ini            ← 1401の設定あり
└── upload_1401.sh            ← アップロードスクリプト
```

## ⚠️ 重要な注意

- **LittleFSアップロードのみ**: このスクリプトはJSONファイルのみをアップロード
- **プログラム書き込み**: Arduino IDEで別途実行してください
- **書き込み順序**: LittleFS → プログラムの順で実行

## 🎨 JSONファイルの編集

`data/my_data_text.json` を編集後、再度 `./upload_1401.sh` を実行するだけでOK

### 画像形式
```json
{
  "id": "p001",
  "flag": "photo",
  "rgb": [124,150,157, 171,191,192, ...]
}
```

### テキスト形式
```json
{
  "id": "t001",
  "flag": "text",
  "text": "HELLO",
  "brightness": 20
}
```

## 📞 問題が解決しない場合

シリアル番号を確認:
```bash
pio device list | grep 1401 -A 3
```

出力例:
```
/dev/cu.usbmodem1401
Hardware ID: USB VID:PID=303A:1001 SER=94:A9:90:39:28:54
```
