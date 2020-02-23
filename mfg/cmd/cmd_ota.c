#include "ti_drivers_config.h"
#include <ti/sysbios/knl/Semaphore.h>
#include <xdc/std.h>
#include <ti/sysbios/knl/Semaphore.h>
#include "hal_types.h"
#include "zstack.h"
#include "zstackapi.h"

#include "zglobals.h"
#include "zcomdef.h"
#include "ota_common.h"

#include "cmd.h"
#include "cmd_ota.h"
#include "cmd_pkt.h"
#include "zcl_ota.h"
#include "ota_srv_app.h"

//[NodeID][EP][cmd][seq][param]
typedef struct
{
  uint16_t nodeId;
  uint8_t  ep;
  uint8_t  cmd;
  uint8_t  seqNo;
}cmdOta_CnfParam_t;

/*

*/
static void cmdOta_SendCnf(uint8 status, uint8 endpoint, uint8 transID, uint16_t clusterID, void* cnfParam);//zcl_SetSendExtParam(cmdZcl_SendCmdCnf,pCmdHdr,option);

static uint8_t cmdOta_ImageNotify( afAddrType_t dstAddr, uint8_t seqNo, uint8_t paramLen, uint8_t* paramData );

void cmdOTA_CmdOutput( uint16_t nodeId, uint8_t ep, uint8_t cmd, uint8_t seqNo, uint8_t paramLen, uint8_t* paramData );

void cmdOTA_CmdSendCnf( uint16_t nodeId, uint8_t ep, uint8_t cmd, uint8_t seqNo );

void cmdOta_CmdInput(uint8_t cmdSeq, uint8_t cmdClass, uint8_t cmdLen, uint8_t* cmdData)
{
  afAddrType_t dstAddr;
  uint8_t cmdId;
  uint8_t seqNo;
  uint8_t status;
  dstAddr.addrMode = afAddr16Bit;
  dstAddr.addr.shortAddr = BUILD_UINT16( cmdData[0], cmdData[1] );
  dstAddr.endPoint = cmdData[2];
  cmdId = cmdData[3];
  seqNo = cmdData[4];
  uint8_t paramLen = cmdLen - 5;
  uint8_t *paramData = cmdData + 5;
  if( cmdId == OTA_CMD_NOTIFY )
  {
    cmdOta_ImageNotify( dstAddr, seqNo, paramLen, paramData );
  }
  else
  {
    zclOTA_ServerHandleFileSysCb ( dstAddr, cmdId, seqNo, paramLen, paramData );
  }
  CmdPkt_AckSend(cmdSeq,cmdClass,1,&status);
}

//[NodeID][EP][cmd][seq][param]
void cmdOTA_CmdOutput( uint16_t nodeId, uint8_t ep, uint8_t cmd, uint8_t seqNo, uint8_t paramLen, uint8_t* paramData )
{
  uint8_t buf[64];
  buf[0] = CmdPkt_GetSeqNum();
  buf[1] = CMD_CLASS_OTA_OUTPUT;
  buf[2] = LO_UINT16( nodeId );
  buf[3] = LO_UINT16( nodeId );
  buf[4] = ep;
  buf[5] = cmd;
  buf[6] = seqNo;
  memcpy( buf+7, paramData, paramLen );
  CmdPkt_TxSend( paramLen+7, buf );
}

void cmdOTA_CmdSendCnf( uint16_t nodeId, uint8_t ep, uint8_t cmd, uint8_t seqNo )
{
  uint8_t buf[7];
  buf[0] = CmdPkt_GetSeqNum();
  buf[1] = CMD_CLASS_OTA_INPUT;
  buf[2] = LO_UINT16( nodeId );
  buf[3] = LO_UINT16( nodeId );
  buf[4] = ep;
  buf[5] = cmd;
  buf[6] = seqNo;
  CmdPkt_TxSend( 7, buf );
}

uint8_t cmd_OtaFileReadReq( afAddrType_t *pAddr, uint8_t seqNo, zclOTA_FileID_t *pFileId,
                            uint8_t size, uint32_t offset)
{
  uint8_t buf[8 + 4 + 1];
  uint8_t *p;
  p = OTA_FileIdToStream( pFileId, buf );
  *p++ = BREAK_UINT32(offset, 0);
  *p++ = BREAK_UINT32(offset, 1);
  *p++ = BREAK_UINT32(offset, 2);
  *p++ = BREAK_UINT32(offset, 3);
  *p = size;

  cmdOTA_CmdOutput( pAddr->addr.shortAddr, pAddr->endPoint, OTA_CMD_FILE_READ, seqNo, sizeof(buf), buf );
  return ZSuccess;
}

uint8_t cmd_OtaGetImage( afAddrType_t *pAddr, uint8_t seqNo, zclOTA_FileID_t *pFileId,
                         uint16_t hwVer, uint8_t *ieee, uint8_t options )
{
  uint8_t buf[8 + 1 + 2 + 8];
  uint8_t len = 11;
  uint8_t *p;
  p = OTA_FileIdToStream( pFileId, buf );
  *p++ = options;
  *p++ = LO_UINT16(hwVer);
  *p++ = HI_UINT16(hwVer);
  if (ieee)
  {
    len += 8;
    OsalPort_memcpy(p, ieee, Z_EXTADDR_LEN);
  }

  cmdOTA_CmdOutput( pAddr->addr.shortAddr, pAddr->endPoint, OTA_CMD_NEXT_IMG, seqNo, sizeof(buf), buf );
  return ZSuccess;
}

uint8_t cmd_OtaSendStatus(afAddrType_t *pAddr, uint8_t seqNo, uint8_t type, uint8_t status, uint8_t optional)
{
  uint8_t buf[3];
  buf[0] = type;
  buf[1] = status;
  buf[2] = optional;

  cmdOTA_CmdOutput( pAddr->addr.shortAddr, pAddr->endPoint, OTA_CMD_STATUS, seqNo, sizeof(buf), buf );
  return ZSuccess;
}


//-----------------------------------------------------------------------------

static void cmdOta_SendCnf(uint8 status, uint8 endpoint, uint8 transID, uint16_t clusterID, void* cnfParam)
{
  if( cnfParam )
  {
    cmdOta_CnfParam_t *param = cnfParam;
    cmdOTA_CmdSendCnf( param->nodeId, param->ep, param->cmd, param->seqNo );
    MFG_free( cnfParam );
  }
}

static uint8_t cmdOta_ImageNotify( afAddrType_t dstAddr, uint8_t seqNo, uint8_t param_len, uint8_t* paramData )
{
  uint8_t status = ZFailure;
  zclOTA_ImageNotifyParams_t param;
  if( param_len < 10 )
  {
    return ZFailure;
  }
  param.payloadType = paramData[0];
  param.queryJitter = paramData[1];
  OTA_StreamToFileId( &param.fileId, paramData+2 );
  
  cmdOta_CnfParam_t *cnfParam = MFG_malloc(sizeof(cmdOta_CnfParam_t));
  if( cnfParam )
  {
    cnfParam->nodeId = dstAddr.addr.shortAddr;
    cnfParam->ep = dstAddr.endPoint;
    cnfParam->cmd = OTA_CMD_NOTIFY;
    cnfParam->seqNo = seqNo;
    zcl_SetSendExtParam(cmdOta_SendCnf,cnfParam,0);
    status = zclOTA_SendImageNotify( ZCL_OTA_ENDPOINT, &dstAddr, seqNo, &param );
    if( status != ZSuccess )
    {
      MFG_free( cnfParam );
    }
  }
  return status;
}




