#include "ti_drivers_config.h"
#include <ti/sysbios/knl/Semaphore.h>
#include <xdc/std.h>
#include <ti/sysbios/knl/Semaphore.h>
#include "string.h"
#include "hal_types.h"
#include "util_timer.h"
#include "zcl.h"
#include "osal_port.h"

#include "hal_defs.h"
#include "cmd.h"
#include "cmd_pkt.h"
#include "cmd_uart.h"
#include "cmd_nwk.h"
#include "cmd_zcl.h"

/*
 * macro
 */


/*
 * typedef
 */
typedef struct
{
    void*  next;
    uint8_t* payload;
    uint8_t  len;
    uint8_t  count;
}cmdPkt_RxCmd_t;

typedef struct
{
    void*  next;
    uint8_t* pData;
    uint16_t size;
    pCmdSendCB_t cb;
    void* param;
}cmdPkt_TxBuf_t;

/*
 * local variable
 */
//uart task
uint8_t  cmdPktTaskId = 0xff;

//uart trans packet
uint8_t cmdPkt_State = CMD_STATE_IDLE;
uint8_t cmdPkd_LastCh = 0x00;
cmdPkt_RxCmd_t* pCmdPkt_IndCmdList = NULL;
cmdPkt_RxCmd_t* pCmdPkt_RxCmd = NULL;
//cmdPkt_HdrBuf_t* pCmdPkt_HdrBuf = NULL;
Clock_Struct cmdPkt_RxTimeoutClkStruct;

cmdPkt_TxBuf_t* pCmdPkt_TxBufList = NULL;
cmdPkt_TxBuf_t* pCmdPkt_TxCbList = NULL;
Clock_Struct cmdPkt_TxTriggerClkStruct;

static uint8_t cmdPkt_OutSeq = 0;

const pCmdIndCB_t CmdPktIndCallbackTab[] = CMD_IND_ARRAY;

uint16_t CmdPkt_RxCharInd(uint8_t ch);
void CmdPkt_RxTimeoutCB(UArg a0);
void CmdPkt_RxCmdPush(cmdPkt_RxCmd_t* newItem);
void CmdPkt_Process(cmdPkt_RxCmd_t *rxCmd);

void CmdPkt_AckInd(uint8_t cmdSeq, uint8_t cmdClass);

void CmdPkt_TxPop(void);
bool CmdPkt_TxPush(cmdPkt_TxBuf_t* newItem);
void CmdPkt_TxTriggerCB(UArg a0);
void CmdPkt_TxCbPush(cmdPkt_TxBuf_t* newItem);

void CmdPkt_ErroWarning(uint8_t erro);
void CmdPkt_InvalidCmd(uint8_t cmdType, uint8_t handle);

//===========================  cmd timer clock, 100us cycle   =====================//
#define  cmd_clock_stop    Timer_stop
#define  cmd_clock_active  Timer_isActive
Clock_Handle cmd_clock_construct(Clock_Struct *pClock, Clock_FuncPtr clockCB, uint32_t clockTicks);
void cmd_clock_startEx(Clock_Struct *pClock, uint32_t clockTicks);

void CmdPkt_Init(uint8_t entity, endPointDesc_t *pEpDesc)
{
  cmdZcl_InitSrcEP(pEpDesc,TRUE);
  if(cmdPktTaskId==0xFF)
  {
    cmdPktTaskId = entity;
  }
  
  //start timer
  cmd_clock_construct( &cmdPkt_RxTimeoutClkStruct, CmdPkt_RxTimeoutCB, CMD_RX_TIMEOUT );
  cmd_clock_construct( &cmdPkt_TxTriggerClkStruct, CmdPkt_TxTriggerCB, CMD_TX_DELAY );
  //init uart
  cmd_uartInit(CMD_UART_BAUD);
  //init seq
  cmdPkt_OutSeq = (uint8_t)(MFG_random() & 0x7F);
}

