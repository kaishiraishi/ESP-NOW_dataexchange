LittleFS アップロード手順
============================

このフォルダ内のファイルをESP32のLittleFSファイルシステムにアップロードします。

【必要なツール】
Arduino IDE用のLittleFSアップロードプラグイン:
https://github.com/earlephilhower/arduino-esp32littlefs-plugin


【インストール方法】
1. 上記リンクから最新版の.zipをダウンロード
2. Arduino IDEのsketchbookフォルダ内に tools フォルダを作成
   (通常: ~/Documents/Arduino/tools/)
3. ダウンロードしたzipを展開して tools フォルダ内に配置
4. Arduino IDEを再起動


【アップロード方法】
1. Arduino IDEで本プロジェクト(ESP-NOW_dataexchange.ino)を開く
2. ESP32ボードを選択
3. メニューから [ツール] → [ESP32 Sketch Data Upload] を選択
4. アップロード完了まで待機（数十秒～1分程度）
5. 完了後、通常通りスケッチをアップロード


【ファイル説明】
- my_data_text.json : LEDマトリクス表示用の画像データ
  - id: 識別子
  - flag: フラグ情報
  - brightness: LED輝度（0-255）
  - rgb: 8×8ピクセル分のRGBデータ（192個の値）


【JSONカスタマイズ】
rgb配列は以下の順序で記述:
- 左上から右へ、次の行へと順番に記述
- 各ピクセルはR,G,Bの3つの値（0-255）
- 合計: 8×8×3 = 192個の値が必要

例: 最初のピクセル(0,0)を赤にする場合
  "rgb": [255, 0, 0, ...]


【確認方法】
シリアルモニタで以下のメッセージを確認:
  [LittleFS] loaded XXX bytes

エラーが出る場合:
  [LittleFS] /my_data_text.json not found. using default

→ アップロードが正しく完了していない可能性があります
