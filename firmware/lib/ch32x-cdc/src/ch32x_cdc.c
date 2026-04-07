/*
 * ch32x_cdc.c — USB CDC-ACM library for CH32X035.
 * SPDX-License-Identifier: MIT
 *
 * Single-file implementation: descriptors, ISR, ring buffer, TX, API.
 * Endpoints: EP0 control, EP1 IN interrupt (CDC notify), EP2 OUT bulk (RX),
 *            EP3 IN bulk (TX).
 */

#include "ch32x_regs.h"
#include "ch32x_cdc.h"
#include <string.h>

/* ── Constants ───────────────────────────────────────────────── */
#define EP0_SIZE    64
#define EP_SIZE     64

/* USB standard request codes */
#define USB_GET_STATUS          0x00
#define USB_CLEAR_FEATURE       0x01
#define USB_SET_FEATURE         0x03
#define USB_SET_ADDRESS         0x05
#define USB_GET_DESCRIPTOR      0x06
#define USB_GET_CONFIGURATION   0x08
#define USB_SET_CONFIGURATION   0x09
#define USB_GET_INTERFACE       0x0A
#define USB_SET_INTERFACE       0x0B

/* USB descriptor types */
#define USB_DESCR_TYP_DEVICE    0x01
#define USB_DESCR_TYP_CONFIG    0x02
#define USB_DESCR_TYP_STRING    0x03

/* USB request type masks */
#define USB_REQ_TYP_MASK        0x60
#define USB_REQ_TYP_STANDARD    0x00
#define USB_REQ_TYP_CLASS       0x20
#define USB_REQ_RECIP_MASK      0x1F
#define USB_REQ_RECIP_DEVICE    0x00
#define USB_REQ_RECIP_ENDP      0x02
#define USB_REQ_FEAT_ENDP_HALT  0x00

/* CDC class requests */
#define CDC_SET_LINE_CODING     0x20
#define CDC_GET_LINE_CODING     0x21
#define CDC_SET_LINE_CTLSTE     0x22
#define CDC_SEND_BREAK          0x23

/* Endpoint direction */
#define UEP_IN   0x80
#define UEP_OUT  0x00

/* ── USB Setup Packet ────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  bRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} USB_SETUP_REQ;

/* ── Descriptors (all const, placed in flash) ────────────────── */
static const uint8_t dev_descr[] = {
    0x12, 0x01, 0x00, 0x02, 0x02, 0x00, 0x00, EP0_SIZE,
    0x86, 0x1A,  /* VID: 0x1A86 */
    0x0C, 0xFE,  /* PID: 0xFE0C */
    0x01, 0x00,  /* bcdDevice 0.01 */
    0x01, 0x02, 0x03, 0x01
};

static const uint8_t cfg_descr[] = {
    /* Configuration descriptor */
    0x09, 0x02, 0x43, 0x00, 0x02, 0x01, 0x00, 0x80, 0x32,
    /* Interface 0: CDC (comm) */
    0x09, 0x04, 0x00, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00,
    /* CDC Header */
    0x05, 0x24, 0x00, 0x10, 0x01,
    /* CDC Call Management */
    0x05, 0x24, 0x01, 0x00, 0x01,
    /* CDC ACM */
    0x04, 0x24, 0x02, 0x02,
    /* CDC Union */
    0x05, 0x24, 0x06, 0x00, 0x01,
    /* EP1 IN: interrupt, 8 bytes, 255ms */
    0x07, 0x05, 0x81, 0x03, 0x08, 0x00, 0xFF,
    /* Interface 1: CDC Data */
    0x09, 0x04, 0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00,
    /* EP2 OUT: bulk, 64 bytes */
    0x07, 0x05, 0x02, 0x02, EP_SIZE, 0x00, 0x00,
    /* EP3 IN: bulk, 64 bytes */
    0x07, 0x05, 0x83, 0x02, EP_SIZE, 0x00, 0x00,
};

static const uint8_t lang_descr[] = { 0x04, 0x03, 0x09, 0x04 };

static const uint8_t manu_descr[] = {
    0x12, 0x03,
    'C',0, 'H',0, '3',0, '2',0, 'X',0, '0',0, '3',0, '5',0
};

static const uint8_t prod_descr[] = {
    0x16, 0x03,
    'U',0, 'S',0, 'B',0, ' ',0, 'S',0, 'e',0, 'r',0, 'i',0, 'a',0, 'l',0
};

