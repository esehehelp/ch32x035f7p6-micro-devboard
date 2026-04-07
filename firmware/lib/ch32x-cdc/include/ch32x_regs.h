/*
 * ch32x_regs.h — Minimal CH32X035 register definitions (MIT)
 *
 * Written from the CH32X035 Technical Reference Manual.
 * Covers: RCC, GPIO, AFIO, FLASH, PFIC, USBFSD, USBPD, IWDG.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef CH32X_REGS_H
#define CH32X_REGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Compiler helpers ─────────────────────────────────────────── */
#define __IO  volatile
#define __I   volatile const
#define __O   volatile

/* ── Peripheral base addresses ────────────────────────────────── */
#define PERIPH_BASE       0x40000000U
#define APB1PERIPH_BASE   (PERIPH_BASE)
#define APB2PERIPH_BASE   (PERIPH_BASE + 0x10000U)
#define AHBPERIPH_BASE    (PERIPH_BASE + 0x20000U)

/* ── RCC ──────────────────────────────────────────────────────── */
typedef struct {
    __IO uint32_t CTLR;          /* 0x00 Clock control */
    __IO uint32_t CFGR0;         /* 0x04 Clock configuration */
    __IO uint32_t RESERVED0;     /* 0x08 */
    __IO uint32_t APB2PRSTR;     /* 0x0C APB2 peripheral reset */
    __IO uint32_t APB1PRSTR;     /* 0x10 APB1 peripheral reset */
    __IO uint32_t AHBPCENR;      /* 0x14 AHB peripheral clock enable */
    __IO uint32_t APB2PCENR;     /* 0x18 APB2 peripheral clock enable */
    __IO uint32_t APB1PCENR;     /* 0x1C APB1 peripheral clock enable */
    __IO uint32_t RESERVED1;     /* 0x20 */
    __IO uint32_t RSTSCKR;       /* 0x24 Reset status / clock */
    __IO uint32_t AHBRSTR;       /* 0x28 AHB peripheral reset */
} RCC_TypeDef;

#define RCC_BASE          (AHBPERIPH_BASE + 0x1000U)
#define RCC               ((RCC_TypeDef *)RCC_BASE)

/* RCC_AHBPCENR bits */
#define RCC_AHBPeriph_USBFS   (1U << 12)
#define RCC_AHBPeriph_USBPD   (1U << 17)

/* RCC_APB2PCENR bits */
#define RCC_APB2Periph_AFIO   (1U << 0)
#define RCC_APB2Periph_GPIOA  (1U << 2)
#define RCC_APB2Periph_GPIOB  (1U << 3)
#define RCC_APB2Periph_GPIOC  (1U << 4)
#define RCC_APB2Periph_ADC1   (1U << 9)

/* RCC_RSTSCKR bits */
#define RCC_RSTSCKR_RMVF      (1U << 24)  /* Remove reset flags */

/* ── GPIO ─────────────────────────────────────────────────────── */
typedef struct {
    __IO uint32_t CFGLR;   /* 0x00 Port configuration low  (pins 0-7) */
    __IO uint32_t CFGHR;   /* 0x04 Port configuration high (pins 8-15) */
    __IO uint32_t INDR;    /* 0x08 Port input data */
    __IO uint32_t OUTDR;   /* 0x0C Port output data */
    __IO uint32_t BSHR;    /* 0x10 Port bit set/reset (high=reset, low=set) */
    __IO uint32_t BCR;     /* 0x14 Port bit reset */
    __IO uint32_t LCKR;    /* 0x18 Port lock */
    __IO uint32_t CFGXR;   /* 0x1C Port configuration extended (PC16-17) */
    __IO uint32_t BSXR;    /* 0x20 Port bit set/reset extended */
} GPIO_TypeDef;

#define GPIOA_BASE  (APB2PERIPH_BASE + 0x0800U)
#define GPIOB_BASE  (APB2PERIPH_BASE + 0x0C00U)
#define GPIOC_BASE  (APB2PERIPH_BASE + 0x1000U)
#define GPIOA       ((GPIO_TypeDef *)GPIOA_BASE)
#define GPIOB       ((GPIO_TypeDef *)GPIOB_BASE)
#define GPIOC       ((GPIO_TypeDef *)GPIOC_BASE)

