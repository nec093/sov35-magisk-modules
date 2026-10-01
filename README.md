# SOV35 (Xperia XZs) Magisk modules

Magisk modules for the Sony Xperia XZs (SOV35, au, `keyaki`, MSM8996)
running a GSI on top of a **self-built kernel and vendor**:

- kernel: [sov35-kernel](https://github.com/nec093/sov35-kernel) — Linux 5.4.302 port
- vendor: [sov35-vendor](https://github.com/nec093/sov35-vendor) — LineageOS 18.1 Treble vendor (flashed to the `oem` partition)

The kernel and vendor fix the hardware support itself. The modules here cover
what still depends on the GSI (the system image), so **each GSI has its own
branch**:

| branch | GSI | kernel / vendor | modules |
|---|---|---|---|
| [`phh-a12l-sq3a`](https://github.com/nec093/sov35-magisk-modules/tree/phh-a12l-sq3a) | phh AOSP 12.1 (Android 12L), `SQ3A.220705.003.A1` (221109), arm64 bgS | sov35-kernel `main` / sov35-vendor `main` | `xzs_hires_audio` |

Use the branch that matches your GSI. A module built for one GSI may check
the system it runs on and turn itself off on a different one (for example
`xzs_hires_audio`), but do not rely on that: modules can depend on exact
library layouts.

## Adding a GSI

1. Branch off the closest existing GSI branch (`git switch -c <gsi-name> <branch>`).
2. Re-verify every module on the new GSI, and adjust what depends on its
   libraries (see each module's README).
3. Add a row to the table above on `main`.

The modules in
[sov35-gsi-fix-magisk-modules](https://github.com/nec093/sov35-gsi-fix-magisk-modules)
were for the older Fake Treble setup (stock-based vendor + 4.9 kernel) and are
not compatible with this one.

---

## 日本語

SOV35（Xperia XZs）で、独自カーネル（sov35-kernel）と独自 vendor（sov35-vendor）の上で GSI を動かすための Magisk モジュール集です。

- ハードウェアの対応はカーネルと vendor で直しています。このリポジトリは、GSI（system イメージ）に依存して残る部分を扱います。
- GSI ごとにブランチを分けています。使っている GSI に合うブランチを選んでください（上の表）。
- 新しい GSI に対応するときは、近いブランチから分岐し、各モジュールを検証し直してから、表に追記します。
