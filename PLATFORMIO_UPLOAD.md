# PlatformIO で LittleFS にファイルをアップロードする方法

## 📋 前提条件

- PlatformIO がインストールされていること
- VS Code + PlatformIO 拡張機能 または PlatformIO CLI

## 🚀 手順

### 1. プロジェクト構造の確認

```
ESP-NOW_dataexchange/
├── platformio.ini          # プロジェクト設定ファイル
├── data/                   # LittleFSにアップロードするファイル
│   ├── my_data_text.json  # あなたのJSONファイル
│   └── README.txt
├── src/                    # ソースコード（必要に応じて作成）
│   └── ESP-NOW_dataexchange.ino → main.cpp（リネーム）
├── Display_image.h
├── Display_image.cpp
├── Display_text.h
├── Display_text.cpp
├── Motion.h
└── Motion.cpp
```

**注意**: PlatformIOでは`.ino`ファイルは`src/main.cpp`に配置するのが標準です。

### 2. ファイル構造の変更（オプション）

PlatformIO標準に合わせる場合:

```bash
# srcディレクトリを作成
mkdir -p src

# .inoファイルをmain.cppにリネームしてsrcに移動
mv ESP-NOW_dataexchange.ino src/main.cpp

# または、.inoのままsrcに移動も可能（PlatformIOが自動変換）
mv ESP-NOW_dataexchange.ino src/
```

**ヘッダーファイルとソースファイル**は以下のいずれかに配置:
- `src/` フォルダ内
- `lib/` フォルダ内（カスタムライブラリとして）
- `include/` フォルダ内（ヘッダーのみ）

### 3. LittleFSファイルシステムのアップロード

#### 方法A: VS Code (PlatformIO IDE)

1. **PlatformIO サイドバー**を開く
2. **PROJECT TASKS** → **Platform** → **Build Filesystem Image** をクリック
3. **PROJECT TASKS** → **Platform** → **Upload Filesystem Image** をクリック

または、下部のタスクバーから：
- 🔨 Build Filesystem Image アイコンをクリック
- ⬆️ Upload Filesystem Image アイコンをクリック

#### 方法B: コマンドライン (PlatformIO CLI)

```bash
# プロジェクトディレクトリに移動
cd /Users/kaishiraishi/Desktop/ESP-NOW_dataexchange

# ファイルシステムイメージをビルド
pio run --target buildfs

# ファイルシステムをアップロード
pio run --target uploadfs
```

### 4. プログラムのアップロード

#### VS Code
- **PROJECT TASKS** → **General** → **Upload** をクリック

#### CLI
```bash
pio run --target upload
```

### 5. シリアルモニタで確認

#### VS Code
- **PROJECT TASKS** → **General** → **Monitor** をクリック

#### CLI
```bash
pio device monitor
```

以下のメッセージが表示されればOK:
```
[LittleFS] loaded XXX bytes
```

## 🔧 トラブルシューティング

### エラー: "data folder not found"
```bash
# dataフォルダが存在することを確認
ls -la data/
```

### エラー: "Upload failed"
- ESP32がUSBで接続されているか確認
- 他のプログラムがシリアルポートを使用していないか確認
- ボードとポート設定を確認:
  ```ini
  [env:esp32dev]
  upload_port = /dev/cu.usbserial-*  ; macOSの場合
  ```

### ファイルシステムサイズの調整

`platformio.ini`に以下を追加してパーティションサイズを変更:

```ini
board_build.partitions = default.csv
; または、カスタムパーティション
board_build.partitions = partitions.csv
```

カスタムパーティションファイル (`partitions.csv`) 例:
```csv
# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     0x9000,  0x5000
otadata,  data, ota,     0xe000,  0x2000
app0,     app,  ota_0,   0x10000, 0x140000
app1,     app,  ota_1,   0x150000,0x140000
spiffs,   data, spiffs,  0x290000,0x170000
```

## 📝 JSON ファイルフォーマット

### 画像用
```json
{
  "id": "p001",
  "flag": "photo",
  "rgb": [
    124,150,157, 171,191,192, ...
  ]
}
```

### テキスト用
```json
{
  "id": "t001",
  "flag": "text",
  "text": "HELLO WORLD",
  "brightness": 20
}
```

## 🎯 ワンライナー（全工程）

```bash
cd /Users/kaishiraishi/Desktop/ESP-NOW_dataexchange
pio run --target buildfs && pio run --target uploadfs && pio run --target upload && pio device monitor
```

これで、ビルド → LittleFSアップロード → プログラムアップロード → モニタ起動が一気に実行されます！
