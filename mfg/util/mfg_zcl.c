#include "ti_drivers_config.h"
#include "zcomdef.h"
//#include "osal.h"
#include "af.h"
#include "zd_config.h"
#include "zcl.h"

#include "mfg_zcl.h"
#include "cmd/cmd_zcl.h"
#include "zcl_general.h"
//#include "Application/zcl_generic.h"

/**********************************************
    typedef
*/
typedef struct
{
  void *next;
  uint8_t ep;
  mfgZclValidAttrDataCB cb;
}mfgValidAttrList_t;
      
/**********************************************
    Local variable
*/
mfgValidAttrList_t  *mfgValidAttrListHead = NULL;

/**********************************************
    extern variable
*/

/**********************************************
    Local function
*/
uint8 MFG_ZclValidateAttrData( zclAttrRec_t *pAttr, zclWriteRec_t *pAttrInfo );
void MFG_ZclValidAttrCbPush(mfgValidAttrList_t *item);
mfgValidAttrList_t* MFG_ZclValidAttrCbLookup(uint8_t ep);
ZStatus_t MFG_ZclPluginCallback( zclIncoming_t *pInMsg );

/**********************************************
    extern function
*/
void MFG_ZclInit(void)
{
  zclGeneral_RegisterUnsupportCallback(MFG_ZclPluginCallback);

  zcl_registerPlugin( 0x001B,
                     0x001B,
                     MFG_ZclPluginCallback );
  
  zcl_registerPlugin( 0x0100,
                     0x0FFF,
                     MFG_ZclPluginCallback );
  
  zcl_registerPlugin( 0xFC00,
                     0xFCFF,
                     MFG_ZclPluginCallback );
}

/**********************************************
    LOCA function
*/
uint8 MFG_ZclValidateAttrData( zclAttrRec_t *pAttr, zclWriteRec_t *pAttrInfo )
{
  uint8_t ep = zcl_getRawAFMsg()->endPoint;
  mfgValidAttrList_t *find = MFG_ZclValidAttrCbLookup( ep );
  if( find )
  {
    return find->cb( ep, pAttr, pAttrInfo );
  }
  return true;
}

void MFG_ZclValidAttrCbPush(mfgValidAttrList_t *item)
{
  if(mfgValidAttrListHead == NULL)
  {
    mfgValidAttrListHead = item;
  }
  else
  {
    mfgValidAttrList_t *findTail = mfgValidAttrListHead;
    while( findTail->next )
    {
      findTail = findTail->next;
    }
    findTail->next = item;
  }
}

mfgValidAttrList_t* MFG_ZclValidAttrCbLookup(uint8_t ep)
{
  mfgValidAttrList_t* lookup = mfgValidAttrListHead;
  while(lookup)
  {
    if(ep == lookup->ep)
    {
      return lookup;
    }
    lookup = lookup->next;
  }
  return NULL;
}
