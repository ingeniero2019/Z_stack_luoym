#include "mfg_Stack.h"
#include "mfg.h"
#include "mfg_coord.h"
#include "string.h"
#ifdef BDB_REPORTING
    #include "bdb_reporting.h"
#endif

#include "sys_ctrl.h"

#include "zd_sec_mgr.h"
#include "zstackapi.h"
#include "osal_nv.h"
#include "aps_groups.h"
#include "addr_mgr.h"

/*****************  Macro  *****************/

#define  MFG_SEQ_REC_SIZE  32

#define NO_WDOG

/*****************  Const  *****************/

#define  LUOYIMING_MARK  "Louyiming1984"

/*****************  Type Def  ************************/

typedef struct
{
  uint8_t ep;
  uint16_t id;
}mfgGroupItem_t;

/*************     variable **************************/

mfgMsgHdr_t *mfgMsgHdrHead = NULL;
uint8_t  mfgTaskId = 0xFF;

static uint8_t  mfgResetFlag = FALSE;

//uint8_t  mfgChannel = 0;
//uint16 mfgNwkAddr = 0xFFFE;
//uint16 mfgPanId = 0xFFFF;

pMfgMsgCB_t mfgMsgCBs[Mfg_Max_End] = {0};

#ifdef HEAPMGR_METRICS
uint32_t MFG_Heap_BlkMax;
uint32_t MFG_Heap_BlkCnt;
uint32_t MFG_Heap_BlkFree;
uint32_t MFG_Heap_MemAlo;
uint32_t MFG_Heap_MemMax;
uint32_t MFG_Heap_MemUB;
#endif

struct {
  uint8_t num;
  mfgGroupItem_t group[APS_MAX_GROUPS];
}mfgGroupList;

__root const char Lym_Mark[] @ ".lym_mark" = LUOYIMING_MARK;

/**************    zstack call back    **************/

ZStatus_t mfgDeviceValidateCallback( uint16 nwkAddr, uint8_t* extAddr, uint16 parentAddr, bool rejoin );

void mfgDeviceLeaveNotifyCallback( uint16 nwkAddr, uint8_t* extAddr, uint16 parentAddr );

bool mfgSendReportCallback( uint8_t srcEP, afAddrType_t *dstAddr, uint16 clusterID,
                           zclReportCmd_t *reportCmd, uint8_t direction, uint8_t disableDefaultRsp,
                           uint16 manuCode, uint8_t seqNum );

/**********************************************
               zstack callback
*/

/*
* @fn          mfgDeviceValidateCallback
*
* @brief       point to pZDSecMgrDeviceValidateCallback in zd_sec_mgr.h
*
* @param       ZDSecMgrDevice_t in zd_sec_mgr.c
*
* @return      ZSuccess or ZNwkUnknownDevice
*
* */
ZStatus_t mfgDeviceValidateCallback( uint16 nwkAddr, uint8_t* extAddr, uint16 parentAddr, bool rejoin )
{
  if( MFG_WhiteListEnable() )
  {
    //lookup white list
    if( MFG_WhiteListCheck(extAddr) )
    {
      return ZSuccess;
    }
    //out put denied extAddr
    mfgMsgHdr_t* pMsg = MFG_CreatMsg(Mfg_Denied_Join, sizeof(mfgMsgDeniedJoin_t) );
    if(pMsg)
    {
      mfgMsgDeniedJoin_t* pDenied = (mfgMsgDeniedJoin_t*)pMsg->data;
      memcpy(pDenied->extAddr, extAddr, 8);
      pDenied->parentAddr = parentAddr;
      pDenied->rejoin = rejoin;
      MFG_SendMsg(pMsg);
    }
    return ZNwkUnknownDevice;
  }
  else
  {
    if( (ZDSecMgrPermitJoiningStatus() == FALSE) && rejoin )
    {
      return ZNwkUnknownDevice;
    }
    return ZSuccess;
  }
}

/*
* @fn          mfgDeviceLeaveNotifyCallback
*
* @brief       point to pZDSecMgrDeviceLeaveNotifyCallback in zd_sec_mgr.h
*
* @param       Leave Update param
*
* @return      none
*
* */
void mfgDeviceLeaveNotifyCallback( uint16_t nwkAddr, uint8_t *extAddr, uint16_t parentAddr )
{
  mfgMsgHdr_t* pMsg = MFG_CreatMsg( Mfg_Device_Leave, sizeof(mfgMsgDeviceLeave_t) );
  if(pMsg)
  {
    mfgMsgDeviceLeave_t *pDevLeave = (mfgMsgDeviceLeave_t*)pMsg->data;
    pDevLeave->nwkAddr = nwkAddr;
    memcpy(pDevLeave->extAddr, extAddr, 8);
    pDevLeave->parentAddr = parentAddr;
    MFG_SendMsg(pMsg);
  }
}

