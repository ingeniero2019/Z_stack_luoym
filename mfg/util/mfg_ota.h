/***************************************************************************************************
 * INCLUDES
 ***************************************************************************************************/
#include "zcomdef.h"
#include "aps_mede.h"
#include "af.h"
#include "ota_common.h"

/***************************************************************************************************
 * CONSTANTS
 ***************************************************************************************************/

#define MFG_OTA_FILE_READ_REQ_LEN                          26
#define MFG_OTA_FILE_READ_RSP_LEN                          26

#define MFG_OTA_GET_IMG_MSG_LEN                            31


/***************************************************************************************************
 * EXTERNAL FUNCTIONS
 ***************************************************************************************************/

/*
 *   Process all the MT OTA commands that are issued by OTA Console
 */
extern uint8 MFG_OtaCommandProcessing(uint8* pBuf);

/*
 *   Send messages to OTA Console from ZCL OTA
 */
extern uint8 MFG_OtaFileReadReq(afAddrType_t *pAddr, zclOTA_FileID_t *pFileId,
                                uint8 len, uint32 offset);

extern uint8 MFG_OtaGetImage(afAddrType_t *pAddr, zclOTA_FileID_t *pFileId, 
                             uint16 hwVer, uint8 *ieee, uint8 options);

extern uint8 MFG_OtaSendStatus(uint16 shortAddr, uint8 type, uint8 status, uint8 optional);

/* 
 * Registration for MT OTA Callback Messages
 */
extern void MFG_OtaRegister(uint8 taskId);

/***************************************************************************************************
***************************************************************************************************/