void CmdPkt_RxInd(uint8_t* buf, uint16_t count)
{
  uint8_t *ch = buf;
  uint16_t timeout = CMD_STATE_ERRO;
  while(count)
  {
    timeout = CmdPkt_RxCharInd(*ch++);
    count--;
  }
  if(timeout == 0)
  {
    cmd_clock_stop( &cmdPkt_RxTimeoutClkStruct );
  }
  else
  {
    cmd_clock_startEx( &cmdPkt_RxTimeoutClkStruct, timeout );
  }
}

uint16_t CmdPkt_RxCharInd(uint8_t ch)
{
  uint16_t timeout = CMD_RX_TIMEOUT;
  switch(cmdPkt_State)
  {
  case CMD_STATE_IDLE:
    if(ch == CMD_HEADER1)
    {
      if(cmdPkd_LastCh == CMD_HEADER0)
      {
        cmdPkt_State = CMD_STATE_START;
        break;
      }
    }
    cmdPkd_LastCh = ch;
    break;
    
  case CMD_STATE_START:
    {
      uint8_t len = ch;
      if( (pCmdPkt_RxCmd == NULL) && (len >= CMD_MIN_LEN) )
      {
        pCmdPkt_RxCmd = MFG_malloc(sizeof(cmdPkt_RxCmd_t)+len);
        if(pCmdPkt_RxCmd)
        {
          pCmdPkt_RxCmd->next = NULL;
          pCmdPkt_RxCmd->payload = (uint8_t*)(pCmdPkt_RxCmd + 1);
          pCmdPkt_RxCmd->len = len;
          pCmdPkt_RxCmd->count = 0;
          cmdPkt_State = CMD_STATE_PAYLOAD;
          break;
        }
      }
      //send invalid ack
      cmdPkt_State = CMD_STATE_ERRO;
    }
    break;
  case CMD_STATE_PAYLOAD:
    if(pCmdPkt_RxCmd)
    {
      pCmdPkt_RxCmd->payload[pCmdPkt_RxCmd->count++] = ch;
      if(pCmdPkt_RxCmd->count >= pCmdPkt_RxCmd->len)
      {
        MFG_CSState key;
        key = MFG_enterCS();
        CmdPkt_RxCmdPush(pCmdPkt_RxCmd);
        pCmdPkt_RxCmd = NULL;
        cmdPkt_State = CMD_STATE_IDLE;
        MFG_leaveCS(key);
        timeout = 0;//rx_success = TRUE;
      }
    }
    else
    {
      cmdPkt_State = CMD_STATE_ERRO;
    }
    break;
    
  case CMD_STATE_ERRO:
    {
      //timeout = CMD_ERRO_TIMEOUT;
    }
    break;
    
  default:
    
    break;
  }
  return timeout;
}

void CmdPkt_RxTimeoutCB(UArg a0)
{
  MFG_CSState key;
  key = MFG_enterCS();
  cmdPkt_State = CMD_STATE_IDLE;
  if(pCmdPkt_RxCmd)
  {
    MFG_free(pCmdPkt_RxCmd);
    pCmdPkt_RxCmd = NULL;
  }
  MFG_leaveCS(key);
  CmdPkt_ErroWarning( 0xFF );//Rx timeout
}

void CmdPkt_RxCmdPush(cmdPkt_RxCmd_t* newItem)
{
  if(newItem == NULL)
  {
    return;
  }
  if(pCmdPkt_IndCmdList == NULL)
  {
    pCmdPkt_IndCmdList = newItem;
  }
  else
  {
    cmdPkt_RxCmd_t* findTail = pCmdPkt_IndCmdList;
    while(findTail->next)
    {
      findTail = findTail->next;
    }
    findTail->next = newItem;
  }
  MFG_setEvent(cmdPktTaskId, MFG_CMD_RX_EVENT);
}

void CmdPkt_RxCmdPoll(void)
{
  while(pCmdPkt_IndCmdList)
  {
    cmdPkt_RxCmd_t *item = pCmdPkt_IndCmdList;
    pCmdPkt_IndCmdList = item->next;
    CmdPkt_Process(item);
    MFG_free(item);
  }
}