#define GPIO_Pin_0   (1U << 0)
#define GPIO_Pin_1   (1U << 1)
#define GPIO_Pin_2   (1U << 2)
#define GPIO_Pin_3   (1U << 3)

/* ── AFIO ─────────────────────────────────────────────────────── */
typedef struct {
    uint32_t      RESERVED0;  /* 0x00 */
    __IO uint32_t PCFR1;      /* 0x04 Remap configuration */
    __IO uint32_t EXTICR[2];  /* 0x08, 0x0C */
    uint32_t      RESERVED1;  /* 0x10 */
    uint32_t      RESERVED2;  /* 0x14 */
    __IO uint32_t CTLR;       /* 0x18 AFIO control */
} AFIO_TypeDef;

#define AFIO_BASE  (APB2PERIPH_BASE + 0x0000U)
#define AFIO       ((AFIO_TypeDef *)AFIO_BASE)

/* AFIO_CTLR bits */
#define AFIO_CTLR_UDM_PUE        0x03U   /* PC16/UDM pull-up mode */
#define AFIO_CTLR_UDP_PUE        0x0CU   /* PC17/UDP pull-up mode */
#define AFIO_CTLR_UDP_PUE_1K5    0x0CU   /* 1.5k pull-up on UDP (full-speed) */
#define AFIO_CTLR_USB_PHY_V33    0x40U   /* USB PHY 3.3V mode */
#define AFIO_CTLR_USB_IOEN       0x80U   /* USB I/O enable */

/* USBPD-related bits in AFIO_CTLR */
#define AFIO_CTLR_USBPD_PHY_V33  (1U << 8)
#define AFIO_CTLR_USBPD_IN_HVT   (1U << 9)

/* ── FLASH ────────────────────────────────────────────────────── */
typedef struct {
    __IO uint32_t ACTLR;          /* 0x00 Access control (wait states) */
    __IO uint32_t KEYR;           /* 0x04 Key register */
    __IO uint32_t OBKEYR;         /* 0x08 Option byte key */
    __IO uint32_t STATR;          /* 0x0C Status register */
    __IO uint32_t CTLR;           /* 0x10 Control register */
    __IO uint32_t ADDR;           /* 0x14 Address register */
    uint32_t      RESERVED;       /* 0x18 */
    __IO uint32_t OBR;            /* 0x1C Option byte register */
    __IO uint32_t WPR;            /* 0x20 Write protection */
    __IO uint32_t MODEKEYR;       /* 0x24 Mode key register */
    __IO uint32_t BOOT_MODEKEYR;  /* 0x28 Boot mode key register */
} FLASH_TypeDef;

#define FLASH_R_BASE  (AHBPERIPH_BASE + 0x2000U)
#define FLASH         ((FLASH_TypeDef *)FLASH_R_BASE)

#define FLASH_KEY1          0x45670123U
#define FLASH_KEY2          0xCDEF89ABU
#define FLASH_STATR_BOOT_MODE  (1U << 14)

/* ── PFIC (Platform-level Fast Interrupt Controller) ──────────── */
typedef struct {
    __I  uint32_t ISR[8];       /* 0x000 Interrupt status */
    __I  uint32_t IPR[8];       /* 0x020 Interrupt pending */
    __IO uint32_t ITHRESDR;     /* 0x040 Interrupt threshold */
    __IO uint32_t RESERVED;     /* 0x044 */
    __IO uint32_t CFGR;         /* 0x048 Configuration (system reset, etc.) */
    __I  uint32_t GISR;         /* 0x04C Global interrupt status */
    __IO uint8_t  VTFIDR[4];    /* 0x050 VTF interrupt ID */
    uint8_t       RESERVED0[12];
    __IO uint32_t VTFADDR[4];   /* 0x060 VTF address */
    uint8_t       RESERVED1[0x90];
    __O  uint32_t IENR[8];      /* 0x100 Interrupt enable set */
    uint8_t       RESERVED2[0x60];
    __O  uint32_t IRER[8];      /* 0x180 Interrupt enable reset */
    uint8_t       RESERVED3[0x60];
    __O  uint32_t IPSR[8];      /* 0x200 Interrupt pending set */
    uint8_t       RESERVED4[0x60];
    __O  uint32_t IPRR[8];      /* 0x280 Interrupt pending reset */
    uint8_t       RESERVED5[0x60];
    __IO uint32_t IACTR[8];     /* 0x300 Interrupt active */
    uint8_t       RESERVED6[0xE0];
    __IO uint8_t  IPRIOR[256];  /* 0x400 Interrupt priority */
    uint8_t       RESERVED7[0x810];
    __IO uint32_t SCTLR;        /* 0xD10 System control */
} PFIC_Type;

