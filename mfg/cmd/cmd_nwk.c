#include "string.h"
#include "hal_types.h"
#include "util_timer.h"

#include "zstack.h"
#include "zcomdef.h"
#include "zcl.h"
#include "zstackapi.h"

#include "util/mfg_Stack.h"
#include "cmd.h"
#include "cmd_nwk.h"
#include "cmd_pkt.h"

/* MACRO */
#define    ZDP_SEQ_TABLE_SIZE   16
#define    ZDP_SEQ_TABLE_LIFE   10


#define    cmdZdp_CmdOutput(cmd,nodeId,status,handle,len,data)    cmdZdp_CmdOutputEx(cmd,nodeId,status,handle,len,data,TRUE)

/* TYPEDEF */
typedef uint8_t (*pfnCmdZdpIndCB_t)(uint16_t cmd, uint16_t dest, uint8_t len, uint8_t *data, uint8_t* handle);

typedef struct
{
    uint16_t cmd;
    pfnCmdZdpIndCB_t cb;
}cmdZdpIndItem_t;

typedef struct
{
  uint16_t cmd;
  uint16_t nodeId;
  uint8_t  handle;
}cmdZdpCnfParam_t;

typedef struct
{
  uint8_t handle;
  uint8_t life;
}cmdZdpSeqTable_t;

/* LOCAL FUNCTION */
void cmdZdp_SendCnf(uint8_t status, uint8_t endpoint, uint8_t transID, uint16_t clusterID, void* cnfParam);
void cmdZdp_SeqPush(uint8_t handle);
bool cmdZdp_SeqCmp(uint8_t handle);

void cmdZdp_CmdOutputEx(uint16_t cmd, uint16_t nodeId, uint8_t status, uint8_t handle, uint8_t len, uint8_t* data, bool rsp);

//ZDP Request
uint8_t cmdZdp_NwkAddrReq(uint16_t cmd, uint16_t dest, uint8_t len, uint8_t *data, uint8_t* handle);
uint8_t cmdZdp_IeeeAddrReq(uint16_t cmd, uint16_t dest, uint8_t len, uint8_t *data, uint8_t* handle);
uint8_t cmdZdp_NodeDescReq(uint16_t cmd, uint16_t dest, uint8_t len, uint8_t *data, uint8_t* handle);
uint8_t cmdZdp_SimpleDescReq(uint16_t cmd, uint16_t dest, uint8_t len, uint8_t *data, uint8_t* handle);
uint8_t cmdZdp_ActiveEpReq(uint16_t cmd, uint16_t dest, uint8_t len, uint8_t *data, uint8_t* handle);
uint8_t cmdZdp_BindReq(uint16_t cmd, uint16_t dest, uint8_t len, uint8_t *data, uint8_t* handle);
uint8_t cmdZdp_UnbindReq(uint16_t cmd, uint16_t dest, uint8_t len, uint8_t *data, uint8_t* handle);
uint8_t cmdZdp_LqiTableReq(uint16_t cmd, uint16_t dest, uint8_t len, uint8_t *data, uint8_t* handle);
uint8_t cmdZdp_BindTableReq(uint16_t cmd, uint16_t dest, uint8_t len, uint8_t *data, uint8_t* handle);
uint8_t cmdZdp_LeaveReq(uint16_t cmd, uint16_t dest, uint8_t len, uint8_t *data, uint8_t* handle);

/* extern variable */
extern uint8_t cmdPktTaskId;

/* local variable */
cmdZdpIndItem_t cmdZdp_IndTable[] =
{
  {ZDP_NWK_ADDR,cmdZdp_NwkAddrReq},
  {ZDP_IEEE_ADDR,cmdZdp_IeeeAddrReq},
  {ZDP_NODE_DESC,cmdZdp_NodeDescReq},
  {ZDP_SIMPLE_DESC,cmdZdp_SimpleDescReq},
  {ZDP_ACTIVE_EP,cmdZdp_ActiveEpReq},
  {ZDP_BIND,cmdZdp_BindReq},
  {ZDP_UNBIND,cmdZdp_UnbindReq},
  {ZDP_LQI_TABLE,cmdZdp_LqiTableReq},
  {ZDP_BIND_TABLE,cmdZdp_BindTableReq},
  {ZDP_LEAVE,cmdZdp_LeaveReq},
  {0xFFFF,NULL},
};

