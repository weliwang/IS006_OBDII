/******************************************************************************

 @file  simple_gatt_profile.c

 @brief This file contains the Simple GATT profile sample GATT service profile
        for use with the BLE sample application.

 Group: WCS, BTS
 Target Device: cc13x2_26x2

 ******************************************************************************
 
 Copyright (c) 2010-2020, Texas Instruments Incorporated
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 *  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

 *  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 *  Neither the name of Texas Instruments Incorporated nor the names of
    its contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ******************************************************************************
 
 
 *****************************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include <string.h>
#include <icall.h>
#include "util.h"
/* This Header file contains all BLE API and icall structure definition */
#include "icall_ble_api.h"

#include "simple_gatt_profile.h"

#ifdef SYSCFG
#include "ti_ble_config.h"

#ifdef USE_GATT_BUILDER
#include "ti_services.h"
#endif

#endif

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */
#ifndef USE_GATT_BUILDER

#define SERVAPP_NUM_ATTR_SUPPORTED        17
extern uint8_t gPairEnable;
extern uint32_t gServiceUUID;
extern uint32_t gNotifyUUID;
extern uint32_t gWriteUUID;
extern uint8_t gServiceUUID128bits[16];
extern uint8_t gNotifyUUID128bits[16];
extern uint8_t gWriteUUID128bits[16];

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */
// Simple GATT Profile Service UUID: 0xFFF0
uint8 simpleProfileServUUID[ATT_UUID_SIZE] =
{
  0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
};
// Characteristic 3 UUID: 0xFFF3
uint8 simpleProfilechar3UUID[ATT_UUID_SIZE] =
{
  0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02
};

// Characteristic 4 UUID: 0xFFF4
uint8 simpleProfilechar4UUID[ATT_UUID_SIZE] =
{
  0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03
};
/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

static simpleProfileCBs_t *simpleProfile_AppCBs = NULL;

/*********************************************************************
 * Profile Attributes - variables
 */

// Simple Profile Service attribute
gattAttrType_t simpleProfileService = { ATT_UUID_SIZE, simpleProfileServUUID };


// Simple Profile Characteristic 1 Properties
static uint8 simpleProfileChar1Props = GATT_PROP_READ | GATT_PROP_WRITE;




// Simple Profile Characteristic 3 Properties
static uint8 simpleProfileChar3Props = GATT_PROP_WRITE;

// Characteristic 3 Value
//static uint8 simpleProfileChar3 = 0;//weli old
static uint8 simpleProfileChar3[SIMPLEPROFILE_CHAR3_LEN] = { 0 };

// Simple Profile Characteristic 3 User Description
static uint8 simpleProfileChar3UserDesp[5] = "Write";


// Simple Profile Characteristic 4 Properties
static uint8 simpleProfileChar4Props = GATT_PROP_NOTIFY;

// Characteristic 4 Value
//static uint8 simpleProfileChar4 = 0;//weli old
static uint8 simpleProfileChar4[SIMPLEPROFILE_CHAR4_LEN] = { 0 };

// Simple Profile Characteristic 4 Configuration Each client has its own
// instantiation of the Client Characteristic Configuration. Reads of the
// Client Characteristic Configuration only shows the configuration for
// that client and writes only affect the configuration of that client.
static gattCharCfg_t *simpleProfileChar4Config;

// Simple Profile Characteristic 4 User Description
static uint8 simpleProfileChar4UserDesp[6] = "Notify";




static uint8 gNotify_Length=0x00;
uint8 gWriteUART_Length=0x00;
/*********************************************************************
 * Profile Attributes - Table
 */

