#include "ti_drivers_config.h"
#include <ti/sysbios/knl/Semaphore.h>
#include <xdc/std.h>
#include "hal_types.h"
#include "util_timer.h"
#include "zcl.h"
#include "zcl_port.h"
#include "zcl_ha.h"

#include "cmd_pkt.h"
#include "cmd_zcl.h"
#include "cmd.h"

/* typedef */
//taget:[AddrMode][NodeID][EP][profile][Cluster]
//control:[CMD][zclSeqNum]
//param:[dir][RSP][manu code]
typedef struct
{
  afAddrType_t dest;
  uint16_t profId;
  uint16_t cluster;
  uint8_t  cmdId;
  uint8_t  seqNum;
  uint8_t  direct;
  uint8_t  noRsp;
  uint8_t  sec;
  uint16_t manuCode;
}cmdZclCmdHdr_t;

/* typedef */
//taget:[AddrMode][NodeID][EP][profile][Cluster]
//control:[CMD][zclSeqNum]
//param:[dir][RSP][manu code]
typedef struct
{
  uint8_t  addrMode;
  uint16_t nodeId;
  uint8_t  ep;
  uint8_t  profile;
  uint16_t cluster;
}cmdZclIndHdr_t;

/* static variable */
uint8_t cmdZcl_FindEpByProfId(uint16_t profId);
uint16_t cmdZcl_FindProfIdByEp(uint8_t ep);
extern endPointDesc_t *zcl_afFindEndPointDesc(uint8_t EndPoint);

//static endPointDesc_t *pCmdZcl_EpDesc_HA = NULL;
static uint8_t cmdZcl_Ep[CMD_EP_ZCL_MAX];

static uint8_t cmdZcl_CmdHdrParse(uint8_t *cmdData, cmdZclCmdHdr_t *pCmdHdr);
//static uint8_t cmdZcl_AttrAlig(uint8_t size);
static void cmdZcl_SendCmdCnf(uint8 status, uint8 endpoint, uint8 transID, uint16_t clusterID, void* cnfParam);
static void cmdZcl_SendCtrCnf(uint8 status, uint8 endpoint, uint8 transID, uint16_t clusterID, void* cnfParam);

//cmd input
static ZStatus_t cmdZcl_Read(uint8_t srcEP, cmdZclCmdHdr_t *pCmdHdr, uint8_t len, uint8_t *data);
static ZStatus_t cmdZcl_Write(uint8_t srcEP, cmdZclCmdHdr_t *pCmdHdr, uint8_t len, uint8_t *data);
static ZStatus_t cmdZcl_CfgRept(uint8_t srcEP, cmdZclCmdHdr_t *pCmdHdr, uint8_t len, uint8_t *data);
static ZStatus_t cmdZcl_ReadRept(uint8_t srcEP, cmdZclCmdHdr_t *pCmdHdr, uint8_t len, uint8_t *data);
static ZStatus_t cmdZcl_DiscAttr(uint8_t srcEP, cmdZclCmdHdr_t *pCmdHdr, uint8_t len, uint8_t *data, bool ext);
static ZStatus_t cmdZcl_DiscCmd(uint8_t srcEP, cmdZclCmdHdr_t *pCmdHdr, uint8_t len, uint8_t *data, uint8_t cmd);

//cmd output
static uint8_t CmdZcl_ReadRspInd(uint8_t *buff, zclReadRspCmd_t *readRspCmd);
static uint8_t CmdZcl_WriteRspInd(uint8_t *buff, zclWriteRspCmd_t *writeRspCmd);
static uint8_t CmdZcl_CfgReptRspInd(uint8_t *buff, zclCfgReportRspCmd_t *cfgReportRspCmd);
static uint8_t CmdZcl_ReadReptRspInd(uint8_t *buff, zclReadReportCfgRspCmd_t *readReportCfgRspCmd);
static uint8_t CmdZcl_ReportInd(uint8_t *buff, zclReportCmd_t *reportCmd);
static uint8_t CmdZcl_DefaultRspInd(uint8_t *buff, zclDefaultRspCmd_t *defaultRspCmd);
static uint8_t CmdZcl_DiscAttrRspInd(uint8_t *buff, zclDiscoverAttrsRspCmd_t *discoverRspCmd);
static uint8_t CmdZcl_DiscAttrExtRspInd(uint8_t *buff, zclDiscoverAttrsExtRsp_t *discoverRspCmd);
static uint8_t CmdZcl_DiscCmdRspInd(uint8_t *buff, zclDiscoverCmdsCmdRsp_t *discoverRspCmd);

