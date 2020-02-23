/**************************************************************************************************
  Filename:       zcl_generic.c
  Revised:        $Date: 2014-10-24 16:04:46 -0700 (Fri, 24 Oct 2014) $
  Revision:       $Revision: 40796 $


  Description:    Zigbee Cluster Library - sample device application.


  Copyright 2006-2014 Texas Instruments Incorporated. All rights reserved.

  IMPORTANT: Your use of this Software is limited to those specific rights
  granted under the terms of a software license agreement between the user
  who downloaded the software, his/her employer (which must be your employer)
  and Texas Instruments Incorporated (the "License").  You may not use this
  Software unless you agree to abide by the terms of the License. The License
  limits your use, and you acknowledge, that the Software may not be modified,
  copied or distributed unless embedded on a Texas Instruments microcontroller
  or used solely and exclusively in conjunction with a Texas Instruments radio
  frequency transceiver, which is integrated into your product.  Other than for
  the foregoing purpose, you may not use, reproduce, copy, prepare derivative
  works of, modify, distribute, perform, display or sell this Software and/or
  its documentation for any purpose.

  YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
  PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
  INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
  NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
  TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
  NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
  LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
  INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
  OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
  OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
  (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

  Should you have any questions regarding your right to use this Software,
  contact Texas Instruments Incorporated at www.TI.com.
**************************************************************************************************/

/*********************************************************************
  This application is a template to get started writing an application
  from scratch.

  Look for the sections marked with "GENERIC_TODO" to add application
  specific code.

  Note: if you would like your application to support automatic attribute
  reporting, include the BDB_REPORTING compile flag.
*********************************************************************/

/*********************************************************************
 * INCLUDES
 */

#include "rom_jt_154.h"
#include "zcomdef.h"

#include "nvintf.h"
#include <string.h>

#include "zstackmsg.h"
#include "zstackapi.h"

#include "zcl.h"
#include "zcl_general.h"
#include "zcl_diagnostic.h"
#include "zcl_generic.h"
#include "zcl_port.h"

//#include "cui.h"
#include "ti_drivers_config.h"
#include "util_timer.h"

#include <ti/sysbios/knl/Semaphore.h>

#if !defined (DISABLE_GREENPOWER_BASIC_PROXY) && (ZG_BUILD_RTR_TYPE)
#include "gp_common.h"
#endif

#include "mfg.h"
#include "util/mfg_Stack.h"
#include "util/mfg_coord.h"
#include "cmd/cmd.h"
#include "cmd/cmd_pkt.h"
#include "cmd/cmd_cfg.h"
#include "cmd/cmd_nwk.h"
#include "cmd/cmd_zcl.h"

#include "ota_srv_app.h"

/*********************************************************************
 * MACROS
 */


/*********************************************************************
 * CONSTANTS
 */


/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */
byte Generic_TaskID;
extern const uint8_t Generic_AppVersion;

/*********************************************************************
 * GLOBAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

// Semaphore used to post events to the application thread
static Semaphore_Handle appSemHandle;
static Semaphore_Struct appSem;

/* App service ID used for messaging with stack service task */
static uint8_t  appServiceTaskId;
/* App service task events, set by the stack service task when sending a message */
static uint32_t appServiceTaskEvents;
static endPointDesc_t  zclGenericEpDesc = {0};

// Passed in function pointers to the NV driver
static NVINTF_nvFuncts_t *pfnZdlNV = NULL;



// Init State

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void Generic_initialization(void);
static void Generic_process_loop(void);
static void Generic_initParameters(void);
static void Generic_processZStackMsgs(zstackmsg_genericReq_t *pMsg);
static void Generic_processAfIncomingMsgInd(zstack_afIncomingMsgInd_t *pInMsg);
static void Generic_initializeClocks(void);
#if ZG_BUILD_ENDDEVICE_TYPE
static void Generic_processEndDeviceRejoinTimeoutCallback(UArg a0);
#endif
static void Generic_Init( void );
static void Generic_BasicResetCB( void );
static void Generic_ProcessCommissioningStatus(bdbCommissioningModeMsg_t *bdbCommissioningModeMsg);

//Identify
static void Generic_Identify( zclIdentify_t *pCmd );
static void Generic_IdentifyQuery( zclIdentifyQuery_t *pCmd );

// Functions to process ZCL Foundation incoming Command/Response messages
static uint8 Generic_ProcessIncomingMsg( zclIncoming_t *pInMsg );

ZStatus_t Generic_DeviceValidateCallback( uint16 nwkAddr, uint8* extAddr,
                                         uint16 parentAddr, uint8 secure, uint8 devStatus );

/*********************************************************************
 * STATUS STRINGS
 */

// TODO?

/*********************************************************************
 * ZCL General Profile Callback table
 */
