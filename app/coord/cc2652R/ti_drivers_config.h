/*
 *  ======== ti_drivers_config.h ========
 *  Configured TI-Drivers module declarations
 *
 *  DO NOT EDIT - This file is generated for the CC26X2R1_LAUNCHXL
 *  by the SysConfig tool.
 */
#ifndef ti_drivers_config_h
#define ti_drivers_config_h

#define CONFIG_SYSCONFIG_PREVIEW

#define CONFIG_CC26X2R1_LAUNCHXL

#ifndef DeviceFamily_CC26X2
#define DeviceFamily_CC26X2
#endif

#include <ti/devices/DeviceFamily.h>

#include <stdint.h>

/* support C++ sources */
#ifdef __cplusplus
extern "C" {
#endif


/*
 *  ======== AESCBC ========
 */

#define CONFIG_AESCBC_0             0


/*
 *  ======== AESCCM ========
 */

#define CONFIG_AESCCM_0             0


/*
 *  ======== AESECB ========
 */

#define CONFIG_AESECB_0             0


/*
 *  ======== ECDH ========
 */

#define CONFIG_ECDH_0               0


/*
 *  ======== ECDSA ========
 */

#define CONFIG_ECDSA_0              0


/*
 *  ======== ECJPAKE ========
 */

#define CONFIG_ECJPAKE_0            0


/*
 *  ======== GPIO ========
 */

/* DIO14, LaunchPad Button BTN-2 (Right) */
#define RESET_BOOT                  0

/* LEDs are active high */
#define CONFIG_GPIO_LED_ON  (1)
#define CONFIG_GPIO_LED_OFF (0)

#define CONFIG_LED_ON  (CONFIG_GPIO_LED_ON)
#define CONFIG_LED_OFF (CONFIG_GPIO_LED_OFF)


/*
 *  ======== NVS ========
 */

#define CONFIG_NVSINTERNAL          0


/*
 *  ======== PIN ========
 */

/* Includes */
#include <ti/drivers/PIN.h>

/* Externs */
extern const PIN_Config BoardGpioInitTable[];

/* XDS110 UART, Parent Signal: UART_0 TX, (DIO3) */
#define CONFIG_PIN_0    0x00000003
/* XDS110 UART, Parent Signal: UART_0 RX, (DIO2) */
#define CONFIG_PIN_1    0x00000002
/* LaunchPad Button BTN-2 (Right), Parent Signal: RESET_BOOT GPIO Pin, (DIO14) */
#define CONFIG_PIN_2    0x0000000e




/*
 *  ======== SHA2 ========
 */

#define CONFIG_SHA2_0               0


/*
 *  ======== TRNG ========
 */

#define CONFIG_TRNG_0               0


/*
 *  ======== UART ========
 */

/*
 *  TX: DIO3
 *  RX: DIO2
 *  XDS110 UART
 */
#define UART_0                      0


/*
 *  ======== Watchdog ========
 */

#define CONFIG_WATCHDOG_0           0


/*
 *  ======== Button ========
 */

/* DIO14, LaunchPad Button BTN-2 (Right) */
#define BUTTON_BOOT                 0


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
