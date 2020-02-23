#ifndef  _MFG_COORD_H
#define  _MFG_COORD_H

/* INCLUDE */
#include "../mfg.h"

/* MACRO */
#define NODE_NONE       0x00
#define NODE_ROUTER     0x01
#define NODE_ENDDEV     0x02

#define  NODE_ST_TC     0x00
#define  NODE_ST_ANNCE  0x01
#define  NODE_ST_EP     0x02

#define  EP_STA_NEW        0
#define  EP_STA_START      1

/* typdef */

/* function */
extern bool MFG_WhiteListAdd(uint8_t *mac);
extern bool MFG_WhiteListCheck(uint8_t *mac);
extern bool MFG_WhiteListEnable(void);
extern void MFG_WhiteListSet(uint8_t enable);
//stack indication
void MFG_TcJoinInd(zstack_zdoTcDeviceInd_t req);
void MFG_DeviceAnnounceInd(zstack_zdoDeviceAnnounceInd_t req);
void MFG_VerifyLinkKey(bdb_TCLinkKeyExchProcess_t req);
bool MFG_ActiveEpInd(zstack_zdoActiveEndpointsRspInd_t req);
bool MFG_SimpleDescInd(zstack_zdoSimpleDescRspInd_t req);
bool MFG_AddFlexEp(uint8_t ep, uint16_t profile);
bool MFG_EzModeStart(uint16_t nodeId, uint8_t *mac);

#endif