/* Serial number: filled at runtime from chip UID (96-bit = 24 hex chars) */
static uint8_t serial_descr[50] = {
    0x32, 0x03,
    '0',0,'0',0,'0',0,'0',0,'0',0,'0',0,'0',0,'0',0,
    '0',0,'0',0,'0',0,'0',0,'0',0,'0',0,'0',0,'0',0,
    '0',0,'0',0,'0',0,'0',0,'0',0,'0',0,'0',0,'0',0
};

/* ── CDC line coding ─────────────────────────────────────────── */
static uint8_t cdc_line_coding[7] = {
    0x00, 0xC2, 0x01, 0x00,  /* 115200 bps LE */
    0x00, 0x00, 0x08          /* 1 stop, no parity, 8 data */
};

/* ── USB state ───────────────────────────────────────────────── */
static const uint8_t *pDescr;
static volatile uint8_t  setupReqCode;
static volatile uint8_t  setupReqType;
static volatile uint16_t setupReqLen;
static volatile uint16_t setupReqValue;
static volatile uint16_t setupReqIndex;
static volatile uint8_t  devConfig;
static volatile uint8_t  devAddr;

/* ── Reboot state (deferred from ISR via SOF) ────────────────── */
static volatile uint8_t  reboot_pending;
static volatile uint8_t  reboot_countdown;

/* ── Endpoint buffers (4-byte aligned for DMA) ───────────────── */
__attribute__((aligned(4))) static uint8_t ep0_buf[EP0_SIZE];
__attribute__((aligned(4))) static uint8_t ep2_rx_buf[EP_SIZE];
__attribute__((aligned(4))) static uint8_t ep3_tx_dma[EP_SIZE];

#define pSetupReq  ((USB_SETUP_REQ *)ep0_buf)

/* ── RX ring buffer (256B, 8-bit wrap = no modulo) ───────────── */
static volatile uint8_t rx_ring[256];
static volatile uint8_t rx_head, rx_tail;

/* ── TX state ────────────────────────────────────────────────── */
static volatile uint8_t tx_busy;

/* ── Config ──────────────────────────────────────────────────── */
static int magic_baud_enable = 1;
static uint32_t magic_baud = 1200;
static void (*pre_reboot_hook)(void);

/* ── Weak RX callback ────────────────────────────────────────── */
__attribute__((weak))
void ch32x_cdc_on_rx(const uint8_t *buf, size_t len) {
    (void)buf; (void)len;
}

/* ── Build serial number from chip UID ───────────────────────── */
static void build_serial(void) {
    static const char hex[] = "0123456789ABCDEF";
    const uint8_t *uid = (const uint8_t *)ESIG_UID_BASE;
    for (int i = 0; i < 12; i++) {
        serial_descr[2 + i*4 + 0] = hex[(uid[i] >> 4) & 0xF];
        serial_descr[2 + i*4 + 1] = 0;
        serial_descr[2 + i*4 + 2] = hex[uid[i] & 0xF];
        serial_descr[2 + i*4 + 3] = 0;
    }
}

/* ── Endpoint init ───────────────────────────────────────────── */
static void endp_init(void) {
    USBFSD->UEP4_1_MOD = USBFS_UEP1_TX_EN;
    USBFSD->UEP2_3_MOD = USBFS_UEP2_RX_EN | USBFS_UEP3_TX_EN;

    USBFSD->UEP0_DMA = (uint32_t)ep0_buf;
    USBFSD->UEP2_DMA = (uint32_t)ep2_rx_buf;
    USBFSD->UEP3_DMA = (uint32_t)ep3_tx_dma;

    USBFSD->UEP0_CTRL_H = USBFS_UEP_R_RES_ACK | USBFS_UEP_T_RES_NAK;
    USBFSD->UEP1_CTRL_H = USBFS_UEP_T_RES_NAK;
    USBFSD->UEP2_CTRL_H = USBFS_UEP_R_RES_ACK | USBFS_UEP_R_AUTO_TOG;
    USBFSD->UEP3_CTRL_H = USBFS_UEP_T_RES_NAK | USBFS_UEP_T_AUTO_TOG;
    USBFSD->UEP1_TX_LEN = 0;
    USBFSD->UEP3_TX_LEN = 0;
}