static zclGeneral_AppCallbacks_t Generic_CmdCallbacks =
{
  Generic_BasicResetCB,                   // Basic Cluster Reset command
  Generic_Identify,                       // Identfiy cmd
  Generic_IdentifyQuery,                  // Identify Query command
  NULL,                                   // Identify Query Response command
  NULL,                                   // Identify Trigger Effect command
#ifdef ZCL_ON_OFF
  NULL,                                   // On/Off cluster commands
  NULL,                                   // On/Off cluster enhanced command Off with Effect
  NULL,                                   // On/Off cluster enhanced command On with Recall Global Scene
  NULL,                                   // On/Off cluster enhanced command On with Timed Off
#endif
#ifdef ZCL_LEVEL_CTRL
  NULL,                                   // Level Control Move to Level command
  NULL,                                   // Level Control Move command
  NULL,                                   // Level Control Step command
  NULL,                                   // Level Control Stop command
#endif
#ifdef ZCL_GROUPS
  NULL,                                   // Group Response commands
#endif
#ifdef ZCL_SCENES
  NULL,                                  // Scene Store Request command
  NULL,                                  // Scene Recall Request command
  NULL,                                  // Scene Response command
#endif
#ifdef ZCL_ALARMS
  NULL,                                  // Alarm (Response) commands
#endif
#ifdef SE_UK_EXT
  NULL,                                  // Get Event Log command
  NULL,                                  // Publish Event Log command
#endif
  NULL,                                  // RSSI Location command
  NULL                                   // RSSI Location Response command
};


/*********************************************************************
 * GENERIC_TODO: Add other callback structures for any additional application specific
 *       Clusters being used, see available callback structures below.
 *
 *       bdbTL_AppCallbacks_t
 *       zclApplianceControl_AppCallbacks_t
 *       zclApplianceEventsAlerts_AppCallbacks_t
 *       zclApplianceStatistics_AppCallbacks_t
 *       zclElectricalMeasurement_AppCallbacks_t
 *       zclGeneral_AppCallbacks_t
 *       zclGp_AppCallbacks_t
 *       zclHVAC_AppCallbacks_t
 *       zclLighting_AppCallbacks_t
 *       zclMS_AppCallbacks_t
 *       zclPollControl_AppCallbacks_t
 *       zclPowerProfile_AppCallbacks_t
 *       zclSS_AppCallbacks_t
 *
 */

/*******************************************************************************
 * @fn          sampleApp_task
 *
 * @brief       Application task entry point for the Z-Stack
 *              Sample Application
 *
 * @param       pfnNV - pointer to the NV functions
 *
 * @return      none
 */
void sampleApp_task(NVINTF_nvFuncts_t *pfnNV)
{
  // Save and register the function pointers to the NV drivers
  pfnZdlNV = pfnNV;
  zclport_registerNV(pfnZdlNV, ZCL_PORT_SCENE_TABLE_NV_ID);
  //MFG_WatchDogOpen();
  MFG_RandomExtPanid();
#if (RF_PA == 20)
  {
    int8_t power = 0;
    MAP_MAC_MlmeSetReq( MAC_PHY_TRANSMIT_POWER_SIGNED, &power );
    int8_t rssiThreshold;
    if( SUCCESS == MAP_MAC_MlmeGetReq( MAC_RSSI_THRESHOLD, &rssiThreshold ))
    {
      rssiThreshold += 20;
      MAP_MAC_MlmeSetReq( MAC_RSSI_THRESHOLD, &rssiThreshold );
    }
  }
#endif
  
  // Initialize application
  Generic_initialization();

  // No return from task process
  Generic_process_loop();
}

/*******************************************************************************
 * @fn          Generic_initialization
 *
 * @brief       Initialize the application
 *
 * @param       none
 *
 * @return      none
 */
static void Generic_initialization(void)
{
    /* Initialize user clocks */
    Generic_initializeClocks();

    /* create semaphores for messages / events
     */
    Semaphore_Params semParam;
    Semaphore_Params_init(&semParam);
    semParam.mode = ti_sysbios_knl_Semaphore_Mode_COUNTING;
    Semaphore_construct(&appSem, 0, &semParam);
    appSemHandle = Semaphore_handle(&appSem);

    appServiceTaskId = OsalPort_registerTask(Task_self(), appSemHandle, &appServiceTaskEvents);

    //Initialize stack
    Generic_Init();
 
#if defined(OTA_SERVER) && (OTA_SERVER == TRUE)
    OTA_Server_Init(appServiceTaskId);
#endif
}

/*********************************************************************
 * @fn          Generic_Init
 *
 * @brief       Initialization function for the zclGeneral layer.
 *
 * @param       none
 *
 * @return      none
 */
