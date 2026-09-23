# CLAUDE.md — oklchRamp

Maya 2025 用のカスタム Ramp テクスチャノード。色の補間を OKLCH / OKLab 色空間で行う。
C++（Maya API）＋ MEL、Viewport 2.0 と Arnold（MtoA）に対応。公開リポジトリ: https://github.com/akashiro000/oklchRamp（MIT）。

## レイアウト

```
src/
  OklchColor.h            OKLab/OKLCH 変換・sRGB 伝達関数・色域マッピング（純粋な数学、Maya 非依存）
  OklchRampEval.h         PreparedRamp: エントリ→補間→RGB。ノード / コマンド / VP2 / Arnold が共有
  OklchRampNode.*         oklchRamp ノード本体（compute、readRamp、アトリビュート定義、ノード ID）
  OklchRampSampleCmd.*    oklchRampSample コマンド（AE プレビュー用のサンプリング）
  OklchRampWatchCmd.*     oklchRampWatch コマンド（MNodeMessage で ramp 変更を監視し MEL を idle 実行）
  OklchRampOverride.*     VP2 MPxShadingNodeOverride（256x1 float テクスチャに焼き込み）
  OklchRampFragments.h    VP2 フラグメント XML（HLSL/GLSL/Cg）を C++ 文字列で保持
  pluginMain.cpp          登録・解除
  arnold/OklchRampTranslator.cpp  MtoA 拡張（別 DLL）。ramp_rgb + uv_transform に翻訳
module/                   Maya モジュール（.mod, plug-ins/, arnold/, scripts/）。バイナリは gitignore
  scripts/AEoklchRampTemplate.mel  AE テンプレート（プレビュー帯・クリック追加・自動更新）
  scripts/oklchRamp.mel            oklchRampCreate / oklchRampSetColors
cmake/deploy.cmake        ビルド後に module/ へ配置（ロード中の DLL は .old にリネームしてから上書き）
tests/                    回帰テスト（後述）
build.ps1                 configure + build（+ -Install で modules に .mod を書く）
```

## ビルド

- 要件: Maya 2025（`include/maya` と `lib` は Maya 本体に同梱。別途 devkit 不要）、VS2022 (BuildTools 可)、CMake 3.20+。MtoA があれば `oklchRampTranslator` も自動でビルド。
- パスは環境変数 `MAYA_LOCATION` / `MTOA_LOCATION`、または `-MayaLocation` / `-MtoaLocation`（CMake は `-DMAYA_LOCATION` / `-DMTOA_LOCATION`）。既定は Program Files。**このマシン固有のパスは `CLAUDE.local.md` を参照**（gitignore 済み）。
- 実行: `.\build.ps1` → `module\plug-ins\oklchRamp.mll`、`module\arnold\oklchRampTranslator.dll`。
- Maya がプラグインをロード中でもビルドできる（deploy.cmake が旧 DLL を `.old` にリネーム）。Maya 側は `unloadPlugin oklchRamp; loadPlugin oklchRamp;` で差し替え。
- MSVC 警告はゼロを維持する（/W4）。Arnold API は `AtString` 版を使う（const char* 版は 7.3 で非推奨）。

## テスト

必ずビルド後に実行する。GUI テストは使い捨ての Maya GUI プロセスを起動する（ユーザーが開いている Maya は触らない）。

| コマンド | 内容 | 判定 |
|---|---|---|
| `tests\run_headless.ps1` | mayapy。ノード評価（OKLCH/OKLab、色相モード、色域、UV、保存/再読込）、クリック追加＋Undo | 各スクリプトが `ALL OK` |
| `tests\run_gui_test.ps1 -Test vp2` | VP2 override を ogsRender で描画 | `tests\out\vp2_test.png` を目視（Read ツールで画像を見る） |
| `tests\run_gui_test.ps1 -Test watch` | oklchRampWatch の発火・合流・解除 | log の hits が期待どおり（子アトリビュート変更で +1、無関係属性で +0、連続 20 回で +1） |
| `tests\run_gui_test.ps1 -Test arnold` | MtoA で .ass 書き出し → kick（透かし付き）で描画 | log に ramp_rgb / uv_transform、`tests\out\arnold_test.png` を目視 |

mayapy でできないこと: VP2 描画、`scriptJob` の発火、idle タスク、MtoA。これらは GUI テストで確認する。
`scriptJob -attributeChange` は mayapy では標準属性でも発火しないので、MEL の挙動検証には使わない。

