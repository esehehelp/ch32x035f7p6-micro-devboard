# CH32X035F7P6 Micro Devboard

CH32X035F7P6 (RISC-V, 48 MHz, 62 KB flash, 20 KB RAM) の小型開発ボード。

## 概要

USB CDC-ACM ライブラリ (`ch32x-cdc`) を中心に、PlatformIO と Arduino IDE の両方をサポート。
ESP32/RP2040 の Arduino Serial と同じ体験 — 初期化を呼ぶだけで USB シリアルが使え、1200bps タッチでファームウェア更新ができる。

## Windows クイックセットアップ

Arduino IDE を閉じ、PowerShell で以下を実行する。Git やリポジトリの事前ダウンロードは不要。

```powershell
curl.exe -fL https://raw.githubusercontent.com/esehehelp/ch32x035f7p6-micro-devboard/fix/usb-c-pd-cdc-stability/install.ps1 -o install.ps1
if ($LASTEXITCODE -ne 0) { throw 'Download failed' }
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\install.ps1
```

実行前にダウンロードした `install.ps1` の内容を確認できる。インストーラはボードのソースを一時取得し、以下を自動で入れる。

- Arduino スケッチブックへのボードコア
- WCH RISC-V GCC ツールチェイン
- 最新の `wchisp` と `CH375DLL64.dll`
- WCH 署名済み USB ISP ドライバ（ここだけ UAC 確認あり）
- PlatformIO 向けの Windows 用アップローダ（`%LOCALAPPDATA%\CH32X035\tools`）

完了後に Arduino IDE を起動し直し、**CH32X035F7P6 Micro Devboard** を選択する。以後は Zadig で WinUSB と WCH ドライバを入れ替える必要はない。

すでにリポジトリがある場合は `setup.bat` をダブルクリックしても同じ。スケッチブックが特殊な位置にある場合だけ次のように指定する。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\install.ps1 -Sketchbook D:\Arduino
```

## PlatformIO (firmware/)

### 構成

```
firmware/
  platformio.ini          # PlatformIO 設定
  ch32_1k2touch.py        # カスタムアップロードスクリプト
  src/
    main.c                # サンプルアプリ (PC3 blink + CDC echo)
    startup.S             # ベクタテーブル + 起動コード
    link.ld               # リンカスクリプト
  lib/
    ch32x-cdc/
      include/
        ch32x_cdc.h       # 公開 API
        ch32x_regs.h      # MIT レジスタ定義 (WCH HAL 非依存)
      src/
        ch32x_cdc.c       # CDC 実装 (ディスクリプタ, ISR, バッファ)
```

### ビルド & 書き込み

```bash
cd firmware

# ビルド
pio run

# 書き込み (初回: wch-link 経由)
pio run -t upload --upload-port wlink

# 書き込み (2回目以降: USB CDC 1200bps タッチ経由, wch-link 不要)
pio run -t upload
```

初回書き込みは wch-link (SWD) が必要。一度ファームウェアが動けば、以降は USB ケーブルだけで `pio run -t upload` でフラッシュできる。

Windows では先に上記の curl セットアップ、または `setup.bat` を一度実行する。PlatformIO 同梱の古い `wchisp` ではなく、WCH 公式ドライバに対応した新しい版が自動的に使われる。

## Arduino IDE (arduino/)

### インストール

Windows では上記の curl セットアップだけで完了する。手動コピー、Python/ツールチェインの PATH 設定、Zadig は不要。

Arduino IDE では次の順に選ぶ。

1. **Tools > Board > CH32X035F7P6 Boards > CH32X035F7P6 Micro Devboard**
2. **Tools > Port** でボードの COM ポート
3. **File > Examples > CH32X035 Examples > 01_Blink**
4. Upload を押す

ボードがすでに BootROM に入って COM ポートが見えない場合も、そのまま Upload を押せば `wchisp` が検出する。

### Examples

- `01_Blink` — オンボード LED (PC3)
- `02_SerialHello` — USB Serial へカウンタ出力
- `03_SerialEcho` — Serial Monitor の送受信
- `04_DigitalInputPullup` — D0/PA0 の内蔵プルアップ入力

### ピンアサイン

基板シルクと同じ `PA0`, `PB12`, `PC1` などの名前をスケッチで直接使える。Arduino 互換の `D0` / `A0` 形式も同じピンを指す。

| Arduino | MCU / silk | MCU pin | Notes |
|---:|---|---:|---|
| D0 / A0 | PA0 | 6 | ADC0 |
| D1 / A1 | PA1 | 7 | ADC1 |
| D2 / A2 | PA2 | 8 | ADC2, USART2 TX capable |
| D3 / A3 | PA3 | 9 | ADC3, USART2 RX capable |
| D4 / A4 | PA4 | 10 | ADC4, SPI SS capable |
| D5 / A5 | PA5 | 11 | ADC5, SPI SCK capable |
| D6 / A6 | PA6 | 12 | ADC6, SPI MISO capable |
| D7 / A7 | PA7 | 13 | ADC7, SPI MOSI capable |
| D8 | PB1 | 14 | ADC9 capable |
| D9 | PB12 | 1 | GPIO |
| D10 | PC1 | 5 | GPIO |
| D11 | PC3 | 4 | `LED_BUILTIN`、RESET ボタン/NRST と共有 |
| D12 | PC18 | 19 | SWDIO と共有 |
| D13 | PC19 | 20 | SWCLK と共有 |

USB 用の PC14 (CC1), PC15 (CC2), PC16 (D-), PC17 (D+) は Arduino デジタルピンに含めない。PC18/PC19 を GPIO にすると SWD デバッグと競合する。また現在の最小コアは `analogRead()` / SPI / HardwareSerial をまだ実装していないため、表の peripheral capable は MCU の配線能力を示す。

### Arduino API サポート

- `Serial` — USB CDC シリアル (`USBSerial` クラス、`Print` 継承)
- `millis()` / `micros()` / `delay()` / `delayMicroseconds()` — SysTick ベース
- `pinMode()` / `digitalWrite()` / `digitalRead()`

### API

```c
#include "ch32x_cdc.h"