cmdZdpSeqTable_t cmdZdpSeqTable[ZDP_SEQ_TABLE_SIZE];
uint8_t cmdZdpSeqIdx = 0;

/**************************************************
                   Function
*/

/**************************************************

*/
void cmdZdp_CmdInput(uint8_t cmdSeq, uint8_t cmdClass, uint8_t cmdLen, uint8_t* cmdData)
{
  uint16_t cmd = BUILD_UINT16(cmdData[0],cmdData[1]);
  uint16_t dest = BUILD_UINT16(cmdData[2],cmdData[3]);
  uint8_t  ack[2];
  uint8_t  n;
  uint8_t  status = ZFailure;
  uint8_t  handle = 0;
  for(n=0; n<(sizeof(cmdZdp_IndTable)/sizeof(cmdZdpIndItem_t)); n++)
  {
    if(cmdZdp_IndTable[n].cmd == cmd)
    {
      if(cmdZdp_IndTable[n].cb)
      {
        status = cmdZdp_IndTable[n].cb( cmd, dest, cmdLen-4, cmdData+4, &handle );
        cmdZdp_SeqPush(handle);
      }
      break;
    }
    else if(cmdZdp_IndTable[n].cmd == 0xFFFF)
    {
      break;
    }
  }
  ack[0] = status;
  ack[1] = handle;
  CmdPkt_AckSend(cmdSeq,cmdClass,2,ack);
}

void cmdZdp_CnfOutput( uint8_t status, uint8_t transID, void* cnfParam )
{
  if( cnfParam )
  {
    cmdZdpCnfParam_t *param = cnfParam;
    cmdZdp_CmdOutputEx( param->cmd, param->nodeId, status, param->handle, 0, NULL, false );
    MFG_free( cnfParam );
  }
}

void cmdZdp_SendCnf(uint8_t status, uint8_t endpoint, uint8_t transID, uint16_t clusterID, void* cnfParam)
{
#ifndef  CMD_ALL_DATACNF
  if(status != ZSuccess)
#endif
  {
    cmdZdp_CnfOutput( status, transID, cnfParam );
  }
}

void cmdZdp_CmdOutputEx(uint16_t cmd, uint16_t nodeId, uint8_t status, uint8_t handle, uint8_t len, uint8_t* data, bool rsp )
{
  //2+2+1+1+ (80-2)
  uint8_t buf[86];
  buf[0] = CmdPkt_GetSeqNum();
  if( rsp )
  {
    buf[1] = CMD_CLASS_NWK_INFO;
  }
  else
  {
    buf[1] = CMD_CLASS_NWK;
  }
  buf[2] = LO_UINT16(cmd);
  buf[3] = HI_UINT16(cmd);
  buf[4] = LO_UINT16(nodeId);
  buf[5] = HI_UINT16(nodeId);
  buf[6] = status;
  buf[7] = handle;
  memcpy( buf+8, data, len );
  CmdPkt_TxSend( len+8, buf );
}

void cmdZdp_SeqPush(uint8_t handle)
{
  cmdZdpSeqTable[cmdZdpSeqIdx].handle = handle;
  cmdZdpSeqTable[cmdZdpSeqIdx].life = ZDP_SEQ_TABLE_LIFE;
  cmdZdpSeqIdx += 1;
  cmdZdpSeqIdx &= (ZDP_SEQ_TABLE_SIZE - 1);
}

bool cmdZdp_SeqCmp(uint8_t handle)
{
  uint8_t n;
  for( n=0; n <ZDP_SEQ_TABLE_SIZE; n++ )
  {
    if( (cmdZdpSeqTable[n].life > 0) &&
       (cmdZdpSeqTable[n].handle == handle) )
    {
      return true;
    }
  }
  return false;
}

void cmdZdp_SeqTablePoll(void)
{
  uint8_t n;
  for( n=0; n <ZDP_SEQ_TABLE_SIZE; n++ )
  {
    if(cmdZdpSeqTable[cmdZdpSeqIdx].life > 0)
    {
      cmdZdpSeqTable[cmdZdpSeqIdx].life--;
    }
  }
}

