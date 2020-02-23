#include "mfg_Stack.h"
#include "mfg.h"
#include "mfg_coord.h"
#include "zstackapi.h"
#include "util_timer.h"
#include "cmd/cmd_cfg.h"
#include "cmd/cmd_nwk.h"
#include "cmd/cmd_zcl.h"

#include "mfg_zcl.h"
#include "zcl_ha.h"
#include "zcl_port.h"
#include "addr_mgr.h"

#include <ti/drivers/apps/Button.h>
#include "ti_drivers_config.h"

/* macro */

#define MFG_EP_LIST_SIZE      16

#define  NODE_JOIN_WAIT_TIME   20   //sec
#define  NODE_ZDP_WAIT_TIME    5   //sec
#define  NODE_ZDP_RETRY        4

#define  MFG_FLEX_EP_NUM       (CMD_EP_ZCL_MAX - 1)

/* typedef */
typedef struct
{
  void*  next;
  uint16_t dest;
  uint8_t *pSeqNum;
  uint8_t cmdId;
  uint8_t cmdSize;
  uint8_t cmdData[];
}mfgZdpQueue_t;

typedef struct
{
  void *next;
  uint8_t mac[8];
  uint8_t timeSec;
}mfgWhiteList_t;

typedef struct
{
  void* next;
  uint8_t  mac[8];
  uint16_t nodeId;
  uint16_t parent;
  uint8_t  join;
  uint8_t  type;
  uint8_t  timeSec;
  uint8_t  retry;
  uint8_t  seq;
  uint8_t  epCnt;
  uint8_t  epIdx;
  uint8_t  epList[MFG_EP_LIST_SIZE];
  void (*cb)(void*);
}mfgNodeList_t;

typedef struct
{
  void* next;
  uint16_t destAddr;
  uint8_t  mac[8];
  uint8_t  ep;
  uint16_t cluster;
}mfgBindList_t;

extern uint8_t cmdZcl_FindEpByProfId(uint16_t profId);
/* variable */
mfgWhiteList_t *mfgWhiteListHead = NULL;
mfgNodeList_t *mfgNodeListHead = NULL;
mfgZdpQueue_t *mfgZdpQueueHead = NULL;

mfgBindList_t *mfgBindListHead = NULL;

bool mfgWhiteListEnable = FALSE;

//Manucode callback
void MFG_QueryNodeDescCB(void*);
//Node callback
void MFG_QueryActiveEpCB(void*);
//Device callback
void MFG_QuerySimpleDescCB(void*);
//Device Verify Join
void MFG_VerifyJoinCB(void*);

/* static function */
static void mfgWlistRefresh(void);
static void mfgWlistPush(mfgWhiteList_t* item);
static mfgWhiteList_t* mfgWlistFind(uint8_t *mac);

//Node List Function
mfgNodeList_t* mfgNodeCreatNew(uint8_t *mac);

void mfgNodeRefresh(void);
void mfgNodePush(mfgNodeList_t* item);
mfgNodeList_t* mfgNodeFindByMac(uint8_t* mac);
mfgNodeList_t* mfgNodeFindByNodeId(uint16_t nodeId);

void mfgMatchBindCluster( uint16_t destAddr, uint8_t* mac, uint8_t ep, uint8_t numCluster, uint16_t* cluserLst );
bool mfgAddBindList( uint16_t destAddr, uint8_t* mac, uint8_t ep, uint16_t cluster );
void mfgSednBindList( void );
void MFG_JoinBindCnf(uint8_t status, uint8_t endpoint, uint8_t transID, uint16_t clusterID, void* cnfParam);

//local func
void MFG_DeviceLeaveInd(mfgMsgType_t msgType, uint8_t size, uint8_t *data);
void MFG_DeniedJoin(mfgMsgType_t msgType, uint8_t size, uint8_t *data);
//timer
Clock_Struct mfgPeriodClkStruct;
void mfgPeriodTimeoutCB(UArg a0);

//key
static void handleButtonCallback(Button_Handle handle, Button_EventMask events);

