#ifndef _MFG_ZCL_H
#define _MFG_ZCL_H

#include "../mfg.h"



/********************************
      MACRO 
*/


/*******************************
      TYPEDEF
*/
typedef uint8_t (*mfgZclValidAttrDataCB)( uint8_t ep, zclAttrRec_t *pAttr, zclWriteRec_t *pAttrInfo );


void MFG_ZclInit(void);


#endif //_MFG_ZCL_H