bool CmdPkt_TxSendEx(uint8_t len, uint8_t* payload, pCmdSendCB_t cb, void* param)
{
  cmdPkt_TxBuf_t *txBuf;
  txBuf = MFG_malloc(sizeof(cmdPkt_TxBuf_t) + CMD_CONST_SIZE + len);
  if(txBuf)
  {
    uint16_t crc;
    txBuf->next = NULL;
    txBuf->pData = (uint8_t*)(txBuf+1);
    txBuf->size = CMD_CONST_SIZE + len;
    txBuf->cb = cb;
    txBuf->param = param;
    txBuf->pData[0] = CMD_HEADER0;
    txBuf->pData[1] = CMD_HEADER1;
    txBuf->pData[2] = len + CMD_CHK_SIZE;
    memcpy(&txBuf->pData[3], payload, len);
    crc = CmdPkt_Check(CMD_CRC_INIT, &txBuf->pData[3], len);
    txBuf->pData[txBuf->size-2] = LO_UINT16(crc);
    txBuf->pData[txBuf->size-1] = HI_UINT16(crc);
    //add buf into tx list
    if(CmdPkt_TxPush(txBuf) && (!cmd_clock_active(&cmdPkt_TxTriggerClkStruct)))
    {
      cmd_clock_startEx( &cmdPkt_TxTriggerClkStruct, 0 ); //CMD_IDLE_TICK
    }
    return true;
  }
  return false;
}

bool CmdPkt_TxPush(cmdPkt_TxBuf_t* newItem)
{
  bool ret = false;
  MFG_CSState key;
  key = MFG_enterCS();
  newItem->next = NULL;
  if(pCmdPkt_TxBufList == NULL)
  {
    pCmdPkt_TxBufList = newItem;
    ret = true;
  }
  else
  {
    cmdPkt_TxBuf_t *findTail = pCmdPkt_TxBufList;
    while(findTail->next)
    {
      findTail = findTail->next;
    }
    findTail->next = newItem;
  }
  MFG_leaveCS(key);
  return ret;
}

void CmdPkt_TxCnf(void* buf)
{
  if(buf)
  {
    cmdPkt_TxBuf_t *item = buf;
    item -= 1;
    if(item == pCmdPkt_TxBufList)
    {
      pCmdPkt_TxBufList = pCmdPkt_TxBufList->next;
      MFG_free(item);
    }
    cmd_clock_startEx( &cmdPkt_TxTriggerClkStruct, CMD_TX_DELAY );
  }
}

void CmdPkt_TxCbPush(cmdPkt_TxBuf_t* newItem)
{
  MFG_CSState key;
  key = MFG_enterCS();
  newItem->next = NULL;
  if(pCmdPkt_TxCbList == NULL)
  {
    pCmdPkt_TxCbList = newItem;
  }
  else
  {
    cmdPkt_TxBuf_t *findTail = pCmdPkt_TxCbList;
    while(findTail->next)
    {
      findTail = findTail->next;
    }
    findTail->next = newItem;
  }
  MFG_leaveCS(key);
}

void CmdPkt_TxTriggerCB(UArg a0)
{
  MFG_CSState key;
  key = MFG_enterCS();
  CmdPkt_TxPop();
  MFG_leaveCS(key);
}

void CmdPkt_TxCmdPoll(void)
{
  while(pCmdPkt_TxCbList)
  {
    cmdPkt_TxBuf_t* item = pCmdPkt_TxCbList;
    pCmdPkt_TxCbList = item->next;
    if(item->cb)
    {
      item->cb(item->param);
    }
    MFG_free(item);
  }
}

