#ifndef _MFG_STACK_H
#define _MFG_STACK_H

/* INCLUDE */
#include "../mfg.h"

/* MACRO */


/* typdef */

typedef enum
{
  Mfg_Denied_Join,
  Mfg_Device_Leave,
  Mfg_Report_Send,
  Mfg_Max_End
}mfgMsgType_t;

//Message Header
typedef struct
{
  void* next;
  mfgMsgType_t type;
  uint8_t size;
  uint8_t data[];
}mfgMsgHdr_t;

typedef void (*pMfgMsgCB_t)(mfgMsgType_t msgType, uint8_t size, uint8_t *data);

//Message ZDP Confirm
typedef struct
{
  uint8_t status;
  uint8_t transID;
  void* cnfParam;
}mfgMsgZdpCnf_t;

//Message Device Leave
typedef struct
{
  uint16_t nwkAddr;
  uint8_t  extAddr[8];
  uint16_t parentAddr;
}mfgMsgDeviceLeave_t;

typedef struct
{
  uint8_t  extAddr[8];
  uint16_t parentAddr;
  uint8_t  rejoin;
}mfgMsgDeniedJoin_t;

typedef struct
{
  uint16_t dstAddr;
  uint8_t  extAddr[Z_EXTADDR_LEN];
  uint8_t  removeChildren;
  uint8_t  rejoin;
  uint8_t  status;
}mfgMsgLeaveCnf_t;

typedef struct
{
  uint16_t attrID;
  uint8_t  dataType;
  uint8_t  attrData[4];
} mfgReportAttr_t;

typedef struct
{
  uint8_t  srcEP;
  uint8_t  dstEP;
  uint16_t clusterID;
  uint8_t  direction;
  uint8_t  disableDefaultRsp;
  uint16_t manuCode;
  uint8_t  seqNum;
  uint8_t  numAttr;
  mfgReportAttr_t attrList[];
}mfgMsgReportSend_t;

/****************************************************
               function 
*/

extern void MFG_InitCallback(void);
extern void MFG_PeriodCallback(void);

extern void MFG_StackInit(uint8_t destTaskId);
extern void MFG_SetupZstackCallbacks(void);
extern uint8_t MFG_TaskId(void);
extern void MFG_HeapMgr(void);

extern uint8_t MFG_Channel(void);
extern uint16_t MFG_NwkAddr(void);
extern uint16_t MFG_PanId(void);

extern mfgMsgHdr_t* MFG_CreatMsg(mfgMsgType_t type, uint8_t size);
extern void MFG_SendMsg(mfgMsgHdr_t* pMsg);
extern mfgMsgHdr_t* MFG_GetMsg(void);
extern void MFG_MfgMsgPoll(void);

extern void MFG_WatchDogOpen(void);
extern void MFG_WatchDogFeed(void);   

extern bool MFG_SeqRecMatch(uint16_t nwkAddr, uint8_t ep, uint8_t seq);
extern void MFG_SeqRecRelease(void);

extern void MFG_RandomExtPanid(void);
extern bool MFG_DeleteAddrMgr(uint8_t *extAddr, uint8_t user);
extern void MFG_AddrMgrInit(void);

extern void MFG_LoopReset(void);
extern void MFG_Reset(void);

/****************************************************
               Variable
*/

extern pMfgMsgCB_t mfgMsgCBs[Mfg_Max_End];

//extern uint8_t MFG_StartScan()


#endif
