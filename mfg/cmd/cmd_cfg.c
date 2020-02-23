#include "ti_drivers_config.h"
#include <ti/sysbios/knl/Semaphore.h>
#include <xdc/std.h>
#include <ti/sysbios/knl/Semaphore.h>
#include "hal_types.h"
#include "util_timer.h"
#include "zstack.h"
#include "zstackapi.h"

#include "util/mfg_Stack.h"

#include "zcl_ha.h"
#include "cmd.h"
#include "cmd_cfg.h"
#include "cmd_pkt.h"
#include "zglobals.h"
#include "zcomdef.h"
#include "osal_nv.h"
#include "addr_mgr.h"
#include "zd_sec_mgr.h"
#include "util/mfg_coord.h"

/* extern variable */
extern uint8_t cmdPktTaskId;
extern uint32_t Generic_UtcTime;
extern const uint8_t Generic_AppVersion;

extern uint8_t cmdZcl_FindEpByProfId(uint16_t profId);
//
bool _scanning = false;
bool _network = false;

void cmdCfg_DeviceType(uint8_t *cmd, uint8 len);
uint8_t  cmdCfg_Status(uint8_t *buf, uint8_t *len);
uint8_t  cmdCfg_Channel(bool set, uint8_t ch, uint32_t *chList);
uint8_t  cmdCfg_Start(void);
uint8_t  cmdCfg_Scan(uint32_t channel, uint8_t duration);
uint8_t  cmdCfg_Reset(uint16_t panid, uint8_t channel);
uint8_t  cmdCfg_Close(void);
uint8_t  cmdCfg_UtcTime(uint32_t UTCTime);
uint8_t  cmdCfg_AddrTable(uint16_t index, uint8_t *buf);
uint8_t  cmdCfg_VerifiedKeyNv(uint16_t idx, uint8_t* len, uint8_t *buf);
uint8_t  cmdCfg_DeleteInstallCode(uint8_t *extAddr);
uint8_t  cmdCfg_DeleteExtAddr(uint8_t *extAddr, uint8_t user);

