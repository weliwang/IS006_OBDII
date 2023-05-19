/*
 *  ======== ti_drivers_config.h ========
 *  Configured TI-Drivers module declarations
 *
 *  The macros defines herein are intended for use by applications which
 *  directly include this header. These macros should NOT be hard coded or
 *  copied into library source code.
 *
 *  Symbols declared as const are intended for use with libraries.
 *  Library source code must extern the correct symbol--which is resolved
 *  when the application is linked.
 *
 *  DO NOT EDIT - This file is generated for the CC1352R1F3RGZ
 *  by the SysConfig tool.
 */
#ifndef ti_drivers_config_h
#define ti_drivers_config_h

#define CONFIG_SYSCONFIG_PREVIEW

#define CONFIG_CC1352R1F3RGZ
#ifndef DeviceFamily_CC13X2
#define DeviceFamily_CC13X2
#endif

#include <ti/devices/DeviceFamily.h>

#include <stdint.h>

/* support C++ sources */
#ifdef __cplusplus
extern "C" {
#endif


/*
 *  ======== CCFG ========
 */


/*
 *  ======== AESCCM ========
 */

extern const uint_least8_t                  CONFIG_AESCCM0_CONST;
#define CONFIG_AESCCM0                      0
#define CONFIG_TI_DRIVERS_AESCCM_COUNT      1


/*
 *  ======== AESCTR ========
 */

extern const uint_least8_t                  CONFIG_AESCTR_0_CONST;
#define CONFIG_AESCTR_0                     0
#define CONFIG_TI_DRIVERS_AESCTR_COUNT      1


/*
 *  ======== AESCTRDRBG ========
 */

extern const uint_least8_t                      CONFIG_AESCTRDRBG_0_CONST;
#define CONFIG_AESCTRDRBG_0                     0
#define CONFIG_TI_DRIVERS_AESCTRDRBG_COUNT      1


/*
 *  ======== AESECB ========
 */

extern const uint_least8_t                  CONFIG_AESECB0_CONST;
#define CONFIG_AESECB0                      0
extern const uint_least8_t                  CONFIG_AESECB_0_CONST;
#define CONFIG_AESECB_0                     1
#define CONFIG_TI_DRIVERS_AESECB_COUNT      2


/*
 *  ======== ECDH ========
 */

extern const uint_least8_t              CONFIG_ECDH0_CONST;
#define CONFIG_ECDH0                    0
#define CONFIG_TI_DRIVERS_ECDH_COUNT    1


/*
 *  ======== GPIO ========
 */

/* DIO15 */
extern const uint_least8_t              GPS_STANDBY_CONST;
#define GPS_STANDBY                     0
/* DIO14 */
extern const uint_least8_t              GPS_PWR_EN_CONST;
#define GPS_PWR_EN                      1
/* DIO3 */
extern const uint_least8_t              CONFIG_INGI_CONST;
#define CONFIG_INGI                     2
/* DIO6 */
extern const uint_least8_t              CONFIG_DOOR_CONST;
#define CONFIG_DOOR                     3
/* DIO10 */
extern const uint_least8_t              GPIO_3V8_EN_CONST;
#define GPIO_3V8_EN                     4
/* DIO8 */
extern const uint_least8_t              GPIO_4G_PWR_CONST;
#define GPIO_4G_PWR                     5
/* DIO7 */
extern const uint_least8_t              GPIO_4G_RST_CONST;
#define GPIO_4G_RST                     6
/* DIO18 */
extern const uint_least8_t              GPIO_LOCK_CONST;
#define GPIO_LOCK                       7
/* DIO19 */
extern const uint_least8_t              GPIO_UNLOCK_CONST;
#define GPIO_UNLOCK                     8
/* DIO4 */
extern const uint_least8_t              CONFIG_ENG_BTN_CONST;
#define CONFIG_ENG_BTN                  9
/* DIO5 */
extern const uint_least8_t              GPS_nRESET_CONST;
#define GPS_nRESET                      10
#define CONFIG_TI_DRIVERS_GPIO_COUNT    11

/* LEDs are active high */
#define CONFIG_GPIO_LED_ON  (1)
#define CONFIG_GPIO_LED_OFF (0)

#define CONFIG_LED_ON  (CONFIG_GPIO_LED_ON)
#define CONFIG_LED_OFF (CONFIG_GPIO_LED_OFF)


/*
 *  ======== NVS ========
 */

extern const uint_least8_t              CONFIG_NVSINTERNAL_CONST;
#define CONFIG_NVSINTERNAL              0
#define CONFIG_TI_DRIVERS_NVS_COUNT     1


/*
 *  ======== PIN ========
 */
#include <ti/drivers/PIN.h>

extern const PIN_Config BoardGpioInitTable[];

/* Parent Signal: CONFIG_DISPLAY_UART TX, (DIO13) */
#define CONFIG_PIN_UART_TX                   0x0000000d
/* Parent Signal: CONFIG_DISPLAY_UART RX, (DIO12) */
#define CONFIG_PIN_UART_RX                   0x0000000c
/* Parent Signal: GPS_STANDBY GPIO Pin, (DIO15) */
#define CONFIG_PIN_BTN1                   0x0000000f
/* Parent Signal: GPS_PWR_EN GPIO Pin, (DIO14) */
#define CONFIG_PIN_BTN2                   0x0000000e
/* Parent Signal: CONFIG_INGI GPIO Pin, (DIO3) */
#define CONFIG_PIN_0                   0x00000003
/* Parent Signal: CONFIG_DOOR GPIO Pin, (DIO6) */
#define CONFIG_PIN_1                   0x00000006
/* Parent Signal: GPIO_3V8_EN GPIO Pin, (DIO10) */
#define CONFIG_PIN_4                   0x0000000a
/* Parent Signal: GPIO_4G_PWR GPIO Pin, (DIO8) */
#define CONFIG_PIN_5                   0x00000008
/* Parent Signal: GPIO_4G_RST GPIO Pin, (DIO7) */
#define CONFIG_PIN_6                   0x00000007
/* Parent Signal: GPIO_LOCK GPIO Pin, (DIO18) */
#define CONFIG_PIN_7                   0x00000012
/* Parent Signal: GPIO_UNLOCK GPIO Pin, (DIO19) */
#define CONFIG_PIN_8                   0x00000013
/* Parent Signal: CONFIG_ENG_BTN GPIO Pin, (DIO4) */
#define CONFIG_PIN_9                   0x00000004
/* Parent Signal: GPS_nRESET GPIO Pin, (DIO5) */
#define CONFIG_PIN_10                   0x00000005
/* Parent Signal: CONFIG_UART_0 TX, (DIO11) */
#define CONFIG_PIN_2                   0x0000000b
/* Parent Signal: CONFIG_UART_0 RX, (DIO16) */
#define CONFIG_PIN_3                   0x00000010
#define CONFIG_TI_DRIVERS_PIN_COUNT    15




/*
 *  ======== TRNG ========
 */

extern const uint_least8_t              CONFIG_TRNG_0_CONST;
#define CONFIG_TRNG_0                   0
#define CONFIG_TI_DRIVERS_TRNG_COUNT    1


/*
 *  ======== UART ========
 */

/*
 *  TX: DIO13
 *  RX: DIO12
 */
extern const uint_least8_t              CONFIG_DISPLAY_UART_CONST;
#define CONFIG_DISPLAY_UART             0
/*
 *  TX: DIO11
 *  RX: DIO16
 */
extern const uint_least8_t              CONFIG_UART_0_CONST;
#define CONFIG_UART_0                   1
#define CONFIG_TI_DRIVERS_UART_COUNT    2


/*
 *  ======== Watchdog ========
 */

extern const uint_least8_t                  CONFIG_WATCHDOG_0_CONST;
#define CONFIG_WATCHDOG_0                   0
#define CONFIG_TI_DRIVERS_WATCHDOG_COUNT    1


/*
 *  ======== Board_init ========
 *  Perform all required TI-Drivers initialization
 *
 *  This function should be called once at a point before any use of
 *  TI-Drivers.
 */
extern void Board_init(void);

/*
 *  ======== Board_initGeneral ========
 *  (deprecated)
 *
 *  Board_initGeneral() is defined purely for backward compatibility.
 *
 *  All new code should use Board_init() to do any required TI-Drivers
 *  initialization _and_ use <Driver>_init() for only where specific drivers
 *  are explicitly referenced by the application.  <Driver>_init() functions
 *  are idempotent.
 */
#define Board_initGeneral Board_init

#ifdef __cplusplus
}
#endif

#endif /* include guard */