/*
* @fn          cmdZcl_CmdInput
*
* @brief       uart input profile command
*
* @param       cmdSeq - input cmd seq
*              cmdClass - input cmd class
*              cmdLen - input cmd length
*              cmdData - input cmd data
*
* @return      none
* */
void cmdZcl_CmdInput(uint8_t cmdSeq, uint8_t cmdClass, uint8_t cmdLen, uint8_t* cmdData)
{
  uint8_t cmdHdrLen;
  uint8_t paramLen;
  uint8_t *param;
  uint8_t srcEP;
  uint8_t retStatus = ZFailure;
  cmdZclCmdHdr_t *pCmdHdr = MFG_malloc(sizeof(cmdZclCmdHdr_t));
  if(pCmdHdr)
  {
    cmdHdrLen = cmdZcl_CmdHdrParse(cmdData, pCmdHdr);
    paramLen = cmdLen - cmdHdrLen;
    param = cmdData + cmdHdrLen;
    srcEP = cmdZcl_FindEpByProfId(pCmdHdr->profId);
    if((srcEP < 0xF0) && (paramLen>0) && (cmdHdrLen))
    {
      switch(pCmdHdr->cmdId)
      {
      case ZCL_CMD_READ:
        retStatus = cmdZcl_Read(srcEP,pCmdHdr,paramLen,param);
        break;
        
      case ZCL_CMD_WRITE:
        retStatus = cmdZcl_Write(srcEP,pCmdHdr,paramLen,param);
        break;
        
      case ZCL_CMD_CONFIG_REPORT:
        retStatus = cmdZcl_CfgRept(srcEP,pCmdHdr,paramLen,param);
        break;
        
      case ZCL_CMD_READ_REPORT_CFG:
        retStatus = cmdZcl_ReadRept(srcEP,pCmdHdr,paramLen,param);
        break;
        
      case ZCL_CMD_DISCOVER_ATTRS:
        retStatus = cmdZcl_DiscAttr(srcEP,pCmdHdr,paramLen,param, false);
        break;
        
      case ZCL_CMD_DISCOVER_CMDS_RECEIVED:
        retStatus = cmdZcl_DiscCmd(srcEP,pCmdHdr,paramLen,param, pCmdHdr->cmdId);
        break;
        
      case ZCL_CMD_DISCOVER_CMDS_GEN:
        retStatus = cmdZcl_DiscCmd(srcEP,pCmdHdr,paramLen,param, pCmdHdr->cmdId);
        break;
        
      case ZCL_CMD_DISCOVER_ATTRS_EXT:
        retStatus = cmdZcl_DiscAttr(srcEP,pCmdHdr,paramLen,param, true);
        break;
      }
    }
    
    if(retStatus != ZSuccess)
    {
      MFG_free(pCmdHdr);
    }
  }
  CmdPkt_AckSend(cmdSeq,cmdClass,1,&retStatus);
}

/*
* @fn          cmdZcl_CtrInput
*
* @brief       uart input specific command
*
* @param       cmdSeq - input cmd seq
*              cmdClass - input cmd class
*              cmdLen - input cmd length
*              cmdData - input cmd data
*
* @return      none
*
* */
void cmdZcl_CtrInput(uint8_t cmdSeq, uint8_t cmdClass, uint8_t cmdLen, uint8_t* cmdData)
{
  uint8_t cmdHdrLen;
  uint8_t paramLen;
  uint8_t *param;
  uint8_t srcEP;
  uint8_t retStatus = ZFailure;
  cmdZclCmdHdr_t *pCmdHdr = MFG_malloc(sizeof(cmdZclCmdHdr_t));
  if(pCmdHdr)
  {
    cmdHdrLen = cmdZcl_CmdHdrParse(cmdData, pCmdHdr);
    if(cmdLen >= cmdHdrLen)
    {
      paramLen = cmdLen - cmdHdrLen;
      param = cmdData + cmdHdrLen;
      srcEP = cmdZcl_FindEpByProfId(pCmdHdr->profId);
      
      uint8_t option = 0;
      if(pCmdHdr->noRsp >= 2)
      {
        option = AF_ACK_REQUEST;
      }
      if( pCmdHdr->sec )
      {
        option |= AF_EN_SECURITY;
      }
      if((srcEP < 0xF0) && (cmdHdrLen))
      {
        zcl_SetSendExtParam(cmdZcl_SendCtrCnf,pCmdHdr,option);
        retStatus = zcl_SendCommand( srcEP, &pCmdHdr->dest, pCmdHdr->cluster, pCmdHdr->cmdId, true, pCmdHdr->direct,
                                    pCmdHdr->noRsp, pCmdHdr->manuCode, pCmdHdr->seqNum, paramLen, param );
      }
    }
    if(retStatus != ZSuccess)
    {
      MFG_free(pCmdHdr);
    }
  }
  CmdPkt_AckSend(cmdSeq,cmdClass,1,&retStatus);
}

/************************************************* 
                  static function
*/

/*
* @fn          cmdZcl_CmdHdrParse
*
* @brief       
*
*/
static uint8_t cmdZcl_CmdHdrParse(uint8_t *cmdData, cmdZclCmdHdr_t *pCmdHdr)
{
  //taget:[AddrMode][NodeID][EP][profile][Cluster]
  //control:[CMD][zclSeqNum][dir][RSP][manu code]
  if(cmdData[CMD_MODE_IDX] == CMD_ZCL_UNICAST)
  {
    pCmdHdr->dest.addrMode = afAddr16Bit;
  }
  else if(cmdData[CMD_MODE_IDX] == CMD_ZCL_MULTCAST)
  {
    pCmdHdr->dest.addrMode = afAddrGroup;
  }
  else
  {
    return 0;
  }
  pCmdHdr->dest.addr.shortAddr = BUILD_UINT16(cmdData[CMD_NODE_L_IDX],cmdData[CMD_NODE_H_IDX]);
  pCmdHdr->dest.endPoint = cmdData[CMD_EP_IDX];
  pCmdHdr->profId = BUILD_UINT16(cmdData[CMD_PROF_L_IDX],cmdData[CMD_PROF_H_IDX]);
  pCmdHdr->cluster = BUILD_UINT16(cmdData[CMD_CLU_L_IDX],cmdData[CMD_CLU_H_IDX]);
  pCmdHdr->cmdId = cmdData[CMD_CMDID_IDX];
  pCmdHdr->seqNum = cmdData[CMD_SEQ_IDX];
  pCmdHdr->direct = cmdData[CMD_DIR_IDX];
  pCmdHdr->noRsp = cmdData[CMD_RSP_IDX] & 0x0F;
  pCmdHdr->sec = ((cmdData[CMD_RSP_IDX] & 0x80) != 0);
  pCmdHdr->manuCode = BUILD_UINT16(cmdData[CMD_MANU_L_IDX],cmdData[CMD_MANU_H_IDX]);
  return 14;
}

