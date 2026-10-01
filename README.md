# SOV35 (Xperia XZs) Magisk modules — phh Android 12L GSI

Branch **`phh-a12l-sq3a`**: modules for the environment below. See
[`main`](https://github.com/nec093/sov35-magisk-modules/tree/main) for the
list of environments and branches.

| component | version |
|---|---|
| device | Sony Xperia XZs, SOV35 (au), `keyaki`, MSM8996 |
| kernel | [sov35-kernel](https://github.com/nec093/sov35-kernel) — Linux 5.4.302 port |
| vendor | [sov35-vendor](https://github.com/nec093/sov35-vendor) — LineageOS 18.1 Treble vendor, flashed to the `oem` partition |
| GSI | phh AOSP 12.1, `google/treble_arm64_bgS/phhgsi_arm64_ab:12/SQ3A.220705.003.A1/221109:userdebug` (AOSP `android-12.1.0_r11`) |
| root | Magisk 30.7 |

## Modules

| module | version | what |
|---|---|---|
| [`xzs_hires_audio`](modules/xzs_hires_audio/) | v1.1 | Hi-res media PCM (24/32-bit, float, >48 kHz) to the direct output, end to end up to 192 kHz / 24-bit on headphones (custom audio policy proxy). |

Install: build the zip with `tools/make-zip.sh modules/<id>`, or take it from
the releases, then install it in Magisk (or run
`magisk --install-module <zip>`) and reboot.

Modules here are not compatible with the older Fake Treble setup
(stock-based vendor + 4.9 kernel) in
[sov35-gsi-fix-magisk-modules](https://github.com/nec093/sov35-gsi-fix-magisk-modules).
Most of what that repository worked around (audio, sensors, video codecs,
Bluetooth, ...) is fixed in sov35-kernel and sov35-vendor instead.

## 日本語

このブランチ（`phh-a12l-sq3a`）は、独自カーネル（sov35-kernel）、独自 vendor（sov35-vendor）、phh Android 12L GSI の組み合わせ用です。
GSI を入れ替える場合は、その GSI 用のブランチを使ってください。ブランチの一覧は `main` にあります。
