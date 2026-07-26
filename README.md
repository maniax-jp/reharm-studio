# Reharm Studio

音楽理論に基づいたコード進行を生成し、VST3プラグインで音を確認できるmacOSデスクトップアプリケーション。

![Reharm Studio](docs/screenshot.png)

## 機能

### ステップシーケンサー

- 最大 8 小節のコード進行を表示・編集・再生
- 1 小節 = 基本 1 コード。小節ごとに **1 / 2 / 4 分割** を切替可能
- セルをクリックしてコードエディタで編集: ルート 12 音 / オンコード /
  23 種のコードクオリティを 2 和音・3 和音・6th・7th のグループで選択
  （5 / maj, m, sus2, sus4, aug, dim / 6, m6, 6sus4 /
  7, M7, m7, mM7, 7sus2, 7sus4, M7sus2, M7sus4, m7b5, dim7, aug7, augM7, blk）
- **OPT** ボタンで omit（omit3 / omit5 / omit7）・5 度変化（b5 / #5）・
  テンション（b9, 9, #9, 11, #11, b13, 13）を付加。衝突しない組み合わせは複数選択でき、
  音楽理論上不可能な組み合わせは選択不可
- コードネームは通称表記を優先（Fadd9, F9, C13, CM11 など。通称がないものは
  C7(b9,13) や C(omit5) の形式）し、ディグリーネームを常に併記
- **クローズ / オープン / 自動ボイシング** の切替（自動は下記）
- 再生中のセルをリアルタイムハイライト
- セルの **コピー / ペースト / 削除**(Cmd+C / Cmd+V / Delete)と
  **Undo / Redo**(Cmd+Z / Shift+Cmd+Z)
- セルを **ドラッグで移動**、Option+ドラッグで複製

### 代表進行プリセット

王道進行・カノン進行・ポップパンク進行・小室進行・丸サ進行・枯葉進行・
Fly Me to the Moon・コンファメーション進行・オンリーワン進行・落ち葉進行・
シャンゼリゼ進行を現在のキーに移調して一括適用。

進行によっては1小節に2コードを含むもの（コンファメーションの `| Bm7-5 E7 |` など）や、
オンコードを含むもの（落ち葉の `Em/B`、シャンゼリゼの `E7/B` など）がある。
ハーモニー解析での進行検出は転回形を問わず一致し、枠によっては代替形も認識する
（オンリーワンの `E` / `E7` / `A♭dim`、シャンゼリゼ2小節目のラインクリシェなど）。

ポップパンク進行はループのどこから始めても検出する
（Ⅰ-Ⅴ-Ⅵm-Ⅳ / Ⅴ-Ⅵm-Ⅳ-Ⅰ / Ⅵm-Ⅳ-Ⅰ-Ⅴ / Ⅳ-Ⅰ-Ⅴ-Ⅵm）。
一括適用とプリセット選択欄は正規形の Ⅰ-Ⅴ-Ⅵm-Ⅳ のみ。

### ハーモニー解析 (HarmonyAnalyzer)

- パターン検出: ツーファイブワン・強進行・限定進行・クリシェ・代表進行との一致
- ノンダイアトニックコードの技法分類をセル上にバッジ表示:
  セカンダリードミナント / ドッペルドミナント / 裏コード /
  サブドミナントマイナー / モーダルインターチェンジ /
  パッシングディミニッシュ / ピカルディ終止 / リレイテッドツーマイナー
- コードの置き換え候補（代理コード・裏コード・ドミナント化）をワンクリックで適用

### 自動ボイシング（スムーズボイスリーディング）

- **自動** を選ぶと、前のコードとの共通音を保持し各声部の移動が最小になる
  転回形・オクターブ配置を自動で選択して、滑らかに繋がる響きで再生する
- 転回形を明示したい場合は、コードエディタで **オンコード表記**（スラッシュコード）の
  ベース音を指定する。指定したベース音は常に最低音として鳴る