void cmdCfg_CmdInput(uint8_t cmdSeq, uint8_t cmdClass, uint8_t cmdLen, uint8_t* cmdData)
{
  uint8_t cmdId = cmdData[0];
  uint8_t *p = NULL;
  uint8_t pLen = cmdLen - 1;
  if(pLen)
  {
    p = cmdData + 1;
  }
  uint8_t ackLen = 0;
  uint8_t ackBuf[128];
  uint8_t *pAck = ackBuf + 1;
  uint8_t status = ZFailure;
  switch(cmdId)
  {
  case CFG_CMD_STATUS:
    {
      status = cmdCfg_Status(ackBuf + 1, &ackLen);
    }
    break;
    
  case CFG_CMD_CHANNEL:
    {
      bool set = p[0];
      uint32_t ch;
      status = cmdCfg_Channel(set, p[1], &ch);
      if(status == ZSuccess)
      {
        pAck[0] = BREAK_UINT32(ch, 0);
        pAck[1] = BREAK_UINT32(ch, 1);
        pAck[2] = BREAK_UINT32(ch, 2);
        pAck[3] = BREAK_UINT32(ch, 3);
        ackLen += 4;
      }
    }
    break;
    
  case CFG_CMD_START:
    {
      if(_network)
      {
        status = cmdCfg_Start();
        if( (pLen >= 1) && ( p[0] == 1 ) )
        {
          MFG_WhiteListSet( TRUE );
        }
        else
        {
          MFG_WhiteListSet( FALSE );
        }
      }
    }
    break;
    
  case CFG_CMD_SCAN:
    {
      uint32_t channel = BUILD_UINT32(p[0], p[1], p[2], p[3]);
      uint8_t duration = p[4];
      status = cmdCfg_Scan(channel, duration);
    }
    break;
    
  case CFG_CMD_WLIST:
    {
      if(pLen>=8)
      {
        uint8_t extAddr[8];
        memcpy( extAddr, p, 8 );
        if(pLen>=(8+18))
        {
          APSME_TCLinkKeyNVEntry_t TCLKDevEntry;
          uint8_t found;
          MFG_CSState key;
          key = MFG_enterCS();
          uint16_t keyNvIndex = APSME_SearchTCLinkKeyEntry(extAddr, &found, &TCLKDevEntry);
          MFG_leaveCS(key);
          if((!found) || (TCLKDevEntry.keyAttributes != ZG_VERIFIED_KEY))
          {
            zstack_bdbAddInstallCodeReq_t req;
            memcpy( req.pExt, extAddr, 8 );
            memcpy( req.pInstallCode, p+8, 18 );
            status = Zstackapi_bdbAddInstallCodeReq(cmdPktTaskId,&req);
          }
        }
        else
        {
          MFG_WhiteListAdd( extAddr );
        }
      }
      //
    }
    break;
    
  case CFG_CMD_RESET:
    {
      uint16_t panid = BUILD_UINT16(p[0],p[1]);
      uint8_t channel = p[2];
      status = cmdCfg_Reset(panid,channel);
    }
    break;
    
  case CFG_CMD_CLOSE:
    status = cmdCfg_Close();
    break;
    
  case CFG_CMD_UTC_SET:
    {
      uint32_t time = BUILD_UINT32(p[0], p[1], p[2], p[3]);
      status = cmdCfg_UtcTime( time );
    }
    break;
    
  case CFG_CMD_UTC_READ:
    {
      uint32_t time = Generic_UtcTime;
      pAck[0] = BREAK_UINT32(time, 0);
      pAck[1] = BREAK_UINT32(time, 1);
      pAck[2] = BREAK_UINT32(time, 2);
      pAck[3] = BREAK_UINT32(time, 3);
      ackLen += 4;
      status = ZSuccess;
    }
    break;
    
  case CFG_CMD_ADDR_TABLE:
    {
      uint16_t idx = BUILD_UINT16(p[0],p[1]);
      status = cmdCfg_AddrTable( idx, pAck );
      ackLen += 12;
    }
    break;
    
  case CFG_CMD_KEY_TABLE:
    {
      uint16_t idx = BUILD_UINT16(p[0],p[1]);
      uint8_t len;
      status = cmdCfg_VerifiedKeyNv( idx, &len, pAck );
      if(status == ZSuccess)
      {
        ackLen += len;
      }
    }
    break;
    
  case CFG_CMD_ADD_GROUP:
    {
      zstack_apsAddGroup_t group;
      group.pName = NULL;
      group.n_name = 0;
      group.groupID = BUILD_UINT16(p[0],p[1]);
      group.endpoint = cmdZcl_FindEpByProfId(ZCL_HA_PROFILE_ID);
      status = (uint8_t)Zstackapi_ApsAddGroupReq(cmdPktTaskId, &group);
    }
    break;
    
  case CFG_CMD_DEL_GROUP:
    {
      zstack_apsRemoveGroup_t group;
      group.groupID = BUILD_UINT16(p[0],p[1]);
      group.endpoint = cmdZcl_FindEpByProfId(ZCL_HA_PROFILE_ID);
      status = (uint8_t)Zstackapi_ApsRemoveGroupReq(cmdPktTaskId, &group);
    }
    break;
    
  case CFG_CMD_CUR_GROUP:
    {
      zstack_apsFindAllGroupsReq_t req;
      zstack_apsFindAllGroupsRsp_t rsp = {0};
      req.endpoint = cmdZcl_FindEpByProfId(ZCL_HA_PROFILE_ID);
      status = (uint8_t)Zstackapi_ApsFindAllGroupsReq(cmdPktTaskId,&req,&rsp);
      if(status == 0x00)
      {
        ackLen = 1;
        pAck[0] = rsp.numGroups;
        if(rsp.numGroups)
        {
          uint8_t x;
          uint8_t *pGroup = pAck+1;
          for(x = 0; x < rsp.numGroups; x++)
          {
            pGroup[0] = LO_UINT16(rsp.pGroupList[x]);
            pGroup[1] = HI_UINT16(rsp.pGroupList[x]);
            pGroup += 2;
            ackLen += 2;
          }
          if(rsp.pGroupList)
          {
            OsalPort_free(rsp.pGroupList);
          }
        }
      }
      break;
    }
    
  case CFG_CMD_DEL_ISTCODE:
    {
      status = cmdCfg_DeleteInstallCode(p);
      break;
    }
    
  case CFG_CMD_DEL_ADDR:
    {
      status = cmdCfg_DeleteExtAddr(p, p[8]);
      break;
    }
  
  case CFG_CMD_FLEX_EP:
    {
      if(pLen>=3)
      {
        uint8_t ep = p[0];
        uint16_t profile = BUILD_UINT16(p[1],p[2]);
        if( MFG_AddFlexEp(ep, profile) )
        {
          status = ZSuccess;
        }
      }
      break;
    }
    
  case CFG_CMD_RECOVER:
    {
      // Start BDB commissioning initialization
      zstack_bdbStartCommissioningReq_t zstack_bdbStartCommissioningReq;
      zstack_bdbStartCommissioningReq.commissioning_mode = BDB_COMMISSIONING_MODE_NWK_FORMATION; //BDB_COMMISSIONING_MODE_IDDLE
      Zstackapi_bdbStartCommissioningReq(cmdPktTaskId, &zstack_bdbStartCommissioningReq);
      status = ZSuccess;
      _network = true;
      break;
    }
    
  case CFG_CMD_NV_READ:
    {
      uint8_t dataLen = 0;
      uint8_t id = p[0];
      uint16_t subId = BUILD_UINT16(p[1],p[2]);
      MFG_CSState key;
      key = MFG_enterCS();
      dataLen = osal_nv_item_len_ex( id, subId );
      if(dataLen)
      {
        status = osal_nv_read_ex( id, subId,0, dataLen, ackBuf+1 );
        ackLen = dataLen;
      }
      MFG_leaveCS(key);
      break;
    }
    
  case CFG_CMD_EZ_MODE:
    {
      if( pLen >= 10 )
      {
        uint16_t nodeId = BUILD_UINT16(p[0],p[1]);
        uint8_t mac[8];
        memcpy( mac, p+2, 8 );
        if( MFG_EzModeStart( nodeId, mac ) )
        {
          status = ZSuccess;
        }
      }
      break;
    }
    
  default:
    break;
  }
  ackLen += 1;
  ackBuf[0] = status;
  CmdPkt_AckSend(cmdSeq,cmdClass,ackLen,ackBuf);
}

