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
git clone <repository-url>
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

## ライセンス

[ライセンス情報を記載]