static void Generic_Init( void )
{
  //Register Endpoint
  zclGenericEpDesc.endPoint = GENERIC_ENDPOINT;
  zclGenericEpDesc.simpleDesc = &Generic_SimpleDesc;
  zclport_registerEndpoint(appServiceTaskId, &zclGenericEpDesc);

  // Register the ZCL General Cluster Library callback functions
  zclGeneral_RegisterCmdCallbacks( GENERIC_ENDPOINT, &Generic_CmdCallbacks );

  // GENERIC_TODO: Register other cluster command callbacks here

  // Register the application's attribute list
  Generic_ResetAttributesToDefaultValues();
  zcl_registerAttrList( GENERIC_ENDPOINT, Generic_NumAttributes, Generic_Attrs );

  // Register the Application to receive the unprocessed Foundation command/response messages
  zclport_registerZclHandleExternal( GENERIC_ENDPOINT, Generic_ProcessIncomingMsg );

#if !defined (DISABLE_GREENPOWER_BASIC_PROXY) && (ZG_BUILD_RTR_TYPE)
  gp_endpointInit(appServiceTaskId);
#endif

  //Write the bdb initialization parameters
  Generic_initParameters();

  //Setup ZDO callbacks
  //SetupZStackCallbacks();
  MFG_StackInit(appServiceTaskId);
  MFG_SetupZstackCallbacks();

#ifdef ZCL_DISCOVER
  // Register the application's command list
  zcl_registerCmdList( GENERIC_ENDPOINT, zclCmdsArraySize, Generic_Cmds );
#endif

  CmdPkt_Init(appServiceTaskId, &zclGenericEpDesc);
#ifdef ZCL_DIAGNOSTIC
  // Register the application's callback function to read/write attribute data.
  // This is only required when the attribute data format is unknown to ZCL.
  zcl_registerReadWriteCB( GENERIC_ENDPOINT, zclDiagnostic_ReadWriteAttrCB, NULL );

  if ( zclDiagnostic_InitStats() == ZSuccess )
  {
    // Here the user could start the timer to save Diagnostics to NV
  }
#endif

#if !defined (DISABLE_GREENPOWER_BASIC_PROXY) && (ZG_BUILD_RTR_TYPE)
  app_Green_Power_Init(appServiceTaskId, &appServiceTaskEvents, appSemHandle, MFG_GP_DATA_SEND_EVT, 
                       MFG_GP_EXPIRE_DUPLICATE_EVT, MFG_GP_TEMP_MASTER_EVT);
#endif

}

/*********************************************************************
 * @fn          Generic_initParameters
 *
 * @brief       Initialization function for the bdb attribute set
 *
 * @param       none
 *
 * @return      none
 */
static void Generic_initParameters(void)
{
    zstack_bdbSetAttributesReq_t zstack_bdbSetAttrReq;

    zstack_bdbSetAttrReq.bdbCommissioningGroupID              = BDB_DEFAULT_COMMISSIONING_GROUP_ID;
    zstack_bdbSetAttrReq.bdbPrimaryChannelSet                 = BDB_DEFAULT_PRIMARY_CHANNEL_SET;
    zstack_bdbSetAttrReq.bdbScanDuration                      = BDB_DEFAULT_SCAN_DURATION;
    zstack_bdbSetAttrReq.bdbSecondaryChannelSet               = BDB_DEFAULT_SECONDARY_CHANNEL_SET;
    zstack_bdbSetAttrReq.has_bdbCommissioningGroupID          = TRUE;
    zstack_bdbSetAttrReq.has_bdbPrimaryChannelSet             = TRUE;
    zstack_bdbSetAttrReq.has_bdbScanDuration                  = TRUE;
    zstack_bdbSetAttrReq.has_bdbSecondaryChannelSet           = TRUE;
#if (ZG_BUILD_COORDINATOR_TYPE)
    zstack_bdbSetAttrReq.has_bdbJoinUsesInstallCodeKey        = TRUE;
    zstack_bdbSetAttrReq.has_bdbTrustCenterNodeJoinTimeout    = TRUE;
    zstack_bdbSetAttrReq.has_bdbTrustCenterRequireKeyExchange = TRUE;
    zstack_bdbSetAttrReq.bdbJoinUsesInstallCodeKey            = BDB_DEFAULT_JOIN_USES_INSTALL_CODE_KEY;
    zstack_bdbSetAttrReq.bdbTrustCenterNodeJoinTimeout        = BDB_DEFAULT_TC_NODE_JOIN_TIMEOUT;
    zstack_bdbSetAttrReq.bdbTrustCenterRequireKeyExchange     = BDB_DEFAULT_TC_REQUIRE_KEY_EXCHANGE;
#endif
#if (ZG_BUILD_JOINING_TYPE)
    zstack_bdbSetAttrReq.has_bdbTCLinkKeyExchangeAttemptsMax  = TRUE;
    zstack_bdbSetAttrReq.has_bdbTCLinkKeyExchangeMethod       = TRUE;
    zstack_bdbSetAttrReq.bdbTCLinkKeyExchangeAttemptsMax      = BDB_DEFAULT_TC_LINK_KEY_EXCHANGE_ATTEMPS_MAX;
    zstack_bdbSetAttrReq.bdbTCLinkKeyExchangeMethod           = BDB_DEFAULT_TC_LINK_KEY_EXCHANGE_METHOD;
#endif

    Zstackapi_bdbSetAttributesReq(appServiceTaskId, &zstack_bdbSetAttrReq);
}

/*******************************************************************************
 * @fn      Generic_initializeClocks
 *
 * @brief   Initialize Clocks
 *
 * @param   none
 *
 * @return  none
 */
static void Generic_initializeClocks(void)
{

}

/*********************************************************************
 * @fn          zclSample_event_loop
 *
 * @brief       Event Loop Processor for zclGeneral.
 *
 * @param       none
 *
 * @return      none
 */