void cmdCfg_CmdOutput(uint8_t cmdId, uint8_t len, uint8_t* data)
{
  uint8_t buf[128];
  buf[0] = CmdPkt_GetSeqNum();
  buf[1] = CMD_CLASS_STATUS;
  buf[2] = cmdId;
  
  memcpy( buf+3, data, len );
  CmdPkt_TxSend( len+3, buf );
}


/*
  Command print
*/
void cmdCfg_Boot(uint8_t boot, uint8_t version)
{
  uint8_t buf[2];
  buf[0] = boot;
  buf[1] = version;
  cmdCfg_CmdOutput(CFG_CMD_BOOT, 2, buf );
}

void cmdCfg_CoordStartNotify(uint8_t sta, uint8_t channel, uint16_t panId, uint8_t *mac)
{
  uint32_t cs;
  nwkKeyDesc tmpKey;
  uint8_t buf[12 + SEC_KEY_LEN + 1 + 1];
  uint8_t len = 12;
  buf[0] = sta;
  buf[1] = channel;
  buf[2] = LO_UINT16(panId);
  buf[3] = HI_UINT16(panId);
  memcpy(&buf[4], mac, 8);
  cs = MFG_enterCS();
  if ( NLME_ReadNwkKeyInfo( 0, sizeof(tmpKey), &tmpKey,
                           ZCD_NV_NWK_ACTIVE_KEY_INFO ) == SUCCESS )
  {
    len += SEC_KEY_LEN + 1;
    memcpy(&buf[12], tmpKey.key, SEC_KEY_LEN);
    buf[12 + SEC_KEY_LEN] = tmpKey.keySeqNum;
  }
  MFG_leaveCS(cs);
  len += 1;
  buf[12 + SEC_KEY_LEN + 1] = Generic_AppVersion;
  cmdCfg_CmdOutput(CFG_CMD_NWK_START, len, buf);
}

