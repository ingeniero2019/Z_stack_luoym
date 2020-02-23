#include "ti_drivers_config.h"
#include <ti/sysbios/knl/Semaphore.h>
#include <xdc/std.h>
#include <ti/drivers/UART.h>
#include "hal_types.h"
#include "util_timer.h"

#include "cmd_pkt.h"
#include "cmd_uart.h"

#define CMD_RETURN_PARTIAL_ENABLE    (UART_CMD_RESERVED + 0)
#define CMD_RETURN_PARTIAL_DISABLE   (UART_CMD_RESERVED + 1)

UART_Handle cmdUartHandle;
uint8_t* pCmdUartRxBuf;

void cmd_uartReadCallback(UART_Handle handle, void *buf, size_t count);
void cmd_uartWriteCallback(UART_Handle handle, void *buf, size_t count);

void cmd_uartInit(uint32_t baud)
{
  UART_Params params;
  UART_init();
  UART_Params_init(&params);
  params.readMode = UART_MODE_CALLBACK;
  params.writeMode = UART_MODE_CALLBACK;
  params.readDataMode = UART_DATA_BINARY;
  params.writeDataMode = UART_DATA_BINARY;
  params.readEcho = UART_ECHO_OFF;
  params.baudRate = baud;
  params.readCallback = cmd_uartReadCallback;
  params.writeCallback = cmd_uartWriteCallback;
  cmdUartHandle = UART_open(CMD_UART_PORT, &params);
  UART_control(cmdUartHandle, CMD_RETURN_PARTIAL_ENABLE, NULL);
  //start rx
  MFG_CSState key;
  key = MFG_enterCS();
  pCmdUartRxBuf = MFG_malloc(CMD_UART_BUF_SIZE);
  UART_read(cmdUartHandle, pCmdUartRxBuf, CMD_UART_BUF_SIZE);
  MFG_leaveCS(key);
}

bool cmd_uartSend(uint8_t* buff, uint16_t size)
{
  if(UART_STATUS_ERROR == UART_write(cmdUartHandle, buff, size))
  {
    return false;
  }
  return true;
}

void cmd_uartReadCallback(UART_Handle handle, void *buf, size_t count)
{
  MFG_CSState key;
  key = MFG_enterCS();
  //Input received data
  CmdPkt_RxInd(buf, count);
  //set next rx
  UART_read(cmdUartHandle, buf, CMD_UART_BUF_SIZE);
  MFG_leaveCS(key);
}

void cmd_uartWriteCallback(UART_Handle handle, void *buf, size_t count)
{
  MFG_CSState key;
  key = MFG_enterCS();
  CmdPkt_TxCnf(buf);
  MFG_leaveCS(key);
}