static gattAttribute_t simpleProfileAttrTbl[SERVAPP_NUM_ATTR_SUPPORTED] =
{
  // Simple Profile Service
  {
    { ATT_BT_UUID_SIZE, primaryServiceUUID }, /* type */
    GATT_PERMIT_READ,                         /* permissions */
    0,                                        /* handle */
    (uint8 *)&simpleProfileService            /* pValue */
  },

    

    // Characteristic 3 Declaration
    {
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ,
      0,
      &simpleProfileChar3Props
    },

      // Characteristic Value 3
      {
        { ATT_UUID_SIZE, simpleProfilechar3UUID },
        GATT_PERMIT_WRITE,
        0,
        //&simpleProfileChar3//weli old
          simpleProfileChar3//weli new
      },

      // Characteristic 3 User Description
      {
        { ATT_BT_UUID_SIZE, charUserDescUUID },
        GATT_PERMIT_READ,
        0,
        simpleProfileChar3UserDesp
      },

    // Characteristic 4 Declaration
    {
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ,
      0,
      &simpleProfileChar4Props
    },

      // Characteristic Value 4
      {
        { ATT_UUID_SIZE, simpleProfilechar4UUID },
        0,
        0,
        //&simpleProfileChar4//weli old
        simpleProfileChar4//weli new add
      },

      // Characteristic 4 configuration
      {
        { ATT_BT_UUID_SIZE, clientCharCfgUUID },
        GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        0,
        (uint8 *)&simpleProfileChar4Config
      },

      // Characteristic 4 User Description
      {
        { ATT_BT_UUID_SIZE, charUserDescUUID },
        GATT_PERMIT_READ,
        0,
        simpleProfileChar4UserDesp
      },
};
static gattAttribute_t simpleProfileAttrTbl_Security[SERVAPP_NUM_ATTR_SUPPORTED] =
{
  // Simple Profile Service
  {
    { ATT_BT_UUID_SIZE, primaryServiceUUID }, /* type */
    GATT_PERMIT_READ,                         /* permissions */
    0,                                        /* handle */
    (uint8 *)&simpleProfileService            /* pValue */
  },
    // Characteristic 3 Declaration
    {
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_AUTHEN_READ,//GATT_PERMIT_READ,
      0,
      &simpleProfileChar3Props
    },

      // Characteristic Value 3
      {
        { ATT_UUID_SIZE, simpleProfilechar3UUID },
        GATT_PERMIT_AUTHEN_WRITE,//GATT_PERMIT_WRITE,
        0,
        //&simpleProfileChar3//weli old
          simpleProfileChar3//weli new
      },

      // Characteristic 3 User Description
      {
        { ATT_BT_UUID_SIZE, charUserDescUUID },
        GATT_PERMIT_AUTHEN_READ,//GATT_PERMIT_READ,
        0,
        simpleProfileChar3UserDesp
      },

    // Characteristic 4 Declaration
    {
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_AUTHEN_READ,//GATT_PERMIT_READ,
      0,
      &simpleProfileChar4Props
    },

      // Characteristic Value 4
      {
        { ATT_UUID_SIZE, simpleProfilechar4UUID },
        0,
        0,
        //&simpleProfileChar4//weli old
        simpleProfileChar4//weli new add
      },

      // Characteristic 4 configuration
      {
        { ATT_BT_UUID_SIZE, clientCharCfgUUID },
        //GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        GATT_PERMIT_AUTHEN_READ|GATT_PERMIT_AUTHEN_WRITE,
        0,
        (uint8 *)&simpleProfileChar4Config
      },

      // Characteristic 4 User Description
      {
        { ATT_BT_UUID_SIZE, charUserDescUUID },
        GATT_PERMIT_AUTHEN_READ,//GATT_PERMIT_READ,
        0,
        simpleProfileChar4UserDesp
      },
};
#endif // USE_GATT_BUILDER
/*********************************************************************
 * LOCAL FUNCTIONS
 */
static bStatus_t simpleProfile_ReadAttrCB(uint16_t connHandle,
                                          gattAttribute_t *pAttr,
                                          uint8_t *pValue, uint16_t *pLen,
                                          uint16_t offset, uint16_t maxLen,
                                          uint8_t method);
static bStatus_t simpleProfile_WriteAttrCB(uint16_t connHandle,
                                           gattAttribute_t *pAttr,
                                           uint8_t *pValue, uint16_t len,
                                           uint16_t offset, uint8_t method);
#ifndef USE_GATT_BUILDER
/*********************************************************************
 * PROFILE CALLBACKS
 */

// Simple Profile Service Callbacks
// Note: When an operation on a characteristic requires authorization and
// pfnAuthorizeAttrCB is not defined for that characteristic's service, the
// Stack will report a status of ATT_ERR_UNLIKELY to the client.  When an
// operation on a characteristic requires authorization the Stack will call
// pfnAuthorizeAttrCB to check a client's authorization prior to calling
// pfnReadAttrCB or pfnWriteAttrCB, so no checks for authorization need to be
// made within these functions.
CONST gattServiceCBs_t simpleProfileCBs =
{
  simpleProfile_ReadAttrCB,  // Read callback function pointer
  simpleProfile_WriteAttrCB, // Write callback function pointer
  NULL                       // Authorization callback function pointer
};

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      SimpleProfile_AddService
 *
 * @brief   Initializes the Simple Profile service by registering
 *          GATT attributes with the GATT server.
 *
 * @param   services - services to add. This is a bit map and can
 *                     contain more than one service.
 *
 * @return  Success or Failure
 */