const uint16_t mfgBindClusterList[] = 
{
  ZCL_CLUSTER_ID_GEN_ON_OFF,
  ZCL_CLUSTER_ID_GEN_LEVEL_CONTROL,
  ZCL_CLUSTER_ID_GEN_ALARMS,
  ZCL_CLUSTER_ID_CLOSURES_DOOR_LOCK,
  ZCL_CLUSTER_ID_CLOSURES_WINDOW_COVERING,
  ZCL_CLUSTER_ID_HVAC_FAN_CONTROL,
  ZCL_CLUSTER_ID_HVAC_DIHUMIDIFICATION_CONTROL,
  ZCL_CLUSTER_ID_LIGHTING_COLOR_CONTROL,
  ZCL_CLUSTER_ID_SS_IAS_ZONE,
};

SimpleDescriptionFormat_t MFG_FlexSimpleDescArray[MFG_FLEX_EP_NUM] = {0};
endPointDesc_t  MFG_FlexEpDescArray[MFG_FLEX_EP_NUM]={0};

/*************************************************************
                extern function
*/

/*
* @fn          MFG_InitCallback
*
* @brief       mfg coord init
*
* @param       none
*
* @return      none
*
* */
void MFG_InitCallback(void)
{
  /*
  if(BL_Mark != 0x12345678)
  {
    while(1);
  }
*/
  mfgMsgCBs[Mfg_Denied_Join] = MFG_DeniedJoin;
  mfgMsgCBs[Mfg_Device_Leave] = MFG_DeviceLeaveInd;
  Timer_construct(
                  &mfgPeriodClkStruct,
                  mfgPeriodTimeoutCB,
                  1000,
                  0, FALSE, 0);
  Timer_start(&mfgPeriodClkStruct);
  
  MFG_ZclInit();
  
  Button_Params params;
  Button_Params_init(&params);
  params.longPressDuration = 1000; // 1000 ms
  Button_open(0, handleButtonCallback, &params);
}

/*
* @fn          MFG_PeriodTick
*
* @brief       called in mfg task per second
*
* @param       none
*
* @return      none
*
* */
void MFG_PeriodCallback(void)
{
  //MFG_WatchDogFeed();
  mfgWlistRefresh();
  mfgNodeRefresh();
  cmdZdp_SeqTablePoll();
  mfgSednBindList();
}

/*
* @fn          mfgPeriodTimeoutCB
*
* @brief       1 second period timer
*
* @param       none
*
* @return      none
*
* */
void mfgPeriodTimeoutCB(UArg a0)
{
  MFG_setEvent(MFG_TaskId(), MFG_PERIOD_EVENT);
  Timer_start(&mfgPeriodClkStruct);
}

/*
* @fn          MFG_WhiteListAdd
*
* @brief       add a ext-address into whitelist
*
* @param       mac - extAddress
*
* @return      message pointer
*
* */
bool MFG_WhiteListAdd( uint8_t *mac )
{
  mfgWhiteList_t *newItem = MFG_malloc( sizeof(mfgWhiteList_t) );
  if(newItem)
  {
    newItem->next = NULL;
    newItem->timeSec = 240;
    memcpy(newItem->mac, mac, 8);
    mfgWlistPush(newItem);
    return true;
  }
  return false;
}

/*
* @fn          MFG_WhiteListCheck
*
* @brief       check white list by ext-address
*
* @param       mac - extAddress
*
* @return      True of False
*
* */
bool MFG_WhiteListCheck(uint8_t *mac)
{
  mfgWhiteList_t* item = mfgWlistFind(mac);
  if(item)
  {
    item->timeSec = 0;
    return true;
  }
  return false;
}

/*
* @fn          MFG_WhiteListEnable
*
* @brief       
*
* @param       NONE
*
* @return      True of False
*
* */
bool MFG_WhiteListEnable(void)
{
  return mfgWhiteListEnable;
}

/*
* @fn          MFG_WhiteListSet
*
* @brief       
*
* @param       enable TRUE or FALSE
*
* @return      NONE
*
* */
void MFG_WhiteListSet( uint8_t enable )
{
  mfgWhiteListEnable = enable;
}

/*
* @fn          MFG_TcJoinInd
*
* @brief       message zstackmsg_CmdIDs_ZDO_TC_DEVICE_IND
*
* @param       zstack_zdoTcDeviceInd_t
* 
* @return      None
*
* */
void MFG_TcJoinInd( zstack_zdoTcDeviceInd_t req )
{
  mfgNodeList_t* item = mfgNodeCreatNew(req.extendedAddr);
  if( item )
  {
    item->nodeId = req.nwkAddr;
    item->parent = req.parentAddr;
  }
}

