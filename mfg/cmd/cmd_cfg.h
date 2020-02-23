#ifndef  APP_CMD_UTIL_H
#define  APP_CMD_UTIL_H

/* Macro */
//[cfg_cmd_id][cfg_cmd_data]
// Input
#define  CFG_CMD_STATUS       0x00
#define  CFG_CMD_CHANNEL      0x01
#define  CFG_CMD_START        0x02
#define  CFG_CMD_SCAN         0x03
#define  CFG_CMD_WLIST        0x04
#define  CFG_CMD_RESET        0x05
#define  CFG_CMD_CLOSE        0x06
#define  CFG_CMD_UTC_SET      0x07
#define  CFG_CMD_UTC_READ     0x08
#define  CFG_CMD_ADDR_TABLE   0x09
#define  CFG_CMD_KEY_TABLE    0x0A
#define  CFG_CMD_ADD_GROUP    0x0B
#define  CFG_CMD_DEL_GROUP    0x0C
#define  CFG_CMD_CUR_GROUP    0x0D
#define  CFG_CMD_FLEX_EP      0x0E
#define  CFG_CMD_RECOVER      0x0F
#define  CFG_CMD_DEL_ISTCODE  0x10
#define  CFG_CMD_DEL_ADDR     0x11
#define  CFG_CMD_EZ_MODE      0x12

#define  CFG_CMD_NV_READ      0x20

#define  CFG_CMD_BOOT         0x80
#define  CFG_CMD_PERMIT       0x81
#define  CFG_CMD_NWK_START    0x82
#define  CFG_CMD_BEACON       0x83
#define  CFG_CMD_NODE_JOIN    0x84
#define  CFG_CMD_DEVICE_JOIN  0x85
#define  CFG_CDM_DELETE       0x86
#define  CFG_CMD_LEAVE_IND    0x87
#define  CFG_CMD_VERIFY_KEY   0x88 
#define  CFG_CMD_JOIN_BIND    0x89
#define  CFG_CMD_IDENTIFY     0x8A
#define  CFG_CMD_DENIED       0x8B

#define  CFG_CMD_DEVICE_ERRO  0x8F



#define  COORD_STA_IDLE       0x00
#define  COORD_STA_NET        0x01
#define  COORD_STA_OPEN       0x02

#define  JOIN_MODE_TC         0x00
#define  JOIN_MODE_JOIN       0x01
#define  JOIN_MODE_REJOIN     0x02

/* Function */
void cmdCfg_Boot(uint8_t boot, uint8_t version);
void cmdCfg_CoordStartNotify(uint8_t sta, uint8_t channel, uint16_t panId, uint8_t *mac);
void cmdCfg_BeaconIndNotify(uint16_t panId, uint16_t nodeId, uint8_t channel, uint8_t lqi, uint8_t* extPanid);
void cmdCfg_ScanEnd(uint8_t status);

void cmdCfg_NodeJoinNotify(uint8_t* mac, uint16_t nodeId, uint16_t parent, uint8_t mode, uint8_t type);
void cmdCfg_DevJoinNotify(mfgDevSn_t sn, mfgDevAddr_t dev, uint16_t profile, uint16_t devId, 
                         uint8_t inNum, uint16_t *inCluster, uint8_t outNum, uint16_t *outCluster);
void cmdCfg_PermitJoinNotify(uint8_t time);
void cmdCfg_LeaveNotify(uint16_t nodeId, uint8_t *mac, uint16_t parent);
void cmdCfg_LeaveInd(zstack_zdoLeaveInd_t req);
void cmdCfg_KeyVerify( uint8_t status, uint8_t* extAddr, uint16_t nwkAddr );
void cmdCfg_DeviceErro(uint16_t nodeId, uint8_t ep, uint8_t status);
void cmdCfg_JoinBind( uint8_t *sn, uint16_t cluster, uint8_t status );
void cmdCfg_Identify( uint16_t time );
void cmdCfg_Denied( uint8_t* mac, uint16_t parent, uint8_t rejoin );
void cmdCfg_AfIncomingErro( aps_FrameFormat_t *aff, zAddrType_t *SrcAddress, uint16_t SrcPanId,
                           NLDE_Signal_t *sig, uint8_t nwkSeqNum, uint8_t SecurityUse,
                           uint32_t timestamp, uint8_t radius );
#endif