void CmdPkt_Process(cmdPkt_RxCmd_t *rxCmd)
{
  uint16_t crcLen = rxCmd->len - 2;
  uint16_t crcInd = BUILD_UINT16(rxCmd->payload[crcLen], rxCmd->payload[crcLen+1]);
  uint16_t crc = CmdPkt_Check(CMD_CRC_INIT, rxCmd->payload, crcLen);
#ifdef CRC_EN
  if(crc == crcInd)
#else
  if((crc == crcInd) || (crcInd == 0x0000))
#endif
  {
    bool ackCmd = (rxCmd->payload[0] & 0x80)? true:false;
    uint8_t cmdSeq = rxCmd->payload[0] & 0x7F;
    uint8_t cmdClass = rxCmd->payload[1];
    uint8_t cmdCount = sizeof(CmdPktIndCallbackTab)/sizeof(pCmdIndCB_t);
    if(ackCmd)
    {
      CmdPkt_AckInd(cmdSeq, cmdClass);//process ack
    }
    else if(cmdClass < cmdCount)
    {
      if(CmdPktIndCallbackTab[cmdClass])
      {
        CmdPktIndCallbackTab[cmdClass](cmdSeq, cmdClass, crcLen-2, rxCmd->payload+2);
      }
    }
    else
    {
      CmdPkt_InvalidCmd(cmdSeq, cmdClass);
    }
  }
  else
  {
    CmdPkt_ErroWarning(0xFE);//CRC Erro
  }
}

void CmdPkt_AckInd(uint8_t cmdSeq, uint8_t cmdClass)
{
  
}

void CmdPkt_TxPop(void)
{
  if(pCmdPkt_TxBufList)
  {
    cmd_uartSend(pCmdPkt_TxBufList->pData, pCmdPkt_TxBufList->size);
  }
}

static uint16_t calccrc(uint8_t crcbuf,uint16_t crc)
{
  uint8_t i;
  uint8_t chk;
  crc=crc ^ crcbuf;
  for(i=0;i<8;i++)
  {
    chk=crc&1; crc=crc>>1;
    crc=crc&0x7fff;
    if (chk==1)
      crc=crc^0xa001;
    crc=crc&0xffff;
  }
  return crc;
}

uint16_t CmdPkt_Check(uint16_t crc, uint8_t* buf, uint16_t len)
{
  uint16_t i;
  for(i=0;i<len;i++)
  {
    crc = calccrc(*buf,crc); buf++;
  }
  return crc;
}

bool CmdPkt_AckSend(uint8_t cmdSeq, uint8_t cmdClass, uint8_t len, uint8_t* extData)
{
  uint8_t ack_cmd[CMD_ACK_MAX_LEN];
  ack_cmd[0] = cmdSeq | 0x80;
  ack_cmd[1] = cmdClass;
  if(len > (CMD_ACK_MAX_LEN-2))
  {
    len = (CMD_ACK_MAX_LEN-2);
  }
  memcpy(ack_cmd+2,extData,len);
  return CmdPkt_TxSend(len+2, ack_cmd);
}

uint8_t CmdPkt_GetSeqNum(void)
{
  cmdPkt_OutSeq ++;
  cmdPkt_OutSeq &= 0x7F;
  return cmdPkt_OutSeq;
}


void CmdPkt_ErroWarning(uint8_t erro)
{
  uint8_t erro_cmd[CMD_ACK_MAX_LEN];
  erro_cmd[0] = CmdPkt_GetSeqNum();
  erro_cmd[1] = 0xFF;
  erro_cmd[2] = erro;
  CmdPkt_TxSend(3, erro_cmd);
}

void CmdPkt_InvalidCmd(uint8_t cmdSeq, uint8_t cmdClass)
{
  CmdPkt_AckSend(cmdSeq, cmdClass, 0, NULL);
}

//===========================  cmd timer clock, 100us cycle   =====================//

Clock_Handle cmd_clock_construct(Clock_Struct *pClock, Clock_FuncPtr clockCB, uint32_t clockTicks)
{
    Clock_Params clockParams;
    Clock_Params_init(&clockParams);
    clockParams.arg = 0;
    clockParams.period = 0;
    clockParams.startFlag = FALSE;
    Clock_construct(pClock, clockCB, clockTicks, &clockParams);
    return Clock_handle(pClock);
}

void cmd_clock_startEx(Clock_Struct *pClock, uint32_t clockTicks)
{
    Clock_Handle handle = Clock_handle(pClock);
    if (Clock_isActive(handle))
    {
      // Stop clock first
      Clock_stop(handle);
    }
    Clock_setTimeout(handle, clockTicks );
    Clock_start(handle);
}