/* ZDP command */

/*******************************************************
                ZDP_NWK_ADDR
*/
uint8_t cmdZdp_NwkAddrReq(uint16_t cmd, uint16_t dest, uint8_t len, uint8_t *data, uint8_t* handle)
{
  zstack_ZStatusValues status = zstack_ZStatusValues_ZFailure;
  if((dest != 0xFFFD) || (len < 10))
  {
    return status;
  }
  cmdZdpCnfParam_t *cnfParam = MFG_malloc(sizeof(cmdZdpCnfParam_t));
  if( cnfParam )
  {
    zstack_zdoNwkAddrReq_t req;
    req.type = (zstack_NwkAddrReqType)data[0];
    req.startIndex = data[1];
    memcpy( req.ieeeAddr, data + 2, 8 );
    req.extParam.afCnfCB = cmdZdp_SendCnf;
    req.extParam.cnfParam = cnfParam;
    status = Zstackapi_ZdoNwkAddrReq(cmdPktTaskId, &req);
    if((zstack_ZStatusValues_ZSuccess == status) && (req.extParam.cnfParam == cnfParam))
    {
      cnfParam->cmd = cmd;
      cnfParam->nodeId = dest;
      cnfParam->handle = req.extParam.seqNum;
      *handle = req.extParam.seqNum;
    }
    else
    {
      MFG_free(cnfParam);
    }
  }
  return (uint8_t)status;
}

void cmdZdp_NwkAddrRsp(zstack_zdoNwkAddrRspInd_t *pRsp)
{
  if(cmdZdp_SeqCmp(pRsp->transSeq))
  {
    uint8_t len = 0;
    uint8_t buf[100];
    uint8_t *p = buf;
    uint8_t n;
    //IEEE
    memcpy( p, pRsp->ieeeAddr, 8);
    p += 8;
    len += 8;
    //start index & assoc num
    *p ++ = pRsp->startIndex;
    *p ++ = pRsp->n_assocDevList;
    len += 2;
    //assoc list
    for( n=0; n<pRsp->n_assocDevList; n++ )
    {
      *p++ = LO_UINT16(pRsp->pAssocDevList[n]);
      *p++ = HI_UINT16(pRsp->pAssocDevList[n]);
      len += 2;
    }
    cmdZdp_CmdOutput( pRsp->nwkAddr, (ZDP_NWK_ADDR | 0x8000), pRsp->status, pRsp->transSeq, len, buf);
  }
}


/*******************************************************
                ZDP_IEEE_ADDR
*/
uint8_t cmdZdp_IeeeAddrReq(uint16_t cmd, uint16_t dest, uint8_t len, uint8_t *data, uint8_t* handle)
{
  zstack_ZStatusValues status = zstack_ZStatusValues_ZFailure;
  cmdZdpCnfParam_t *cnfParam = MFG_malloc(sizeof(cmdZdpCnfParam_t));
  if( cnfParam )
  {
    zstack_zdoIeeeAddrReq_t req;
    req.nwkAddr = dest;
    req.type = (zstack_NwkAddrReqType)data[0];
    req.startIndex = data[1];
    req.extParam.afCnfCB = cmdZdp_SendCnf;
    req.extParam.cnfParam = cnfParam;
    status = Zstackapi_ZdoIeeeAddrReq(cmdPktTaskId, &req);
    if((zstack_ZStatusValues_ZSuccess == status) && (req.extParam.cnfParam == cnfParam))
    {
      cnfParam->cmd = cmd;
      cnfParam->nodeId = dest;
      cnfParam->handle = req.extParam.seqNum;
      *handle = req.extParam.seqNum;
    }
    else
    {
      MFG_free(cnfParam);
    }
  }
  return (uint8_t)status;
}