/*
* @fn          MFG_DeviceAnnounceInd
*
* @brief       message zstackmsg_CmdIDs_ZDO_TC_DEVICE_IND
*
* @param       zstack_zdoDeviceAnnounceInd_t
* 
* @return      None
*
* */
void MFG_DeviceAnnounceInd(zstack_zdoDeviceAnnounceInd_t req)
{
  mfgNodeList_t* item = mfgNodeFindByMac(req.devExtAddr);
  uint8_t type = (req.capInfo.ffd)? NODE_ROUTER:NODE_ENDDEV;
  uint16_t parent = 0xFFFC;
  if(item)
  {
    parent = item->parent;
    if( item->join )
    {
      item->type = type;
      cmdCfg_NodeJoinNotify( req.devExtAddr, req.devAddr, item->parent, JOIN_MODE_JOIN, type );
      item->cb = MFG_QueryActiveEpCB;
      item->retry = NODE_ZDP_RETRY;
      item->timeSec = 1;
      return;
    }
  }
  cmdCfg_NodeJoinNotify( req.devExtAddr, req.devAddr, parent, JOIN_MODE_REJOIN, type );
}

/*
* @fn          MFG_VerifyLinkKey
*
* @brief       message zstackmsg_CmdIDs_BDB_TC_LINK_KEY_EXCHANGE_NOTIFICATION_IND
*
* @param       bdb_TCLinkKeyExchProcess_t
* 
* @return      None
*
* */
void MFG_VerifyLinkKey(bdb_TCLinkKeyExchProcess_t req)
{
  //output aps-key
  cmdCfg_KeyVerify( req.status, req.extAddr, req.nwkAddr );
  if(req.status == 0)
  {
    mfgNodeList_t* item = mfgNodeCreatNew(req.extAddr);
    if( item )
    {
      item->join = 1;
      item->nodeId = req.nwkAddr;
    }
  }
  else
  {
    mfgNodeList_t* item = mfgNodeFindByMac(req.extAddr);
    if(item)
    {
      if((item->type == NODE_NONE) && (req.status==2))
      {
        cmdCfg_NodeJoinNotify(item->mac, item->nodeId, item->parent, JOIN_MODE_TC, NODE_NONE);
        item->timeSec = 0;
        item->retry = 0;
      }
    }
  }
}

/*
* @fn          MFG_ActiveEpInd
*
* @brief       message zstackmsg_CmdIDs_ZDO_ACTIVE_EP_RSP
*
* @param       nodeId - short Address
*              seq - rsp trans seq
*              epCnt - endpoint count
*              epArray - endpoint list
* 
* @return      None
*
* */
bool MFG_ActiveEpInd( zstack_zdoActiveEndpointsRspInd_t rsp )
{
  mfgNodeList_t* find = mfgNodeFindByNodeId(rsp.nwkAddrOfInterest);
  if( find && (find->seq == rsp.transSeq) )
  {
    if(rsp.status == ZSuccess)
    {
      find->epCnt = rsp.n_activeEPList;
      if(find->epCnt > MFG_EP_LIST_SIZE)
      {
        find->epCnt = MFG_EP_LIST_SIZE;
      }
      find->epIdx = 0;
      memcpy( find->epList, rsp.pActiveEPList, find->epCnt );
      find->cb = MFG_QuerySimpleDescCB;
      find->retry = NODE_ZDP_RETRY;
      find->timeSec = 0;
    }
    else
    {
      find->cb = NULL;
      find->retry = 0;
      find->timeSec = 0;
    }
    return true;
  }
  return false;
}

