#!/bin/bash
# PlatformIO LittleFS アップロードスクリプト

echo "=========================================="
echo "PlatformIO LittleFS Upload Script"
echo "=========================================="
echo ""

# プロジェクトディレクトリに移動
cd "$(dirname "$0")"

# dataフォルダの確認
if [ ! -d "data" ]; then
    echo "❌ エラー: dataフォルダが見つかりません"
    exit 1
fi

echo "📁 dataフォルダの内容:"
ls -lh data/
echo ""

# ファイルシステムイメージのビルド
echo "🔨 ファイルシステムイメージをビルド中..."
pio run --target buildfs
if [ $? -ne 0 ]; then
    echo "❌ ビルド失敗"
    exit 1
fi
echo "✅ ビルド完了"
echo ""

# ファイルシステムのアップロード
echo "⬆️  ファイルシステムをアップロード中..."
pio run --target uploadfs
if [ $? -ne 0 ]; then
    echo "❌ アップロード失敗"
    exit 1
fi
echo "✅ アップロード完了"
echo ""

echo "=========================================="
echo "✅ LittleFSアップロード成功！"
echo "=========================================="
echo ""
echo "次のステップ:"
echo "1. プログラムをアップロード: pio run --target upload"
echo "2. シリアルモニタを起動: pio device monitor"
echo ""