void cmdZdp_IeeeAddrRsp(zstack_zdoIeeeAddrRspInd_t *pRsp)
{
  if(cmdZdp_SeqCmp(pRsp->transSeq))
  {
    uint8_t len = 0;
    uint8_t buf[100];
    uint8_t *p = buf;
    uint8_t n;
    //IEEE
    memcpy( p, pRsp->ieeeAddr, 8);
    p += 8;
    len += 8;
    //start index & assoc num
    *p ++ = pRsp->startIndex;
    *p ++ = pRsp->n_assocDevList;
    len += 2;
    //assoc list
    for( n=0; n<pRsp->n_assocDevList; n++ )
    {
      *p++ = LO_UINT16(pRsp->pAssocDevList[n]);
      *p++ = HI_UINT16(pRsp->pAssocDevList[n]);
      len += 2;
    }
    cmdZdp_CmdOutput( pRsp->nwkAddr, (ZDP_IEEE_ADDR | 0x8000), pRsp->status, pRsp->transSeq, len, buf);
  }
}

/*******************************************************
               ZDP_NODE_DESC
*/
uint8_t cmdZdp_NodeDescReq(uint16_t cmd, uint16_t dest, uint8_t len, uint8_t *data, uint8_t* handle)
{
  zstack_ZStatusValues status = zstack_ZStatusValues_ZFailure;
  cmdZdpCnfParam_t *cnfParam = MFG_malloc(sizeof(cmdZdpCnfParam_t));
  if( cnfParam )
  {
    zstack_zdoNodeDescReq_t req;
    req.dstAddr = dest;
    req.nwkAddrOfInterest = dest;
    
    req.extParam.afCnfCB = cmdZdp_SendCnf;
    req.extParam.cnfParam = cnfParam;
    status = Zstackapi_ZdoNodeDescReq(cmdPktTaskId, &req);
    if((zstack_ZStatusValues_ZSuccess == status) && (req.extParam.cnfParam == cnfParam))
    {
      cnfParam->cmd = cmd;
      cnfParam->nodeId = dest;
      cnfParam->handle = req.extParam.seqNum;
      *handle = req.extParam.seqNum;
    }
    else
    {
      MFG_free(cnfParam);
    }
  }
  return (uint8_t)status;
}

void cmdZdp_NodeDescRsp(zstack_zdoNodeDescRspInd_t *pRsp)
{
  if(cmdZdp_SeqCmp(pRsp->transSeq))
  {
    uint8_t len = 0;
    uint8_t buf[13];
    if(pRsp->status == zstack_ZdpStatus_SUCCESS)
    {
      uint8_t *p = buf;
      *p ++ = pRsp->nodeDesc.logicalType;
      *p ++ = pRsp->nodeDesc.freqBand;
      *p ++ = pRsp->nodeDesc.capInfo.rxOnWhenIdle;
      *p ++ = pRsp->nodeDesc.capInfo.ffd;
      *p ++ = LO_UINT16(pRsp->nodeDesc.manufacturerCode);
      *p ++ = HI_UINT16(pRsp->nodeDesc.manufacturerCode);
      *p ++ = pRsp->nodeDesc.maxBufferSize;
      *p ++ = LO_UINT16(pRsp->nodeDesc.maxInTransferSize);
      *p ++ = HI_UINT16(pRsp->nodeDesc.maxInTransferSize);
      *p ++ = LO_UINT16(pRsp->nodeDesc.maxOutTransferSize);
      *p ++ = HI_UINT16(pRsp->nodeDesc.maxOutTransferSize);
      *p ++ = pRsp->nodeDesc.stackRev;
      *p ++ = 0;
      len += 13;
    }
    cmdZdp_CmdOutput( (ZDP_NODE_DESC | 0x8000), pRsp->nwkAddrOfInterest, pRsp->status, pRsp->transSeq, len, buf);
  }
}

/*******************************************************
               ZDP_SIMPLE_DESC
*/
uint8_t cmdZdp_SimpleDescReq(uint16_t cmd, uint16_t dest, uint8_t len, uint8_t *data, uint8_t* handle)
{
  if(len < 1)
  {
    return zstack_ZStatusValues_ZFailure;
  }
  zstack_ZStatusValues status = zstack_ZStatusValues_ZFailure;
  cmdZdpCnfParam_t *cnfParam = MFG_malloc(sizeof(cmdZdpCnfParam_t));
  if( cnfParam )
  {
    zstack_zdoSimpleDescReq_t req;
    req.dstAddr = dest;
    req.nwkAddrOfInterest = dest;
    req.endpoint = data[0];
    req.extParam.afCnfCB = cmdZdp_SendCnf;
    req.extParam.cnfParam = cnfParam;
    status = Zstackapi_ZdoSimpleDescReq(cmdPktTaskId, &req);
    if((zstack_ZStatusValues_ZSuccess == status) && (req.extParam.cnfParam == cnfParam))
    {
      cnfParam->cmd = cmd;
      cnfParam->nodeId = dest;
      cnfParam->handle = req.extParam.seqNum;
      *handle = req.extParam.seqNum;
    }
    else
    {
      MFG_free(cnfParam);
    }
  }
  return (uint8_t)status;
}

