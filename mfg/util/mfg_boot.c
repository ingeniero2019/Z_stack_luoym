#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <xdc/std.h>

#include <ti/devices/cc13x2_cc26x2/driverlib/ioc.h>
#include <ti/devices/cc13x2_cc26x2/driverlib/cpu.h>

#include <ti/drivers/pin/PINCC26XX.h>

#include "sys_ctrl.h"

#include "mfg_Stack.h"
#include "mfg.h"
#include "mfg_boot.h"

static PIN_Handle bootPinHandle;

void MFG_BootInit( void )
{
  PIN_Config bootPinTable[] =
  {
    IOID_14 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_INPUT_DIS | PIN_DRVSTR_MIN, /* HGM = H */
    PIN_TERMINATE /* Terminate list     */
  };
  PIN_State bootPinState;
  bootPinHandle = PIN_open(&bootPinState, bootPinTable);
  PIN_setOutputValue(bootPinHandle, IOID_14, 1);
}

void MFG_BootReset( void )
{
  PIN_setOutputValue(bootPinHandle, IOID_14, 0);
  
  //SysCtrlSystemReset();
}


