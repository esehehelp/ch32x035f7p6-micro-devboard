/*
 * Sample app: PC3 blink + USB CDC serial echo.
 * SPDX-License-Identifier: MIT
 */

#include "ch32x_regs.h"
#include "ch32x_cdc.h"

void NMI_Handler(void)       __attribute__((interrupt("WCH-Interrupt-fast")));
void HardFault_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void NMI_Handler(void)       {}
void HardFault_Handler(void) { ch32x_cdc_reboot_to_bootrom(); }

int main(void) {
    /* USB CDC init (includes clock setup, NULL = default config) */
    ch32x_cdc_init(NULL);

    /* PC3: output push-pull 50 MHz */
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC;
    GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xFu << 12)) | (0x3u << 12);

    uint32_t tick = 0;
    for (;;) {
        /* Blink PC3 ~every 500ms */
        if (++tick >= 200000) {
            tick = 0;
            GPIOC->OUTDR ^= GPIO_Pin_3;
        }

        /* Echo received bytes in batch */
        {
            uint8_t buf[64];
            size_t n = ch32x_cdc_read_buf(buf, sizeof(buf));
            if (n > 0)
                ch32x_cdc_write(buf, n);
        }

        IWDG->CTLR = 0xAAAA;  /* feed in case previous firmware left IWDG running */
        __asm volatile("");
    }
}