void cmdZdp_SimpleDescRsp(zstack_zdoSimpleDescRspInd_t *pRsp)
{
  if(cmdZdp_SeqCmp(pRsp->transSeq))
  {
    uint8_t len = 0;
    uint8_t buf[80];
    if(pRsp->status == zstack_ZdpStatus_SUCCESS)
    {
      uint8_t *p = buf + 6;
      uint8_t  i;
      len = 8 + pRsp->simpleDesc.n_inputClusters*2 + pRsp->simpleDesc.n_outputClusters*2;
      buf[0] = pRsp->simpleDesc.endpoint;
      buf[1] = LO_UINT16(pRsp->simpleDesc.profileID);
      buf[2] = HI_UINT16(pRsp->simpleDesc.profileID);
      buf[3] = LO_UINT16(pRsp->simpleDesc.deviceID);
      buf[4] = HI_UINT16(pRsp->simpleDesc.deviceID);
      buf[5] = pRsp->simpleDesc.deviceVer;
      *p++ = pRsp->simpleDesc.n_inputClusters;
      for( i=0 ;i < pRsp->simpleDesc.n_inputClusters; i++ )
      {
        *p++ = LO_UINT16(pRsp->simpleDesc.pInputClusters[i]);
        *p++ = HI_UINT16(pRsp->simpleDesc.pInputClusters[i]);
      }
      *p++ = pRsp->simpleDesc.n_outputClusters;
      for( i=0 ; i < pRsp->simpleDesc.n_outputClusters; i++ )
      {
        *p++ = LO_UINT16(pRsp->simpleDesc.pOutputClusters[i]);
        *p++ = HI_UINT16(pRsp->simpleDesc.pOutputClusters[i]);
      }
    }
    cmdZdp_CmdOutput( (ZDP_SIMPLE_DESC | 0x8000), pRsp->nwkAddrOfInterest, pRsp->status, pRsp->transSeq, len, buf);
  }
}

/*******************************************************
               ZDP_ACTIVE_EP
*/
uint8_t cmdZdp_ActiveEpReq(uint16_t cmd, uint16_t dest, uint8_t len, uint8_t *data, uint8_t* handle)
{
  zstack_ZStatusValues status = zstack_ZStatusValues_ZFailure;
  cmdZdpCnfParam_t *cnfParam = MFG_malloc(sizeof(cmdZdpCnfParam_t));
  if( cnfParam )
  {
    zstack_zdoActiveEndpointReq_t req;
    req.dstAddr = dest;
    req.nwkAddrOfInterest = dest;
    req.extParam.afCnfCB = cmdZdp_SendCnf;
    req.extParam.cnfParam = cnfParam;
    status = Zstackapi_ZdoActiveEndpointReq(cmdPktTaskId, &req);
    if((zstack_ZStatusValues_ZSuccess == status) && (req.extParam.cnfParam == cnfParam))
    {
      cnfParam->cmd = cmd;
      cnfParam->nodeId = dest;
      cnfParam->handle = req.extParam.seqNum;
      *handle = req.extParam.seqNum;
    }
    else
    {
      MFG_free(cnfParam);
    }
  }
  return (uint8_t)status;
}