- ブロック再生時に適用される。アルペジオ再生では選択肢に現れない
- 声部が **短2度（半音）で隣接する配置を避ける**。ただし Csus4(b5) や C6(#5) のように
  基本のクローズボイシング自体が半音を含むコードは、その響きがコード固有のものなので対象外
- 閉じた転回形に加えて **ドロップ配置**（drop2 / drop3 / drop2&4）も候補にするため、
  CM7 を B E G C のように半音を避けた形で転回できる。広がりすぎる配置は抑制される

### アルペジオ再生

- ブロック再生に加え、アルペジオ(上行 / 下行 / 上下)で再生可能
- レートは 1/4・1/8・1/16 から選択
- ボイシングはクローズ / オープンから選択（自動はブロック再生専用）

### 自動保存とユーザープリセット

- 進行・キー・BPM・音量・ボイシング・アルペジオ設定・プラグイン(音色状態含む)・
  MIDI 入力デバイス・最近読み込んだプラグインを自動保存し、次回起動時に自動復元。
  ファイル操作は不要
- ヘッダーの「保存」ボタンで現在の進行に名前を付けて保存。
  プリセット選択欄の User セクションからいつでも呼び出せる

### MIDI 書き出し(ドラッグ&ドロップ)

- トランスポートの MIDI バッジを DAW へドラッグ&ドロップすると、
  現在の進行(ボイシング / アルペジオ設定を反映)を SMF として持ち出せる

### VST3 ホスト

- VST3 インストゥルメントプラグインの音色でコード進行を再生
- トランスポート右端のボタン1つで完結。未ロード時は **Load VST3**、
  ロード後はプラグイン名を表示し、クリックで GUI エディタを開く
- 同ボタンを右クリックすると「プラグインを読み込む...」「内蔵音源」に加え、
  過去に読み込んだプラグインを新しい順に最大5件表示し、選び直せる

### MIDI 入力

- 接続した MIDI キーボードの演奏を、ロード中の VST3
  （未ロード時は内蔵音源）でそのまま鳴らせる
- 入力デバイスの選択は設定画面から。ヘッダー右端の歯車ボタン、または
  アプリケーションメニューの「設定」で開く
- 選択したデバイスは自動保存され、次回起動時に開き直す。
  取り外している間は選択がグレー表示で残り、挿し直すと自動で復帰する

## 使用方法

1. アプリケーションを起動する
2. **Load VST3** ボタンで VST3 プラグイン（`.vst3`）を選択する
   （右クリックで過去に読み込んだプラグインからも選べる）
3. プラグイン名になったボタンをクリックすると、プラグインの GUI エディタが開く
4. ヘッダーでキー / スケール / プリセット進行を選択する
5. セルをクリックしてコードを編集する（**OPT** ボタンで omit / テンションを追加）
6. 丸い再生ボタンで再生 / 停止（再生スタイルでアルペジオを選択可能）
7. **保存** ボタンで進行に名前を付けて保存し、プリセット欄の User から呼び出す
8. **MIDI** バッジを DAW へドラッグ&ドロップして進行を書き出す

作業状態は自動保存され、次回起動時にプラグインの音色ごと復元されます。

## トラブルシューティング

### 起動時にセキュリティ警告が出る場合
「"Reharm Studio.app"は開いていません Appleは,"Reharm Studio.app"にMacに損害を与えたり、プライバシーを侵害する可能性のあるマルウェアが含まれていないことを検証できませんでした」などの警告が表示されることがあります。これはアプリが Apple による有料の署名を受けていないために発生します。

**対処方法 (macOS 15 Sequoia 以降):**
1. アプリをダブルクリックして起動し、警告ダイアログが表示されたら「完了」または「キャンセル」を押して閉じます。
2. **「システム設定」** → **「プライバシーとセキュリティ」** を開きます。
3. 右側の画面を一番下までスクロールし、「セキュリティ」セクションにある **「"Reharm Studio.app" がブロックされました」** の横の **「このまま開く」** ボタンをクリックします。
4. 確認ダイアログで再度 **「このまま開く」** をクリックし、パスワードまたは Touch ID で認証してください。

一度この操作を行えば、次回以降は通常通りに起動できます。

### 「アプリは壊れているため開けません」と表示される場合
「"Reharm Studio.app"は壊れているため開けません」と表示される場合は、以下のコマンドをターミナルで実行してください：

```bash
xattr -d com.apple.quarantine /Applications/Reharm\ Studio.app
```

## プロジェクト構造

```
.
├── Source/                          # 本体アプリケーションのソースコード
│   ├── Main.cpp                     # エントリポイント
│   ├── MainComponent.*              # 画面全体の統合・プラグイン読み込み・再生制御
│   ├── AudioEngine.*                # リアルタイム音声処理・可変長スロットのMIDIスケジューリング・VST3 ホスト・HostPlayHead
│   ├── ChordModel.*                 # コード理論コア（23種のコードクオリティ・omit/addオプション・命名・ディグリー・ボイシング）
│   ├── VoiceLeading.*               # 自動ボイシング（共通音保持・最小移動の転回形選択）
│   ├── PlayModeSelection.h          # 再生モード選択の純粋表現（メニューID・ラベル・正規化）
│   ├── ProgressionModel.*           # 8小節×1/2/4分割のシーケンサーモデル・代表進行プリセット
│   ├── HarmonyAnalyzer.*            # パターン検出・ノンダイアトニック分類・置換候補
│   ├── PlaybackSettings.h / PlaybackBuilder.*  # 再生データ構築（アルペジオのマイクロスロット展開）
│   ├── StateStore.*                 # 状態のJSON保存/復元（セッション自動保存・ユーザープリセット）
│   ├── UndoHistory.h                # スナップショット方式の Undo/Redo
│   ├── MidiExporter.*               # SMF 書き出し（MIDI ドラッグ&ドロップ）
│   ├── MidiInputRouter.*            # MIDI 入力デバイスの開閉とオーディオコールバックへの受け渡し
│   ├── RecentPlugins.*              # 最近読み込んだプラグインのリスト（重複解消・上限5件）
│   ├── Theme.*                      # Studio Noir デザインシステム（LookAndFeel・パレット）
│   ├── HeaderBar.* / SequencerView.* / AnalysisStrip.* / ChordEditorPanel.* / TransportBar.* / SettingsPanel.*  # UIコンポーネント
│   └── Localization.h               # UI日本語表示（UTF-8安全な変換層）
├── Tests/                           # 単体テスト
│   ├── TestRunner.cpp               # テストランナー
│   ├── AudioEngineTest.cpp          # AudioEngine の動作テスト（可変長スロット含む）
│   ├── ChordModelTest.cpp           # コード理論コア・ボイシングのテスト
│   ├── ProgressionModelTest.cpp     # シーケンサーモデル・プリセットのテスト
│   ├── HarmonyAnalyzerTest.cpp      # ハーモニー解析のテスト
│   ├── StateStoreTest.cpp           # 状態保存/復元のテスト
│   ├── UndoHistoryTest.cpp          # Undo/Redo のテスト
│   ├── PlaybackBuilderTest.cpp      # 再生データ構築・アルペジオ展開のテスト
│   ├── VoiceLeadingTest.cpp         # 自動ボイシングのテスト
│   ├── PlayModeSelectionTest.cpp    # 再生モード選択の表示と内部状態の一致のテスト
│   ├── MidiExporterTest.cpp         # SMF 書き出しのテスト
│   ├── MidiInputRouterTest.cpp      # 保存した MIDI 入力デバイスの解決のテスト
│   ├── RecentPluginsTest.cpp        # 最近読み込んだプラグインのリストのテスト
│   └── TimingTest.cpp               # タイミング計算テスト
├── Experiments/                     # 実験的なプログラム（仕様確認・動作検証用）
│   ├── PluginLoadTest.cpp           # VST3 プラグインのチャンネル構成検証（VPS Avenger 用）
│   └── PresetSwitchTest.cpp         # プリセット切替・プレイヘッド・エディタ表示の統合テスト
├── docs/                            # ドキュメント用アセット（スクリーンショット等）
├── CMakeLists.txt
└── CMakePresets.json
```

## ビルド方法

### 標準手順（推奨）: make

正規のビルドディレクトリは `build/` のみです。迷ったら `make` を実行すると使い方が表示されます。

```bash
make                        # 使い方を表示
make app                    # アプリ本体をビルド (build/, Debug)
make test                   # テストを build/ でビルド・実行（正規）
make release-check          # リリース前の最終テスト（唯一の正式検証）
make agent-test AGENT=codex # エージェント用サンドボックス (.agent-builds/codex)
```

`build/` 以外（エージェント用サンドボックス等）で実行したテストの出力には
`[AGENT SANDBOX BUILD]` の印が自動で付き、リリース検証としてはカウントされません。

### 前提条件

- macOS
- Xcode (Command Line Tools)
- CMake 3.15 以上
- Homebrew（CMake インストール用）

### ビルドターゲット

| ターゲット | オプション | 説明 |
|---|---|---|
| ReharmStudio | 常にビルド | メインアプリケーション |
| ReharmStudioTests | `BUILD_TESTS=ON` | テストスイート |
| PluginLoadTest | `BUILD_EXPERIMENTS=ON` | VST3 チャンネル構成検証プログラム |
| PresetSwitchTest | `BUILD_EXPERIMENTS=ON` | プリセット切替統合テストプログラム |

### ビルドモード

| モード | `CMAKE_BUILD_TYPE` | 用途 |
|---|---|---|
| Debug（デフォルト） | `Debug` | 開発・デバッグ（`REHARM_DEBUG_LOG` 有効） |
| Release | `Release` | リリース前の最終確認 |

### CMake Presets によるビルド（推奨）

```bash
# リポジトリをクローン
git clone https://github.com/maniax-jp/reharm-studio.git
cd reharm-studio

# 本体のDebugビルド（デフォルト）
# ※ JUCE 8.0.14 は CMake 構成時に FetchContent で自動取得されます
cmake --preset default
cmake --build --preset default
```

ビルド成果物（Debug）:

```
build/ReharmStudio_artefacts/Debug/Reharm Studio.app
```

### 利用可能なプリセット

| プリセット名 | 内容 |
|---|---|
| `default` | 本体のみ（Debug） |
| `debug-tests` | 本体 + テスト（Debug） |
| `debug-experiments` | 本体 + 実験プログラム（Debug） |
| `debug-all` | 全てをビルド（Debug） |
| `release` | 本体のみ（Release / arm64+x86_64 ユニバーサル） |
| `release-all` | 全てをビルド（Release） |
| `agent-codex` | エージェント用サンドボックス（`.agent-builds/codex`、リリース検証には使えない） |
| `agent-grok` | エージェント用サンドボックス（`.agent-builds/grok`、リリース検証には使えない） |

```bash
# テスト付きDebugビルド
cmake --preset debug-tests
cmake --build --preset debug-tests

# Releaseビルド（リリース前の最終確認用）
cmake --preset release
cmake --build --preset release
```

### プリセットを使わない場合

```bash
# 通常のDebugビルド（本体のみ）
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 8

# テストを有効にしてビルド
cmake -B build -DBUILD_TESTS=ON
cmake --build build --parallel 8

# 実験プログラムを有効にしてビルド
cmake -B build -DBUILD_EXPERIMENTS=ON
cmake --build build --target PluginLoadTest
cmake --build build --target PresetSwitchTest
```

### テストの実行

`make test` が構成・ビルド・実行までを一括で行います（推奨）。手動で実行する場合は
コンソールバイナリを使います:

```bash
./build/ReharmStudioTests_artefacts/Debug/ReharmStudioTests
```

リリース前の最終テストは必ず `make release-check`（= `build/` での実行）で行ってください。
サンドボックス（`.agent-builds/`）でのテスト結果は出力に `[AGENT SANDBOX BUILD]` の印が付き、
最終検証としては無効です。

## 技術仕様

- フレームワーク: JUCE 8.0.14
- 言語: C++17
- プラグイン形式: VST3
- プラットフォーム: macOS
- ビルドシステム: CMake 3.15+

## ライセンス

このプロジェクトは [GNU Affero General Public License v3.0 (AGPLv3)](https://www.gnu.org/licenses/agpl-3.0.html) の下で公開されています。

詳細は [LICENSE](./LICENSE) ファイルを参照してください。
