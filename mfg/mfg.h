#ifndef _MFG_H
#define _MFG_H

#include "osal_port.h"
#include "zcl_port.h"
#include "zstack.h"
#include "util/mfg_zcl.h"

/* MACRO */

#define  MFG_CSState    uint32

#define  MFG_malloc        OsalPort_malloc
#define  MFG_free          OsalPort_free
#define  MFG_enterCS       OsalPort_enterCS
#define  MFG_leaveCS       OsalPort_leaveCS
#define  MFG_setEvent      OsalPort_setEvent
#define  MFG_startTimer    OsalPortTimers_startTimer
#define  MFG_random        OsalPort_rand

#define  MFG_LOAD_U16(p,w)   (*p=LO_UINT16(w),*(p+1)=HI_UINT16(w))
#define  MFG_SelfTaskId()    OsalPort_getTaskId(Task_self())

#define  MFG_Nv_Item_Init(id, len, buf)       zclport_initializeNVItem( ZCD_NV_EX_MFG, id, len, buf )
#define  MFG_Nv_Read(id, ndx, len, buf)       zclport_readNV( ZCD_NV_EX_MFG, id, ndx, len, buf )
#define  MFG_Nv_Write(id, len, buf)           zclport_writeNV( ZCD_NV_EX_MFG, id, len, buf )
#define  MFG_NV_Delete(id, len)               zclport_deleteNV( ZCD_NV_EX_MFG, id, len )

#define  MFG_MAC_OR( mac )    (mac[0]|mac[1]|mac[2]|mac[3]|mac[4]|mac[5]|mac[6]|mac[7])
#define  MFG_MAC_AND( mac )   (mac[0]&mac[1]&mac[2]&mac[3]&mac[4]&mac[5]&mac[6]&mac[7])
#define  MFG_MAC_ZERO( mac )  (MFG_MAC_OR(mac) == 0)
#define  MFG_MAC_FF( mac )    (MFG_MAC_AND(mac) == 0xFF)

// Green Power Events
#define  MFG_GP_TEMP_MASTER_EVT            0x4000
#define  MFG_GP_EXPIRE_DUPLICATE_EVT       0x2000
#define  MFG_GP_DATA_SEND_EVT              0x1000
//MFG events
#define  MFG_MSG_IN_EVENT    0x80000000
#define  MFG_CMD_RX_EVENT    0x40000000
#define  MFG_CMD_TX_EVENT    0x20000000

//
#define  MFG_PERIOD_EVENT                  0x08000000  //every second
#define  MFG_SEQ_REC_EVENT                 0x04000000  //
// Coord event

//MFG NV Item ID
#define ZCD_NV_EX_MFG                      0x0010

//MFG NV IDs

/* CONST */

#if !defined (ZCL_PORT_MAX_ENDPOINTS)
#define ZCL_PORT_MAX_ENDPOINTS 5
#endif

/* TYPEDEF*/
typedef uint8_t mfgDevSn_t[9];
typedef uint8_t mfgDevAddr_t[3];

#endif