/*
* @fn          cmdZcl_SendCmdCnf
*
* @brief       
*
*/
void cmdZcl_SendCmdCnf(uint8 status, uint8 endpoint, uint8 transID, uint16_t clusterID, void* cnfParam)
{
  if(cnfParam)
  {
    uint8_t CmdBuff[14];
    cmdZclCmdHdr_t *pCmdHdr = cnfParam;
    uint8_t *p = CmdBuff+2;
    uint8_t mode = CMD_ZCL_UNICAST;
    if(pCmdHdr->dest.addrMode == afAddrGroup)
    {
      mode = CMD_ZCL_MULTCAST;
    }
    CmdBuff[0] = CmdPkt_GetSeqNum();
    CmdBuff[1] = CMD_CLASS_PROFILE;
    
    p[CMD_MODE_IDX] = mode;
    p[CMD_NODE_L_IDX] = LO_UINT16(pCmdHdr->dest.addr.shortAddr);
    p[CMD_NODE_H_IDX] = HI_UINT16(pCmdHdr->dest.addr.shortAddr);
    p[CMD_EP_IDX] = pCmdHdr->dest.endPoint;
    p[CMD_PROF_L_IDX] = LO_UINT16(pCmdHdr->profId);
    p[CMD_PROF_H_IDX] = HI_UINT16(pCmdHdr->profId);
    p[CMD_CLU_L_IDX] = LO_UINT16(pCmdHdr->cluster);
    p[CMD_CLU_H_IDX] = HI_UINT16(pCmdHdr->cluster);
    p[CMD_CMDID_IDX] = pCmdHdr->cmdId;
    p[CMD_SEQ_IDX] = pCmdHdr->seqNum;
    p[CMD_DIR_IDX] = pCmdHdr->direct;
    p[CMD_RSP_IDX] = status;
    MFG_free(cnfParam);
#ifndef  CMD_ALL_DATACNF    
    if(status != ZSuccess)
#endif
    {
      CmdPkt_TxSend(14, CmdBuff);
    }
  }
}

/*
* @fn          cmdZcl_SendCtrCnf
*
* @brief       
*
*/
void cmdZcl_SendCtrCnf(uint8 status, uint8 endpoint, uint8 transID, uint16_t clusterID, void* cnfParam)
{
  if(cnfParam)
  {
    uint8_t CmdBuff[14];
    cmdZclCmdHdr_t *pCmdHdr = cnfParam;
    uint8_t *p = CmdBuff+2;
    uint8_t mode = CMD_ZCL_UNICAST;
    if(pCmdHdr->dest.addrMode == afAddrGroup)
    {
      mode = CMD_ZCL_MULTCAST;
    }
    CmdBuff[0] = CmdPkt_GetSeqNum();
    CmdBuff[1] = CMD_CLASS_SPECIFIC;
    
    p[CMD_MODE_IDX] = mode;
    p[CMD_NODE_L_IDX] = LO_UINT16(pCmdHdr->dest.addr.shortAddr);
    p[CMD_NODE_H_IDX] = HI_UINT16(pCmdHdr->dest.addr.shortAddr);
    p[CMD_EP_IDX] = pCmdHdr->dest.endPoint;
    p[CMD_PROF_L_IDX] = LO_UINT16(pCmdHdr->profId);
    p[CMD_PROF_H_IDX] = HI_UINT16(pCmdHdr->profId);
    p[CMD_CLU_L_IDX] = LO_UINT16(pCmdHdr->cluster);
    p[CMD_CLU_H_IDX] = HI_UINT16(pCmdHdr->cluster);
    p[CMD_CMDID_IDX] = pCmdHdr->cmdId;
    p[CMD_SEQ_IDX] = pCmdHdr->seqNum;
    p[CMD_DIR_IDX] = pCmdHdr->direct;
    p[CMD_RSP_IDX] = status;
    MFG_free(cnfParam);
#ifndef  CMD_ALL_DATACNF
    if(status != ZSuccess)
#endif
    {
      CmdPkt_TxSend(14, CmdBuff);
    }
  }
}

/*
* @fn          cmdZcl_InitSrcEP
*
* @brief       Initial UART endpoint flowing profile ID
*
* @param       pEpDesc - address of chose endpoint
*
* @return      none
*
* */
bool cmdZcl_InitSrcEP(endPointDesc_t *pEpDesc, bool haEp)
{
  if((pEpDesc->simpleDesc->AppProfId == ZCL_HA_PROFILE_ID) && (haEp == TRUE))
  {
    if((cmdZcl_Ep[0]<1)||(cmdZcl_Ep[0]>240))
    {
      cmdZcl_Ep[0] = pEpDesc->endPoint;
      return true;
    }
  }
  else
  {
    uint8_t n;
    for( n=1; n<CMD_EP_ZCL_MAX; n++)
    {
      if((cmdZcl_Ep[n]<1)||(cmdZcl_Ep[n]>240))
      {
        cmdZcl_Ep[n] = pEpDesc->endPoint;
        return true;
      }
    }
  }
  return false;
}

/*
* @fn          cmdZcl_FindEpByProfId
*
* @brief       find endpoint from profile ID
*
* @param       profId - profile ID
*
* @return      endpoint
*
* */
uint8_t cmdZcl_FindEpByProfId(uint16_t profId)
{
  if(profId == ZCL_HA_PROFILE_ID)
  {
    return cmdZcl_Ep[0];
  }
  else
  {
    uint8_t n;
    for( n=1; n<CMD_EP_ZCL_MAX; n++)
    {
      if((cmdZcl_Ep[n]>0)&&(cmdZcl_Ep[n]<=240))
      {
        endPointDesc_t *epDesc = zcl_afFindEndPointDesc(cmdZcl_Ep[n]);
        if( epDesc && (epDesc->simpleDesc->AppProfId == profId) )
        {
          return cmdZcl_Ep[n];
        }
      }
    }
  }
  return 0xFE;
}