#define PFIC  ((PFIC_Type *)0xE000E000U)
#define NVIC  PFIC

#define PFIC_KEY3       0xBEEF0000U
#define PFIC_SYSRST     (1U << 7)

#define USBFS_IRQn      45

static inline void NVIC_EnableIRQ(uint32_t irq) {
    PFIC->IENR[irq >> 5] = 1U << (irq & 0x1F);
}
static inline void NVIC_DisableIRQ(uint32_t irq) {
    PFIC->IRER[irq >> 5] = 1U << (irq & 0x1F);
    __asm volatile("fence.i");
}

/* ── USBFS Device ─────────────────────────────────────────────── */
typedef struct {
    __IO uint8_t  BASE_CTRL;     /* 0x00 */
    __IO uint8_t  UDEV_CTRL;     /* 0x01 */
    __IO uint8_t  INT_EN;        /* 0x02 */
    __IO uint8_t  DEV_ADDR;      /* 0x03 */
    uint8_t       RESERVED0;     /* 0x04 */
    __IO uint8_t  MIS_ST;        /* 0x05 */
    __IO uint8_t  INT_FG;        /* 0x06 */
    __IO uint8_t  INT_ST;        /* 0x07 */
    __IO uint16_t RX_LEN;        /* 0x08 */
    uint16_t      RESERVED1;     /* 0x0A */
    __IO uint8_t  UEP4_1_MOD;    /* 0x0C */
    __IO uint8_t  UEP2_3_MOD;    /* 0x0D */
    __IO uint8_t  UEP567_MOD;    /* 0x0E */
    uint8_t       RESERVED2;     /* 0x0F */
    __IO uint32_t UEP0_DMA;      /* 0x10 */
    __IO uint32_t UEP1_DMA;      /* 0x14 */
    __IO uint32_t UEP2_DMA;      /* 0x18 */
    __IO uint32_t UEP3_DMA;      /* 0x1C */
    __IO uint16_t UEP0_TX_LEN;   /* 0x20 */
    __IO uint16_t UEP0_CTRL_H;   /* 0x22 */
    __IO uint16_t UEP1_TX_LEN;   /* 0x24 */
    __IO uint16_t UEP1_CTRL_H;   /* 0x26 */
    __IO uint16_t UEP2_TX_LEN;   /* 0x28 */
    __IO uint16_t UEP2_CTRL_H;   /* 0x2A */
    __IO uint16_t UEP3_TX_LEN;   /* 0x2C */
    __IO uint16_t UEP3_CTRL_H;   /* 0x2E */
    __IO uint16_t UEP4_TX_LEN;   /* 0x30 */
    __IO uint16_t UEP4_CTRL_H;   /* 0x32 */
} USBFSD_TypeDef;

#define USBFS_BASE  (AHBPERIPH_BASE + 0x3400U)
#define USBFSD      ((USBFSD_TypeDef *)USBFS_BASE)

/* BASE_CTRL bits */
#define USBFS_UC_DMA_EN       0x01
#define USBFS_UC_CLR_ALL      0x02
#define USBFS_UC_RESET_SIE    0x04
#define USBFS_UC_INT_BUSY     0x08
#define USBFS_UC_DEV_PU_EN    0x20

/* DEV_ADDR bits */
#define USBFS_UDA_GP_BIT      0x80

/* UDEV_CTRL bits */
#define USBFS_UD_PORT_EN      0x01
#define USBFS_UD_PD_DIS       0x80

/* INT_EN bits */
#define USBFS_UIE_BUS_RST     0x01
#define USBFS_UIE_TRANSFER    0x02
#define USBFS_UIE_SUSPEND     0x04
#define USBFS_UIE_SOF         0x08
#define USBFS_UIE_DEV_SOF     0x80

/* INT_FG bits */
#define USBFS_UIF_BUS_RST     0x01
#define USBFS_UIF_TRANSFER    0x02
#define USBFS_UIF_SUSPEND     0x04
#define USBFS_UIF_HST_SOF     0x08
#define USBFS_UIF_FIFO_OV     0x10

