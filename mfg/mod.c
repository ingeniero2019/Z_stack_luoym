extern void zclXXX_RegisterUnsupportCallback( ZStatus_t (*callback)(zclIncoming_t*pInMsg) );

static ZStatus_t (*zclXxxUnsupportCallback)(zclIncoming_t* pInMsg) = NULL;

/*********************************************************************
 * @fn      zclXXX_RegisterUnsupportCallback
 *
 * @brief   Register an callback for unsupport endpoint
 *
 * @param   callback - pointer to the callback record.
 *
 * @return  NONE
 */
void zclXXX_RegisterUnsupportCallback( ZStatus_t (*callback)(zclIncoming_t*pInMsg) )
{
  // Register as a ZCL Plugin
  zclXxxUnsupportCallback = callback;
}

static ZStatus_t zclXXX_HdlIncoming( zclIncoming_t *pInMsg )
{
  ZStatus_t stat = ZSuccess;

#if defined ( INTER_PAN ) || defined ( BDB_TL_INITIATOR ) || defined ( BDB_TL_TARGET )
  if ( StubAPS_InterPan( pInMsg->msg->srcAddr.panId, pInMsg->msg->srcAddr.endPoint ) )
    return ( stat ); // Cluster not supported thru Inter-PAN
#endif
  if ( zcl_ClusterCmd( pInMsg->hdr.fc.type ) )
  {
    // Is this a manufacturer specific command?
    stat = ZFailure;
    if ( pInMsg->hdr.fc.manuSpecific == 0 )
    {
#ifdef ZCL_DISCOVER
      zclCommandRec_t cmdRec;
      if( TRUE == zclFindCmdRec( pInMsg->msg->endPoint, pInMsg->msg->clusterId,
                                pInMsg->hdr.commandID, &cmdRec ) )
      {
        if( ( zcl_ServerCmd(pInMsg->hdr.fc.direction) && (cmdRec.flag & CMD_DIR_SERVER_RECEIVED) ) ||
           ( zcl_ClientCmd(pInMsg->hdr.fc.direction) && (cmdRec.flag & CMD_DIR_CLIENT_RECEIVED) ) )
        {
          stat = zclXXX_HdlInSpecificCommands( pInMsg );
        }
      }
#else
      stat = zclXXX_HdlInSpecificCommands( pInMsg );
#endif
    }
    else
    {
      // We don't support any manufacturer specific command.
      stat = ZFailure;
    }
    //execute unsupported command,luoyiming fix at 2019-07-20
    if( stat == ZFailure)
    {
      if( zclXxxUnsupportCallback )
      {
        stat = zclXxxUnsupportCallback( pInMsg );
      }
    }
  }
  else
  {
    // Handle all the normal (Read, Write...) commands -- should never get here
    stat = ZFailure;
  }
  return ( stat );
}

      
      