bStatus_t SimpleProfile_AddService( uint32 services )
{
  uint8 status;

  // Allocate Client Characteristic Configuration table
  simpleProfileChar4Config = (gattCharCfg_t *)ICall_malloc( sizeof(gattCharCfg_t) *
                                                            MAX_NUM_BLE_CONNS );
  if ( simpleProfileChar4Config == NULL )
  {
    return ( bleMemAllocError );
  }

  // Initialize Client Characteristic Configuration attributes
  GATTServApp_InitCharCfg( LINKDB_CONNHANDLE_INVALID, simpleProfileChar4Config );

  if ( services & SIMPLEPROFILE_SERVICE )
  {
    /*simpleProfileServUUID[0] = LO_UINT16(gServiceUUID);
    simpleProfileServUUID[1] = HI_UINT16(gServiceUUID);
    simpleProfilechar3UUID[0] = LO_UINT16(gWriteUUID);
    simpleProfilechar3UUID[1] = HI_UINT16(gWriteUUID);
    simpleProfilechar4UUID[0] = LO_UINT16(gNotifyUUID);
    simpleProfilechar4UUID[1] = HI_UINT16(gNotifyUUID);*/
    int i=0;
    for(i=0;i<16;i++)
    {
      simpleProfileServUUID[i] = gServiceUUID128bits[i];
      simpleProfilechar3UUID[i] = gWriteUUID128bits[i];
      simpleProfilechar4UUID[i] = gNotifyUUID128bits[i];
    }
    /*uint8 writeUUID[ATT_BT_UUID_SIZE] =
    {
      LO_UINT16(gWriteUUID), HI_UINT16(gWriteUUID)
    };
    gattAttrType_t t={ATT_BT_UUID_SIZE, writeUUID};
    simpleProfileAttrTbl_Security[2].type =  t;*/
    // Register GATT attribute list and CBs with GATT Server App
    if(gPairEnable==1)
    {
      status = GATTServApp_RegisterService( simpleProfileAttrTbl_Security,
                                            GATT_NUM_ATTRS( simpleProfileAttrTbl_Security ),
                                            GATT_MAX_ENCRYPT_KEY_SIZE,
                                            &simpleProfileCBs );
    }
    else
    {
      status = GATTServApp_RegisterService( simpleProfileAttrTbl,
                                            GATT_NUM_ATTRS( simpleProfileAttrTbl ),
                                            GATT_MAX_ENCRYPT_KEY_SIZE,
                                            &simpleProfileCBs );
    }
  }
  else
  {
    status = SUCCESS;
  }

  return ( status );
}

/*********************************************************************
 * @fn      SimpleProfile_RegisterAppCBs
 *
 * @brief   Registers the application callback function. Only call
 *          this function once.
 *
 * @param   callbacks - pointer to application callbacks.
 *
 * @return  SUCCESS or bleAlreadyInRequestedMode
 */
bStatus_t SimpleProfile_RegisterAppCBs( simpleProfileCBs_t *appCallbacks )
{
  if ( appCallbacks )
  {
    simpleProfile_AppCBs = appCallbacks;

    return ( SUCCESS );
  }
  else
  {
    return ( bleAlreadyInRequestedMode );
  }
}

/*********************************************************************
 * @fn      SimpleProfile_SetParameter
 *
 * @brief   Set a Simple Profile parameter.
 *
 * @param   param - Profile parameter ID
 * @param   len - length of data to write
 * @param   value - pointer to data to write.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate
 *          data type (example: data type of uint16 will be cast to
 *          uint16 pointer).
 *
 * @return  bStatus_t
 */