/*
* @fn          mfgSendReportCallback
*
* @brief       bdb send report callback
*
* @param       report param
*
* @return      none
*
* */
bool mfgSendReportCallback( uint8_t srcEP, afAddrType_t *dstAddr, uint16 clusterID,
                           zclReportCmd_t *reportCmd, uint8_t direction, uint8_t disableDefaultRsp,
                           uint16 manuCode, uint8_t seqNum )
{
  mfgMsgHdr_t *pMsg = MFG_CreatMsg( Mfg_Report_Send, sizeof(mfgMsgReportSend_t) +
                                          (reportCmd->numAttr) * sizeof(zclReport_t) );
  if(pMsg)
  {
    mfgMsgReportSend_t *pReport = (mfgMsgReportSend_t*)pMsg->data;
    uint8_t n;
    pReport->srcEP = srcEP;
    pReport->dstEP = dstAddr->endPoint;
    pReport->clusterID = clusterID;
    pReport->direction = direction;
    pReport->disableDefaultRsp = disableDefaultRsp;
    pReport->manuCode = manuCode;
    pReport->seqNum = seqNum;
    pReport->numAttr = reportCmd->numAttr;
    for(n=0; n<pReport->numAttr; n++)
    {
      pReport->attrList[n].attrID = reportCmd->attrList[n].attrID;
      pReport->attrList[n].dataType = reportCmd->attrList[n].dataType;
      memcpy( pReport->attrList[n].attrData, reportCmd->attrList[n].attrData, 4 );
    }
    MFG_SendMsg(pMsg);
  }
  return TRUE;
}

/* function */

/*
* @fn          MFG_StackInit
*
* @brief       
*
* @param       destTaskId - task id
*
* @return      none
*
* */
void MFG_StackInit(uint8_t destTaskId)
{
  uint8_t i;
  if( 0 != memcmp( Lym_Mark, LUOYIMING_MARK, sizeof(Lym_Mark) ) )
  {
    while(1);
  }
  
  for(i=0; i<(uint8_t)Mfg_Max_End; i++)
  {
    mfgMsgCBs[i] = NULL;
  }
  
  mfgTaskId = destTaskId;
  MFG_InitCallback();
  pZDSecMgrDeviceValidateCallback = mfgDeviceValidateCallback;  //SecMgr DeviceValidate
  pZDSecMgrDeviceLeaveNotifyCallback = mfgDeviceLeaveNotifyCallback;  //SecMgr DeviceLeaveNotify
#ifdef BDB_REPORTING
  pBdb_SendReportCmdCallback = mfgSendReportCallback;
#endif
}

/*
* @fn          MFG_TaskId
*
* @brief       
*
* @param       none
*
* @return      destTaskId - task id
*
* */
uint8_t MFG_TaskId(void)
{
  return mfgTaskId;
}

/*
* @fn          MFG_Channel
*
* @brief       
*
* @param       none
*
* @return      channel
*
* */
uint8_t MFG_Channel(void)
{
  zstack_sysNwkInfoReadRsp_t sysNwkInfoReadRsp;
  Zstackapi_sysNwkInfoReadReq( mfgTaskId, &sysNwkInfoReadRsp );
  return sysNwkInfoReadRsp.logicalChannel;
}

/*
* @fn          MFG_NwkAddr
*
* @brief       
*
* @param       none
*
* @return      NWK Address
*
* */
uint16_t MFG_NwkAddr(void)
{
  zstack_sysNwkInfoReadRsp_t sysNwkInfoReadRsp;
  Zstackapi_sysNwkInfoReadReq( mfgTaskId, &sysNwkInfoReadRsp );
  return sysNwkInfoReadRsp.nwkAddr;
}

/*
* @fn          MFG_PanId
*
* @brief       
*
* @param       none
*
* @return      PAN ID
*
* */
uint16_t MFG_PanId(void)
{
  zstack_sysNwkInfoReadRsp_t sysNwkInfoReadRsp;
  Zstackapi_sysNwkInfoReadReq( mfgTaskId, &sysNwkInfoReadRsp );
  return sysNwkInfoReadRsp.panId;
}

/*
* @fn          MFG_CreatMsg
*
* @brief       creat mfg message
*
* @param       type - message type
*              size - message data size
*
* @return      message pointer
*
* */
mfgMsgHdr_t* MFG_CreatMsg(mfgMsgType_t type, uint8_t size)
{
  mfgMsgHdr_t* item = MFG_malloc(sizeof(mfgMsgHdr_t)+size);
  if(item)
  {
    item->type = type;
    item->size = size;
    item->next = NULL;
  }
  return item;
}

