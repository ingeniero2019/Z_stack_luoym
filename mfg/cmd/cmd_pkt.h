#ifndef CMD_PKT_H
#define CMD_PKT_H

#ifdef __cplusplus
extern "C"
{
#endif

/******************************************************************************
 Include
 *****************************************************************************/
#include "zcl_port.h"
#include "cmd.h"

/******************************************************************************
 Macro
 *****************************************************************************/

#define CMD_STATE_IDLE       0x00
#define CMD_STATE_START      0x01
#define CMD_STATE_PAYLOAD    0x02
#define CMD_STATE_ERRO       0xFF

//[0xA5][0x5A][len]{payload}
//                 [DIR-SEQ][class]{cmd-frame}[CRC16]
//                           0x00 CFG
//                           0x01 NWK
//                           0x02 ZCL    [NodeID][EP][cluster][CTR][SEQ][cmd]

#define CMD_HEADER0          0xA5
#define CMD_HEADER1          0x5A
#define CMD_HDR_SIZE         2
#define CMD_LEN_SIZE         sizeof(uint8_t)
#define CMD_CHK_SIZE         sizeof(uint16_t)
#define CMD_CONST_SIZE       (CMD_HDR_SIZE+CMD_LEN_SIZE+CMD_CHK_SIZE)

#define CMD_MIN_LEN          4

#ifndef CMD_UART_BAUD
#define CMD_UART_BAUD        460800
#endif

#define CMD_IDLE_TICK        (1000000ul/CMD_UART_BAUD)  // 1 byte tick, tick is 10us
#if ( CMD_IDLE_TICK < 1 )
  #error  uart baud is too high
#endif

#define CMD_TX_DELAY         (CMD_IDLE_TICK*5)   // 5-byte Tx idle
#define CMD_RX_TIMEOUT       (CMD_IDLE_TICK*128) // 128-byte RX timeout


#define CMD_IDX_TYPE         0
#define CMD_IDX_HANDLE       1
#define CMD_IDX_STATUS       2
#define CMD_IDX_DATA         3

#define CMD_STATUS_SUCCESS   0x00
#define CMD_STATUS_FAIL      0x01
#define CMD_STAtUS_ERRO      0x02

#define CMD_STATUS_INVALID   0xFF

#define CMD_ACK_MAX_LEN      32

#define CMD_ERRO_HANDLE      0xFF
#define CMD_CRC_INIT         0xFFFF

/******************************************************************************
 Typedef
 *****************************************************************************/

typedef void (*pCmdIndCB_t)(uint8_t cmdSeq, uint8_t cmdClass, uint8_t cmdLen, uint8_t* cmdData);
typedef void (*pCmdSendCB_t)(void* param);

/*
 * Public Function
 */
extern void CmdPkt_Init(uint8_t entity, endPointDesc_t *pEpDesc);
extern bool CmdPkt_Receive(void);
extern void CmdPkt_RxInd(uint8_t* buf, uint16_t count);
extern void CmdPkt_RxCmdPoll(void);

#define CmdPkt_TxSend(a,b)  CmdPkt_TxSendEx(a,b,NULL,NULL)
extern bool CmdPkt_TxSendEx(uint8_t len, uint8_t* payload, pCmdSendCB_t cb, void* param);
extern void CmdPkt_TxCnf(void* buf);
extern void CmdPkt_TxCmdPoll(void);

extern uint16_t CmdPkt_Check(uint16_t crc, uint8_t* buf, uint16_t len);

extern uint8_t CmdPkt_GetSeqNum(void);
extern bool CmdPkt_AckSend(uint8_t cmdSeq, uint8_t cmdClass, uint8_t len, uint8_t* extData);

#ifdef __cplusplus
}
#endif

#endif  /* APP_CMD_H */