void cmdCfg_PermitJoinNotify(uint8_t time)
{
  cmdCfg_CmdOutput(CFG_CMD_PERMIT, 1, &time);
  if( (time == 0) && MFG_WhiteListEnable() )
  {
    MFG_WhiteListSet( FALSE );
  }
}

void cmdCfg_BeaconIndNotify(uint16_t nodeId, uint16_t panId, uint8_t channel, uint8_t lqi, uint8_t* extPanid)
{
  uint8_t buf[14];
  buf[0] = channel;
  buf[1] = LO_UINT16(nodeId);
  buf[2] = HI_UINT16(nodeId);  
  buf[3] = LO_UINT16(panId);
  buf[4] = HI_UINT16(panId);
  memcpy( buf+5, extPanid, 8 );
  buf[13] = lqi;
  
  cmdCfg_CmdOutput(CFG_CMD_BEACON, 14, buf);
}

void cmdCfg_ScanEnd(uint8_t status)
{
  uint8_t buf[2];
  buf[0] = 0x00;
  buf[1] = status;
  cmdCfg_CmdOutput(CFG_CMD_BEACON, 2, buf);
  _scanning = false;
}

void cmdCfg_NodeJoinNotify(uint8_t* mac, uint16_t nodeId, uint16_t parent, uint8_t mode, uint8_t type)
{
  uint8_t buf[14];
  memcpy(buf, mac, 8);
  buf[8] = LO_UINT16(nodeId);
  buf[9] = HI_UINT16(nodeId);
  buf[10] = LO_UINT16(parent);
  buf[11] = HI_UINT16(parent);
  buf[12] = mode;
  buf[13] = type;
  cmdCfg_CmdOutput(CFG_CMD_NODE_JOIN, 14, buf);
}

void cmdCfg_DevJoinNotify(mfgDevSn_t sn, mfgDevAddr_t dev, uint16_t profile, uint16_t devId,
                          uint8_t inNum, uint16_t *inCluster, uint8_t outNum, uint16_t *outCluster)
{
  uint8_t buf[80];
  uint8_t len = 0;
  uint8_t *p = buf;
  uint8_t n;

  memcpy(p, sn, sizeof(mfgDevSn_t));
  p += sizeof(mfgDevSn_t);
  len += sizeof(mfgDevSn_t);

  memcpy(p, dev, sizeof(mfgDevAddr_t));
  p += sizeof(mfgDevAddr_t);
  len += sizeof(mfgDevAddr_t);

  *p++ = LO_UINT16(profile);
  *p++ = HI_UINT16(profile);
  len += 2;

  *p++ = LO_UINT16(devId);
  *p++ = HI_UINT16(devId);
  len += 2;

  *p++ = inNum;
  len+=1;
  for(n=0;n<inNum;n++)
  {
    *p++ = LO_UINT16(inCluster[n]);
    *p++ = HI_UINT16(inCluster[n]);
    len += 2;
  }

  *p++ = outNum;
  len+=1;
  for(n=0;n<outNum;n++)
  {
    *p++ = LO_UINT16(outCluster[n]);
    *p++ = HI_UINT16(outCluster[n]);
    len += 2;
  }

  cmdCfg_CmdOutput(CFG_CMD_DEVICE_JOIN, len, buf);
}

void cmdCfg_LeaveNotify(uint16_t nodeId, uint8_t *mac, uint16_t parent)
{
  uint8_t buf[12];
  buf[0] = LO_UINT16(nodeId);
  buf[1] = HI_UINT16(nodeId);
  memcpy(buf+2,mac,8);
  buf[10] = LO_UINT16(parent);
  buf[11] = HI_UINT16(parent);
  cmdCfg_CmdOutput(CFG_CDM_DELETE, 12, buf);
}

void cmdCfg_LeaveInd(zstack_zdoLeaveInd_t req)
{
  uint8_t buf[13];
  buf[0] = LO_UINT16(req.srcAddr);
  buf[1] = HI_UINT16(req.srcAddr);
  memcpy(buf+2,req.extendedAddr,8);
  buf[10] = req.request;
  buf[11] = req.removeChildren;
  buf[12] = req.rejoin;
  cmdCfg_CmdOutput(CFG_CMD_LEAVE_IND, 13, buf);
}