## 設計上の決定（変更時に壊さないこと）

- **`colorEntryList` は `MRampAttribute::createColorRamp` で作る。** Maya 標準 ramp と同じ構造なので `AEaddRampControl` と gradientControl がそのまま使える。子アトリビュート名は `colorEntryList_Position / _Color / _Interp`。
- **補間は必ず `OklchRampEval.h` の `PreparedRamp` を通す。** compute、oklchRampSample、VP2 のベイク、Arnold のベイクの 4 箇所が同じ結果になることが前提。
- **色空間**: エントリ値は既定でシーンリニア。`inputColorSpace = sRGB` のときだけ伝達関数を掛けて戻す。Arnold にはリニアで渡す。`outAlpha` は OKLab の L。
- **VP2 / Arnold はシェーダ内で OKLCH 計算をしない。** CPU で密にサンプリングしてテクスチャ／ramp_rgb のキーにする。VP2 は 256x1 RGBA float、Arnold は 256 キー線形。
- **AE の自動更新は `oklchRampWatch`。** `scriptJob -attributeChange` は複合配列の子（ramp の各エントリ）で発火しないため。通知は idle で 1 回に合流させる。
- **place2dTexture との接続は `outUV → uvCoord`、`outUvFilterSize → uvFilterSize` のみ。** カスタム MPxNode に coverage/repeatUV 等は無い（標準テクスチャ専用）。Arnold 側では place2dTexture の値を `uv_transform` に写す。
- **Maya は新規 ramp アトリビュートにエントリ [0] を自動生成する。** postConstructor はそれを黒 @0 に上書きし、白 @1 を追加する（重複エントリを作らない）。ただし `MFileIO::isReadingFile()` 中は何もしない。ファイル読込時に既定値を入れると、保存された ramp がインデックス 0 を使っていない場合に黒のエントリが残るため（`tests/headless/test_reload_entries.py`）。
- **ノード ID**: Autodesk 登録ブロック `0x00142C40`〜`0x00142C7F`。`0x00142C40` = oklchRamp。新ノードは `0x00142C41` から順に使う。
- **AE プレビュー帯**: 40 セル × 6px = 240px（AE の横スクロールを出さない幅）。`$gAEoklchRampPreviewSamples` で分解能と幅が連動。

## MtoA トランスレータの要点

- `CShaderTranslator` 派生。`CreateArnoldNodes` で place2dTexture 接続の有無により `uv_transform`（root）+ `ramp_rgb`（tag "ramp"）か `ramp_rgb` 単体を作る。`NodeChanged` で `uvCoord` / `inputMode` の変更は `AI_RECREATE_NODE`。
- `extension.Requires("oklchRamp")` により、Maya プラグイン未ロード時は無効。DLL は `MTOA_EXTENSIONS_PATH`（.mod が `module/arnold` を追加）。
- MtoA は `outAlpha` の接続を色出力に付け替えるため、`DependsOnOutputPlug` で outAlpha 用ノードを作っても呼ばれない。Arnold での outAlpha は RGB→float 変換になる（README に明記済み）。
- MtoA バージョン依存（ビルド時 5.4.5 / Arnold 7.3.4.1）。MtoA 更新時は再ビルド。

## リリース手順

CI は無い（Maya SDK を CI で取得できない）。ローカルで:

1. `.\build.ps1`（警告ゼロ）→ `tests\run_headless.ps1` → 必要な GUI テスト。
2. `README.md` / `pluginMain.cpp` のバージョン、`module/oklchRamp.mod` の版を更新。
3. zip を作る: `oklchRamp/module/**`（`.old` 除外）+ `README.md` + `LICENSE` → `oklchRamp-<ver>-maya2025-win64.zip`。
4. `git tag v<ver>` → `gh release create v<ver> <zip> --title ... --notes ...`。
5. remote は HTTPS を使う（SSH は非対話で止まる）。

## 作業上の注意

- ビルド／テストで出た一時ファイルはリポジトリに入れない（`build/`、`tests/out/`、`*.old` は gitignore）。
- ユーザーの Maya が起動中のことが多い。プラグインを差し替えたら、Maya 側で unload/load が必要なことを伝える。
- 日本語 UI の Maya では `ogsRender` の出力名が `無題.png` になる（`<project>/images/tmp/`）。
- Bash ツールのヒアドキュメント内でバックスラッシュ入り文字列を書くと化けることがある。複数行ファイルは Write ツールで書く。
