#ifndef CMD_OTA_H
#define CMD_OTA_H

//zigbee -> uart
#define  OTA_CMD_NOTIFY                     0x00
#define  OTA_CMD_FILE_READ                  0x01
#define  OTA_CMD_NEXT_IMG                   0x02
#define  OTA_CMD_STATUS                     0x03

//
typedef struct
{
  uint8_t len;
  uint8_t cmd;
  uint8_t data[];
} OTA_Msg_t;

typedef struct
{
  uint16_t dest;
  uint8_t ep;
  uint8_t seq;
  uint8_t cmd;
} OTA_CnfParam_t;


extern uint8_t cmd_OtaFileReadReq( afAddrType_t *pAddr, uint8_t seqNo, zclOTA_FileID_t *pFileId,
                                   uint8_t size, uint32_t offset);

extern uint8_t cmd_OtaGetImage( afAddrType_t *pAddr, uint8_t seqNo, zclOTA_FileID_t *pFileId,
                                uint16_t hwVer, uint8_t *ieee, uint8_t options );

extern uint8_t cmd_OtaSendStatus(afAddrType_t *pAddr, uint8_t seqNo, uint8_t type, uint8_t status, uint8_t optional);

#endif