void cmdCfg_DeviceErro(uint16_t nodeId, uint8_t ep, uint8_t status)
{
  uint8_t buf[4];
  buf[0] = LO_UINT16(nodeId);
  buf[1] = HI_UINT16(nodeId);
  buf[2] = ep;
  buf[3] = status;
  cmdCfg_CmdOutput(CFG_CMD_DEVICE_ERRO, 4, buf);
}

void cmdCfg_JoinBind( uint8_t *sn, uint16_t cluster, uint8_t status )
{
  uint8_t buf[12];
  memcpy( buf, sn, 9 );
  buf[9] = LO_UINT16(cluster);
  buf[10] = HI_UINT16(cluster);
  buf[11] = status;
  cmdCfg_CmdOutput(CFG_CMD_JOIN_BIND, 12, buf);
}

void cmdCfg_KeyVerify( uint8_t status, uint8_t* extAddr, uint16_t nwkAddr )
{
  uint8_t buf[19];
  uint8_t len = 9;
  buf[0] = status;
  memcpy( &buf[1], extAddr, 8 );
  if( status != BDB_TC_LK_EXCH_PROCESS_JOINING )
  {
    APSME_TCLinkKeyNVEntry_t TCLKDevEntry;
    uint8_t found;
    
    MFG_CSState key;
    key = MFG_enterCS();
    uint16_t keyNvIndex = APSME_SearchTCLinkKeyEntry(extAddr,&found, &TCLKDevEntry);
    MFG_leaveCS(key);
    if( found )
    {
      uint8_t *p = buf + 9;
      keyNvIndex -= ZCD_NV_LEGACY_TCLK_TABLE_START;
      p[0] = LO_UINT16(keyNvIndex);
      p[1] = HI_UINT16(keyNvIndex);
      p[2] = TCLKDevEntry.keyAttributes;
      p[3] = TCLKDevEntry.keyType;
      p[4] = TCLKDevEntry.SeedShift_IcIndex;
      len += 5;
    }
  }
  cmdCfg_CmdOutput( CFG_CMD_VERIFY_KEY, len, buf);
}

void cmdCfg_Identify( uint16_t time )
{
  uint8_t buf[2];
  uint8_t len = 2;
  buf[0] = LO_UINT16(time);
  buf[1] = HI_UINT16(time);
  cmdCfg_CmdOutput( CFG_CMD_IDENTIFY, len, buf );
}

void cmdCfg_Denied( uint8_t* mac, uint16_t parent, uint8_t rejoin )
{
  uint8_t buf[11];
  memcpy( buf, mac, 8 );
  buf[8] = LO_UINT8( parent );
  buf[9] = HI_UINT8( parent );
  buf[10] = rejoin;
  cmdCfg_CmdOutput( CFG_CMD_DENIED, 11, buf );
}

/*
 * Command config
 */
void cmdCfg_DeviceType(uint8_t *cmd, uint8 len)
{
  MFG_CSState key;
  key = MFG_enterCS();
  uint8_t devType = DEVICE_LOGICAL_TYPE;
  if(cmd[0] == 0)
  {
    devType = ZG_DEVICETYPE_COORDINATOR;
  }
  else if(cmd[0] == 1)
  {
    devType = ZG_DEVICETYPE_ROUTER;
  }
  else if(cmd[0] == 2)
  {
    devType = ZG_DEVICETYPE_ENDDEVICE;
  }
  zgSetItem(ZCD_NV_LOGICAL_TYPE, sizeof(devType), &devType);
  osal_nv_write(ZCD_NV_LOGICAL_TYPE, sizeof(devType), &devType);
  MFG_leaveCS(key);
}

