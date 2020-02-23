#ifndef CMD_UART_H
#define CMD_UART_H


/******************************************************************************
 Macro
 *****************************************************************************/
#define CMD_UART_PORT          UART_0
#define CMD_UART_BUF_SIZE      32
//#define CMD_UART_BAUD          115200

void cmd_uartInit(uint32_t baud);
bool cmd_uartSend(uint8_t* buff, uint16_t size);


#endif