/* INT_ST bits */
#define USBFS_UIS_ENDP_MASK   0x0F
#define USBFS_UIS_TOKEN_MASK  0x30
#define USBFS_UIS_TOKEN_OUT   0x00
#define USBFS_UIS_TOKEN_SOF   0x10
#define USBFS_UIS_TOKEN_IN    0x20
#define USBFS_UIS_TOKEN_SETUP 0x30
#define USBFS_UIS_TOG_OK      0x40

/* UEP_MOD bits (UEP4_1_MOD) */
#define USBFS_UEP1_TX_EN      0x40
#define USBFS_UEP1_RX_EN      0x80
/* UEP_MOD bits (UEP2_3_MOD) */
#define USBFS_UEP2_TX_EN      0x04
#define USBFS_UEP2_RX_EN      0x08
#define USBFS_UEP3_TX_EN      0x40
#define USBFS_UEP3_RX_EN      0x80

/* UEP CTRL_H bits */
#define USBFS_UEP_T_RES_ACK   0x00
#define USBFS_UEP_T_RES_NONE  0x01
#define USBFS_UEP_T_RES_NAK   0x02
#define USBFS_UEP_T_RES_STALL 0x03
#define USBFS_UEP_T_RES_MASK  0x03
#define USBFS_UEP_T_TOG       (1U << 6)
#define USBFS_UEP_T_AUTO_TOG  (1U << 4)

#define USBFS_UEP_R_RES_ACK   0x00
#define USBFS_UEP_R_RES_NONE  0x04
#define USBFS_UEP_R_RES_NAK   0x08
#define USBFS_UEP_R_RES_STALL 0x0C
#define USBFS_UEP_R_RES_MASK  0x0C
#define USBFS_UEP_R_TOG       (1U << 7)
#define USBFS_UEP_R_AUTO_TOG  (1U << 4)

/* ── USBPD ────────────────────────────────────────────────────── */
typedef struct {
    __IO uint16_t CONFIG;        /* 0x00 */
    __IO uint16_t BMC_CLK_CNT;   /* 0x02 */
    __IO uint8_t  CONTROL;       /* 0x04 */
    __IO uint8_t  TX_SEL;        /* 0x05 */
    __IO uint16_t BMC_TX_SZ;     /* 0x06 */
    __IO uint8_t  DATA_BUF;      /* 0x08 */
    __IO uint8_t  STATUS;        /* 0x09 */
    __IO uint16_t BMC_BYTE_CNT;  /* 0x0A */
    __IO uint16_t PORT_CC1;      /* 0x0C */
    __IO uint16_t PORT_CC2;      /* 0x0E */
} USBPD_TypeDef;

#define USBPD_BASE  (AHBPERIPH_BASE + 0x7000U)
#define USBPD       ((USBPD_TypeDef *)USBPD_BASE)

/* PORT_CCx bits */
#define CC_PD        (1U << 1)
#define CC_CMP_66    (5U << 5)

/* ── IWDG ─────────────────────────────────────────────────────── */
typedef struct {
    __IO uint32_t CTLR;   /* 0x00 Key register */
    __IO uint32_t PSCR;   /* 0x04 Prescaler */
    __IO uint32_t RLDR;   /* 0x08 Reload */
    __IO uint32_t STATR;  /* 0x0C Status */
} IWDG_TypeDef;

#define IWDG_BASE  (APB1PERIPH_BASE + 0x3000U)
#define IWDG       ((IWDG_TypeDef *)IWDG_BASE)

/* ── ESIG (Electronic Signature) ──────────────────────────────── */
#define ESIG_UID_BASE     0x1FFFF7E8U  /* 96-bit unique ID (12 bytes) */
#define ESIG_FLACAP       0x1FFFF7E0U  /* Flash capacity (16 bits, KB) */

/* ── Interrupt enable/disable ─────────────────────────────────── */
static inline void __disable_irq(void) {
    __asm volatile("csrc 0x800, %0" :: "r"(0x88));
}
static inline void __enable_irq(void) {
    __asm volatile("csrs 0x800, %0" :: "r"(0x88));
}

#ifdef __cplusplus
}
#endif

#endif /* CH32X_REGS_H */