bStatus_t SimpleProfile_SetParameter( uint8 param, uint8 len, void *value )
{
  bStatus_t ret = SUCCESS;
  switch ( param )
  {
      case SIMPLEPROFILE_CHAR3:
        if ( len == SIMPLEPROFILE_CHAR3_LEN )
        {
          VOID osal_memcpy(simpleProfileChar3,  value, SIMPLEPROFILE_CHAR3_LEN );
        }
        else
        {
          ret = bleInvalidRange;
        }
        break;

    case SIMPLEPROFILE_CHAR4:
      if (len>0 && len <= SIMPLEPROFILE_CHAR4_LEN )
      {
        VOID osal_memcpy(simpleProfileChar4,  value, len );
        gNotify_Length = len;

        // See if Notification has been enabled
        if(gPairEnable==1)
        {
          GATTServApp_ProcessCharCfg( simpleProfileChar4Config, simpleProfileChar4, FALSE,
                                      simpleProfileAttrTbl_Security, GATT_NUM_ATTRS( simpleProfileAttrTbl_Security ),
                                      INVALID_TASK_ID, simpleProfile_ReadAttrCB );
        }
        else
        {
          GATTServApp_ProcessCharCfg( simpleProfileChar4Config, simpleProfileChar4, FALSE,
                                      simpleProfileAttrTbl, GATT_NUM_ATTRS( simpleProfileAttrTbl ),
                                      INVALID_TASK_ID, simpleProfile_ReadAttrCB );
        }
      }
      else
      {
        ret = bleInvalidRange;
      }
      break;

    default:
      ret = INVALIDPARAMETER;
      break;
  }

  return ( ret );
}

/*********************************************************************
 * @fn      SimpleProfile_GetParameter
 *
 * @brief   Get a Simple Profile parameter.
 *
 * @param   param - Profile parameter ID
 * @param   value - pointer to data to put.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate
 *          data type (example: data type of uint16 will be cast to
 *          uint16 pointer).
 *
 * @return  bStatus_t
 */
bStatus_t SimpleProfile_GetParameter( uint8 param, void *value )
{
  bStatus_t ret = SUCCESS;
  switch ( param )
  {

    case SIMPLEPROFILE_CHAR3:
      //*((uint8*)value) = simpleProfileChar3;//weli old
      VOID memcpy( value, simpleProfileChar3, /*strlen(simpleProfileChar3)*/ gWriteUART_Length );//weli new
      memset(simpleProfileChar3,'\0',SIMPLEPROFILE_CHAR3_LEN);
      break;

    case SIMPLEPROFILE_CHAR4:
      //*((uint8*)value) = simpleProfileChar4;//weli old
      VOID memcpy( value, simpleProfileChar4, gNotify_Length );//weli new
      break;
    default:
      ret = INVALIDPARAMETER;
      break;
  }

  return ( ret );
}
#endif // USE_GATT_BUILDER
/*********************************************************************
 * @fn          simpleProfile_ReadAttrCB
 *
 * @brief       Read an attribute.
 *
 * @param       connHandle - connection message was received on
 * @param       pAttr - pointer to attribute
 * @param       pValue - pointer to data to be read
 * @param       pLen - length of data to be read
 * @param       offset - offset of the first octet to be read
 * @param       maxLen - maximum length of data to be read
 * @param       method - type of read message
 *
 * @return      SUCCESS, blePending or Failure
 */
static bStatus_t simpleProfile_ReadAttrCB(uint16_t connHandle,
                                          gattAttribute_t *pAttr,
                                          uint8_t *pValue, uint16_t *pLen,
                                          uint16_t offset, uint16_t maxLen,
                                          uint8_t method)
{
  bStatus_t status = SUCCESS;

  // Make sure it's not a blob operation (no attributes in the profile are long)
  if ( offset > 0 )
  {
    return ( ATT_ERR_ATTR_NOT_LONG );
  }

  if ( pAttr->type.len == ATT_UUID_SIZE )
  {
#if 0
    // 16-bit UUID
    uint16 uuid = BUILD_UINT16( pAttr->type.uuid[0], pAttr->type.uuid[1]);
    if(uuid == gNotifyUUID)//add by weli for modify uuid can't not notify 
      uuid = SIMPLEPROFILE_CHAR4_UUID;//add by weli for modify uuid can't not notify
    switch ( uuid )
    {
      // No need for "GATT_SERVICE_UUID" or "GATT_CLIENT_CHAR_CFG_UUID" cases;
      // gattserverapp handles those reads

      // characteristics 1 and 2 have read permissions
      // characteritisc 3 does not have read permissions; therefore it is not
      //   included here
      // characteristic 4 does not have read permissions, but because it
      //   can be sent as a notification, it is included here
      case SIMPLEPROFILE_CHAR4_UUID://add by weli notify 
        *pLen = gNotify_Length;
        VOID memcpy( pValue, pAttr->pValue, gNotify_Length );
        break;

      default:
        // Should never get here! (characteristics 3 and 4 do not have read permissions)
        *pLen = 0;
        status = ATT_ERR_ATTR_NOT_FOUND;
        break;
    }
#else 
    uint8_t i=0;
    for(i=0;i<16;i++)
    {
      if(pAttr->type.uuid[i] != gNotifyUUID128bits[i])
        break;
    }
    if(i>=16)
    {
      *pLen = gNotify_Length;
      VOID memcpy( pValue, pAttr->pValue, gNotify_Length );
    }
    else
    {
      *pLen = 0;
      status = ATT_ERR_ATTR_NOT_FOUND;
    }
  #endif
  }
  else
  {
    // 128-bit UUID
    *pLen = 0;
    status = ATT_ERR_INVALID_HANDLE;
  }

  return ( status );
}

