# Reharm Studio

音楽理論に基づいたコード進行を生成し、VST3プラグインで音を確認できるmacOSデスクトップアプリケーション。

## 機能

- キー（C, C#, D, ...）とスケール（Major/Minor）の選択
- 基本的なコード進行生成（I-IV-V-I）
- 音楽理論的な置き換え（例: V → vii°）
- VST3インストゥルメントプラグインのホスト
- 生成した進行のMIDI再生

## ビルド方法

### 前提条件

- macOS
- Xcode (Command Line Tools)
- CMake
- Homebrew (CMakeインストール用)

### 手順

1. リポジトリをクローン
2. JUCEをサブモジュールとして取得
3. CMakeでビルド

```bash
git clone https://github.com/maniax-jp/reharm-studio.git
cd reharm-studio
git submodule update --init --recursive
mkdir build
cd build
cmake ..
make
```

## 使用方法

1. アプリケーションを起動
2. "Load VST3"ボタンでVST3プラグインを選択
3. キーとスケールを選択
4. 生成された進行を確認
5. "Play Progression"で再生

## 技術仕様

- フレームワーク: JUCE
- 言語: C++17
- プラグイン形式: VST3
- プラットフォーム: macOS

## トラブルシューティング

リリース版を起動した際に「“Reharm Studio.app”は壊れているため開けません。 ゴミ箱に入れる必要があります。」と表示される場合は、macOSのセキュリティ機能（Gatekeeper）による制限です。

以下のコマンドをターミナルで実行することで解決できます：

```bash
xattr -d com.apple.quarantine /Applications/Reharm\ Studio.app
```

## ライセンス

このプロジェクトは [GNU Affero General Public License v3.0 (AGPLv3)](https://www.gnu.org/licenses/agpl-3.0.html) の下で公開されています。

詳細は [LICENSE](./LICENSE) ファイルを参照してください。