/*
* @fn          cmdZcl_FindProfIdByEp
*
* @brief       find profile ID by endpoint
*
* @param       ep - endpoint
*
* @return      profile ID
*
* */
uint16_t cmdZcl_FindProfIdByEp(uint8_t ep)
{
  uint8_t n;
  for( n=0; n<CMD_EP_ZCL_MAX; n++)
  {
    if( cmdZcl_Ep[n] == ep )
    {
      endPointDesc_t *epDesc = zcl_afFindEndPointDesc(ep);
      if( epDesc )
      {
        return epDesc->simpleDesc->AppProfId;
      }
    }
  }
  return CMD_INVALID_PROFILE;
}

/*********************************************************************
* @fn      CmdZcl_ProcessIncomingMsg
*
* @brief   Process ZCL Foundation incoming message
*
* @param   pInMsg - pointer to the received message
*
* @return  none
*/
uint8 CmdZcl_ProcessIncomingMsg( zclIncoming_t *pInMsg )
{
  //src:[AddrMode][NodeID][EP][profile][Cluster]
  //control:[CMD][zclSeqNum][dir][Bcst][manu code]
  uint8 handled = FALSE;
  uint8_t cmdBuff[256];
  uint8_t *p = cmdBuff;
  uint8_t pLen = 0;
  uint8_t cmdId = pInMsg->hdr.commandID;
  uint16_t profile = cmdZcl_FindProfIdByEp(pInMsg->msg->endPoint);
  if(CMD_INVALID_PROFILE == profile)
  {
    return handled;
  }
  //Uart Header
  p[0] = CmdPkt_GetSeqNum();
  p[1] = CMD_CLASS_PROF_IND;
  p += 2;
  pLen += 2;
  //[AddrMode]
  if(pInMsg->msg->wasBroadcast)
  {
    *p = CMD_ZCL_BCST;
  }
  else
  {
    *p = CMD_ZCL_UNICAST;
  }
  p += 1;
  pLen += 1;
  //[NodeID]
  MFG_LOAD_U16(p, pInMsg->msg->srcAddr.addr.shortAddr);
  p += 2;
  pLen += 2;
  //[EP]
  *p++ = pInMsg->msg->srcAddr.endPoint;
  pLen += 1;
  //[profile]
  MFG_LOAD_U16(p, profile);
  p += 2;
  pLen += 2;
  //[Cluster]
  MFG_LOAD_U16(p, pInMsg->msg->clusterId);
  p += 2;
  pLen += 2;
  //[CMD]
  *p++ = cmdId;
  pLen += 1;
  //[zclSeqNum]
  *p++ = pInMsg->hdr.transSeqNum;
  pLen += 1;
  //[dir]
  *p++ = pInMsg->hdr.fc.direction;
  pLen += 1;
  //[Bcst]
  *p++ = pInMsg->msg->radius;
  pLen += 1;
  //[manu code]
  MFG_LOAD_U16(p, pInMsg->hdr.manuCode);
  p += 2;
  pLen += 2;
  //file cmd data
  switch (cmdId)
  {
  case ZCL_CMD_READ_RSP:
    pLen += CmdZcl_ReadRspInd(p, (zclReadRspCmd_t*)pInMsg->attrCmd);
    handled = TRUE;
    break;
    
  case ZCL_CMD_WRITE_RSP:
    pLen += CmdZcl_WriteRspInd(p, (zclWriteRspCmd_t*)pInMsg->attrCmd);
    handled = TRUE;
    break;
    
  case ZCL_CMD_CONFIG_REPORT_RSP:
    pLen += CmdZcl_CfgReptRspInd(p, (zclCfgReportRspCmd_t*)pInMsg->attrCmd);
    handled = TRUE;
    break;
    
  case ZCL_CMD_READ_REPORT_CFG_RSP:
    pLen += CmdZcl_ReadReptRspInd(p, (zclReadReportCfgRspCmd_t*)pInMsg->attrCmd);
    handled = TRUE;
    break;
    
  case ZCL_CMD_REPORT:
    pLen += CmdZcl_ReportInd(p, (zclReportCmd_t*)pInMsg->attrCmd);
    handled = TRUE;
    break;
  case ZCL_CMD_DEFAULT_RSP:
    pLen += CmdZcl_DefaultRspInd(p, (zclDefaultRspCmd_t*)pInMsg->attrCmd);
    handled = TRUE;
    break;
    
  case ZCL_CMD_DISCOVER_ATTRS_RSP:
    pLen += CmdZcl_DiscAttrRspInd(p, (zclDiscoverAttrsRspCmd_t*)pInMsg->attrCmd);
    handled = TRUE;
    break;
    
  case ZCL_CMD_DISCOVER_CMDS_RECEIVED_RSP:
    pLen += CmdZcl_DiscCmdRspInd(p, (zclDiscoverCmdsCmdRsp_t*)pInMsg->attrCmd);
    handled = TRUE;
    break;
    
  case ZCL_CMD_DISCOVER_CMDS_GEN_RSP:
    pLen += CmdZcl_DiscCmdRspInd(p, (zclDiscoverCmdsCmdRsp_t*)pInMsg->attrCmd);
    handled = TRUE;
    break;
    
  case ZCL_CMD_DISCOVER_ATTRS_EXT_RSP:
    pLen += CmdZcl_DiscAttrExtRspInd(p, (zclDiscoverAttrsExtRsp_t*)pInMsg->attrCmd);
    handled = TRUE;
    break;
    
  default:
    break;
  }
  //Uart Send
  CmdPkt_TxSend(pLen,cmdBuff);
  return handled;
}

