# Reharm Studio

音楽理論に基づいたコード進行を生成し、VST3プラグインで音を確認できるmacOSデスクトップアプリケーション。

## 機能

- キー（C, C#, D, ...）とスケール（Major/Minor）の選択
- 基本的なコード進行生成（I-IV-V-I）
- 音楽理論的な置き換え（例: V → vii°）
- VST3インストゥルメントプラグインのホスト
- 生成した進行のMIDI再生

## 使用方法

1. アプリケーションを起動
2. "Load VST3"ボタンでVST3プラグインを選択
3. キーとスケールを選択
4. 生成された進行を確認
5. "Play Progression"で再生

## トラブルシューティング

### 起動時にセキュリティ警告が出る場合
「“Reharm Studio.app”は開いていません Appleは、“Reharm Studio.app”にMacに損害を与えたり、プライバシーを侵害する可能性のあるマルウェアが含まれていないことを検証できませんでした」などの警告が表示されることがあります。これはアプリが Apple による有料の署名を受けていないために発生します。

**対処方法 (macOS 15 Sequoia 以降):**
1. アプリをダブルクリックして起動し、警告ダイアログが表示されたら「完了」または「キャンセル」を押して閉じます。
2. **「システム設定」** $\rightarrow$ **「プライバシーとセキュリティ」** を開きます。
3. 右側の画面を一番下までスクロールし、「セキュリティ」セクションにある **「"Reharm Studio.app" がブロックされました」** の横の **「このまま開く」** ボタンをクリックします。
4. 確認ダイアログで再度 **「このまま開く」** をクリックし、パスワードまたは Touch ID で認証してください。

一度この操作を行えば、次回以降は通常通りに起動できます。

### 「アプリは壊れているため開けません」と表示される場合
「“Reharm Studio.app”は壊れているため開けません」と表示される場合は、以下のコマンドをターミナルで実行してください：

```bash
xattr -d com.apple.quarantine /Applications/Reharm\ Studio.app
```

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

### テストのビルドと実行

テストをビルドして実行するには、`BUILD_TESTS` オプションを有効にして CMake を実行してください。

```bash
# ビルドディレクトリに移動
cd build

# テストを有効にして構成
cmake -DBUILD_TESTS=ON ..

# テストターゲットをビルド
cmake --build . --target ReharmStudioTests

# テストを実行
"./ReharmStudioTests_artefacts/Reharm Studio Tests.app/Contents/MacOS/Reharm Studio Tests"
```


## 技術仕様

- フレームワーク: JUCE
- 言語: C++17
- プラグイン形式: VST3
- プラットフォーム: macOS

## ライセンス

このプロジェクトは [GNU Affero General Public License v3.0 (AGPLv3)](https://www.gnu.org/licenses/agpl-3.0.html) の下で公開されています。

詳細は [LICENSE](./LICENSE) ファイルを参照してください。