/*
* @fn          MFG_SendMsg
*
* @brief       send message to mfg task queue
*
* @param       pMsg - message pointer
*
* @return      none
*
* */
void MFG_SendMsg(mfgMsgHdr_t* pMsg)
{
  if( (pMsg != NULL) && (mfgTaskId != 0xFF) )
  {
    if(mfgMsgHdrHead == NULL)
    {
      mfgMsgHdrHead = pMsg;
    }
    else
    {
      mfgMsgHdr_t* tail = mfgMsgHdrHead;
      while(tail->next != NULL)
      {
        tail = tail->next;
      }
      tail->next = pMsg;
    }
    MFG_setEvent(mfgTaskId, MFG_MSG_IN_EVENT);
  }
}

/*
* @fn          MFG_GetMsg
*
* @brief       get message in mfg task
*
* @param       none
*
* @return      message pointer
*
* */
mfgMsgHdr_t* MFG_GetMsg(void)
{
  mfgMsgHdr_t* msg = mfgMsgHdrHead;
  if(mfgMsgHdrHead != NULL)
  {
    mfgMsgHdrHead = mfgMsgHdrHead->next;
  }
  return msg;
}

/*
* @fn          MFG_MfgMsgPoll
*
* @brief       process MFG message
*
* @param       none
*
* @return      none
*
* */
void MFG_MfgMsgPoll(void)
{
  mfgMsgHdr_t* mfgMsg = MFG_GetMsg();
  while( mfgMsg )
  {
    if(NULL != mfgMsgCBs[mfgMsg->type])
    {
      mfgMsgCBs[mfgMsg->type]( mfgMsg->type, mfgMsg->size, mfgMsg->data );
    }
    MFG_free(mfgMsg);
    mfgMsg = MFG_GetMsg();
  }
}

#ifndef NO_WDOG
#include <ti/drivers/Watchdog.h>
#include <ti/drivers/watchdog/WatchdogCC26XX.h>

Watchdog_Handle mfgWatchDog = NULL;

void MFG_WatchDog_Callback(uintptr_t handle)
{
  while(1)
  {
    asm("nop");
  }
}

#endif
/*
* @fn          MFG_WatchDogOpen
*
* @brief       none
*
* @param       none
*
* @return      none
*
* */
void MFG_WatchDogOpen(void)
{
#ifndef NO_WDOG
  MFG_CSState key;
  key = MFG_enterCS();    
  if(mfgWatchDog == NULL)
  {
    Watchdog_init();
    Watchdog_Params params;
    params.callbackFxn = MFG_WatchDog_Callback;
    params.custom = Watchdog_RESET_OFF;
    Watchdog_Params_init(&params);
    mfgWatchDog = Watchdog_open(0, &params);
  }
  MFG_leaveCS(key);
  uint32_t reloadValue = Watchdog_convertMsToTicks(mfgWatchDog, 2000);
  if (reloadValue != 0) {
    Watchdog_setReload(mfgWatchDog, reloadValue);
  }
  
#endif
}

/*
* @fn          MFG_WatchDogFeed
*
* @brief       none
*
* @param       none
*
* @return      none
*
* */
void MFG_WatchDogFeed(void)
{
#ifndef NO_WDOG
  MFG_CSState key;
  key = MFG_enterCS();  
  if(mfgWatchDog)
  {
    Watchdog_clear(mfgWatchDog);
  }
  MFG_leaveCS(key);
#endif
}

/*
* @fn          MFG_SetupZstackCallbacks
*
* @brief       none
*
* @param       none
*
* @return      none
*
* */
void MFG_SetupZstackCallbacks(void)
{
  zstack_devZDOCBReq_t zdoCBReq = {0};
  
  zdoCBReq.has_devStateChange = true;
  zdoCBReq.devStateChange = true;
  
  zdoCBReq.has_tcDeviceInd =true;
  zdoCBReq.tcDeviceInd = true;
  
  zdoCBReq.has_devPermitJoinInd = true;
  zdoCBReq.devPermitJoinInd = true;
  
  zdoCBReq.has_deviceAnnounce = true;
  zdoCBReq.deviceAnnounce = true;
  
  zdoCBReq.has_nwkAddrRsp = true;
  zdoCBReq.nwkAddrRsp = true;
  
  zdoCBReq.has_ieeeAddrRsp = true;
  zdoCBReq.ieeeAddrRsp = true;
  
  zdoCBReq.has_nodeDescRsp = true;
  zdoCBReq.nodeDescRsp = true;
  
  zdoCBReq.has_activeEndpointRsp = true;
  zdoCBReq.activeEndpointRsp = true;
  
  zdoCBReq.has_simpleDescRsp = true;
  zdoCBReq.simpleDescRsp = true;
  
  zdoCBReq.has_bindRsp = true;
  zdoCBReq.bindRsp = true;
  
  zdoCBReq.has_unbindRsp = true;
  zdoCBReq.unbindRsp = true;
  
  zdoCBReq.has_mgmtLeaveRsp = true;
  zdoCBReq.mgmtLeaveRsp = true;
  
  zdoCBReq.has_mgmtLqiRsp = true;
  zdoCBReq.mgmtLqiRsp = true;

  zdoCBReq.has_mgmtBindRsp = true;
  zdoCBReq.mgmtBindRsp = true;
  
  zdoCBReq.has_leaveIndCB = true;
  zdoCBReq.leaveIndCB = true;
  
  (void)Zstackapi_DevZDOCBReq(mfgTaskId, &zdoCBReq);
}