/*
* @fn          MFG_SimpleDescInd
*
* @brief       message zstackmsg_CmdIDs_ZDO_SIMPLE_DESC_RSP
*
* @param       
* 
* @return      None
*
* */
bool MFG_SimpleDescInd( zstack_zdoSimpleDescRspInd_t rsp )                        
{
  mfgNodeList_t* find = mfgNodeFindByNodeId(rsp.nwkAddrOfInterest);
  if(find)
  {
    if(find->seq == rsp.transSeq)
    {
      if(rsp.status == ZSuccess)
      {
        mfgDevSn_t sn;
        mfgDevAddr_t dev;
        sn[0] = rsp.simpleDesc.endpoint;
        memcpy( sn+1, find->mac, 8);
        dev[0] = LO_UINT16(find->nodeId);
        dev[1] = HI_UINT16(find->nodeId);
        dev[2] = rsp.simpleDesc.endpoint;
        
        cmdCfg_DevJoinNotify(sn, dev, rsp.simpleDesc.profileID, rsp.simpleDesc.deviceID, 
                             rsp.simpleDesc.n_inputClusters, rsp.simpleDesc.pInputClusters,
                             rsp.simpleDesc.n_outputClusters, rsp.simpleDesc.pOutputClusters);
        //Add Bind
        mfgMatchBindCluster(rsp.nwkAddrOfInterest, find->mac, rsp.simpleDesc.endpoint, rsp.simpleDesc.n_inputClusters, rsp.simpleDesc.pInputClusters);

        if( find->epList[find->epIdx++] == rsp.simpleDesc.endpoint )
        {
          if( find->epIdx < find->epCnt ) //query next endpoint
          {
            find->retry = NODE_ZDP_RETRY;
            find->timeSec = 0;
            return true;
          }
        }
      }
      find->cb = NULL;
      find->retry = 0;
      find->timeSec = 0;
      return true;
    }
  }
  return false;
}

/*
* @fn          MFG_AddFlexEp
*
* @brief       
*
* @param       
* 
* @return      None
*
* */
bool MFG_AddFlexEp(uint8_t ep, uint16_t profile)
{
  extern endPointDesc_t *zcl_afFindEndPointDesc(uint8_t EndPoint);
  uint8_t n;
  zstack_bdbGetAttributesRsp_t bdbGetRsp;
  Zstackapi_bdbGetAttributesReq(MFG_TaskId(), &bdbGetRsp);
  if(bdbGetRsp.bdbNodeIsOnANetwork)
  {
    return FALSE;
  }
  if(NULL != zcl_afFindEndPointDesc(ep))
  {
    return FALSE;
  }
  for(n=0; n<MFG_FLEX_EP_NUM; n++)
  {
    if(MFG_FlexSimpleDescArray[n].EndPoint == 0)
    {
      MFG_FlexSimpleDescArray[n].EndPoint = ep;
      MFG_FlexSimpleDescArray[n].AppProfId = profile;
      MFG_FlexSimpleDescArray[n].AppDeviceId = ZCL_HA_DEVICEID_TEST_DEVICE;
      MFG_FlexSimpleDescArray[n].AppDevVer = 0;
      MFG_FlexSimpleDescArray[n].Reserved = 0;
      MFG_FlexSimpleDescArray[n].AppNumInClusters = 0;
      MFG_FlexSimpleDescArray[n].pAppInClusterList = NULL;
      MFG_FlexSimpleDescArray[n].AppNumOutClusters = 0;
      MFG_FlexSimpleDescArray[n].pAppOutClusterList = NULL;
      //
      MFG_FlexEpDescArray[n].endPoint = MFG_FlexSimpleDescArray[n].EndPoint;
      MFG_FlexEpDescArray[n].simpleDesc = &MFG_FlexSimpleDescArray[n];
      zclport_registerEndpoint(MFG_TaskId(), &MFG_FlexEpDescArray[n]);
      cmdZcl_InitSrcEP( &MFG_FlexEpDescArray[n], FALSE );
      return TRUE;
    }
  }
  return FALSE;
}

/*
* @fn          MFG_EzModeStart
*
* @brief       
*
* @param       
* 
* @return      TRUE - start success
*
* */
bool MFG_EzModeStart( uint16_t nodeId, uint8_t *mac )
{
  mfgNodeList_t* item = mfgNodeCreatNew( mac );
  if( item )
  {
    item->nodeId = nodeId;
    item->parent = 0x0000;
    item->join = 0;
    item->type = NODE_NONE;
    item->cb = MFG_QueryActiveEpCB;
    item->retry = NODE_ZDP_RETRY;
    item->timeSec = 0;
    return TRUE;
  }
  return FALSE;
}

