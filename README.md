# epochfs

## はじめに

### これは何？

EPOCH時刻補正を行うオーバーレイFSです。  
EPOCH時間をずらしているような特殊な環境にてファイルの時間を1970年起点のEPOCH時間でアクセスします。

## 使い方



## ビルド方法

### 前提

以下のパッケージをインストールしてください。

```
libfuse-dev
pkg-config
```

### コンパイル

以下のコマンドを実行することでビルドできます。

```
gcc -Wall epochfs.c `pkg-config fuse --cflags --libs` -o epochfs
```

## 実行方法

```
./epochfs　-oepoch=1970 {ベースディレクトリ} {マウントポイント}
```

### usage

```
epochfs -o{options...} mountpoint

base_dir          オーバーレイのベースとなるディレクトリ
mountpoint        マウントポイント
-o{options...}    マウントオプション

options
    base_path={path}  オーバーレイ元のディレクトリを指定。（必須）
    epoch={year}      mountpointのEPOCHを指定する。省略した場合、現在のシステムのEPOCHを使用する。
```
