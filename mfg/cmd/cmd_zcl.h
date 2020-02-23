#ifndef CMD_ZCL_H
#define CMD_ZCL_H

#ifdef __cplusplus
extern "C"
{
#endif

#define   CMD_INVALID_PROFILE  0xFFFE
  
#define   CMD_ZCL_UNICAST     0x00
#define   CMD_ZCL_MULTCAST    0x01
#define   CMD_ZCL_BCST        0x02
#define   CMD_ZCL_SEND_ERRO   0x10
#define   CMD_ZCL_INCOMING    0x80

//taget:[AddrMode][NodeID][EP][profile][Cluster]
//control:[CMD][zclSeqNum][dir][RSP][manu code]

#define  CMD_MODE_IDX     0
#define  CMD_NODE_L_IDX   1
#define  CMD_NODE_H_IDX   2
#define  CMD_EP_IDX       3
#define  CMD_PROF_L_IDX   4
#define  CMD_PROF_H_IDX   5
#define  CMD_CLU_L_IDX    6
#define  CMD_CLU_H_IDX    7
#define  CMD_CMDID_IDX    8
#define  CMD_SEQ_IDX      9
#define  CMD_DIR_IDX      10
#define  CMD_RSP_IDX      11
#define  CMD_MANU_L_IDX   12
#define  CMD_MANU_H_IDX   13

#define  CMD_EP_ZCL_MAX   (ZCL_PORT_MAX_ENDPOINTS - 2)

#define  CMD_EP_IDX_HA    0

#define  MFG_REPORTABLE_CHANGE_SIZE    4
  
/* TYPEDEF */
typedef struct
{
    uint16_t attrId;
    uint8_t dataType;
    uint8_t attrData[4];
}cmdZclAttrItem_t;

#ifdef __cplusplus
}
#endif


/* FUNCTION */
uint8 CmdZcl_ProcessIncomingMsg( zclIncoming_t *pInMsg );
ZStatus_t CmdZcl_ZclPluginCallback( zclIncoming_t *pInMsg );
bool cmdZcl_InitSrcEP(endPointDesc_t *pEpDesc, bool haEp);

#endif  /* CMD_ZCL_H */