void cmdZdp_ActiveEpRsp(zstack_zdoActiveEndpointsRspInd_t *pRsp)
{
  if(cmdZdp_SeqCmp(pRsp->transSeq))
  {
    uint8_t len = 0;
    uint8_t buf[80];
    if(pRsp->status == zstack_ZdpStatus_SUCCESS)
    {
      uint8_t *p = buf + 1;
      len = 1 + pRsp->n_activeEPList;
      buf[0] = pRsp->n_activeEPList;
      memcpy( p, pRsp->pActiveEPList, pRsp->n_activeEPList );
      cmdZdp_CmdOutput( (ZDP_ACTIVE_EP | 0x8000), pRsp->nwkAddrOfInterest, pRsp->status, pRsp->transSeq, len, buf);
    }
  }
}

/*******************************************************
               ZDP_BIND
*/
uint8_t cmdZdp_BindReq(uint16_t cmd, uint16_t dest, uint8_t len, uint8_t *data, uint8_t* handle)
{
  if(len < 20)
  {
    return zstack_ZStatusValues_ZFailure;
  }
  uint16_t cluster = BUILD_UINT16(data[0], data[1]);
  uint8_t *srcDevSn = data+2;
  uint8_t *dstDevSn = srcDevSn+9;
  zstack_ZStatusValues status = zstack_ZStatusValues_ZFailure;
  cmdZdpCnfParam_t *cnfParam = MFG_malloc(sizeof(cmdZdpCnfParam_t));
  if( cnfParam )
  {
    zstack_zdoBindReq_t req;
    req.nwkAddr = dest;
    //set bind cluster ID
    req.bindInfo.clusterID = cluster;
    //set src dev SN
    req.bindInfo.srcEndpoint = srcDevSn[0];
    memcpy( req.bindInfo.srcAddr, srcDevSn+1, 8);
    //set dst dev SN
    req.bindInfo.dstAddr.addrMode = zstack_AFAddrMode_EXT;
    req.bindInfo.dstAddr.endpoint = dstDevSn[0];
    memcpy( req.bindInfo.dstAddr.addr.extAddr, dstDevSn+1, 8);

    req.extParam.afCnfCB = cmdZdp_SendCnf;
    req.extParam.cnfParam = cnfParam;
    status = Zstackapi_ZdoBindReq(cmdPktTaskId, &req);
    if((zstack_ZStatusValues_ZSuccess == status) && (req.extParam.cnfParam == cnfParam))
    {
      cnfParam->cmd = cmd;
      cnfParam->nodeId = dest;
      cnfParam->handle = req.extParam.seqNum;
      *handle = req.extParam.seqNum;
    }
    else
    {
      MFG_free(cnfParam);
    }
  }
  return (uint8_t)status;
}

void cmdZdp_BindRsp(zstack_zdoBindRspInd_t* pRsp)
{
  if(cmdZdp_SeqCmp(pRsp->transSeq))
  {
    cmdZdp_CmdOutput( (ZDP_BIND | 0x8000), pRsp->srcAddr, pRsp->status, pRsp->transSeq, 0, NULL);
  }
}

/*******************************************************
               ZDP_Unind
*/
uint8_t cmdZdp_UnbindReq(uint16_t cmd, uint16_t dest, uint8_t len, uint8_t *data, uint8_t* handle)
{
  if(len < 20)
  {
    return zstack_ZStatusValues_ZFailure;
  }
  uint16_t cluster = BUILD_UINT16(data[0], data[1]);
  uint8_t *srcDevSn = data+2;
  uint8_t *dstDevSn = srcDevSn+9;
  zstack_ZStatusValues status = zstack_ZStatusValues_ZFailure;
  cmdZdpCnfParam_t *cnfParam = MFG_malloc(sizeof(cmdZdpCnfParam_t));
  if( cnfParam )
  {
    zstack_zdoUnbindReq_t req;
    req.nwkAddr = dest;
    //set bind cluster ID
    req.bindInfo.clusterID = cluster;
    //set src dev SN
    req.bindInfo.srcEndpoint = srcDevSn[0];
    memcpy( req.bindInfo.srcAddr, srcDevSn+1, 8);
    //set dst dev SN
    req.bindInfo.dstAddr.addrMode = zstack_AFAddrMode_EXT;
    req.bindInfo.dstAddr.endpoint = dstDevSn[0];
    memcpy( req.bindInfo.dstAddr.addr.extAddr, dstDevSn+1, 8);

    req.extParam.afCnfCB = cmdZdp_SendCnf;
    req.extParam.cnfParam = cnfParam;
    status = Zstackapi_ZdoUnbindReq(cmdPktTaskId, &req);
    if((zstack_ZStatusValues_ZSuccess == status) && (req.extParam.cnfParam == cnfParam))
    {
      cnfParam->cmd = cmd;
      cnfParam->nodeId = dest;
      cnfParam->handle = req.extParam.seqNum;
      *handle = req.extParam.seqNum;
    }
    else
    {
      MFG_free(cnfParam);
    }
  }
  return (uint8_t)status;
}