/*********************************************************************
* @fn      CmdZcl_ZclPluginCallback
*
* @brief   Process ZCL Foundation incoming message
*
* @param   pInMsg - pointer to the received message
*
* @return  none
*/
ZStatus_t CmdZcl_ZclPluginCallback( zclIncoming_t *pInMsg )
{
  uint8_t cmdBuff[256];
  uint8_t *p = cmdBuff;
  uint8_t pLen = 0;
  uint8_t cmdId = pInMsg->hdr.commandID;
  uint16_t profile = cmdZcl_FindProfIdByEp(pInMsg->msg->endPoint);
  if(CMD_INVALID_PROFILE == profile)
  {
    return ZFailure;
  }
  //Uart Header
  p[0] = CmdPkt_GetSeqNum();
  p[1] = CMD_CLASS_SPEC_IND;
  p += 2;
  pLen += 2;
  //[AddrMode]
  if(pInMsg->msg->groupId != 0x0000)
  {
    *p = CMD_ZCL_MULTCAST;
  }
  else if(pInMsg->msg->wasBroadcast)
  {
    *p = CMD_ZCL_BCST;
  }
  else
  {
    *p = CMD_ZCL_UNICAST;
  }
  p += 1;
  pLen += 1;
  //[NodeID]
  MFG_LOAD_U16(p, pInMsg->msg->srcAddr.addr.shortAddr);
  p += 2;
  pLen += 2;
  //[EP]
  *p++ = pInMsg->msg->srcAddr.endPoint;
  pLen += 1;
  //[profile]
  MFG_LOAD_U16(p, cmdZcl_FindProfIdByEp(pInMsg->msg->endPoint));
  p += 2;
  pLen += 2;
  //[Cluster]
  MFG_LOAD_U16(p, pInMsg->msg->clusterId);
  p += 2;
  pLen += 2;
  //[CMD]
  *p++ = cmdId;
  pLen += 1;
  //[zclSeqNum]
  *p++ = pInMsg->hdr.transSeqNum;
  pLen += 1;
  //[dir]
  *p++ = pInMsg->hdr.fc.direction;
  pLen += 1;
  //[Bcst]
  *p++ = pInMsg->msg->radius;
  pLen += 1;
  //[manu code]
  MFG_LOAD_U16(p, pInMsg->hdr.manuCode);
  p += 2;
  pLen += 2;
  //data
  memcpy(p, pInMsg->pData, pInMsg->pDataLen );
  pLen += pInMsg->pDataLen;
  //Uart Send
  CmdPkt_TxSend(pLen,cmdBuff);
  return ZSuccess;
}

/*******************************************************************************************************************
*                                                UART -> ZCL command
* */


/*
*  @fn      cmdZcl_Read
*
*  @brief   [count][attr ID 0]...[attr ID n]
*
*/
static ZStatus_t cmdZcl_Read(uint8_t srcEP, cmdZclCmdHdr_t *pCmdHdr, uint8_t len, uint8_t *data)
{
  //[attr ID]
  uint8_t status = ZFailure;
  zclReadCmd_t *readCmd = NULL;
  uint8_t count = data[0];
  uint8_t *p = data+1;
  uint8_t total = (len-1)/sizeof(uint16_t);
  uint8_t option = 0;
  if(pCmdHdr->noRsp >= 2)
  {
    option = AF_ACK_REQUEST;
  }
  if(count)
  {
    readCmd = MFG_malloc(sizeof(zclReadCmd_t)+(sizeof(uint16)*count));
    if(readCmd)
    {
      uint8_t n;
      for(n = 0; n<count; n++)
      {
        if(n>total)
        {
          break;
        }
        readCmd->attrID[n] = BUILD_UINT16(p[0],p[1]);
        p += 2;
      }
      readCmd->numAttr = n;
      zcl_SetSendExtParam(cmdZcl_SendCmdCnf,pCmdHdr,option);
      status = zcl_SendRead( srcEP, &pCmdHdr->dest, pCmdHdr->cluster,
                            readCmd, pCmdHdr->direct, pCmdHdr->noRsp,
                            pCmdHdr->manuCode, pCmdHdr->seqNum );
      MFG_free(readCmd);
    }
  }
  return status;
}

/*
*  @fn      cmdZcl_Write
*
*  @brief   [count]{[attr ID][data type][data] 0}...{[attr ID][data type][data] n}
*
*/
static ZStatus_t cmdZcl_Write(uint8_t srcEP, cmdZclCmdHdr_t *pCmdHdr, uint8_t len, uint8_t *data)
{
  //[attr ID][data type][data]
  uint8_t status = ZFailure;
  uint8_t count = data[0];
  zclWriteCmd_t *writeCmd = NULL;
  uint8_t *p = data + 1;
  uint8_t remain = len - 1;
  uint8_t option = 0;
  if(pCmdHdr->noRsp >= 2)
  {
    option = AF_ACK_REQUEST;
  }
  if(count)
  {
    writeCmd = MFG_malloc(sizeof(zclWriteCmd_t)+(sizeof(zclWriteRec_t)*count));
    if(writeCmd)
    {
      uint8_t n;
      for(n=0; n<count; n++)
      {
        if(remain<=3)
        {
          break;
        }
        uint16_t attrID = BUILD_UINT16(p[0],p[1]);
        uint8_t  dataType = p[2];
        uint8_t  *attrData = p+3;
        uint8_t  size = zclGetAttrDataLength(dataType,attrData);
        remain -= 3;
        p += 3;
        if(remain<size)
        {
          break;
        }
        remain -= size;
        p += size;
        writeCmd->attrList[n].attrID = attrID;
        writeCmd->attrList[n].dataType = dataType;
        writeCmd->attrList[n].attrData = attrData;
      }
      writeCmd->numAttr = n;
      zcl_SetSendExtParam(cmdZcl_SendCmdCnf,pCmdHdr,option);
      status = zcl_SendWrite( srcEP, &pCmdHdr->dest, pCmdHdr->cluster,
                             writeCmd, pCmdHdr->direct, pCmdHdr->noRsp,
                             pCmdHdr->manuCode, pCmdHdr->seqNum );
      MFG_free(writeCmd);
    }
  }
  return status;
}

