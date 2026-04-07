# CH32X035F7P6 Micro Devboard

CH32X035F7P6 (RISC-V, 48 MHz, 62 KB flash, 20 KB RAM) の小型開発ボード。

## Firmware

USB CDC-ACM ライブラリ (`ch32x-cdc`) を含む PlatformIO プロジェクト。
ESP32/RP2040 の Arduino Serial と同じ体験 — `ch32x_cdc_init()` を呼ぶだけで USB シリアルが使え、1200bps タッチでファームウェア更新ができる。

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

### サンプルアプリ

`firmware/src/main.c` — PC3 LED 点滅 + USB CDC エコー。

```c
#include "ch32x_regs.h"
#include "ch32x_cdc.h"

int main(void) {
    RCC->CTLR |= 1;
    FLASH->ACTLR = (FLASH->ACTLR & ~0x03) | 0x02;
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC;
    GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xFu << 12)) | (0x3u << 12);

    ch32x_cdc_config_t cfg = { .magic_baud_enable = 1 };
    ch32x_cdc_init(&cfg);

    uint32_t tick = 0;
    for (;;) {
        if (++tick >= 200000) {
            tick = 0;
            GPIOC->OUTDR ^= GPIO_Pin_3;
        }
        uint8_t buf[64];
        size_t n = ch32x_cdc_read_buf(buf, sizeof(buf));
        if (n > 0)
            ch32x_cdc_write(buf, n);
    }
}
```

### 技術メモ

- **レジスタ定義**: `ch32x_regs.h` は TRM から自作した MIT ライセンスの定義。WCH HAL/EVT に依存しない。
- **割り込み**: `USBFS_IRQHandler` はライブラリが strong シンボルとして提供。`startup.S` の weak シンボルをリンカが解決。
- **BootROM 突入**: `FLASH->BOOT_MODEKEYR` アンロック → `FLASH->STATR |= BOOT_MODE` → PFIC システムリセット。チップ内蔵 ISP (0x1FFF0000) が起動し、wchisp で書き込み可能。
- **USB-C**: PC14/PC15 に CC プルダウンを設定 (USBPD ペリフェラル経由)。ホストが VBUS を供給するために必要。
- **HardFault**: サンプルアプリでは HardFault 時に BootROM へリブート。クラッシュしてもファームウェア書き直し可能。
- **IWDG**: 旧ファームウェアが IWDG を起動していた場合、ソフトウェアリセットでは停止しない。メインループで `IWDG->CTLR = 0xAAAA` で給餌するか、電源を入れ直す。

## License

MIT
