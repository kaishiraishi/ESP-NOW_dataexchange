#!/bin/bash
# 1401デバイス専用 LittleFSアップロードスクリプト

echo "=========================================="
echo "📱 ESP32 1401 - LittleFS Upload"
echo "=========================================="
echo ""

# プロジェクトディレクトリに移動
cd "$(dirname "$0")"

# デバイスの確認
if [ ! -e "/dev/cu.usbmodem1401" ]; then
    echo "❌ エラー: /dev/cu.usbmodem1401 が見つかりません"
    echo ""
    echo "接続されているデバイス:"
    pio device list | grep -A 3 "cu.usbmodem"
    exit 1
fi
echo "✅ デバイス検出: /dev/cu.usbmodem1401"
echo ""

# dataフォルダの確認
if [ ! -d "data" ]; then
    echo "❌ エラー: dataフォルダが見つかりません"
    exit 1
fi

echo "📁 dataフォルダの内容:"
ls -lh data/
echo ""

# JSONファイルの内容確認
if [ -f "data/my_data_text.json" ]; then
    echo "📄 my_data_text.json の内容:"
    echo "---"
    cat data/my_data_text.json | head -10
    echo "---"
    echo ""
fi

# 確認プロンプト
read -p "このファイルを 1401 にアップロードしますか? (y/N): " confirm
if [ "$confirm" != "y" ] && [ "$confirm" != "Y" ]; then
    echo "キャンセルしました"
    exit 0
fi
echo ""

# ファイルシステムイメージのビルド
echo "🔨 ファイルシステムイメージをビルド中..."
pio run -e esp32_1401 --target buildfs
if [ $? -ne 0 ]; then
    echo "❌ ビルド失敗"
    exit 1
fi
echo "✅ ビルド完了"
echo ""

# ファイルシステムのアップロード
echo "⬆️  1401 へファイルシステムをアップロード中..."
pio run -e esp32_1401 --target uploadfs
if [ $? -ne 0 ]; then
    echo "❌ アップロード失敗"
    exit 1
fi
echo "✅ アップロード完了"
echo ""

echo "=========================================="
echo "✅ 1401 への LittleFSアップロード成功！"
echo "=========================================="
echo ""
echo "次のステップ:"
echo "1. Arduino IDEでプログラムをアップロード"
echo "2. シリアルモニタで確認: pio device monitor -e esp32_1401"
echo ""
echo "動作確認コマンド:"
echo "  pio device monitor -e esp32_1401"
echo ""
