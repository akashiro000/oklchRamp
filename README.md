# oklchRamp — Maya 2025 用 OKLCH 補間 Ramp ノード

Maya 標準の `ramp` と同じグラデーションエディタ UI を持ちながら、色の補間を
**OKLCH（または OKLab）色空間**で行うテクスチャノードです。
RGB 直線補間で起きる「中間が濁る／暗くなる」問題を避け、知覚的に均一なグラデーションを作れます。

```
module/
  oklchRamp.mod                  … Maya モジュール定義
  plug-ins/oklchRamp.mll         … ビルド成果物
  arnold/oklchRampTranslator.dll … MtoA（Arnold）トランスレータ
  scripts/AEoklchRampTemplate.mel… アトリビュートエディタ
  scripts/oklchRamp.mel          … 作成ヘルパー (oklchRampCreate / oklchRampSetColors)
src/                             … C++ ソース（src/arnold はトランスレータ）
CMakeLists.txt / build.ps1       … ビルド
```

## ビルド

要件: Maya 2025（ヘッダ・lib 同梱）、Visual Studio 2022 (BuildTools 可)、CMake 3.20+
任意: MtoA（Arnold for Maya）。`C:\Program Files\Autodesk\Arnold\Maya2025` にあれば Arnold トランスレータも一緒にビルドされます
（別の場所なら `-DMTOA_LOCATION=...`）。

```powershell
.\build.ps1            # module\plug-ins\oklchRamp.mll を生成
.\build.ps1 -Install   # さらに Documents\maya\2025\modules\oklchRamp.mod を書き出し
```

Maya の既定パスは `C:\Program Files\Autodesk\Maya2025` です。別の場所にある場合は
`-MayaLocation "D:\Autodesk\Maya2025"` を指定するか、環境変数 `MAYA_LOCATION` を設定してください
（MtoA も同様に `-MtoaLocation` / `MTOA_LOCATION`）。

## インストール

`build.ps1 -Install` を使わない場合は、次のいずれか。