static void Generic_process_loop( void )
{
    MFG_AddrMgrInit();
    cmdCfg_Boot(0,Generic_AppVersion);
    MFG_WatchDogOpen();
    /* Forever loop */
    for(;;)
    {
        zstackmsg_genericReq_t *pMsg = NULL;
        bool msgProcessed = FALSE;

        MFG_LoopReset();

        /* Wait for response message */
        if(Semaphore_pend(appSemHandle, BIOS_WAIT_FOREVER ))
        {
            /* Retrieve the response message */
            if( (pMsg = (zstackmsg_genericReq_t*) OsalPort_msgReceive( appServiceTaskId )) != NULL)
            {
                /* Process the message from the stack */
                Generic_processZStackMsgs(pMsg);

                // Free any separately allocated memory
                msgProcessed = Zstackapi_freeIndMsg(pMsg);
            }

            //process mfg message
            if(appServiceTaskEvents & MFG_MSG_IN_EVENT)
            {
              MFG_MfgMsgPoll();
              OsalPort_clearEvent(appServiceTaskId,MFG_MSG_IN_EVENT);
            }

            if((msgProcessed == FALSE) && (pMsg != NULL))
            {
                OsalPort_msgDeallocate((uint8_t*)pMsg);
            }
            //process uart cmd input
            if(appServiceTaskEvents & MFG_CMD_RX_EVENT)
            {
                CmdPkt_RxCmdPoll();
                OsalPort_clearEvent(appServiceTaskId,MFG_CMD_RX_EVENT);
            }
            //process uart cmd output
            if(appServiceTaskEvents & MFG_CMD_TX_EVENT)
            {
                CmdPkt_TxCmdPoll();
                OsalPort_clearEvent(appServiceTaskId,MFG_CMD_TX_EVENT);
            }
            //process period
            if(appServiceTaskEvents & MFG_PERIOD_EVENT)
            {
                MFG_PeriodCallback();
                Generic_UtcTime ++;
                OsalPort_clearEvent( appServiceTaskId, MFG_PERIOD_EVENT );
            }
            //seq rec release
#ifdef MATCH_SEQ_REC
            if(appServiceTaskEvents & MFG_SEQ_REC_EVENT)
            {
                MFG_SeqRecRelease();
                OsalPort_clearEvent( appServiceTaskId, MFG_SEQ_REC_EVENT );
            }
#endif

#if !defined (DISABLE_GREENPOWER_BASIC_PROXY) && (ZG_BUILD_RTR_TYPE)
            if(appServiceTaskEvents & MFG_GP_DATA_SEND_EVT)
            {
                if(zgGP_ProxyCommissioningMode == TRUE)
                {
                  zcl_gpSendCommissioningNotification();
                }
                else
                {
                  zcl_gpSendNotification();
                }
                appServiceTaskEvents &= ~MFG_GP_DATA_SEND_EVT;
            }

            if(appServiceTaskEvents & MFG_GP_EXPIRE_DUPLICATE_EVT)
            {
                gp_expireDuplicateFiltering();
                appServiceTaskEvents &= ~MFG_GP_EXPIRE_DUPLICATE_EVT;
            }

            if(appServiceTaskEvents & MFG_GP_TEMP_MASTER_EVT)
            {
                gp_returnOperationalChannel();
                appServiceTaskEvents &= ~MFG_GP_TEMP_MASTER_EVT;
            }
#endif
        }
    }
}

/*******************************************************************************
 * @fn      Generic_processZStackMsgs
 *
 * @brief   Process event from Stack
 *
 * @param   pMsg - pointer to incoming ZStack message to process
 *
 * @return  void
 */
