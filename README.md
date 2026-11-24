# F2_Input_Tap

マイクなどの入力デバイスの音を、そのままプラグインの出力に流す VST3 エフェクトです。
Windows 11 + FL Studio 向けに作っています。

- FL Studio が使っているオーディオデバイス（ASIO など）とは別に、プラグインが自分で入力デバイスを開きます。
- 入力は WASAPI 共有モードで開くので、FL Studio や他のアプリと同じマイクを同時に使えます。
- 複数インスタンスに対応しています。同じデバイスを選んだインスタンス同士はデバイスを 1 回だけ開いて共有し、別々のデバイスを選べばそれぞれ独立して動きます。
- デバイスと FL Studio のクロックのずれは、リサンプリングで自動補正します。

## 機能

| 項目 | 内容 |
|---|---|
| Input device | 入力デバイスの選択。選んだデバイスはプロジェクトに保存されます |
| Rescan | デバイス一覧の再取得（USB マイクを後から挿したとき） |
| Reconnect | デバイスを開き直す（抜き差しや、Windows 側の設定変更の後） |
| Gain | 入力音量（-60 〜 +24 dB） |
| Buffer | 目安となる遅延量（3 〜 200 ms）。音が途切れるときは増やします |
| Pass through track input | オンにすると、ミキサートラックに入ってきた音に入力デバイスの音を足して出力します。オフにすると入力デバイスの音だけを出力します |
| Mute | 入力デバイスの音だけをミュートします |
| Pan | 入力デバイスの音の左右定位（100L 〜 C 〜 100R） |
| Reset | Gain・Buffer・Pan の右にあるボタン。その項目を初期値に戻します |

ステータス欄には、サンプルレート、バッファ量、途切れた回数（Underruns）、このデバイスを使っているインスタンスの数が表示されます。

## ビルド方法

### A. 自分の PC でビルドする

1. 次をインストールします。
   - [Visual Studio 2022 Community](https://visualstudio.microsoft.com/ja/vs/community/)（「C++ によるデスクトップ開発」ワークロードを選択）
   - [CMake](https://cmake.org/download/) 3.22 以上（インストール時に「Add CMake to the system PATH」を選択）
   - [Git for Windows](https://git-scm.com/download/win)
2. このフォルダで PowerShell を開き、次を実行します。初回は JUCE のダウンロードで数分かかります。

   ```powershell
   cmake -B build -A x64
   cmake --build build --config Release
   ```

3. `build\F2_Input_Tap_artefacts\Release\VST3\F2_Input_Tap.vst3` ができます。

### B. GitHub でビルドする（Visual Studio 不要）

1. GitHub に新しいリポジトリを作り、このフォルダの中身をすべてアップロードします。
2. 「Actions」タブで「Build Windows VST3」が自動で動きます（約 10 分）。
3. 完了した実行結果のページ下部にある「Artifacts」から `F2_Input_Tap-v1.1.0-Windows-x64` をダウンロードして展開します。

## FL Studio への導入

1. `F2_Input_Tap.vst3` フォルダごと `C:\Program Files\Common Files\VST3\` にコピーします（管理者権限が必要です）。
2. FL Studio で「Options → Manage plugins」を開き、「Find installed plugins」を押します。
3. ミキサーのトラックのエフェクトスロットに「F2_Input_Tap」を挿し、Input device を選びます。

### FL Studio での注意点

- **Smart disable をオフにしてください。** FL Studio は無音のトラックのプラグインを自動で止めることがあり、止まるとマイクの音も出なくなります。ミキサーのプラグインスロットのメニュー、または「Tools → Macros → Switch smart disable for all plugins」で切り替えられます。
- 同じマイクを複数のトラックで使いたい場合は、各トラックに F2_Input_Tap を挿して同じデバイスを選ぶだけで構いません。
- 1 本のマイクを FL Studio の ASIO 入力と F2_Input_Tap の両方で使う構成はおすすめしません。ASIO ドライバがデバイスを排他的に掴んでいると、InputTap からは開けないことがあります。
- 遅延は「Buffer の設定値」と「FL Studio のバッファ長」の大きい方が目安になります。モニター用途で遅延を減らしたいときは、FL Studio の Audio Settings でバッファを小さくし、Buffer も下げてください。

## うまく動かないとき

| 症状 | 対処 |
|---|---|
| デバイス一覧に出てこない | Windows の「サウンドの設定 → 入力」でデバイスが有効か確認し、Rescan を押します |
| `Could not create device` / 開けない | 他のアプリがそのデバイスを排他モードで使っていないか確認します。Windows のサウンドコントロールパネルで、デバイスのプロパティ「詳細」タブにある「アプリケーションによりこのデバイスを排他的に制御できるようにする」をオフにすると直ることがあります |
| 無音 | Windows の「設定 → プライバシーとセキュリティ → マイク」で「デスクトップ アプリがマイクにアクセスできるようにする」がオンになっているか確認します |
| ときどきプツプツ途切れる（Underruns が増える） | Buffer を 40〜60 ms に上げます |
| 抜き差し後に音が出ない | Reconnect を押します |

## ファイル構成

- `dist/readme.txt` — 配布用の説明書。Actions のビルド結果に自動で同梱されます

- `Source/InputHub.*` — 入力デバイスの管理。プロセス内で共有し、同じデバイスは 1 回だけ開きます
- `Source/PluginProcessor.*` — FIFO からの読み出し、リサンプリング、クロックずれの補正
- `Source/PluginEditor.*` — 画面
