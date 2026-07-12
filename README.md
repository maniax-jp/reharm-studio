# Reharm Studio

音楽理論に基づいたコード進行を生成し、VST3プラグインで音を確認できるmacOSデスクトップアプリケーション。

## 機能

- キー（C, C#, D, … B）とスケール（Major / Minor）の選択
- コード進行の自動生成（Major: `I - vi - IV - V`、Minor: `i - VI - iv - v`）
- ローマ数字コード記号から MIDI ノートへの変換（三和音）
- VST3 インストゥルメントプラグインのホスト
  - プラグイン名の表示
  - プラグイン GUI エディタの別ウィンドウ表示
  - ホスト側プレイヘッド（BPM / 拍位置 / 再生状態 / PPQ位置 / サンプルカウント）の提供
  - スレッドセーフなプラグイン読み込み（読み込み中はロード中フラグでオーディオコールバックをガードし、オーディオデバイスを閉じずに読み込み）
- 生成した進行の MIDI 再生（各コード 4 拍、ループ再生）
- 再生の停止
- 音量（Volume）とテンポ（BPM 40–240）の調整

## 使用方法

1. アプリケーションを起動する
2. **Load VST3** ボタンで VST3 プラグイン（`.vst3`）を選択する
3. プラグイン名ボタンをクリックすると、プラグインの GUI エディタが開く
4. キーとスケールを選択する（進行テキストが自動更新される）
5. 必要に応じて Volume / BPM を調整する
6. **Play Progression** で再生する（進行はループする）
7. **Stop** で再生を止める

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
│   ├── MainComponent.*              # UI・プラグイン読み込み・再生制御
│   ├── AudioEngine.*                # リアルタイム音声処理・MIDI スケジューリング・VST3 ホスト・HostPlayHead
│   ├── ChordProgressionGenerator.*  # コード進行生成
│   └── ChordTheory.*                # コード記号 → MIDI ノート変換
├── Tests/                           # 単体テスト
│   ├── TestRunner.cpp               # テストランナー
│   ├── AudioEngineTest.cpp          # AudioEngine の動作テスト
│   ├── ChordProgressionTest.cpp     # コード進行生成テスト
│   ├── ChordProgressionGeneratorTest.cpp  # 進行ジェネレータテスト
│   ├── ChordTheoryTest.cpp          # コード理論変換テスト
│   └── TimingTest.cpp               # タイミング計算テスト
├── Experiments/                     # 実験的なプログラム（仕様確認・動作検証用）
│   ├── PluginLoadTest.cpp           # VST3 プラグインのチャンネル構成検証（VPS Avenger 用）
│   └── PresetSwitchTest.cpp         # プリセット切替・プレイヘッド・エディタ表示の統合テスト
├── CMakeLists.txt
├── CMakePresets.json
└── .github/workflows/release.yml    # CI/CD（GitHub Actions による自動ビルド・リリース）
```

## ビルド方法

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
# ビルドディレクトリに移動
mkdir build
cd build

# 通常のDebugビルド（本体のみ）
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# テストを有効にしてビルド
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON ..
cmake --build . --target ReharmStudioTests

# 実験プログラムを有効にしてビルド
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_EXPERIMENTS=ON ..
cmake --build . --target PluginLoadTest
cmake --build . --target PresetSwitchTest
```

### テストの実行

```bash
# テスト付きでビルドしたあと、テストバイナリを実行
"./build/ReharmStudioTests_artefacts/Debug/Reharm Studio Tests.app/Contents/MacOS/Reharm Studio Tests"
```

## CI/CD

GitHub Actions により、`main` ブランチへの push 時に自動的にビルド・リリースが行われます。

- **ビルド**: `cmake --preset release` による Release ビルド
- **署名**: ad-hoc コード署名（`codesign --sign -`）
- **拡張属性の削除**: `xattr -cr` によるクアランティン属性のクリア
- **アーカイブ**: `.app` を ZIP 圧縮
- **リリース**: GitHub Releases に自動アップロード（タグ: `v${{ github.run_number }}`）

## 技術仕様

- フレームワーク: JUCE 8.0.14（FetchContent で取得）
- 言語: C++17
- プラグイン形式: VST3
- プラットフォーム: macOS
- ビルドシステム: CMake 3.15+
- ホストプレイヘッド: `HostPlayHead`（BPM / 拍子 / PPQ位置 / サンプルカウント / 再生状態をプラグインに提供）
- プラグイン読み込み: ロード中フラグでオーディオコールバックをガードし、オーディオデバイスを閉じずに読み込む設計

## ライセンス

このプロジェクトは [GNU Affero General Public License v3.0 (AGPLv3)](https://www.gnu.org/licenses/agpl-3.0.html) の下で公開されています。

詳細は [LICENSE](./LICENSE) ファイルを参照してください。