- 環境変数 `MAYA_MODULE_PATH` に `<このフォルダ>\module` を追加
- または `module\oklchRamp.mod` を `Documents\maya\2025\modules\` にコピーし、末尾の `.` を
  `module` フォルダの絶対パスに書き換え

Maya 起動後:

```mel
loadPlugin oklchRamp;
string $ramp = oklchRampCreate();   // oklchRamp + place2dTexture を作成して接続
```

Hypershade の Create タブ（2D Textures）にも `oklchRamp` が現れます。

## ノードのアトリビュート

| アトリビュート | 内容 |
|---|---|
| `colorEntryList` | Maya 標準と同じ Ramp 構造（Position / Color / Interp）。`AEaddRampControl` でそのまま編集可 |
| `inputMode` | UV Coordinates（place2dTexture 駆動）／ Input Value（`inputValue` を直接使用） |
| `rampType` | V / U / Diagonal / Radial / Circular / Box |
| `inputValue` | Input Value モードのときの位置（0〜1） |
| `interpolationSpace` | OKLCH（色相を角度で補間）／ OKLab（直線補間） |
| `hueInterpolation` | Shorter / Longer / Increasing / Decreasing（CSS `color-mix` と同じ意味） |
| `gamutMapping` | None / Clip / Reduce Chroma（既定。明度と色相を保ったまま彩度を落として色域内へ） |
| `inputColorSpace` | Linear（既定。Maya のレンダリング空間）／ sRGB（エントリ値を sRGB 表示値として補間・出力する。Maya 自身のスウォッチやレンダリングは設定に関係なく格納値をシーンリニアとして扱うので、通常は Linear のままにする） |
| `outColor` / `outAlpha` | 出力色 / OKLab 明度 L |

各エントリの Interp（None / Linear / Smooth / Spline）も OKLCH 空間上で適用されます。
無彩色（黒・白・灰）のエントリは隣の有彩色の色相を引き継ぐので、赤→白でピンクに濁ることなく補間されます。

## コマンド

```mel
// 均等に N サンプル（r g b r g b …）。-srgb で表示用 sRGB に変換
float $c[] = `oklchRampSample -node oklchRamp1 -count 64 -srgb`;
// 特定位置
float $c[] = `oklchRampSample -node oklchRamp1 -position 0.5`;
```

アトリビュートエディタの「OKLCH Preview」帯はこのコマンドで描画し、Maya の表示変換（`colorManagementConvert`）を通しているので
標準スウォッチと同じ見え方になります（上のグラデーションエディタは Maya 内蔵の RGB 補間プレビューなので、補間の中間色は異なります）。
プレビュー帯をクリックすると、その位置にその色のエントリが追加されます（Ctrl+Z で取り消し可能）。
補間タイプは直前のエントリのものを引き継ぎます。
Ramp や補間設定を編集すると自動で再描画されます（グラデーションエディタでのドラッグ中も追従）。

```mel
// 内部で使っている監視コマンド。Ramp の内容・補間設定が変わるたびに <mel> を idle 時に 1 回実行
int $id = `oklchRampWatch -node oklchRamp1 -command "<mel>"`;
oklchRampWatch -remove $id;
```
`scriptJob -attributeChange` は colorEntryList の子アトリビュートの変更で発火しないため、
プラグイン側の `MNodeMessage` コールバックで代替しています。

## Viewport 2.0

`MPxShadingNodeOverride`（[OklchRampOverride.cpp](src/OklchRampOverride.cpp)）で対応しています。
ノードの OKLCH 補間結果を 256×1 の float テクスチャに焼き込み、カスタムフラグメント
（[OklchRampFragments.h](src/OklchRampFragments.h)、HLSL / GLSL / Cg）が UV から Ramp 位置を計算して
サンプリングします。Ramp やパラメータを編集すると自動的に再ベイクされます。
`outColor` と `outAlpha`（OKLab 明度）の両方をビューポートに供給します。

## ビルド時の補足

ビルド成果物は `build\Release\oklchRamp.mll` に生成され、POST_BUILD で `module\plug-ins` にコピーされます。
Maya がプラグインを読み込んだままでも、古い `.mll` を `.mll.old` にリネームしてからコピーするので
ビルドは失敗しません（`.old` は次回ビルド時に削除されます）。Maya 側では `unloadPlugin oklchRamp; loadPlugin oklchRamp;`
で新しいバイナリに差し替わります。

## Arnold（MtoA）

[OklchRampTranslator.cpp](src/arnold/OklchRampTranslator.cpp) が MtoA の拡張として `oklchRamp` を翻訳します。
Arnold 側にカスタムシェーダは不要で、OKLCH 補間結果を 256 キーの Arnold 標準 `ramp_rgb` に焼き込みます。
モジュール定義が `MTOA_EXTENSIONS_PATH` に `modulernold` を追加するので、モジュール経由でインストールしていれば
追加設定なしで Arnold レンダー・IPR・.ass 書き出しで使えます。

| Maya 側 | Arnold 側 |
|---|---|
| `colorEntryList` ＋ 補間設定 | `ramp_rgb`（256 キー、キー間は線形） |
| `rampType` V / U / Diagonal / Radial / Circular / Box | `ramp_rgb.type` v / u / diagonal / radial / circular / box（Arnold 組み込みのマッピング） |
| `inputMode` = Input Value | `type = custom`、`input` ← `inputValue`（接続もそのまま翻訳） |
| `uvCoord` に接続された place2dTexture | `uv_transform`（coverage / translateFrame / rotateFrame / mirror / wrap / stagger / repeatUV / offset / rotateUV / noiseUV） |

注意:
- `outAlpha` の接続は MtoA が色出力に付け替えるため、Arnold では RGB→float の変換値になります（OKLab 明度ではありません）。
- Radial などの形状は Arnold 組み込みの定義を使うので、Maya ソフトウェア／ビューポートと開始角度などがわずかに異なることがあります。
- MtoA のバージョンに依存します（ビルド時 5.4.5 / Arnold 7.3.4.1）。MtoA を更新したら再ビルドしてください。

## ビルド済みバイナリ

ビルド環境が無い場合は [Releases](../../releases) の zip（`module` フォルダ一式）を展開し、
「インストール」の手順で `oklchRamp.mod` を登録してください。Maya 2025 / Windows x64 用です。

## 注意

- ノード ID は Autodesk 登録済みブロック `0x00142C40`〜`0x00142C7F` のうち `0x00142C40` を使用しています。
  フォークして別ノードを追加する場合は、ご自身の登録ブロックの ID に変更してください。

## ライセンス

MIT License — [LICENSE](LICENSE)