uint8_t cmdCfg_Status(uint8_t *buf, uint8_t *len)
{
  zstack_bdbGetAttributesRsp_t bdbGetRsp;
  zstack_sysNwkInfoReadRsp_t sysNwkInfoReadRsp;
  
  Zstackapi_bdbGetAttributesReq(cmdPktTaskId, &bdbGetRsp);
  Zstackapi_sysNwkInfoReadReq(cmdPktTaskId,&sysNwkInfoReadRsp);
  
  memcpy( buf, sysNwkInfoReadRsp.ieeeAddr, 8 );
  *len = 8;
  
  if(bdbGetRsp.bdbNodeIsOnANetwork)
  {
    return ZSuccess;
  }
  return ZFailure;
}

uint8_t  cmdCfg_Channel(bool set, uint8_t ch, uint32_t *chList)
{
  zstack_bdbGetAttributesRsp_t bdbGetRsp = {0};
  Zstackapi_bdbGetAttributesReq(cmdPktTaskId, &bdbGetRsp);
  *chList = bdbGetRsp.bdbPrimaryChannelSet;
  if( bdbGetRsp.bdbNodeIsOnANetwork )
  {
    return ZFailure;
  }
  else if((ch >= 11) && (ch <= 26))
  {
    uint32_t chBit = 1<<ch;
    zstack_bdbSetAttributesReq_t bdbSetReq = {0};
    if(set == TRUE)
    {
      *chList |= chBit;
    }
    else
    {
      *chList &= ~chBit;
    }
    bdbSetReq.bdbPrimaryChannelSet = *chList;
    bdbSetReq.has_bdbPrimaryChannelSet  = TRUE;
    Zstackapi_bdbSetAttributesReq(cmdPktTaskId, &bdbSetReq);
  }
  return ZSuccess;
}

uint8_t cmdCfg_Start(void)
{
  zstack_bdbStartCommissioningReq_t startReq = {0};
  startReq.commissioning_mode = BDB_COMMISSIONING_MODE_NWK_FORMATION | BDB_COMMISSIONING_MODE_NWK_STEERING;
  return (uint8_t)Zstackapi_bdbStartCommissioningReq( cmdPktTaskId, &startReq );
}

uint8_t cmdCfg_Scan(uint32_t channel, uint8_t duration)
{
  zstack_devNwkDiscReq_t scanReq = {0};
  uint8_t status;
  if( _scanning )
  {
    return ZFailure;
  }
  if(channel == 0)
  {
    channel = DEFAULT_CHANLIST;
  }
  scanReq.scanChannels = channel & 0x07FFF800;
  scanReq.scanDuration = duration;
  status = (uint8_t)Zstackapi_DevNwkDiscReq( cmdPktTaskId, &scanReq);
  if( status == ZSuccess )
  {
    _scanning = true;
  }
  return status;
}

uint8_t cmdCfg_Reset(uint16_t panid, uint8_t channel)
{
  if((panid == _NIB.nwkPanId) && (channel == _NIB.nwkLogicalChannel))
  {
    //osal_nv_delete( ZCD_NV_EXTENDED_PAN_ID, Z_EXTADDR_LEN );
    //Zstackapi_bdbResetLocalActionReq(cmdPktTaskId);
    
    extern void NVOCMP_NvErase(void);
    MFG_CSState cs;
    cs = MFG_enterCS();
    NVOCMP_NvErase();
    MFG_leaveCS(cs);
    Zstackapi_bdbResetLocalActionReq(cmdPktTaskId);
    return ZSuccess;
  }
  else if(panid == 0xFFFF)
  {
    //zstack_sysResetReq_t req = {zstack_ResetTypes_DEVICE, false};
    //Zstackapi_sysResetReq(cmdPktTaskId, &req);
    MFG_Reset();
    return ZSuccess;
  }
  return ZFailure;
}

uint8_t cmdCfg_Close(void)
{
  zstack_zdoMgmtPermitJoinReq_t req = {0};
  req.tcSignificance = true;
  req.duration = 0;
  req.nwkAddr = 0xFFFC;
  return Zstackapi_ZdoMgmtPermitJoinReq(cmdPktTaskId, &req);
}

uint8_t cmdCfg_UtcTime(uint32_t UTCTime)
{
  Generic_UtcTime = UTCTime;
  return ZSuccess;
}