extern void OsalPort_heapMgrGetMetrics(uint32_t *pBlkMax,
                                       uint32_t *pBlkCnt,
                                       uint32_t *pBlkFree,
                                       uint32_t *pMemAlo,
                                       uint32_t *pMemMax,
                                       uint32_t *pMemUB);

void MFG_HeapMgr(void)
{
#ifdef HEAPMGR_METRICS
  OsalPort_heapMgrGetMetrics(&MFG_Heap_BlkMax,
                             &MFG_Heap_BlkCnt,
                             &MFG_Heap_BlkFree,
                             &MFG_Heap_MemAlo,
                             &MFG_Heap_MemMax,
                             &MFG_Heap_MemUB);
#endif
}

/*
* @fn          MFG_RandomExtPanid
*
* @brief       generate random Ext-PanId
*
* @param       none
*
* @return      none
*
* */
void MFG_RandomExtPanid(void)
{
  extern uint8_t zgExtendedPANID[];
  extern uint8_t aExtendedAddress[];
  uint8_t defExtPanid[] = ZDAPP_CONFIG_EPID;
  
  osal_nv_read( ZCD_NV_EXTENDED_PAN_ID, 0, Z_EXTADDR_LEN, zgExtendedPANID );
  if( ( 0 == memcmp( zgExtendedPANID, defExtPanid, Z_EXTADDR_LEN ) ) || ( 0 == memcmp( zgExtendedPANID, aExtendedAddress, Z_EXTADDR_LEN ) ) )
  {
    uint8_t  extAddr[8];
    uint16_t random = OsalPort_rand();
    memcpy( extAddr, aExtendedAddress, 8);
    //Create the key from the seed
    uint8_t i;
    for(i = 0; i < 8; i++)
    {
      extAddr[i] ^= (random & 0x000000FF);
      random >>= 1;
      zgExtendedPANID[i] ^= extAddr[i];
    }
    osal_cpyExtAddr( _NIB.extendedPANID, zgExtendedPANID );
    osal_nv_write( ZCD_NV_EXTENDED_PAN_ID, Z_EXTADDR_LEN, zgExtendedPANID );
  }
}

/*
* @fn          MFG_DeleteAddrMgr
*
* @brief       delete AddrMgr record completely.
*
* @param       none
*
* @return      none
*
* */
bool MFG_DeleteAddrMgr(uint8_t *extAddr, uint8_t user)
{
  bool ret = FALSE;
  AddrMgrEntry_t entry = {0};
  entry.user = user;
  memcpy( entry.extAddr, extAddr, 8 );
  
  MFG_CSState key;
  key = MFG_enterCS();
  if(AddrMgrEntryLookupExt( &entry) )
  {
    if( AddrMgrEntryRelease( &entry ) )
    {
      ret = true;
    }
  }
  MFG_leaveCS(key);
  return ret;
}

/*
* @fn          MFG_AddrMgrInit
*
* @brief       repair AddrMgr.
*
* @param       none
*
* @return      none
*
* */
void MFG_AddrMgrInit(void)
{

}

/*
* @fn          MFG_LoopReset
*
* @brief       repair AddrMgr.
*
* @param       none
*
* @return      none
*
* */
void MFG_LoopReset(void)
{
  if( mfgResetFlag )
  {
    SysCtrlSystemReset();
  }
}

/*
* @fn          MFG_LoopReset
*
* @brief       repair AddrMgr.
*
* @param       none
*
* @return      none
*
* */
void MFG_Reset(void)
{
  mfgResetFlag = TRUE;
}

/*
* @fn          MFG_WlistSet
*
* @brief       turn on off whitelist
*
* @param       none
*
* @return      none
*
* */