static void Generic_processZStackMsgs(zstackmsg_genericReq_t *pMsg)
{
    switch(pMsg->hdr.event)
    {
    case zstackmsg_CmdIDs_BDB_TC_LINK_KEY_EXCHANGE_NOTIFICATION_IND:
      {
        zstackmsg_bdbTCLinkKeyExchangeInd_t *pInd = (zstackmsg_bdbTCLinkKeyExchangeInd_t*)pMsg;
        MFG_VerifyLinkKey(pInd->Req);
      }
      break;
      
    case zstackmsg_CmdIDs_ZDO_TC_DEVICE_IND:
      {
        //Creat New Device
        zstackmsg_zdoTcDeviceInd_t *pInd = (zstackmsg_zdoTcDeviceInd_t*)pMsg;
        MFG_TcJoinInd(pInd->req);//(pInd->req.nwkAddr, pInd->req.extendedAddr, pInd->req.parentAddr );
      }
      break;
    case zstackmsg_CmdIDs_BDB_NOTIFICATION:
      {
        zstackmsg_bdbNotificationInd_t *pInd;
        pInd = (zstackmsg_bdbNotificationInd_t*)pMsg;
        Generic_ProcessCommissioningStatus(&(pInd->Req));
      }
      break;
      
    case zstackmsg_CmdIDs_BDB_IDENTIFY_TIME_CB:
      {
        zstackmsg_bdbIdentifyTimeoutInd_t *pInd;
        pInd = (zstackmsg_bdbIdentifyTimeoutInd_t*) pMsg;
        if(pInd->EndPoint == GENERIC_ENDPOINT)
        {
          cmdCfg_Identify( Generic_IdentifyTime );
        }
      }
      break;
      
    case zstackmsg_CmdIDs_BDB_BIND_NOTIFICATION_CB:
      {
        //                zstackmsg_bdbBindNotificationInd_t *pInd;
        //                pInd = (zstackmsg_bdbBindNotificationInd_t*) pMsg;
        //                uiProcessBindNotification(&(pInd->Req));
      }
      break;
      
    case zstackmsg_CmdIDs_AF_DATA_CONFIRM_IND:
      {
        zstackmsg_afDataConfirmInd_t *pInd;
        pInd = (zstackmsg_afDataConfirmInd_t *)pMsg;
        if(pInd->req.afCnfCB)
        {
          pInd->req.afCnfCB(pInd->req.status,pInd->req.endpoint,pInd->req.transID, pInd->req.clusterID, pInd->req.cnfParam);
        }
      }
      break;
      
    case zstackmsg_CmdIDs_AF_INCOMING_MSG_IND:
      {
        // Process incoming data messages
        zstackmsg_afIncomingMsgInd_t *pInd;
        pInd = (zstackmsg_afIncomingMsgInd_t *)pMsg;
        Generic_processAfIncomingMsgInd( &(pInd->req) );
      }
      break;
      
    case zstackmsg_CmdIDs_DEV_PERMIT_JOIN_IND:
      {
        zstackmsg_devPermitJoinInd_t *pInd;
        pInd = (zstackmsg_devPermitJoinInd_t*)pMsg;
        cmdCfg_PermitJoinNotify(pInd->Req.duration);
      }
      break;
      
    case zstackmsg_CmdIDs_ZDO_DEVICE_ANNOUNCE:
      {
        zstackmsg_zdoDeviceAnnounceInd_t *pInd = (zstackmsg_zdoDeviceAnnounceInd_t*)pMsg;
        MFG_DeviceAnnounceInd( pInd->req );
      }
      break;
      
    case zstackmsg_CmdIDs_ZDO_NWK_DISC_CNF:
      {
        zstackmsg_zdoNwkDiscCnf_t *pInd = (zstackmsg_zdoNwkDiscCnf_t*)pMsg;
        cmdCfg_ScanEnd( pInd->req.status );
      }
      break;
      
    case zstackmsg_CmdIDs_ZDO_BEACON_NOTIFY_IND:
      {
        zstackmsg_zdoBeaconNotifyInd_t *pInd = (zstackmsg_zdoBeaconNotifyInd_t*)pMsg;
        cmdCfg_BeaconIndNotify( pInd->req.sourceAddr, pInd->req.panID,
                               pInd->req.logicalChannel, pInd->req.lqi, pInd->req.extendedPANID );
      }
      break;
      
    case zstackmsg_CmdIDs_ZDO_NODE_DESC_RSP:
      {
        zstackmsg_zdoNodeDescRspInd_t *pInd = (zstackmsg_zdoNodeDescRspInd_t*)pMsg;
        cmdZdp_NodeDescRsp( &(pInd->rsp));
      }
      break;
      
    case zstackmsg_CmdIDs_ZDO_NWK_ADDR_RSP:
      {
        zstackmsg_zdoNwkAddrRspInd_t *pInd = (zstackmsg_zdoNwkAddrRspInd_t*)pMsg;
        cmdZdp_NwkAddrRsp(&(pInd->rsp));
      }
      break;
      
    case zstackmsg_CmdIDs_ZDO_IEEE_ADDR_RSP:
      {
        zstackmsg_zdoIeeeAddrRspInd_t *pInd = (zstackmsg_zdoIeeeAddrRspInd_t*)pMsg;
        cmdZdp_IeeeAddrRsp(&(pInd->rsp));
      }
      break;
      
    case zstackmsg_CmdIDs_ZDO_SIMPLE_DESC_RSP:
      {
        zstackmsg_zdoSimpleDescRspInd_t *pInd = (zstackmsg_zdoSimpleDescRspInd_t*)pMsg;
        //zstack_SimpleDescriptor_t *simpleDesc = &(pInd->rsp.simpleDesc);
        if(FALSE == MFG_SimpleDescInd( pInd->rsp ))
        {
          cmdZdp_SimpleDescRsp( &(pInd->rsp) );
        }
      }
      break;
      
    case zstackmsg_CmdIDs_ZDO_ACTIVE_EP_RSP:
      {
        zstackmsg_zdoActiveEndpointsRspInd_t *pInd = (zstackmsg_zdoActiveEndpointsRspInd_t*)pMsg;
        if(FALSE == MFG_ActiveEpInd( pInd->rsp ))
        {
          cmdZdp_ActiveEpRsp( &(pInd->rsp) );
        }
      }
      break;
      
    case zstackmsg_CmdIDs_ZDO_BIND_RSP:
      {
        zstackmsg_zdoBindRspInd_t *pInd = (zstackmsg_zdoBindRspInd_t*)pMsg;
        cmdZdp_BindRsp( &(pInd->rsp) );
      }
      break;
      
    case zstackmsg_CmdIDs_ZDO_UNBIND_RSP:
      {
        zstackmsg_zdoUnbindRspInd_t *pInd = (zstackmsg_zdoUnbindRspInd_t*)pMsg;
        cmdZdp_UnbindRsp( &(pInd->rsp) );
      }
      break;
      
    case zstackmsg_CmdIDs_ZDO_MGMT_LQI_RSP:
      {
        zstackmsg_zdoMgmtLqiRspInd_t *pInd = (zstackmsg_zdoMgmtLqiRspInd_t*)pMsg;
        cmdZdp_LqiTableRsp( &(pInd->rsp) );
      }
      break;
      
    case zstackmsg_CmdIDs_ZDO_MGMT_BIND_RSP:
      {
        zstackmsg_zdoMgmtBindRspInd_t *pInd = (zstackmsg_zdoMgmtBindRspInd_t*)pMsg;
        cmdZdp_BindTableRsp( &(pInd->rsp) );
      }
      break;
      
    case zstackmsg_CmdIDs_ZDO_MGMT_LEAVE_RSP:
      {
        zstackmsg_zdoMgmtLeaveRspInd_t *pInd = (zstackmsg_zdoMgmtLeaveRspInd_t*)pMsg;
        cmdZdp_LeaveRsp( &(pInd->rsp) );
        break;
      }
      
    case zstackmsg_CmdIDs_ZDO_LEAVE_IND:
      {
        zstackmsg_zdoLeaveInd_t *pInd = (zstackmsg_zdoLeaveInd_t*)pMsg;
        if(pInd->req.srcAddr)
        {
          //MFG_DeleteAddrMgr(pInd->req.extendedAddr);
          cmdCfg_LeaveInd(pInd->req);
        }
      }
      break;
      
#if (ZG_BUILD_JOINING_TYPE)
    case zstackmsg_CmdIDs_BDB_CBKE_TC_LINK_KEY_EXCHANGE_IND:
      {
        zstack_bdbCBKETCLinkKeyExchangeAttemptReq_t zstack_bdbCBKETCLinkKeyExchangeAttemptReq;
        /* Z3.0 has not defined CBKE yet, so lets attempt default TC Link Key exchange procedure
        * by reporting CBKE failure.
        */
        
        zstack_bdbCBKETCLinkKeyExchangeAttemptReq.didSuccess = FALSE;
        
        Zstackapi_bdbCBKETCLinkKeyExchangeAttemptReq(appServiceTaskId,
                                                     &zstack_bdbCBKETCLinkKeyExchangeAttemptReq);
      }
      break;
      
    case zstackmsg_CmdIDs_BDB_FILTER_NWK_DESCRIPTOR_IND:
      
      /*   User logic to remove networks that do not want to join
      *   Networks to be removed can be released with Zstackapi_bdbNwkDescFreeReq
      */
      
      Zstackapi_bdbFilterNwkDescComplete(appServiceTaskId);
      break;
      
#endif
    case zstackmsg_CmdIDs_DEV_STATE_CHANGE_IND:
      {
        zstackmsg_devStateChangeInd_t *pInd = (zstackmsg_devStateChangeInd_t*)pMsg;
        if(pInd->req.state == zstack_DevState_DEV_ZB_COORD)
        {
          //MFG_WatchDogOpen();
        }
      }
      break;
      
      
      
      /*
      * These are messages/indications from ZStack that this
      * application doesn't process.  These message can be
      * processed by your application, remove from this list and
      * process them here in this switch statement.
      */
      
#if !defined (DISABLE_GREENPOWER_BASIC_PROXY) && (ZG_BUILD_RTR_TYPE)
    case zstackmsg_CmdIDs_GP_DATA_IND:
      {
        zstackmsg_gpDataInd_t *pInd;
        pInd = (zstackmsg_gpDataInd_t*)pMsg;
        gp_processDataIndMsg( &(pInd->Req) );
      }
      break;
      
    case zstackmsg_CmdIDs_GP_SECURITY_REQ:
      {
        zstackmsg_gpSecReq_t *pInd;
        pInd = (zstackmsg_gpSecReq_t*)pMsg;
        gp_processSecRecMsg( &(pInd->Req) );
      }
      break;
      
    case zstackmsg_CmdIDs_GP_CHECK_ANNCE:
      {
        zstackmsg_gpCheckAnnounce_t *pInd;
        pInd = (zstackmsg_gpCheckAnnounce_t*)pMsg;
        gp_processCheckAnnceMsg( &(pInd->Req) );
      }
      break;
      
#endif
      
      
#ifdef BDB_TL_TARGET
    case zstackmsg_CmdIDs_BDB_TOUCHLINK_TARGET_ENABLE_IND:
      {
        
      }
      break;
#endif
      
    case zstackmsg_CmdIDs_ZDO_POWER_DESC_RSP:
      //case zstackmsg_CmdIDs_ZDO_SIMPLE_DESC_RSP:
      //case zstackmsg_CmdIDs_ZDO_ACTIVE_EP_RSP:
    case zstackmsg_CmdIDs_ZDO_COMPLEX_DESC_RSP:
    case zstackmsg_CmdIDs_ZDO_USER_DESC_RSP:
    case zstackmsg_CmdIDs_ZDO_USER_DESC_SET_RSP:
    case zstackmsg_CmdIDs_ZDO_SERVER_DISC_RSP:
    case zstackmsg_CmdIDs_ZDO_END_DEVICE_BIND_RSP:
    case zstackmsg_CmdIDs_ZDO_MGMT_NWK_DISC_RSP:
      
    case zstackmsg_CmdIDs_ZDO_MGMT_RTG_RSP:
    case zstackmsg_CmdIDs_ZDO_MGMT_DIRECT_JOIN_RSP:
    case zstackmsg_CmdIDs_ZDO_MGMT_PERMIT_JOIN_RSP:
    case zstackmsg_CmdIDs_ZDO_MGMT_NWK_UPDATE_NOTIFY:
    case zstackmsg_CmdIDs_ZDO_SRC_RTG_IND:
    case zstackmsg_CmdIDs_ZDO_CONCENTRATOR_IND:
    case zstackmsg_CmdIDs_ZDO_LEAVE_CNF:
    case zstackmsg_CmdIDs_SYS_RESET_IND:
    case zstackmsg_CmdIDs_AF_REFLECT_ERROR_IND:
      //case zstackmsg_CmdIDs_ZDO_TC_DEVICE_IND:
      break;
      
    default:
      break;
  }
}

