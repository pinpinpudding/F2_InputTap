# F2_InputTap_vst3

入力デバイス直結VST3プラグイン（Windows 64bit）

**[ダウンロード（F2_InputTap_vst3-v1.1.0-Windows-x64.zip）](../../releases/latest/download/F2_InputTap_vst3-v1.1.0-Windows-x64.zip)**

## 概要

- マイクやオーディオインターフェースなどの「入力デバイス」の音を、そのままプラグインの出力に流すVST3エフェクトです。
- DAWで指定しているデバイスとは別に、プラグイン自身が入力デバイスを開きます。
- WASAPI共有モードで開くため、他アプリと干渉せずDAW内で入力を複数設定することができます。
- 入力デバイスとDAWのクロックのずれは自動で補正します。

## 動作環境

- Windows 10 / 11（64bit）
- VST3に対応したDAW

## インストール

1. 「F2_InputTap.vst3」をフォルダごと次の場所に直接コピーします。
   `C:\Program Files\Common Files\VST3\`
2. DAW でプラグインを再スキャンします。
3. ミキサーのエフェクトスロットに「F2_InputTap_vst3」を挿します。

## アンインストール

`F2_InputTap.vst3` を直接削除してください。

## 画面の説明

| 項目 | 説明 |
|---|---|
| Input device | 入力デバイスの選択。プロジェクトに保存されます。 |
| Rescan | 入力デバイス一覧の更新。 |
| Reconnect | 入力デバイスを開き直します。 |
| Gain | 入力デバイスの音量です。 |
| Buffer | 入力デバイスの遅延です。音がプツプツ途切れるときは大きくしてください。 |
| Pan | 入力デバイスの定位です。入力デバイスの音にだけ効きます。 |
| Pass through track input | オン：トラックに来ている音に、入力デバイスの音を足して出力します。<br>オフ：入力デバイスの音だけを出力します。 |
| Mute | 入力デバイスの音だけをミュートします。 |

| ステータス表示 | 説明 |
|---|---|
| Running / Stopped | 入力デバイスのインジケータです。 |
| Hz | 入力デバイスのサンプリングレートです。 |
| buffered | 蓄積されている音の量です。遅延量とほぼ同義です。 |
| Underruns | 音が途切れた回数です。増えるならBufferの値を上げてください。 |
| Instances on this device | 同じデバイスを使用しているプラグインの数です。 |

## 注意点

- Smart disable はオフにしてください。オンのままだと、無音のトラックでプラグインが自動停止し、マイクの音も出なくなります。
- 「Could not create device」などのエラーが出るときは、他のアプリがそのデバイスを独占していないか確認してください。

## 免責事項

本ソフトウェアは無保証です。使用によって生じたいかなる損害についても、作者は責任を負いません。

This plugin uses JUCE (https://juce.com/).
VST is a registered trademark of Steinberg Media Technologies GmbH.

---

## 開発者向け

### ビルド

GitHub に push すると、Actions（`.github/workflows/build-windows.yml`）が Windows 版をビルドし、
[Releases](../../releases) の zip を自動で更新します。

自分の PC でビルドする場合は、Visual Studio 2022（C++ によるデスクトップ開発）と CMake 3.22 以上を入れて、次を実行します。

```powershell
cmake -B build -A x64
cmake --build build --config Release
```

`build\F2_InputTap_artefacts\Release\VST3\F2_InputTap.vst3` ができます。

### ファイル構成

- `dist/readme.txt` — 配布用の説明書。zip に自動で同梱されます
- `dist/release-notes.md` — Releases ページの説明文
- `Source/InputHub.*` — 入力デバイスの管理。同じデバイスはプロセス内で 1 回だけ開いて共有します
- `Source/PluginProcessor.*` — FIFO からの読み出し、リサンプリング、クロックずれの補正
- `Source/PluginEditor.*` — 画面