// 初期化 (NULL でデフォルト設定)
ch32x_cdc_config_t cfg = { .magic_baud_enable = 1 };
ch32x_cdc_init(&cfg);

// 読み込み
int ch32x_cdc_available(void);       // 受信バイト数
int ch32x_cdc_read(void);            // 1バイト読み込み (-1 = 空)
size_t ch32x_cdc_read_buf(buf, max); // バッファに一括読み込み

// 書き込み
size_t ch32x_cdc_write(buf, len);    // バイト列送信
size_t ch32x_cdc_print(str);         // 文字列送信
void ch32x_cdc_flush(void);          // TX 完了待ち

// BootROM へリブート (手動呼び出し用)
void ch32x_cdc_reboot_to_bootrom(void);
```

#### 設定

```c
typedef struct {
    int      magic_baud_enable;  // 1200bps リブートトリガ (デフォルト: 有効)
    uint32_t magic_baud;         // トリガボーレート (デフォルト: 1200)
    void (*pre_reboot)(void);    // リブート前コールバック
} ch32x_cdc_config_t;
```

#### RX コールバック

```c
// ISR コンテキストで呼ばれる (weak シンボル、アプリ側でオーバーライド可)
void ch32x_cdc_on_rx(const uint8_t *buf, size_t len) {
    // 受信データを処理
}
```

### サンプルアプリ (PlatformIO)

`firmware/src/main.c` — PC3 LED 点滅 (500ms) + USB CDC エコー。

```c
#include "ch32x_regs.h"
#include "ch32x_cdc.h"

int main(void) {
    ch32x_cdc_init(NULL);

    /* PC3 push-pull output (cdc_init already enabled GPIOC clock) */
    GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xFu << 12)) | (0x3u << 12);

    for (;;) {
        uint8_t buf[64];
        size_t n = ch32x_cdc_read_buf(buf, sizeof(buf));
        if (n > 0)
            ch32x_cdc_write(buf, n);
        ch32x_cdc_poll();
        IWDG->CTLR = 0xAAAA;
    }
}
```

### 技術メモ

- **クロック設定**: `ch32x_cdc_init()` 内で HSI 有効化、Flash 2 wait-state 設定、HPRE=DIV1 (HCLK=48MHz) を行う。BootROM が HPRE を div6 (8MHz) のまま残すため、明示的なクリアが必要。
- **SysTick**: QingKe V4C の 64-bit カウンタ。HCLK/8 (6MHz) で動作。カウンタリセットは CTLR bit 4 で行う（CNTL/CNTH 直接書き込みは不可）。
- **レジスタ定義**: `ch32x_regs.h` は TRM から自作した MIT ライセンスの定義。WCH HAL/EVT に依存しない。
- **割り込み**: `USBFS_IRQHandler` はライブラリが strong シンボルとして提供。`startup.S` の weak シンボルをリンカが解決。
- **ISR 属性**: GCC 8.2 (Arduino/WCH ツールチェイン) では `__attribute__((interrupt))` を使用。`WCH-Interrupt-fast` はカリーセーブドレジスタしか保存しないバグがある。GCC 12 (PlatformIO) では `WCH-Interrupt-fast` で問題なし。
- **BootROM 突入**: `FLASH->BOOT_MODEKEYR` アンロック → `FLASH->STATR |= BOOT_MODE` → PFIC システムリセット。チップ内蔵 ISP (0x1FFF0000) が起動し、wchisp で書き込み可能。1200bps の line coding だけでは発火せず、DTR 解放まで完了した touch だけを受け付ける。
- **USB-C**: PC14/PC15 に CC プルダウンを設定 (USBPD ペリフェラル経由)。ホストが VBUS を供給するために必要。
- **HardFault**: サンプルアプリでは HardFault 時に BootROM へリブート。クラッシュしてもファームウェア書き直し可能。
- **IWDG**: 旧ファームウェアが IWDG を起動していた場合、ソフトウェアリセットでは停止しない。メインループで `IWDG->CTLR = 0xAAAA` で給餌するか、電源を入れ直す。
- **C++ 初期化**: startup.S で `__libc_init_array()` を呼び出し、vtable やグローバルコンストラクタを初期化。`-nostartfiles` 使用時は `_init`/`_fini` スタブも必要。

## License

MIT
