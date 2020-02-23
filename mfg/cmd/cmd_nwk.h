#ifndef CMD_NWK_H
#define CMD_NWK_H

/* MACRO */
#define ZDP_NWK_ADDR            0x0000
#define ZDP_IEEE_ADDR           0x0001
#define ZDP_NODE_DESC           0x0002
//#define ZDP_POWER_DESC          0x0003
#define ZDP_SIMPLE_DESC         0x0004
#define ZDP_ACTIVE_EP           0x0005
//#define ZDP_MATCH_DESC          0x0006

#define ZDP_BIND                0x0021
#define ZDP_UNBIND              0x0022

#define ZDP_NWK_DISC            0x0030
#define ZDP_LQI_TABLE           0x0031
#define ZDP_RTG_TABLE           0x0032
#define ZDP_BIND_TABLE          0x0033
#define ZDP_LEAVE               0x0034

/* CONST */


/* global function*/
void cmdZdp_SeqTablePoll(void);
//void cmdZdp_CnfOutput(mfgMsgType_t msgType, uint8_t size, uint8_t *data);
void cmdZdp_CnfOutput( uint8_t status, uint8_t transID, void* cnfParam );
//ZDP Response
void cmdZdp_NwkAddrRsp(zstack_zdoNwkAddrRspInd_t *pRsp);
void cmdZdp_IeeeAddrRsp(zstack_zdoIeeeAddrRspInd_t *pRsp);
void cmdZdp_NodeDescRsp(zstack_zdoNodeDescRspInd_t *pRsp);
void cmdZdp_SimpleDescRsp(zstack_zdoSimpleDescRspInd_t *pRsp);
void cmdZdp_ActiveEpRsp(zstack_zdoActiveEndpointsRspInd_t *pRsp);
void cmdZdp_BindRsp(zstack_zdoBindRspInd_t *pRsp);
void cmdZdp_UnbindRsp(zstack_zdoUnbindRspInd_t *pRsp);
void cmdZdp_LqiTableRsp(zstack_zdoMgmtLqiRspInd_t *pRsp);
void cmdZdp_BindTableRsp(zstack_zdoMgmtBindRspInd_t *pRsp);
void cmdZdp_LeaveRsp(zstack_zdoMgmtLeaveRspInd_t *pRsp);

#endif