/* ── CC pull-down (USB-C sink detection) ─────────────────────── */
static void cc_pulldown_init(void) {
    RCC->APB2PCENR |= RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOC;
    RCC->AHBPCENR  |= RCC_AHBPeriph_USBPD;

    /* PC14, PC15 = floating input: CNF=01 MODE=00 = 0x4 each */
    GPIOC->CFGHR = (GPIOC->CFGHR & ~(0xFFu << 24)) | (0x44u << 24);

    AFIO->CTLR |= AFIO_CTLR_USBPD_IN_HVT | AFIO_CTLR_USBPD_PHY_V33;
    USBPD->PORT_CC1 = CC_CMP_66 | CC_PD;
    USBPD->PORT_CC2 = CC_CMP_66 | CC_PD;
}

/* ══════════════════════════════════════════════════════════════
 *  Reboot to BootROM
 * ══════════════════════════════════════════════════════════════ */
void ch32x_cdc_reboot_to_bootrom(void) {
    __disable_irq();
    if (pre_reboot_hook) pre_reboot_hook();
    USBFSD->BASE_CTRL &= ~USBFS_UC_DEV_PU_EN;
    for (volatile int i = 0; i < 800000; i++) __asm volatile("");
    RCC->RSTSCKR |= RCC_RSTSCKR_RMVF;
    FLASH->BOOT_MODEKEYR = FLASH_KEY1;
    FLASH->BOOT_MODEKEYR = FLASH_KEY2;
    FLASH->STATR |= FLASH_STATR_BOOT_MODE;
    PFIC->CFGR = PFIC_KEY3 | PFIC_SYSRST;
    for (;;) __asm volatile("nop");
}

/* ══════════════════════════════════════════════════════════════
 *  USBFS IRQ Handler
 * ══════════════════════════════════════════════════════════════ */
void USBFS_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