/*
*  @fn      cmdZcl_CfgRept
*
*  @brief   [count]{[attr ID][0][min][max][type][delta] 0}...{n}
*                  {[attr ID][1][timeout] 0}
*
*/
static ZStatus_t cmdZcl_CfgRept(uint8_t srcEP, cmdZclCmdHdr_t *pCmdHdr, uint8_t len, uint8_t *data)
{
  //[attr ID][direction]
  //         [0][min time][max time][data type][delta data]
  //         [1][timeout period]
  uint8_t status = ZFailure;
  uint8_t count = data[0];
  zclCfgReportCmd_t *cfgReportCmd = NULL;
  uint8_t *dataPtr = NULL;
  uint8_t *p = data + 1;
  uint8_t remain = len - 1;
  uint8_t option = 0;
  if(pCmdHdr->noRsp >= 2)
  {
    option = AF_ACK_REQUEST;
  }
  if(count)
  {
    dataPtr = MFG_malloc(sizeof(zclCfgReportCmd_t)+( (sizeof(zclCfgReportRec_t) + MFG_REPORTABLE_CHANGE_SIZE)*count) );
    if(dataPtr)
    {
      uint8_t n;
      cfgReportCmd = (zclCfgReportCmd_t*)dataPtr;
      dataPtr += sizeof(zclCfgReportCmd_t) + sizeof(zclCfgReportRec_t)*count;
      for(n=0; n<count; n++)
      {
        cfgReportCmd->attrList[n].attrID = BUILD_UINT16(p[0],p[1]);
        cfgReportCmd->attrList[n].direction = p[2];
        if(cfgReportCmd->attrList[n].direction == ZCL_SEND_ATTR_REPORTS)
        {
          cfgReportCmd->attrList[n].minReportInt = BUILD_UINT16(p[3],p[4]);
          cfgReportCmd->attrList[n].maxReportInt = BUILD_UINT16(p[5],p[6]);
          cfgReportCmd->attrList[n].dataType = p[7];
          cfgReportCmd->attrList[n].timeoutPeriod = 0;
          p += 8;
          if(remain < 8)
          {
            break;
          }
          remain -= 8;
          memcpy( dataPtr, p, MFG_REPORTABLE_CHANGE_SIZE );
          cfgReportCmd->attrList[n].reportableChange = dataPtr;
          dataPtr += MFG_REPORTABLE_CHANGE_SIZE;
          p += MFG_REPORTABLE_CHANGE_SIZE;
          if(remain < MFG_REPORTABLE_CHANGE_SIZE)
          {
            break;
          }
          remain -= MFG_REPORTABLE_CHANGE_SIZE;
        }
        else
        {
          cfgReportCmd->attrList[n].minReportInt = 0xFFFF;
          cfgReportCmd->attrList[n].maxReportInt = 0xFFFF;
          cfgReportCmd->attrList[n].dataType = 0;
          cfgReportCmd->attrList[n].reportableChange = NULL;
          cfgReportCmd->attrList[n].timeoutPeriod = BUILD_UINT16(p[3],p[4]);
          p += 5;
          if(remain < 5)
          {
            break;
          }
          remain -= 5;
        }
      }
      cfgReportCmd->numAttr = n;
      zcl_SetSendExtParam(cmdZcl_SendCmdCnf,pCmdHdr,option);
      status = zcl_SendConfigReportCmd( srcEP, &pCmdHdr->dest, pCmdHdr->cluster,
                                       cfgReportCmd, pCmdHdr->direct, pCmdHdr->noRsp,
                                       pCmdHdr->manuCode, pCmdHdr->seqNum );
      MFG_free(cfgReportCmd);
    }
  }
  return status;
}

/*
*  @fn      cmdZcl_ReadRept
*
*  @brief   [count]{[attr ID][0] 0}...{n}
*
*/
static ZStatus_t cmdZcl_ReadRept(uint8_t srcEP, cmdZclCmdHdr_t *pCmdHdr, uint8_t len, uint8_t *data)
{
  //[attr ID][direction]
  uint8_t status = ZFailure;
  uint8_t count = data[0];
  zclReadReportCfgCmd_t* readReportCfgCmd = NULL;
  uint8_t *p = data + 1;
  uint8_t remain = len - 1;
  uint8_t option = 0;
  if(pCmdHdr->noRsp >= 2)
  {
    option = AF_ACK_REQUEST;
  }
  if(count)
  {
    readReportCfgCmd = MFG_malloc(sizeof(zclReadReportCfgCmd_t)+(sizeof(zclReadReportCfgRec_t)*count));
    if(readReportCfgCmd)
    {
      uint8_t n;
      for(n=0; n<count; n++)
      {
        if(remain < 3)
        {
          break;
        }
        readReportCfgCmd->attrList[n].attrID = BUILD_UINT16(p[0],p[1]);
        readReportCfgCmd->attrList[n].direction = p[2];
        remain -= 3;
        p += 3;
      }
      readReportCfgCmd->numAttr = n;
      zcl_SetSendExtParam(cmdZcl_SendCmdCnf,pCmdHdr,option);
      status = zcl_SendReadReportCfgCmd( srcEP, &pCmdHdr->dest, pCmdHdr->cluster,
                                        readReportCfgCmd, pCmdHdr->direct, pCmdHdr->noRsp,
                                        pCmdHdr->manuCode, pCmdHdr->seqNum );
      MFG_free(readReportCfgCmd);
    }
  }
  return status;
}

/*
*  @fn      cmdZcl_DiscAttr
*
*  @brief   [attr count][start attr]
*
*/
static ZStatus_t cmdZcl_DiscAttr(uint8_t srcEP, cmdZclCmdHdr_t *pCmdHdr, uint8_t len, uint8_t *data, bool ext)
{
  zclDiscoverAttrsCmd_t discAttr = {0};
  uint8_t option = 0;
  if(pCmdHdr->noRsp >= 2)
  {
    option = AF_ACK_REQUEST;
  }
  if(len >= 3)
  {
    discAttr.maxAttrIDs = data[0];
    discAttr.startAttr = BUILD_UINT16(data[1], data[2]);
    zcl_SetSendExtParam(cmdZcl_SendCmdCnf,pCmdHdr,option);
    if(ext)
    {
      return zcl_SendDiscoverAttrsExt( srcEP, &pCmdHdr->dest, pCmdHdr->cluster,
                                      &discAttr, pCmdHdr->direct, pCmdHdr->noRsp,
                                      pCmdHdr->manuCode, pCmdHdr->seqNum );
    }
    else
    {
      return zcl_SendDiscoverAttrsCmd( srcEP, &pCmdHdr->dest, pCmdHdr->cluster,
                                      &discAttr, pCmdHdr->direct, pCmdHdr->noRsp,
                                      pCmdHdr->manuCode, pCmdHdr->seqNum );
    }
  }
  return ZFailure;
}

