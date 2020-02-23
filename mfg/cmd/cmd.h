#ifndef _CMD_H
#define _CMD_H

#include "../mfg.h"

//Input Cmd
#define CMD_CLASS_CFG         0x00
#define CMD_CLASS_NWK         0x01
#define CMD_CLASS_PROFILE     0x02
#define CMD_CLASS_SPECIFIC    0x03
#define CMD_CLASS_OTA_INPUT   0x04

//Output Cmd
#define CMD_CLASS_STATUS       0x80
#define CMD_CLASS_NWK_INFO     0x81
#define CMD_CLASS_PROF_IND     0x82
#define CMD_CLASS_SPEC_IND     0x83
#define CMD_CLASS_OTA_OUTPUT   0x84

#define CMD_IND_ARRAY {cmdCfg_CmdInput,cmdZdp_CmdInput,cmdZcl_CmdInput,cmdZcl_CtrInput}

extern void cmdCfg_CmdInput(uint8_t cmdSeq, uint8_t cmdClass, uint8_t cmdLen, uint8_t* cmdData);
extern void cmdZdp_CmdInput(uint8_t cmdSeq, uint8_t cmdClass, uint8_t cmdLen, uint8_t* cmdData);
extern void cmdZcl_CmdInput(uint8_t cmdSeq, uint8_t cmdClass, uint8_t cmdLen, uint8_t* cmdData);
extern void cmdZcl_CtrInput(uint8_t cmdSeq, uint8_t cmdClass, uint8_t cmdLen, uint8_t* cmdData);
extern void cmdOta_CmdInput(uint8_t cmdSeq, uint8_t cmdClass, uint8_t cmdLen, uint8_t* cmdData);

#endif