void cmdZdp_UnbindRsp(zstack_zdoUnbindRspInd_t* pRsp)
{
  if(cmdZdp_SeqCmp(pRsp->transSeq))
  {
    cmdZdp_CmdOutput( (ZDP_UNBIND | 0x8000), pRsp->srcAddr, pRsp->status, pRsp->transSeq, 0, NULL);
  }
}

/*******************************************************
               ZDP_LQI_TABLE
*/
uint8_t cmdZdp_LqiTableReq(uint16_t cmd, uint16_t dest, uint8_t len, uint8_t *data, uint8_t* handle)
{
  if(!len)
  {
    return zstack_ZStatusValues_ZFailure;
  }
  zstack_ZStatusValues status = zstack_ZStatusValues_ZFailure;
  cmdZdpCnfParam_t *cnfParam = MFG_malloc(sizeof(cmdZdpCnfParam_t));
  if( cnfParam )
  {
    zstack_zdoMgmtLqiReq_t req;
    req.nwkAddr = dest;
    req.startIndex = data[0];

    req.extParam.afCnfCB = cmdZdp_SendCnf;
    req.extParam.cnfParam = cnfParam;
    status = Zstackapi_ZdoMgmtLqiReq(cmdPktTaskId, &req);
    if((zstack_ZStatusValues_ZSuccess == status) && (req.extParam.cnfParam == cnfParam))
    {
      cnfParam->cmd = cmd;
      cnfParam->nodeId = dest;
      cnfParam->handle = req.extParam.seqNum;
      *handle = req.extParam.seqNum;
    }
    else
    {
      MFG_free(cnfParam);
    }
  }
  return (uint8_t)status;
}

void cmdZdp_LqiTableRsp(zstack_zdoMgmtLqiRspInd_t *pRsp)
{
  if(cmdZdp_SeqCmp(pRsp->transSeq))
  {
    uint8_t len = 0;
    uint8_t buf[100];
    if(pRsp->status == zstack_ZdpStatus_SUCCESS)
    {
      uint8_t n;
      uint8_t *p = buf + 3;
      len += 3;
      buf[0] = pRsp->neighborLqiEntries;
      buf[1] = pRsp->startIndex;
      buf[2] = pRsp->n_lqiList;
      for(n=0; n < pRsp->n_lqiList; n++)
      {
        zstack_nwkLqiItem_t *pList = &pRsp->pLqiList[n];
        *p++ = LO_UINT16(pList->nwkAddr);
        *p++ = HI_UINT16(pList->nwkAddr);
        len += 2;
        memcpy(p, pList->extendedAddr, 8);
        p += 8;
        len += 8;
        //Info
        *p++ = (uint8_t)pList->deviceType;
        *p++ = (uint8_t)pList->relationship;
        *p++ = (uint8_t)pList->depth;
        *p++ = pList->rxLqi;
        len += 4;
      }
    }

    cmdZdp_CmdOutput( (ZDP_LQI_TABLE | 0x8000), pRsp->srcAddr, pRsp->status, pRsp->transSeq, len, buf );
  }
}