/*
*  @fn      cmdZcl_DiscCmd
*
*  @brief   [attr count][start cmd Id]
*
*/
static ZStatus_t cmdZcl_DiscCmd(uint8_t srcEP, cmdZclCmdHdr_t *pCmdHdr, uint8_t len, uint8_t *data, uint8_t cmd)
{
  zclDiscoverCmdsCmd_t discCmd = {0};
  uint8_t option = 0;
  if(pCmdHdr->noRsp >= 2)
  {
    option = AF_ACK_REQUEST;
  }
  if(len >= 2)
  {
    discCmd.maxCmdID = data[0];
    discCmd.startCmdID = data[1];
    zcl_SetSendExtParam(cmdZcl_SendCmdCnf,pCmdHdr,option);
    return zcl_SendDiscoverCmdsCmd( srcEP, &pCmdHdr->dest, pCmdHdr->cluster, cmd, &discCmd,
                                   pCmdHdr->direct, pCmdHdr->noRsp,  pCmdHdr->manuCode, pCmdHdr->seqNum );
  }
  return ZFailure;
}

/*******************************************************************************************************************
*                                              ZCL command -> UART
* */

/*
*  @fn      CmdZcl_ReadRspInd
*
*  @brief   [count]{[attr ID][status][data type][data] 0}...{n}
*
*/
static uint8_t CmdZcl_ReadRspInd(uint8_t *buff, zclReadRspCmd_t *readRspCmd)
{
  //zcl -> uart
  //[attr ID][status][data type][data]
  buff[0] = readRspCmd->numAttr;
  if(readRspCmd->numAttr > 0)
  {
    uint8_t pLen = 1;
    uint8_t *p = buff + 1;
    uint8_t n;
    for(n = 0; n < readRspCmd->numAttr; n++)
    {
      MFG_LOAD_U16(p, readRspCmd->attrList[n].attrID);
      p += 2;
      *p++ = readRspCmd->attrList[n].status;
      pLen += 3;
      if(readRspCmd->attrList[n].status == ZCL_STATUS_SUCCESS)
      {
        uint16_t size = zclGetAttrDataLength(readRspCmd->attrList[n].dataType,
                                             readRspCmd->attrList[n].data);
        *p++ = readRspCmd->attrList[n].dataType;
        memcpy(p, readRspCmd->attrList[n].data, size);
        p += size;
        pLen += size + 1;
      }
    }
    return pLen;
  }
  return 1;
}

/*
*  @fn      CmdZcl_WriteRspInd
*
*  @brief   [count]{[attr ID][status] 0}...{n}
*
*/
static uint8_t CmdZcl_WriteRspInd(uint8_t *buff, zclWriteRspCmd_t *writeRspCmd)
{
  //[attr ID][status]
  buff[0] = writeRspCmd->numAttr;
  if(writeRspCmd->numAttr > 0)
  {
    uint8_t pLen = 1;
    uint8_t *p = buff + 1;
    uint8_t n;
    for(n = 0; n < writeRspCmd->numAttr; n++)
    {
      MFG_LOAD_U16(p, writeRspCmd->attrList[n].attrID);
      p += 2;
      *p++ = writeRspCmd->attrList[n].status;
      pLen += 3;
    }
    return pLen;
  }
  return 1;
}

/*
*  @fn      CmdZcl_CfgReptRspInd
*
*  @brief   [count]{[attr ID][status][direction] 0}...{n}
*/
static uint8_t CmdZcl_CfgReptRspInd(uint8_t *buff, zclCfgReportRspCmd_t *cfgReportRspCmd)
{
  //[attr ID][status][direction]
  buff[0] = cfgReportRspCmd->numAttr;
  if(cfgReportRspCmd->numAttr)
  {
    uint8_t pLen = 1;
    uint8_t *p = buff + 1;
    uint8_t n;
    for(n = 0; n < cfgReportRspCmd->numAttr; n++)
    {
      MFG_LOAD_U16(p, cfgReportRspCmd->attrList[n].attrID);
      p += 2;
      *p ++ = cfgReportRspCmd->attrList[n].status;
      *p ++ = cfgReportRspCmd->attrList[n].direction;
      pLen += 4;
    }
    return pLen;
  }
  return 1;
}

