# xzs_hires_audio — hi-res PCM to the direct output

Lets hi-res players (for example Poweramp's *Hi-Res output*) reach the headphone
DAC bit-perfect at up to 192 kHz / 24-bit instead of being resampled to the
48 kHz mixer.

## Why it is needed

On this GSI the audio policy is AOSP's `AudioPolicyManager`
(`libaudiopolicymanagerdefault.so`). Android 12's AOSP policy never sends a
linear PCM track to a direct output unless the track itself asks for
`AUDIO_OUTPUT_FLAG_DIRECT`. Qualcomm devices normally ship the CAF
`AudioPolicyManagerCustom`, which routes hi-res PCM to the `direct_pcm` output
("PCM offload"); a GSI does not have it.

A player that does ask for direct output usually asks for
`DIRECT|DEEP_BUFFER` (Poweramp: flags `0x9`). The AOSP policy opens the direct
output with exactly those flags. The CAF audio HAL then sees `DEEP_BUFFER` and
runs the stream as **deep-buffer** playback: the 48 kHz/16-bit app type is
applied, and the DSP's ADM resamples to 48 kHz, even though the backend runs at
192 kHz.

## What it does

`AudioPolicyService` loads `libaudiopolicymanagercustom.so` when it exists.
This module provides that library, but it does **not** reimplement the policy:

- It creates the GSI's own `AudioPolicyManager` (its constructor and
  `initialize()` are resolved with `dlsym`).
- It returns a proxy object whose vtable forwards every slot unchanged to that
  manager. Each slot is a 5-instruction AArch64 tail-call thunk that touches no
  argument register.
- Only `getOutputForAttr()` (slot 12) is intercepted. A request with usage
  `MEDIA` and linear PCM (at most 2 channels, at least 44.1 kHz) in a hi-res
  format (24/32-bit, float, or above 48 kHz), whose flags are only
  `NONE`/`DEEP_BUFFER`/`DIRECT`, gets plain `AUDIO_OUTPUT_FLAG_DIRECT`. With
  that, the HAL takes its direct-PCM path (compress offload with PCM, app type
  `direct_pcm_24`).
- If no direct output can take a track (float, an unsupported rate, A2DP/USB),
  the AOSP policy falls back to the mixer by itself (`openDirectOutput()`
  returns `NAME_NOT_FOUND` for linear PCM).

### Safety

The proxy depends only on the manager's vtable and on the size of its object,
not on its class layout. Before anything is constructed, the library checks
the GSI's `_ZTVN7android18AudioPolicyManagerE`:

- slots 0/1 are the destructors,
- slot 12 is `getOutputForAttr` (full mangled signature),
- slot 102 is `canBeSpatializedInt`,
- slot 103 is the secondary vtable's offset-to-top (`-8`).

If any of these does not hold (another GSI), the library returns the stock
manager without the proxy, which behaves exactly like having no module.

The manager object is allocated with 16 KiB (the real size is `0x530`).

Set `persist.sys.xzs.hires_direct=0` to turn the interception off without
removing the module. The decisions are logged with the tag `xzs_apm`.

## Verified (2026-10-01)

Poweramp *Hi-Res output*, a 192 kHz / 24-bit FLAC, wired headphones:

| stage | state |
|---|---|
| AudioFlinger | DIRECT thread, 192000 Hz, `PCM_24_BIT_PACKED` (`xzs_apm: req flags 0x9 -> flags 0x1`) |
| audio HAL | `compress-offload-playback2` (direct PCM), backend `SLIM_6_RX` 24-bit / `KHZ_192` |
| DSP | ADM COPP opened at 192000 Hz, AFE `SLIMBUS_6_RX` 24-bit / 192 kHz |
| codec | WCD9335 INT1/INT2 mix inputs at 192 kHz (`RX1/RX2_RX_PATH_MIX_CTL` = `0x26`), HPH in `CLS_H_HIFI` |

This needs the headphone route on `SLIMBUS_6_RX` from
[sov35-vendor](https://github.com/nec093/sov35-vendor), because the HAL
configures the headphone backend through the `SLIM_6_RX` controls.

## Build

```sh
tools/fetch-aosp12-headers.sh /path/aosp12-hdr      # system/media + system/core @ android-12.1.0_r11
adb pull /system/lib64/libc.so  devlibs/
adb pull /system/lib64/libdl.so devlibs/
adb pull /system/lib64/liblog.so devlibs/
LOS_ROOT=/path/lineage-18.1 AOSP12_HDR=/path/aosp12-hdr DEVLIBS=$PWD/devlibs \
    modules/xzs_hires_audio/build.sh
tools/make-zip.sh modules/xzs_hires_audio out/
```

`LOS_ROOT` is the LineageOS 18.1 tree that sov35-vendor is built in. The build
uses its clang (`clang-r383902b1`), bionic and libc++ headers. The output is
bit-identical to the binary in `system/lib64/`.

The vtable slot numbers, the object size and the checked symbols are specific
to `android-12.1.0_r11` (`SQ3A.220705.003.A1`). For another GSI, dump its
vtable (for example with `llvm-readelf -r` on `libaudiopolicymanagerdefault.so`;
the relocations are RELR) and adjust `kSlots`, `kGetOutputForAttrSlot` and the
checks on a new branch.

---

## 日本語の概要

Poweramp の「Hi-Res 出力」などで、ハイレゾ音源を 48 kHz のミキサーに通さず、最大 192 kHz / 24 bit のままヘッドホンに出力するためのモジュールです。

- GSI の音声ポリシー（AOSP 版）は、アプリが DIRECT を要求しない PCM を、すべてミキサー（48 kHz）に回します。
  DIRECT|DEEP_BUFFER を要求した場合でも、HAL が deep-buffer として扱うため、DSP 内部で 48 kHz に変換されます。
- このモジュールは、GSI の標準ポリシーをそのまま使ったまま、`getOutputForAttr()` だけに介入します。
  メディア用途のハイレゾ PCM について、フラグを DIRECT 単独に揃えます。それ以外の呼び出しは、すべて素通しです。
- 起動時に GSI の vtable を検査し、想定と違う場合は何もしません（標準ポリシーのままになります）。
  `persist.sys.xzs.hires_direct=0` で無効にできます。
- ヘッドホンの経路を SLIMBUS_6_RX にした [sov35-vendor](https://github.com/nec093/sov35-vendor) が必要です。