/*******************************************************
               ZDP_BIND_TABLE
*/
uint8_t cmdZdp_BindTableReq(uint16_t cmd, uint16_t dest, uint8_t len, uint8_t *data, uint8_t* handle)
{
  if(!len)
  {
    return zstack_ZStatusValues_ZFailure;
  }
  zstack_ZStatusValues status = zstack_ZStatusValues_ZFailure;
  cmdZdpCnfParam_t *cnfParam = MFG_malloc(sizeof(cmdZdpCnfParam_t));
  if( cnfParam )
  {
    zstack_zdoMgmtBindReq_t req;
    req.nwkAddr = dest;
    req.startIndex = data[0];

    req.extParam.afCnfCB = cmdZdp_SendCnf;
    req.extParam.cnfParam = cnfParam;
    status = Zstackapi_ZdoMgmtBindReq(cmdPktTaskId, &req);
    if((zstack_ZStatusValues_ZSuccess == status) && (req.extParam.cnfParam == cnfParam))
    {
      cnfParam->cmd = cmd;
      cnfParam->nodeId = dest;
      cnfParam->handle = req.extParam.seqNum;
      *handle = req.extParam.seqNum;
    }
    else
    {
      MFG_free(cnfParam);
    }
  }
  return (uint8_t)status;
}

void cmdZdp_BindTableRsp(zstack_zdoMgmtBindRspInd_t* pRsp)
{
  if(cmdZdp_SeqCmp(pRsp->transSeq))
  {
    uint8_t len = 3;
    uint8_t buf[100];
    uint8_t *p = buf + 3;
    uint8_t n;
    buf[0] = pRsp->bindEntries;
    buf[1] = pRsp->startIndex;
    buf[2] = pRsp->n_bindList;
    for(n=0; n<pRsp->n_bindList; n++)
    {
      zstack_bindItem_t *pList = &pRsp->pBindList[n];
      //cluster
      *p++ = LO_UINT16(pList->clustedID);
      *p++ = HI_UINT16(pList->clustedID);
      len += 2;
      //-------------- src SN --------------
      //src EP
      *p++ = pList->srcEndpoint;
      len += 1;
      //src MAC
      memcpy(p, pList->srcAddr, 8);
      p += 8;
      len += 8;
      //------------ dest SN ----------------
      //dest EP
      *p++ = pList->dstAddr.endpoint;
      len += 1;
      //dest addr
      if(pList->dstAddr.addrMode == zstack_AFAddrMode_EXT)
      {
        memcpy( p, pList->dstAddr.addr.extAddr, 8 );
      }
      else
      {
        memset( p, 0, 8);
        p[0] = LO_UINT16(pList->dstAddr.addr.shortAddr);
        p[1] = HI_UINT16(pList->dstAddr.addr.shortAddr);
      }
      p += 8;
      len += 8;
    }
    cmdZdp_CmdOutput( (ZDP_BIND_TABLE | 0x8000), pRsp->srcAddr, pRsp->status, pRsp->transSeq, len, buf );
  }
}

/*******************************************************
               ZDP_LEAVE
*/
uint8_t cmdZdp_LeaveReq(uint16_t cmd, uint16_t dest, uint8_t len, uint8_t *data, uint8_t* handle)
{
  if(len < 10)
  {
    return zstack_ZStatusValues_ZFailure;
  }
  zstack_ZStatusValues status = zstack_ZStatusValues_ZFailure;
  cmdZdpCnfParam_t *cnfParam = MFG_malloc(sizeof(cmdZdpCnfParam_t));
  if( cnfParam )
  {
    zstack_zdoMgmtLeaveReq_t req;
    req.nwkAddr = dest;
    memcpy( req.deviceAddress, data, 8 );
    req.options.rejoin = data[8];
    req.options.removeChildren = data[9];
    
    req.extParam.afCnfCB = cmdZdp_SendCnf;
    req.extParam.cnfParam = cnfParam;
    status = Zstackapi_ZdoMgmtLeaveReq(cmdPktTaskId, &req);
    if((zstack_ZStatusValues_ZSuccess == status) && (req.extParam.cnfParam == cnfParam))
    {
      cnfParam->cmd = cmd;
      cnfParam->nodeId = dest;
      cnfParam->handle = req.extParam.seqNum;
      *handle = req.extParam.seqNum;
    }
    else
    {
      MFG_free(cnfParam);
    }
  }
  return (uint8_t)status;
}

void cmdZdp_LeaveRsp(zstack_zdoMgmtLeaveRspInd_t* pRsp)
{
  if(cmdZdp_SeqCmp(pRsp->transSeq))
  {
    cmdZdp_CmdOutput( (ZDP_LEAVE | 0x8000), pRsp->srcAddr, pRsp->status, pRsp->transSeq, 0, NULL );
  }
}