/*
*  @fn      CmdZcl_ReadReptRspInd
*
*  @brief   [count]{[attr ID][status][0][min][max][data type][delta data] 0}...{n}
*                  {[attr ID][status][1][timeout] 0}
*
*/
static uint8_t CmdZcl_ReadReptRspInd(uint8_t *buff, zclReadReportCfgRspCmd_t *readReportCfgRspCmd)
{
  //[attr ID][status][direction]
  //                 [0][min time][max time][data type][delta data]
  //                 [1][timeout period]
  buff[0] = readReportCfgRspCmd->numAttr;
  if(readReportCfgRspCmd->numAttr)
  {
    uint8_t pLen = 1;
    uint8_t *p = buff + 1;
    uint8_t n;
    for(n = 0; n < readReportCfgRspCmd->numAttr; n++)
    {
      MFG_LOAD_U16(p, readReportCfgRspCmd->attrList[n].attrID);
      p += 2;
      *p++ = readReportCfgRspCmd->attrList[n].status;
      *p++ = readReportCfgRspCmd->attrList[n].direction;
      pLen += 4;
      if(readReportCfgRspCmd->attrList[n].status == ZCL_STATUS_SUCCESS)
      {
        if(readReportCfgRspCmd->attrList[n].direction == ZCL_SEND_ATTR_REPORTS)
        {
          uint8_t size = 0;
          MFG_LOAD_U16(p, readReportCfgRspCmd->attrList[n].minReportInt);
          p += 2;
          MFG_LOAD_U16(p, readReportCfgRspCmd->attrList[n].maxReportInt);
          p += 2;
          *p++ = readReportCfgRspCmd->attrList[n].dataType;
          pLen += 5;
          //set reportable change
          if ( zclAnalogDataType( readReportCfgRspCmd->attrList[n].dataType ) )
          {
            size = zclGetDataTypeLength( readReportCfgRspCmd->attrList[n].dataType );
          }
          memset( p, 0, MFG_REPORTABLE_CHANGE_SIZE );
          if(size > MFG_REPORTABLE_CHANGE_SIZE)
          {
            size = MFG_REPORTABLE_CHANGE_SIZE;
          }
          memcpy( p, readReportCfgRspCmd->attrList[n].reportableChange, size);
          p += MFG_REPORTABLE_CHANGE_SIZE;
          pLen += MFG_REPORTABLE_CHANGE_SIZE;
        }
        else
        {
          MFG_LOAD_U16(p, readReportCfgRspCmd->attrList[n].timeoutPeriod);
          p += 2;
          pLen += 2;
        }
      }
    }
    return pLen;
  }
  return 1;
}

/*
*  @fn      CmdZcl_ReportInd
*
*  @brief   [count]{[attr ID][data type][data] 0}...{n}
*
*/
static uint8_t CmdZcl_ReportInd(uint8_t *buff, zclReportCmd_t *reportCmd)
{
  //[attr ID][data type][data]
  buff[0] = reportCmd->numAttr;
  if(reportCmd->numAttr)
  {
    uint8_t pLen = 1;
    uint8_t *p = buff + 1;
    uint8_t n;
    for(n = 0; n < reportCmd->numAttr; n++)
    {
      uint16_t size = 0;
      MFG_LOAD_U16(p, reportCmd->attrList[n].attrID);
      p += 2;
      *p ++ = reportCmd->attrList[n].dataType;
      pLen += 3;
      size = zclGetAttrDataLength(reportCmd->attrList[n].dataType,
                                  reportCmd->attrList[n].attrData);
      memcpy(p,reportCmd->attrList[n].attrData,size);
      p += size;
      pLen += size;
    }
    return pLen;
  }
  return 1;
}

/*
*  @fn      CmdZcl_DefaultRspInd
*
*  @brief   [cmd ID][status]
*
*/
static uint8_t CmdZcl_DefaultRspInd(uint8_t *buff, zclDefaultRspCmd_t *defaultRspCmd)
{
  //[cmd ID][status]
  buff[0] = defaultRspCmd->commandID;
  buff[1] = defaultRspCmd->statusCode;
  return 2;
}

/*
*  @fn      CmdZcl_DiscAttrRspInd
*
*  @brief   [complete][count]{[attrID][data type]...}
*
*/
static uint8_t CmdZcl_DiscAttrRspInd(uint8_t *buff, zclDiscoverAttrsRspCmd_t *discoverRspCmd)
{
  buff[0] = discoverRspCmd->discComplete;
  buff[1] = discoverRspCmd->numAttr;
  if( discoverRspCmd->numAttr )
  {
    uint8_t *p = buff + 2;
    uint8_t pLen = 2;
    uint8_t n;
    for( n = 0; n < discoverRspCmd->numAttr; n++ )
    {
      p[0] = LO_UINT16(discoverRspCmd->attrList[n].attrID);
      p[1] = HI_UINT16(discoverRspCmd->attrList[n].attrID);
      p[2] = discoverRspCmd->attrList[n].dataType;
      p += 3;
      pLen += 3;
    }
    return pLen;
  }
  return 2;
}

/*
*  @fn      CmdZcl_DiscAttrExtRspInd
*
*  @brief   [complete][count]{[attrID][data type][access control]...}
*
*/
static uint8_t CmdZcl_DiscAttrExtRspInd(uint8_t *buff, zclDiscoverAttrsExtRsp_t *discoverRspCmd)
{
  buff[0] = discoverRspCmd->discComplete;
  buff[1] = discoverRspCmd->numAttr;
  if( discoverRspCmd->numAttr )
  {
    uint8_t *p = buff + 2;
    uint8_t pLen = 2;
    uint8_t n;
    for( n = 0; n < discoverRspCmd->numAttr; n++ )
    {
      p[0] = LO_UINT16(discoverRspCmd->aExtAttrInfo[n].attrID);
      p[1] = HI_UINT16(discoverRspCmd->aExtAttrInfo[n].attrID);
      p[2] = discoverRspCmd->aExtAttrInfo[n].attrDataType;
      p[3] = discoverRspCmd->aExtAttrInfo[n].attrAccessControl;
      p += 4;
      pLen += 4;
    }
    return pLen;
  }
  return 2;
}

/*
*  @fn      CmdZcl_DiscCmdRspInd
*
*  @brief   [complete][count]{[cmdId]...}
*
*/
static uint8_t CmdZcl_DiscCmdRspInd(uint8_t *buff, zclDiscoverCmdsCmdRsp_t *discoverRspCmd)
{
  buff[0] = discoverRspCmd->discComplete;
  buff[1] = discoverRspCmd->numCmd;
  if(discoverRspCmd->numCmd)
  {
    uint8_t n;
    uint8_t *p = buff +2;
    uint8_t pLen = 2;
    for(n=0; n<discoverRspCmd->numCmd; n++)
    {
      *p++ = discoverRspCmd->pCmdID[n];
      pLen++;
    }
    return pLen;
  }
  return 2;
}