/*******************************************************************************
 *
 * @fn          Generic_processAfIncomingMsgInd
 *
 * @brief       Process AF Incoming Message Indication message
 *
 * @param       pInMsg - pointer to incoming message
 *
 * @return      none
 *
 */
static void Generic_processAfIncomingMsgInd(zstack_afIncomingMsgInd_t *pInMsg)
{
    afIncomingMSGPacket_t afMsg;

    /*
     * All incoming messages are passed to the ZCL message processor,
     * first convert to a structure that ZCL can process.
     */
    afMsg.groupId = pInMsg->groupID;
    afMsg.clusterId = pInMsg->clusterId;
    afMsg.srcAddr.endPoint = pInMsg->srcAddr.endpoint;
    afMsg.srcAddr.panId = pInMsg->srcAddr.panID;
    afMsg.srcAddr.addrMode = (afAddrMode_t)pInMsg->srcAddr.addrMode;
    if( (afMsg.srcAddr.addrMode == afAddr16Bit)
        || (afMsg.srcAddr.addrMode == afAddrGroup)
        || (afMsg.srcAddr.addrMode == afAddrBroadcast) )
    {
        afMsg.srcAddr.addr.shortAddr = pInMsg->srcAddr.addr.shortAddr;
    }
    else if(afMsg.srcAddr.addrMode == afAddr64Bit)
    {
        OsalPort_memcpy(afMsg.srcAddr.addr.extAddr, &(pInMsg->srcAddr.addr.extAddr), 8);
    }
    afMsg.macDestAddr = pInMsg->macDestAddr;
    afMsg.endPoint = pInMsg->endpoint;
    afMsg.wasBroadcast = pInMsg->wasBroadcast;
    afMsg.LinkQuality = pInMsg->linkQuality;
    afMsg.correlation = pInMsg->correlation;
    afMsg.rssi = pInMsg->rssi;
    afMsg.SecurityUse = pInMsg->securityUse;
    afMsg.timestamp = pInMsg->timestamp;
    afMsg.nwkSeqNum = pInMsg->nwkSeqNum;
    afMsg.macSrcAddr = pInMsg->macSrcAddr;
    afMsg.radius = pInMsg->radius;
    afMsg.cmd.DataLength = pInMsg->n_payload;
    afMsg.cmd.Data = pInMsg->pPayload;

    zcl_ProcessMessageMSG(&afMsg);
}