/*
* @fn          MFG_JoinBindCnf
*
* @brief       
*
* @param       
* 
* @return      None
*
* */
void MFG_JoinBindCnf(uint8_t status, uint8_t endpoint, uint8_t transID, uint16_t clusterID, void* cnfParam)
{
  if( cnfParam )
  {
    uint8_t* buf = (uint8_t*)cnfParam;
    uint16 cluster = BUILD_UINT16(buf[9],buf[10]);
    cmdCfg_JoinBind( buf, cluster, status );
    MFG_free(cnfParam);
  }
}

//
typedef struct
{
  uint16_t nodeId;
  uint8_t ep;
}mfgQueryErro_t;

static void MFG_QueryErro(uint8_t status, uint8_t endpoint, uint8_t transID, uint16_t clusterID, void* cnfParam)
{
  if(cnfParam)
  {
    mfgQueryErro_t *erro = (mfgQueryErro_t*)cnfParam;
    if(status != 0x00)
    {
      cmdCfg_DeviceErro(erro->nodeId, erro->ep, status);
    }
    MFG_free(cnfParam);
  }
}

//Query Node Desc
void MFG_QueryNodeDescCB(void* p)
{
  
}

//Query Activ EP
void MFG_QueryActiveEpCB(void *p)
{
  mfgNodeList_t *item = p;
  mfgQueryErro_t *erro = MFG_malloc(sizeof(mfgQueryErro_t));
  if(erro)
  {
    erro->nodeId = item->nodeId;
    erro->ep = 0xFF;
  }
  if( item->retry )
  {
    zstack_zdoActiveEndpointReq_t req = {0};
    req.dstAddr = item->nodeId;
    req.nwkAddrOfInterest = item->nodeId;
    req.extParam.afCnfCB = MFG_QueryErro;
    req.extParam.cnfParam = erro;
    if(zstack_ZStatusValues_ZSuccess == Zstackapi_ZdoActiveEndpointReq(MFG_TaskId(), &req))
    {
      item->seq = req.extParam.seqNum;
      item->timeSec = 5;
    }
    else
    {
      item->retry = 0; 
      item->timeSec = 0;
    }
  }
}

//Query Simple Desc
void MFG_QuerySimpleDescCB(void *p)
{
  mfgNodeList_t *item = p;
  mfgQueryErro_t *erro = MFG_malloc(sizeof(mfgQueryErro_t));
  if(erro)
  {
    erro->nodeId = item->nodeId;
    erro->ep = item->epList[item->epIdx];
  }
  if( item->retry )
  {
    zstack_zdoSimpleDescReq_t req = {0};
    req.dstAddr = item->nodeId;
    req.nwkAddrOfInterest = item->nodeId;
    req.endpoint = item->epList[item->epIdx];
    req.extParam.afCnfCB = MFG_QueryErro;
    req.extParam.cnfParam = erro;
    if( zstack_ZStatusValues_ZSuccess == Zstackapi_ZdoSimpleDescReq(MFG_TaskId(), &req) )
    {
      item->seq = req.extParam.seqNum;
      item->timeSec = 5;
    }
    else
    {
      item->timeSec = 0;
      item->retry = 0;
    }
  }
}

/*
*        Local Function
*/

/*
* @fn          mfgWlistRefresh
*
* @brief       refresh whitelist
*
* @param       none
*
* @return      none
*
* */
void mfgWlistRefresh(void)
{
  mfgWhiteList_t* poll = mfgWhiteListHead;
  mfgWhiteList_t* pre = NULL;
  while(poll)
  {
    void *next = poll->next;
    if(poll->timeSec == 0)
    {
      mfgWhiteList_t* free = poll;
      if(free == mfgWhiteListHead)
      {
        mfgWhiteListHead = free->next;
      }
      else
      {
        pre->next = free->next;
      }
      MFG_free(free);
    }
    else
    {
      poll->timeSec -= 1;
      pre = poll;
    }
    poll = next;
  }
}

/*
* @fn          mfgWlistPush
*
* @brief       add whitelist item into queue
*
* @param       none
*
* @return      none
*
* */
static void mfgWlistPush(mfgWhiteList_t* item)
{
  if(mfgWhiteListHead == NULL)
  {
    mfgWhiteListHead = item;
  }
  else
  {
    mfgWhiteList_t* tail = mfgWhiteListHead;
    while(tail->next != NULL)
    {
      tail = tail->next;
    }
    tail->next = item;
  }
}