/*********************************************************************
 * @fn      simpleProfile_WriteAttrCB
 *
 * @brief   Validate attribute data prior to a write operation
 *
 * @param   connHandle - connection message was received on
 * @param   pAttr - pointer to attribute
 * @param   pValue - pointer to data to be written
 * @param   len - length of data
 * @param   offset - offset of the first octet to be written
 * @param   method - type of write message
 *
 * @return  SUCCESS, blePending or Failure
 */
static bStatus_t simpleProfile_WriteAttrCB(uint16_t connHandle,
                                           gattAttribute_t *pAttr,
                                           uint8_t *pValue, uint16_t len,
                                           uint16_t offset, uint8_t method)
{
  bStatus_t status = SUCCESS;
  uint8 notifyApp = 0xFF;

  if ( pAttr->type.len == ATT_BT_UUID_SIZE )
  {
    // 16-bit UUID
    uint16 uuid = BUILD_UINT16( pAttr->type.uuid[0], pAttr->type.uuid[1]);
    if(uuid == gWriteUUID)//add by weli for modify uuid can't not write
      uuid = SIMPLEPROFILE_CHAR3_UUID;//add by weli for modify uuid can't not write
    switch ( uuid )
    {
#if 0
      case SIMPLEPROFILE_CHAR3_UUID:
        if ( status == SUCCESS )

        {  

        VOID osal_memcpy( pAttr->pValue, pValue, /*strlen(pValue)*/ /*SIMPLEPROFILE_CHAR3_LEN*/ len );
          gWriteUART_Length=len;

        notifyApp = SIMPLEPROFILE_CHAR3;

        }

        break;
#endif
      case GATT_CLIENT_CHAR_CFG_UUID:
        status = GATTServApp_ProcessCCCWriteReq( connHandle, pAttr, pValue, len,
                                                 offset, GATT_CLIENT_CFG_NOTIFY );
        break;

      default:
        // Should never get here! (characteristics 2 and 4 do not have write permissions)
        status = ATT_ERR_ATTR_NOT_FOUND;
        break;
    }
  }
  else
  {
    // 128-bit UUID
    //status = ATT_ERR_INVALID_HANDLE;
    uint8_t i=0;
    for(i=0;i<16;i++)
    {
      if(pAttr->type.uuid[i] != gWriteUUID128bits[i])
        break;
    }
    if(i>=16)
    {
      if ( status == SUCCESS )
      {  
        VOID osal_memcpy( pAttr->pValue, pValue, /*strlen(pValue)*/ /*SIMPLEPROFILE_CHAR3_LEN*/ len );
          gWriteUART_Length=len;
        notifyApp = SIMPLEPROFILE_CHAR3;
      }
    }
    else
    {
      status = ATT_ERR_ATTR_NOT_FOUND;
    }
  }

  // If a characteristic value changed then callback function to notify application of change
  if ( (notifyApp != 0xFF ) && simpleProfile_AppCBs && simpleProfile_AppCBs->pfnSimpleProfileChange )
  {
    simpleProfile_AppCBs->pfnSimpleProfileChange( notifyApp );
  }

  return ( status );
}

/*********************************************************************
*********************************************************************/