/*********************************************************************
 * @fn      Generic_ProcessCommissioningStatus
 *
 * @brief   Callback in which the status of the commissioning process are reported
 *
 * @param   bdbCommissioningModeMsg - Context message of the status of a commissioning process
 *
 * @return  none
 */
static void Generic_ProcessCommissioningStatus(bdbCommissioningModeMsg_t *bdbCommissioningModeMsg)
{
  switch(bdbCommissioningModeMsg->bdbCommissioningMode)
  {
    case BDB_COMMISSIONING_FORMATION:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS)
      {
        zstack_bdbStartCommissioningReq_t zstack_bdbStartCommissioningReq;
        zstack_bdbStartCommissioningReq.commissioning_mode = BDB_COMMISSIONING_MODE_NWK_STEERING;//open net after start
        Zstackapi_bdbStartCommissioningReq(appServiceTaskId,&zstack_bdbStartCommissioningReq);
      }
    break;
    case BDB_COMMISSIONING_NWK_STEERING:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS)
      {
        zstack_sysNwkInfoReadRsp_t sysNwkInfoReadRsp;
        Zstackapi_sysNwkInfoReadReq(appServiceTaskId,&sysNwkInfoReadRsp);
        cmdCfg_CoordStartNotify( COORD_STA_OPEN, sysNwkInfoReadRsp.logicalChannel, sysNwkInfoReadRsp.panId, sysNwkInfoReadRsp.extendedPanId );
      }
    break;
    case BDB_COMMISSIONING_INITIALIZATION:
      {
        uint8_t sta = COORD_STA_IDLE;
        zstack_sysNwkInfoReadRsp_t sysNwkInfoReadRsp;
        Zstackapi_sysNwkInfoReadReq(appServiceTaskId,&sysNwkInfoReadRsp);
        //zstack_bdbGetAttributesRsp_t bdbGetRsp;
        //Zstackapi_bdbGetAttributesReq(appServiceTaskId, &bdbGetRsp);
        if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_NETWORK_RESTORED)//(bdbGetRsp.bdbNodeIsOnANetwork)
        {
          sta = COORD_STA_NET;
        }
        cmdCfg_CoordStartNotify( sta, sysNwkInfoReadRsp.logicalChannel, sysNwkInfoReadRsp.panId, sysNwkInfoReadRsp.ieeeAddr );
      }
    break;
  }
}