/*
* @fn          mfgWlistFind
*
* @brief       lookup whitelist item by extAddr
*
* @param       mac - extAddr
*
* @return      whitelist item
*
* */
static mfgWhiteList_t* mfgWlistFind(uint8_t *mac)
{
  mfgWhiteList_t* find = mfgWhiteListHead;
  while(find)
  {
    if(0 == memcmp(mac, find->mac, 8))
    {
      return find;
    }
    find = find->next;
  }
  return NULL;
}

/*
* @fn          mfgNodeCreatNew
*
* @brief       creat new item into NodeList
*
* @param       mac - input IEEE addr
*
* @return      item record
*
* */
mfgNodeList_t* mfgNodeCreatNew(uint8_t *mac)
{
  mfgNodeList_t* item = mfgNodeFindByMac(mac);
  if( item == NULL )
  {
    if( item = MFG_malloc(sizeof(mfgNodeList_t)) )
    {
      memset(item, 0, sizeof(mfgNodeList_t));
      memcpy(item->mac, mac, 8);
      mfgNodePush(item);
    }
  }
  if(item)
  {
    item->timeSec = NODE_JOIN_WAIT_TIME;//refresh wait time
  }
  return item;
}

/*
* @fn          mfgNodeRefresh
*
* @brief       Node List Refresh
*
* @param       None
*
* @return      None
*
* */
void mfgNodeRefresh(void)
{
  mfgNodeList_t* poll = mfgNodeListHead;
  mfgNodeList_t* pre = NULL;
  while(poll)
  {
    void *next = poll->next;
    if((poll->timeSec == 0)&&(poll->retry == 0))
    {
      mfgNodeList_t* free = poll;
      if(free == mfgNodeListHead)
      {
        mfgNodeListHead = free->next;
      }
      else
      {
        pre->next = free->next;
      }
      if(poll->cb)
      {
        poll->cb(poll);
      }
      MFG_free(free);
    }
    else
    {
      if(poll->timeSec > 0)
      {
        poll->timeSec -= 1;
      }
      else if(poll->retry)
      {
        poll->retry -= 1;
        if(poll->cb)
        {
          poll->cb(poll);
        }
      }
      pre = poll;
    }
    poll = next;
  }
}

/*
* @fn          mfgNodePush
*
* @brief       Add new item into Node List
*
* @param       item - new item of Node List
*
* @return      None
*
* */
void mfgNodePush(mfgNodeList_t* item)
{
  if(item)
  {
    if(mfgNodeListHead == NULL)
    {
      mfgNodeListHead = item;
    }
    else
    {
      mfgNodeList_t* tail = mfgNodeListHead;
      while(tail->next)
      {
        tail = tail->next;
      }
      tail->next = item;
    }
  }
}

/*
* @fn          mfgNodeFindByMac
*
* @brief       lookup Node List by ext-Address
*
* @param       mac - ext Address
*
* @return      Node List item
*
* */
mfgNodeList_t* mfgNodeFindByMac(uint8_t* mac)
{
  mfgNodeList_t* find = mfgNodeListHead;
  while(find)
  {
    if(0 == memcmp(find->mac,mac,8))
    {
      return find;
    }
    find = find->next;
  }
  return NULL;
}

/*
* @fn          mfgNodeFindByNodeId
*
* @brief       lookup Node List by short address
*
* @param       nodeId - short address
*
* @return      Node List item
*
* */
mfgNodeList_t* mfgNodeFindByNodeId(uint16_t nodeId)
{
  mfgNodeList_t* find = mfgNodeListHead;
  while(find)
  {
    if(find->nodeId == nodeId)
    {
      return find;
    }
    find = find->next;
  }
  return NULL;
}

/* Local function */
/*
* @fn          MFG_DeviceLeaveInd
*
* @brief       
*
* @param       none
*
* @return      none
*
* */
void MFG_DeviceLeaveInd(mfgMsgType_t msgType, uint8_t size, uint8_t *data)
{
  mfgMsgDeviceLeave_t *pLeave = (mfgMsgDeviceLeave_t*)data;
  //MFG_DeleteAddrMgr(pLeave->extAddr);
  cmdCfg_LeaveNotify(pLeave->nwkAddr,pLeave->extAddr,pLeave->parentAddr);
}