void USBFS_IRQHandler(void) {
    uint8_t intflag = USBFSD->INT_FG;
    uint8_t intst   = USBFSD->INT_ST;

    if (intflag & USBFS_UIF_TRANSFER) {
        uint8_t token = intst & USBFS_UIS_TOKEN_MASK;
        uint8_t ep    = intst & USBFS_UIS_ENDP_MASK;

        switch (token) {

        /* ──── SETUP ──────────────────────────────────────────── */
        case USBFS_UIS_TOKEN_SETUP: {
            USBFSD->UEP0_CTRL_H = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_NAK
                                 | USBFS_UEP_R_TOG | USBFS_UEP_R_RES_NAK;

            setupReqType  = pSetupReq->bRequestType;
            setupReqCode  = pSetupReq->bRequest;
            setupReqLen   = pSetupReq->wLength;
            setupReqValue = pSetupReq->wValue;
            setupReqIndex = pSetupReq->wIndex;

            uint16_t len = 0;
            uint8_t  err = 0;

            if ((setupReqType & USB_REQ_TYP_MASK) == USB_REQ_TYP_CLASS) {
                switch (setupReqCode) {
                case CDC_GET_LINE_CODING:
                    pDescr = cdc_line_coding; len = 7; break;
                case CDC_SET_LINE_CODING:
                case CDC_SET_LINE_CTLSTE:
                case CDC_SEND_BREAK:
                    break;
                default: err = 1; break;
                }
                if (!err && len) {
                    if (setupReqLen > len) setupReqLen = len;
                    len = (setupReqLen >= EP0_SIZE) ? EP0_SIZE : setupReqLen;
                    memcpy(ep0_buf, pDescr, len);
                    pDescr += len;
                }
            }
            else if ((setupReqType & USB_REQ_TYP_MASK) == USB_REQ_TYP_STANDARD) {
                switch (setupReqCode) {

                case USB_GET_DESCRIPTOR:
                    switch ((uint8_t)(setupReqValue >> 8)) {
                    case USB_DESCR_TYP_DEVICE:
                        pDescr = dev_descr;  len = sizeof(dev_descr);  break;
                    case USB_DESCR_TYP_CONFIG:
                        pDescr = cfg_descr;  len = sizeof(cfg_descr);  break;
                    case USB_DESCR_TYP_STRING:
                        switch ((uint8_t)(setupReqValue & 0xFF)) {
                        case 0: pDescr = lang_descr;   len = sizeof(lang_descr);   break;
                        case 1: pDescr = manu_descr;   len = sizeof(manu_descr);   break;
                        case 2: pDescr = prod_descr;   len = sizeof(prod_descr);   break;
                        case 3: pDescr = serial_descr;  len = sizeof(serial_descr); break;
                        default: err = 1; break;
                        }
                        break;
                    default: err = 1; break;
                    }
                    if (!err) {
                        if (setupReqLen > len) setupReqLen = len;
                        len = (setupReqLen >= EP0_SIZE) ? EP0_SIZE : setupReqLen;
                        memcpy(ep0_buf, pDescr, len);
                        pDescr += len;
                    }
                    break;

                case USB_SET_ADDRESS:
                    devAddr = (uint8_t)setupReqValue;
                    break;

                case USB_GET_CONFIGURATION:
                    ep0_buf[0] = devConfig;
                    if (setupReqLen > 1) setupReqLen = 1;
                    break;

                case USB_SET_CONFIGURATION:
                    devConfig = (uint8_t)setupReqValue;
                    break;

                case USB_CLEAR_FEATURE:
                    if ((setupReqType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP) {
                        switch ((uint8_t)setupReqIndex) {
                        case (UEP_IN|1):  USBFSD->UEP1_CTRL_H = USBFS_UEP_T_RES_NAK; break;
                        case (UEP_OUT|2): USBFSD->UEP2_CTRL_H = USBFS_UEP_R_RES_ACK; break;
                        case (UEP_IN|3):  USBFSD->UEP3_CTRL_H = USBFS_UEP_T_RES_NAK; break;
                        default: err = 1; break;
                        }
                    } else {
                        err = (setupReqType & USB_REQ_RECIP_MASK) != USB_REQ_RECIP_DEVICE;
                    }
                    break;

                case USB_SET_FEATURE:
                    if ((setupReqType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP &&
                        (uint8_t)setupReqValue == USB_REQ_FEAT_ENDP_HALT) {
                        switch ((uint8_t)setupReqIndex) {
                        case (UEP_IN|1):
                            USBFSD->UEP1_CTRL_H = (USBFSD->UEP1_CTRL_H & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_STALL; break;
                        case (UEP_OUT|2):
                            USBFSD->UEP2_CTRL_H = (USBFSD->UEP2_CTRL_H & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_STALL; break;
                        case (UEP_IN|3):
                            USBFSD->UEP3_CTRL_H = (USBFSD->UEP3_CTRL_H & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_STALL; break;
                        default: err = 1; break;
                        }
                    } else { err = 1; }
                    break;

                case USB_GET_STATUS:
                    ep0_buf[0] = 0; ep0_buf[1] = 0;
                    if ((setupReqType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP) {
                        switch ((uint8_t)setupReqIndex) {
                        case (UEP_IN|1):
                            if ((USBFSD->UEP1_CTRL_H & USBFS_UEP_T_RES_MASK) == USBFS_UEP_T_RES_STALL) ep0_buf[0] = 1;
                            break;
                        case (UEP_OUT|2):
                            if ((USBFSD->UEP2_CTRL_H & USBFS_UEP_R_RES_MASK) == USBFS_UEP_R_RES_STALL) ep0_buf[0] = 1;
                            break;
                        case (UEP_IN|3):
                            if ((USBFSD->UEP3_CTRL_H & USBFS_UEP_T_RES_MASK) == USBFS_UEP_T_RES_STALL) ep0_buf[0] = 1;
                            break;
                        default: err = 1; break;
                        }
                    }
                    if (setupReqLen > 2) setupReqLen = 2;
                    break;

                case USB_GET_INTERFACE:
                    ep0_buf[0] = 0;
                    if (setupReqLen > 1) setupReqLen = 1;
                    break;

                case USB_SET_INTERFACE:
                    break;

                default: err = 1; break;
                }
            }
            else { err = 1; }

            /* Handshake */
            if (err) {
                USBFSD->UEP0_CTRL_H = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_STALL
                                     | USBFS_UEP_R_TOG | USBFS_UEP_R_RES_STALL;
            } else if (setupReqType & 0x80) {  /* IN */
                len = (setupReqLen > EP0_SIZE) ? EP0_SIZE : setupReqLen;
                setupReqLen -= len;
                USBFSD->UEP0_TX_LEN = len;
                USBFSD->UEP0_CTRL_H = (USBFSD->UEP0_CTRL_H & ~USBFS_UEP_T_RES_MASK)
                                     | USBFS_UEP_T_TOG | USBFS_UEP_T_RES_ACK;
            } else {                           /* OUT */
                if (setupReqLen == 0) {
                    USBFSD->UEP0_TX_LEN = 0;
                    USBFSD->UEP0_CTRL_H = (USBFSD->UEP0_CTRL_H & ~USBFS_UEP_T_RES_MASK)
                                         | USBFS_UEP_T_TOG | USBFS_UEP_T_RES_ACK;
                } else {
                    USBFSD->UEP0_CTRL_H = (USBFSD->UEP0_CTRL_H & ~USBFS_UEP_R_RES_MASK)
                                         | USBFS_UEP_R_TOG | USBFS_UEP_R_RES_ACK;
                }
            }
            break;
        }

        /* ──── IN ─────────────────────────────────────────────── */
        case USBFS_UIS_TOKEN_IN:
            if (ep == 3) {
                USBFSD->UEP3_CTRL_H = (USBFSD->UEP3_CTRL_H & ~USBFS_UEP_T_RES_MASK)
                                     | USBFS_UEP_T_RES_NAK;
                tx_busy = 0;
            } else if (ep == 0) {
                if (setupReqLen == 0)
                    USBFSD->UEP0_CTRL_H = (USBFSD->UEP0_CTRL_H & ~USBFS_UEP_R_RES_MASK)
                                         | USBFS_UEP_R_TOG | USBFS_UEP_R_RES_ACK;
                if ((setupReqType & USB_REQ_TYP_MASK) == USB_REQ_TYP_STANDARD) {
                    if (setupReqCode == USB_GET_DESCRIPTOR) {
                        uint16_t l = (setupReqLen >= EP0_SIZE) ? EP0_SIZE : setupReqLen;
                        memcpy(ep0_buf, pDescr, l);
                        setupReqLen -= l;
                        pDescr += l;
                        USBFSD->UEP0_TX_LEN  = l;
                        USBFSD->UEP0_CTRL_H ^= USBFS_UEP_T_TOG;
                    } else if (setupReqCode == USB_SET_ADDRESS) {
                        USBFSD->DEV_ADDR = (USBFSD->DEV_ADDR & USBFS_UDA_GP_BIT) | devAddr;
                    }
                }
            }
            break;

        /* ──── OUT ────────────────────────────────────────────── */
        case USBFS_UIS_TOKEN_OUT:
            if (ep == 0) {
                if (intst & USBFS_UIS_TOG_OK) {
                    if ((setupReqType & USB_REQ_TYP_MASK) != USB_REQ_TYP_STANDARD) {
                        if (setupReqCode == CDC_SET_LINE_CODING) {
                            memcpy(cdc_line_coding, ep0_buf, 7);
                            if (magic_baud_enable) {
                                uint32_t baud = (uint32_t)cdc_line_coding[0]
                                              | ((uint32_t)cdc_line_coding[1] << 8)
                                              | ((uint32_t)cdc_line_coding[2] << 16)
                                              | ((uint32_t)cdc_line_coding[3] << 24);
                                if (baud == magic_baud) {
                                    reboot_pending = 1;
                                    reboot_countdown = 50; /* ~50ms via SOF */
                                }
                            }
                        }
                        setupReqLen = 0;
                    }
                    if (setupReqLen == 0) {
                        USBFSD->UEP0_TX_LEN = 0;
                        USBFSD->UEP0_CTRL_H = (USBFSD->UEP0_CTRL_H & ~USBFS_UEP_T_RES_MASK)
                                             | USBFS_UEP_T_TOG | USBFS_UEP_T_RES_ACK;
                    }
                }
            } else if (ep == 2) {
                if (intst & USBFS_UIS_TOG_OK) {
                    uint16_t rxlen = USBFSD->RX_LEN;
                    for (uint16_t i = 0; i < rxlen; i++) {
                        uint8_t next = (uint8_t)(rx_head + 1);
                        if (next != rx_tail) {
                            rx_ring[rx_head] = ep2_rx_buf[i];
                            rx_head = next;
                        }
                    }
                    if (rxlen > 0)
                        ch32x_cdc_on_rx(ep2_rx_buf, rxlen);
                }
            }
            break;

        /* ──── SOF ────────────────────────────────────────────── */
        case USBFS_UIS_TOKEN_SOF:
            if (reboot_pending) {
                if (--reboot_countdown == 0)
                    ch32x_cdc_reboot_to_bootrom();
            }
            break;

        default: break;
        }
        USBFSD->INT_FG = USBFS_UIF_TRANSFER;
    }
    else if (intflag & USBFS_UIF_BUS_RST) {
        devConfig = 0;
        devAddr   = 0;
        USBFSD->DEV_ADDR = 0;
        endp_init();
        reboot_pending = 0;
        cdc_line_coding[0] = 0x00; cdc_line_coding[1] = 0xC2;
        cdc_line_coding[2] = 0x01; cdc_line_coding[3] = 0x00;
        cdc_line_coding[4] = 0x00; cdc_line_coding[5] = 0x00;
        cdc_line_coding[6] = 0x08;
        USBFSD->INT_FG = USBFS_UIF_BUS_RST;
    }
    else if (intflag & USBFS_UIF_SUSPEND) {
        USBFSD->INT_FG = USBFS_UIF_SUSPEND;
    }
    else {
        USBFSD->INT_FG = intflag;
    }
}

/* ══════════════════════════════════════════════════════════════
 *  Public API
 * ══════════════════════════════════════════════════════════════ */

void ch32x_cdc_init(const ch32x_cdc_config_t *cfg) {
    if (cfg) {
        if (cfg->magic_baud)
            magic_baud = cfg->magic_baud;
        magic_baud_enable = cfg->magic_baud_enable;
        pre_reboot_hook = cfg->pre_reboot;
    }

    /* System clock: HSI on, 2 wait-states for 48 MHz */
    RCC->CTLR |= 1;
    FLASH->ACTLR = (FLASH->ACTLR & ~0x03) | 0x02;

    build_serial();

    /* Enable clocks */
    RCC->APB2PCENR |= RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOC;
    RCC->AHBPCENR  |= RCC_AHBPeriph_USBFS;

    /* USB GPIO: PC16=DM floating, PC17=DP pull-up */
    GPIOC->CFGXR = (GPIOC->CFGXR & ~0xFFu) | 0x84u;
    GPIOC->BSHR  = (1 << 17);

    /* AFIO: 3.3V, 1.5k pull-up on D+, USB I/O enable */
    AFIO->CTLR = (AFIO->CTLR & ~(AFIO_CTLR_UDP_PUE | AFIO_CTLR_UDM_PUE))
               | AFIO_CTLR_USB_PHY_V33 | AFIO_CTLR_UDP_PUE_1K5 | AFIO_CTLR_USB_IOEN;

    /* USB controller init */
    USBFSD->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;
    __asm volatile("nop; nop; nop; nop");
    USBFSD->BASE_CTRL = 0;
    endp_init();
    USBFSD->DEV_ADDR  = 0;
    USBFSD->BASE_CTRL = USBFS_UC_DEV_PU_EN | USBFS_UC_INT_BUSY | USBFS_UC_DMA_EN;
    USBFSD->INT_FG    = 0xFF;
    USBFSD->UDEV_CTRL = USBFS_UD_PD_DIS | USBFS_UD_PORT_EN;
    USBFSD->INT_EN    = USBFS_UIE_SUSPEND | USBFS_UIE_BUS_RST | USBFS_UIE_TRANSFER
                       | USBFS_UIE_DEV_SOF;  /* SOF needed for deferred reboot */
    NVIC_EnableIRQ(USBFS_IRQn);

    /* USB-C CC pull-down */
    cc_pulldown_init();
}

int ch32x_cdc_available(void) {
    return (uint8_t)(rx_head - rx_tail);
}

int ch32x_cdc_peek(void) {
    if (rx_head == rx_tail) return -1;
    return rx_ring[rx_tail];
}

int ch32x_cdc_read(void) {
    if (rx_head == rx_tail) return -1;
    uint8_t c = rx_ring[rx_tail];
    rx_tail = (uint8_t)(rx_tail + 1);
    return c;
}

size_t ch32x_cdc_read_buf(uint8_t *buf, size_t max) {
    size_t n = 0;
    while (n < max && rx_head != rx_tail) {
        buf[n++] = rx_ring[rx_tail];
        rx_tail = (uint8_t)(rx_tail + 1);
    }
    return n;
}

size_t ch32x_cdc_write(const uint8_t *buf, size_t len) {
    if (!devConfig) return 0;
    size_t total = 0;
    while (total < len) {
        uint32_t timeout = 1000000;
        while (tx_busy && --timeout) __asm volatile("");
        if (tx_busy) return total;
        size_t chunk = len - total;
        if (chunk > EP_SIZE) chunk = EP_SIZE;
        memcpy(ep3_tx_dma, buf + total, chunk);
        USBFSD->UEP3_TX_LEN = chunk;
        tx_busy = 1;
        USBFSD->UEP3_CTRL_H = (USBFSD->UEP3_CTRL_H & ~USBFS_UEP_T_RES_MASK)
                             | USBFS_UEP_T_RES_ACK;
        total += chunk;
    }
    return total;
}

void ch32x_cdc_flush(void) {
    uint32_t timeout = 1000000;
    while (tx_busy && --timeout) __asm volatile("");
}

size_t ch32x_cdc_print(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return ch32x_cdc_write((const uint8_t *)s, len);
}