/*********************************************************************
 * @fn      Generic_ProcessTouchlinkTargetEnable
 *
 * @brief   Called to process when the touchlink target functionality
 *          is enabled or disabled
 *
 * @param   none
 *
 * @return  none
 */
#if ( defined ( BDB_TL_TARGET ) && (BDB_TOUCHLINK_CAPABILITY_ENABLED == TRUE) )
static void Generic_ProcessTouchlinkTargetEnable( uint8_t enable )
{
  if ( enable )
  {
    HalLedSet ( HAL_LED_1, HAL_LED_MODE_ON );
  }
  else
  {
    HalLedSet ( HAL_LED_1, HAL_LED_MODE_OFF );
  }
}
#endif

/*********************************************************************
 * @fn      Generic_BasicResetCB
 *
 * @brief   Callback from the ZCL General Cluster Library
 *          to set all the Basic Cluster attributes to default values.
 *
 * @param   none
 *
 * @return  none
 */
static void Generic_BasicResetCB( void )
{

  /* GENERIC_TODO: remember to update this function with any
     application-specific cluster attribute variables */

  Generic_ResetAttributesToDefaultValues();
  CmdZcl_ZclPluginCallback(zcl_getRawZCLCommand());
}

static void Generic_Identify( zclIdentify_t *pCmd )
{
  CmdZcl_ZclPluginCallback(zcl_getRawZCLCommand());
}

static void Generic_IdentifyQuery( zclIdentifyQuery_t *pCmd )
{
  CmdZcl_ZclPluginCallback(zcl_getRawZCLCommand());
}

/******************************************************************************
 *
 *  Functions for processing ZCL Foundation incoming Command/Response messages
 *
 *****************************************************************************/

/*********************************************************************
 * @fn      Generic_ProcessIncomingMsg
 *
 * @brief   Process ZCL Foundation incoming message
 *
 * @param   pInMsg - pointer to the received message
 *
 * @return  none
 */
static uint8_t Generic_ProcessIncomingMsg( zclIncoming_t *pInMsg )
{
  uint8 handled = FALSE;
  handled = CmdZcl_ProcessIncomingMsg(pInMsg);
  if ( pInMsg->attrCmd )
  {
    MAP_osal_mem_free( pInMsg->attrCmd );
    pInMsg->attrCmd = NULL;
  }
  return handled;
}

/*********************************************************************
 * @fn      MFG_ZclPluginCallback
 *
 * @brief   Process ZCL Foundation incoming message
 *
 * @param   pInMsg - pointer to the received message
 *
 * @return  none
 */
ZStatus_t MFG_ZclPluginCallback( zclIncoming_t *pInMsg )
{
  extern endPointDesc_t *zcl_afFindEndPointDesc(uint8_t EndPoint);
  //extern SimpleDescriptionFormat_t Generic_SimpleDesc;
  endPointDesc_t* epDesc = zcl_afFindEndPointDesc( pInMsg->msg->endPoint );
  
  if( epDesc != NULL )
  {
    SimpleDescriptionFormat_t *simpleDesc = epDesc->simpleDesc;
    if( zcl_ClientCmd( pInMsg->hdr.fc.direction ) )
    {
      return CmdZcl_ZclPluginCallback( pInMsg );
    }
    else
    {
      if( pInMsg->msg->groupId )
      {
        return CmdZcl_ZclPluginCallback( pInMsg );
      }
      else
      {
        uint8_t  num = simpleDesc->AppNumInClusters;
        cId_t    *pCluster = simpleDesc->pAppInClusterList;
        uint8_t n;
        for(n=0; n<num; n++)
        {
          if(pInMsg->msg->clusterId == pCluster[n])
          {
            return CmdZcl_ZclPluginCallback( pInMsg );
          }
        }
      }
    }
  }
  return ZFailure;
}

/****************************************************************************
****************************************************************************/