/*
* @fn          MFG_DeniedJoin
*
* @brief       
*
* @param       none
*
* @return      none
*
* */
void MFG_DeniedJoin(mfgMsgType_t msgType, uint8_t size, uint8_t *data)
{
  mfgMsgDeniedJoin_t *pDenied = (mfgMsgDeniedJoin_t*)data;
  cmdCfg_Denied( pDenied->extAddr, pDenied->parentAddr, pDenied->rejoin );
}

/*
* @fn          mfgMatchBindCluster
*
* @brief       
*
* @param       none
*
* @return      none
*
* */
void mfgMatchBindCluster( uint16_t destAddr, uint8_t* mac, uint8_t ep, uint8_t numCluster, uint16_t* cluserLst )
{
  uint8_t n;
  uint8_t tabSize = sizeof(mfgBindClusterList)/sizeof(uint16_t);
  uint8_t m;
  for(n=0;n<numCluster;n++)
  {
    for( m=0; m<tabSize; m++ )
    {
      if( mfgBindClusterList[m] == cluserLst[n])
      {
        mfgAddBindList( destAddr, mac, ep, cluserLst[n] );
      }
    }
  }
}

bool mfgAddBindList( uint16_t destAddr, uint8_t* mac, uint8_t ep, uint16_t cluster )
{
  mfgBindList_t *item = MFG_malloc( sizeof(mfgBindList_t) );
  if( item )
  {
    item->next = NULL;
    item->destAddr = destAddr;
    memcpy( item->mac, mac, 8 );
    item->ep = ep;
    item->cluster = cluster;
    if( mfgBindListHead == NULL )
    {
      mfgBindListHead = item;
    }
    else
    {
      mfgBindList_t *tail = mfgBindListHead;
      while( tail->next )
      {
        tail = tail->next;
      }
      tail->next = item;
    }
    return TRUE;
  }
  return FALSE;
}

void mfgSednBindList( void )
{
  if( mfgBindListHead )
  {
    mfgBindList_t *item = mfgBindListHead;
    mfgBindListHead = mfgBindListHead->next;

    uint8_t  mac[8];
    uint16_t destAddr = item->destAddr;
    uint8_t  ep = item->ep;
    uint8_t  cluster = item->cluster;
    memcpy( mac, item->mac, 8 );
    MFG_free( item );

    //Send Bind Req
    zstack_zdoBindReq_t req;
    zstack_sysNwkInfoReadRsp_t sysNwkInfoReadRsp;
    Zstackapi_sysNwkInfoReadReq( MFG_TaskId(),&sysNwkInfoReadRsp );
    
    req.nwkAddr = destAddr;
    req.bindInfo.clusterID = cluster;
    req.bindInfo.srcEndpoint = ep;
    memcpy( req.bindInfo.srcAddr, mac , 8);
    req.bindInfo.dstAddr.addrMode = zstack_AFAddrMode_EXT;
    req.bindInfo.dstAddr.endpoint = cmdZcl_FindEpByProfId(ZCL_HA_PROFILE_ID);
    memcpy( req.bindInfo.dstAddr.addr.extAddr, sysNwkInfoReadRsp.ieeeAddr, 8 );
    req.extParam.afCnfCB = MFG_JoinBindCnf;
    req.extParam.cnfParam = MFG_malloc(9+2); //cluster + sn
    if(req.extParam.cnfParam)
    {
      uint8_t* buf = req.extParam.cnfParam;
      buf[0] = ep;
      memcpy( buf + 1, mac, 8 );
      buf[9] = LO_UINT16(cluster);
      buf[10] = HI_UINT16(cluster);
      if( zstack_ZStatusValues_ZSuccess != Zstackapi_ZdoBindReq( MFG_TaskId(), &req ) )
      {
        MFG_free(req.extParam.cnfParam);
      }
    }
  }
}

Button_Handle But_handle;
Button_EventMask But_Envent[16];
uint8_t But_Idx = 0;
static void handleButtonCallback(Button_Handle handle, Button_EventMask events)
{
  if ( ((Button_HWAttrs*)handle->hwAttrs)->gpioIndex == RESET_BOOT )
  {
    if( Button_EV_LONGPRESSED & events )
    {
      MFG_Reset();
    }
  }
}