uint8_t cmdCfg_AddrTable(uint16_t index, uint8_t *buf)
{
  uint8_t stat = ZFailure;
  AddrMgrEntry_t entry = {0};
  entry.user = ADDRMGR_USER_DEFAULT;
  entry.index = index;
  MFG_CSState key;
  
  key = MFG_enterCS();
  if(AddrMgrEntryGet( &entry ))
  {
    stat = ZSuccess;
  }
  MFG_leaveCS(key);
  
  buf[0] = LO_UINT16(index);
  buf[1] = HI_UINT16(index);
  buf[2] = LO_UINT16(entry.nwkAddr);
  buf[3] = HI_UINT16(entry.nwkAddr);
  memcpy(&buf[4], entry.extAddr, 8 );
  return stat;
}

uint8_t  cmdCfg_VerifiedKeyNv(uint16_t idx, uint8_t* len, uint8_t *buf )
{
  *len = 0;
  if(idx < gZDSECMGR_TC_DEVICE_MAX)
  {
    APSME_TCLinkKeyNVEntry_t tcLinkKey;
    MFG_CSState cs;
    uint8_t *key;
    
    cs = MFG_enterCS();
    osal_nv_read_ex(ZCD_NV_EX_TCLK_TABLE, idx, 0,
                    sizeof(APSME_TCLinkKeyNVEntry_t),
                    &tcLinkKey);
    MFG_leaveCS(cs);
    
    memcpy( buf, tcLinkKey.extAddr, 8 );
    buf[8] = tcLinkKey.keyAttributes;
    buf[9] = tcLinkKey.keyType;
    buf[10] = tcLinkKey.SeedShift_IcIndex;
    *len = 11;
    key = buf + 11;
    if(tcLinkKey.keyAttributes == ZG_PROVISIONAL_KEY)
    {
      ZDSecMgrReadKeyFromNv(ZCD_NV_EX_TCLK_IC_TABLE, tcLinkKey.SeedShift_IcIndex, key);
    }
    else if(tcLinkKey.keyAttributes == ZG_VERIFIED_KEY)
    {
      osal_nv_read(ZCD_NV_TCLK_SEED, 0, SEC_KEY_LEN, key);
    }
    else if((tcLinkKey.keyAttributes == ZG_DEFAULT_KEY) || (tcLinkKey.keyAttributes == ZG_NON_R21_NWK_JOINED))
    {
      ZDSecMgrReadKeyFromNv(ZCD_NV_EX_LEGACY, ZCD_NV_TCLK_DEFAULT, key);
    }
    else
    {
      ZDSecMgrReadKeyFromNv(ZCD_NV_EX_LEGACY, ZCD_NV_TCLK_JOIN_DEV, key);
    }
    *len += SEC_KEY_LEN;
    return ZSuccess;
  }
  return ZFailure;
}

uint8_t  cmdCfg_DeleteInstallCode(uint8_t *extAddr)
{
  APSME_TCLinkKeyNVEntry_t TCLKDevEntry;
  uint8_t found;
  MFG_CSState key;
  key = MFG_enterCS();
  uint16_t keyNvIndex = APSME_SearchTCLinkKeyEntry(extAddr, &found, &TCLKDevEntry);
  MFG_leaveCS(key);
  if(found && (TCLKDevEntry.keyAttributes == ZG_PROVISIONAL_KEY))
  {
    //delete key
    key = MFG_enterCS();
    APSME_EraseICEntry(&TCLKDevEntry.SeedShift_IcIndex);
    MFG_leaveCS(key);
    //delete 
    memset(&TCLKDevEntry,0,sizeof(APSME_TCLinkKeyNVEntry_t));
    TCLKDevEntry.keyAttributes = ZG_DEFAULT_KEY;
    
    key = MFG_enterCS();
    osal_nv_write_ex(ZCD_NV_EX_TCLK_TABLE, keyNvIndex,
                     sizeof(APSME_TCLinkKeyNVEntry_t),
                     &TCLKDevEntry);
    MFG_leaveCS(key);
    
    return ZSuccess;
  }
  return ZFailure;
}

uint8_t  cmdCfg_DeleteExtAddr(uint8_t *extAddr, uint8_t user)
{
  if( MFG_DeleteAddrMgr( extAddr, user ) )
  {
    return ZSuccess;
  }
  return ZFailure;
}
