/*
 * ch32x_cdc.h — USB CDC-ACM library for CH32X035.
 * SPDX-License-Identifier: MIT
 */
#ifndef CH32X_CDC_H
#define CH32X_CDC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int      magic_baud_enable;  /* 1200bps reboot trigger (default: on) */
    uint32_t magic_baud;         /* default: 1200 */
    void (*pre_reboot)(void);    /* called before BootROM entry */
} ch32x_cdc_config_t;

/* Init / reboot */
void     ch32x_cdc_init(const ch32x_cdc_config_t *cfg);
void     ch32x_cdc_reboot_to_bootrom(void);

/* Read */
int      ch32x_cdc_available(void);
int      ch32x_cdc_peek(void);
int      ch32x_cdc_read(void);
size_t   ch32x_cdc_read_buf(uint8_t *buf, size_t max);

/* Write */
size_t   ch32x_cdc_write(const uint8_t *buf, size_t len);
size_t   ch32x_cdc_print(const char *s);
void     ch32x_cdc_flush(void);

/* Weak callback: override in app to handle RX in ISR context */
void     ch32x_cdc_on_rx(const uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* CH32X_CDC_H */
