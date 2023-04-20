/******************************************************************************
//
//                       _oo0oo_
//                      o8888888o
//                      88" . "88
//                      (| -_- |)
//                      0\  =  /0
//                    ___/`---'\___
//                  .' \\|     |// '.
//                 / \\|||  :  |||// \
//                / _||||| -:- |||||- \
//               |   | \\\  -  /// |   |
//               | \_|  ''\---/''  |_/ |
//               \  .-\__  '-'  ___/-. /
//             ___'. .'  /--.--\  `. .'___
//          ."" '<  `.___\_<|>_/___.' >' "".
//         | | :  `- \`.;`\ _ /`;.`/ - ` : | |
//         \  \ `_.   \_ __\ /__ _/   .-` /  /
//     =====`-.____`.___ \_____/___.-`___.-'=====
//                       `=---='
//
//
//     ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//               佛祖保佑         永無BUG
//
//
//

******************************************************************************


*****************************************************************************/

/*********************************************************************
* INCLUDES
*/
#include <string.h>
#include <stdio.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Queue.h>
#include <ti/display/Display.h>
#include <ti/sysbios/knl/Semaphore.h>//add by weli
#include <ti/sysbios/BIOS.h>//add by weli
#include <ti/drivers/dpl/SystemP.h>//add by weli

#if !(defined __TI_COMPILER_VERSION__) && !(defined __GNUC__)
#include <intrinsics.h>
#endif

#include <ti/drivers/utils/List.h>

#include <icall.h>
#include "util.h"
#include <bcomdef.h>
/* This Header file contains all BLE API and icall structure definition */
#include <icall_ble_api.h>

#include <devinfoservice.h>
#include <simple_gatt_profile.h>

#include <ti_drivers_config.h>
#include <board_key.h>

//#include <menu/two_btn_menu.h>

#include "ti_ble_config.h"
//#include "multi_role_menu.h"
#include "multi_role.h"

/*********************************************************************
 * Add by weli begin for include header file
 */
#include "bjja_lm_utility.h"
#include "bjja_lm_uart.h"
#include "bjja_lm_uart2.h"
#include "osal_snv.h"//add by weli
//#define FACTORY 1
#define SNV_ID_BJJA_SAVE 0x85
#define BUF_OF_BJJA_SAVE_LEN 200
#define SNV_ID_BJJA_FLASH 0x86
#define BUF_OF_BJJA_FLASH_LEN 200
/*********************************************************************
 * Add by weli end
 */


/*********************************************************************
 * MACROS
 */

/*********************************************************************
* CONSTANTS
*/

// Application events
#define MR_EVT_CHAR_CHANGE         1
#define MR_EVT_KEY_CHANGE          2
#define MR_EVT_ADV_REPORT          3
#define MR_EVT_SCAN_ENABLED        4
#define MR_EVT_SCAN_DISABLED       5
#define MR_EVT_SVC_DISC            6
#define MR_EVT_ADV                 7
#define MR_EVT_PAIRING_STATE       8
#define MR_EVT_PASSCODE_NEEDED     9
#define MR_EVT_SEND_PARAM_UPDATE   10
#define MR_EVT_PERIODIC            11
#define MR_EVT_READ_RPA            12
#define MR_EVT_INSUFFICIENT_MEM    13
#define MR_EVT_OBDD_PERIODIC       14//add by weli
#define SBP_UART_INCOMING_EVT      15//add by weli
#define SBP_UART2_INCOMING_EVT      16//add by weli
#define BJJA_LM_Notify_DATA_EVT     17//add by weli

// Internal Events for RTOS application
#define MR_ICALL_EVT                         ICALL_MSG_EVENT_ID // Event_Id_31
#define MR_QUEUE_EVT                         UTIL_QUEUE_EVENT_ID // Event_Id_30

#define MR_ALL_EVENTS                        (MR_ICALL_EVT           | \
                                              MR_QUEUE_EVT)

// Supervision timeout conversion rate to miliseconds
#define CONN_TIMEOUT_MS_CONVERSION            10

// Task configuration
#define MR_TASK_PRIORITY                     1
#ifndef MR_TASK_STACK_SIZE
#define MR_TASK_STACK_SIZE                   20480//10240
#endif

#define MR_TASK_STACK_SUBG_SIZE                   2048

// Discovery states
typedef enum {
  BLE_DISC_STATE_IDLE,                // Idle
  BLE_DISC_STATE_MTU,                 // Exchange ATT MTU size
  BLE_DISC_STATE_SVC,                 // Service discovery
  BLE_DISC_STATE_CHAR                 // Characteristic discovery
} discState_t;


// address string length is an ascii character for each digit +
#define MR_ADDR_STR_SIZE     15

// How often to perform periodic event (in msec)
#define MR_PERIODIC_EVT_PERIOD               1000

#define CONNINDEX_INVALID  0xFF

// Spin if the expression is not true
#define MULTIROLE_ASSERT(expr) if (!(expr)) multi_role_spin();

/*********************************************************************
* TYPEDEFS
*/

// App event passed from profiles.
typedef struct
{
  uint8_t event;    // event type
  void *pData;   // event data pointer
} mrEvt_t;

// Container to store paring state info when passing from gapbondmgr callback
// to app event. See the pfnPairStateCB_t documentation from the gapbondmgr.h
// header file for more information on each parameter.
typedef struct
{
  uint8_t state;
  uint16_t connHandle;
  uint8_t status;
} mrPairStateData_t;

// Container to store passcode data when passing from gapbondmgr callback
// to app event. See the pfnPasscodeCB_t documentation from the gapbondmgr.h
// header file for more information on each parameter.
typedef struct
{
  uint8_t deviceAddr[B_ADDR_LEN];
  uint16_t connHandle;
  uint8_t uiInputs;
  uint8_t uiOutputs;
  uint32_t numComparison;
} mrPasscodeData_t;

// Scanned device information record
typedef struct
{
  uint8_t addrType;         // Peer Device's Address Type
  uint8_t addr[B_ADDR_LEN]; // Peer Device Address
} scanRec_t;

// Container to store information from clock expiration using a flexible array
// since data is not always needed
typedef struct
{
  uint8_t event;
  uint8_t data[];
} mrClockEventData_t;

// Container to store advertising event data when passing from advertising
// callback to app event. See the respective event in GapAdvScan_Event_IDs
// in gap_advertiser.h for the type that pBuf should be cast to.
typedef struct
{
  uint32_t event;
  void *pBuf;
} mrGapAdvEventData_t;

typedef struct 
{
  uint8_t data_len;
  uint8_t pBuf[255];
} bjja_lm_uart_data_t;

// List element for parameter update and PHY command status lists
typedef struct
{
  List_Elem elem;
  uint16_t  connHandle;
} mrConnHandleEntry_t;

// Connected device information
typedef struct
{
  uint16_t              connHandle;           // Connection Handle
  mrClockEventData_t*   pParamUpdateEventData;// pointer to paramUpdateEventData
  uint16_t              charHandle;           // Characteristic Handle
  uint8_t               addr[B_ADDR_LEN];     // Peer Device Address
  Clock_Struct*         pUpdateClock;         // pointer to clock struct
  uint8_t               discState;            // Per connection deiscovery state
  uint16_t gNotify_charHdl;
} mrConnRec_t;

/*********************************************************************
* GLOBAL VARIABLES
*/

// Display Interface
//Display_Handle dispHandle = NULL;

/*********************************************************************
* LOCAL VARIABLES
*/

#define APP_EVT_EVENT_MAX  0x13
char *appEventStrings[] = {
  "APP_ZERO             ",
  "APP_CHAR_CHANGE      ",
  "APP_KEY_CHANGE       ",
  "APP_ADV_REPORT       ",
  "APP_SCAN_ENABLED     ",
  "APP_SCAN_DISABLED    ",
  "APP_SVC_DISC         ",
  "APP_ADV              ",
  "APP_PAIRING_STATE    ",
  "APP_SEND_PARAM_UPDATE",
  "APP_PERIODIC         ",
  "APP_READ_RPA         ",
  "APP_INSUFFICIENT_MEM ",
};

/*********************************************************************
* LOCAL VARIABLES
*/

// Entity ID globally used to check for source and/or destination of messages
static ICall_EntityID selfEntity;

// Event globally used to post local events and pend on system and
// local events.
static ICall_SyncHandle syncEvent;

// Clock instances for internal periodic events.
static Clock_Struct clkPeriodic;
// Clock instance for RPA read events.
static Clock_Struct clkRpaRead;

// Memory to pass periodic event to clock handler
mrClockEventData_t periodicUpdateData =
{
  .event = MR_EVT_PERIODIC
};

// Memory to pass RPA read event ID to clock handler
mrClockEventData_t argRpaRead =
{
  .event = MR_EVT_READ_RPA
};

// Queue object used for app messages
static Queue_Struct appMsg;
static Queue_Handle appMsgQueue;

// Task configuration
Task_Struct mrTask;
#if defined __TI_COMPILER_VERSION__
#pragma DATA_ALIGN(mrTaskStack, 8)
#else
#pragma data_alignment=8
#endif
uint8_t mrTaskStack[MR_TASK_STACK_SIZE];

// Maximim PDU size (default = 27 octets)
static uint16 mrMaxPduSize;

#if (DEFAULT_DEV_DISC_BY_SVC_UUID == TRUE)
// Number of scan results filtered by Service UUID
static uint8_t numScanRes = 0;

// Scan results filtered by Service UUID
static scanRec_t scanList[DEFAULT_MAX_SCAN_RES];
scanRec_t gOBDII_dev;
#endif // DEFAULT_DEV_DISC_BY_SVC_UUID

// Discovered service start and end handle
static uint16_t svcStartHdl = 0;
static uint16_t svcEndHdl = 0;

// Value to write
static uint8_t charVal = 0;

// Number of connected devices
static uint8_t numConn = 0;

// Connection handle of current connection
static uint16_t mrConnHandle = LINKDB_CONNHANDLE_INVALID;

// List to store connection handles for queued param updates
static List_List paramUpdateList;

// Per-handle connection info
static mrConnRec_t connList[MAX_NUM_BLE_CONNS];

// Advertising handles
static uint8 advHandle;

static bool mrIsAdvertising = false;
// Address mode
static GAP_Addr_Modes_t addrMode = DEFAULT_ADDRESS_MODE;

// Current Random Private Address
static uint8 rpa[B_ADDR_LEN] = {0};

// Initiating PHY
static uint8_t mrInitPhy = INIT_PHY_1M;


/**************add by weli begin for json function***********************/
#include <ti/utils/json/json.h>
#include <stdint.h>
#include <stddef.h>
#define MQTT_PERIODIC_UPLOAD_SCHEMA     \
"{"                                     \
  "\"commandId\": string,"              \
  "\"command\": string,"                \
  "\"payload\": string"                 \
"}"
#define MQTT_DOWNLINK_SCHEMA            \
"{"                                     \
  "\"commandId\": string,"              \
  "\"command\": string,"                \
  "\"uuid\":    string,"                \
  "\"externalId\": string,"             \
  "\"password\": string,"               \
  "\"originalCommand\": string,"        \
  "\"payload\": string"                 \
"}"

#define MQTT_UPLINK_REQUEST_SCHEMA      \
"{"                                     \
  "\"commandId\": string,"              \
  "\"command\": string,"                \
  "\"uuid\":    string,"                \
  "\"externalId\": string"             \
"}"

#define BLE_RESPONSE_SCHEMA             \
"{"                                     \
  "\"commandId\": string,"              \
  "\"command\": string,"                \
  "\"externalId\": string,"             \
  "\"payload\": string"                \
"}"


#define REQUEST_SCHEMA                  \
"{"                                     \
  "\"commandId\": string,"              \
  "\"command\": string,"                \
  "\"uuid\":    string,"                \
  "\"externalId\": string,"             \
  "\"password\": string"                \
"}"
#define AUTHORIZE_SCHEMA                \
"{"                                     \
  "\"commandId\": string,"              \
  "\"command\": string,"                \
  "\"originalCommand\": string,"        \
  "\"uuid\": string,"                   \
  "\"externalId\": string"              \
"}"
#define RESPONSE_SCHEMA                 \
"{"                                     \
  "\"commandId\": string,"              \
  "\"command\": string,"                \
  "\"originalCommand\": string,"        \
  "\"uuid\": string,"                   \
  "\"externalId\": string,"             \
  "\"payload\": string"                 \
"}"
#define RESPONSE_SCHEMA_UPLINK          \
"{"                                     \
  "\"commandId\": string,"              \
  "\"command\": string,"                \
  "\"uuid\": string,"                   \
  "\"externalId\": string,"             \
  "\"payload\": string"                 \
"}"
#define EXAMPLE_OF_REQUEST                              \
"{"                                                     \
  "\"commandId\": \"1q2z3er56\","                       \
  "\"command\": \"GetStatus\","                          \
  "\"externalId\": \"someidsuppliedbyapp\""             \
"}"
#define EXAMPLE_OF_MQTT_PERIODIC_UPLOAD                 \
"{"                                                     \
  "\"commandId\": \"\","                                \
  "\"command\": \"PeriodicStatusResponse\","            \
  "\"payload\": \"Rejected \""                          \
"}"
#define EXAMPLE_PASSWORD_OF_REQUEST                     \
"{"                                                     \
  "\"commandId\": \"1q2z3er56\","                       \
  "\"command\": \"SetBLEPass\","                        \
  "\"externalId\": \"someidsuppliedbyapp\","            \
  "\"password\": \"123456\""                            \
"}"

#define EXAMPLE_OF_AUTHORIZE                            \
"{"                                                     \
  "\"commandId\": \"1q2z3er56\","                       \
  "\"command\": \"Authorize\","                         \
  "\"originalCommand\": \"OpenDoor\","                  \
  "\"uuid\": \"112233445566\","                         \
  "\"externalId\": \"someidsuppliedbyapp\""             \
"}"
#define EXAMPLE_OF_RESPONSE_UPLINK                      \
"{"                                                     \
  "\"commandId\": \"commandId\","                       \
  "\"command\": \"command\","                   \
  "\"uuid\": \"uuid\","                         \
  "\"externalId\": \"externalId\""             \
  "\"payload\": \"payload\""                           \
"}"
#define EXAMPLE_OF_RESPONSE                             \
"{"                                                     \
  "\"commandId\": \"1q2z3er56\","                       \
  "\"command\": \"OpenDoorReponse\","                   \
  "\"payload\": \"Rejected\""                           \
"}"
#define EXAMPLE_AUTHORIZE_OF_RESPONSE                   \
"{"                                                     \
  "\"commandId\": \"1q2z3er56\","                       \
  "\"command\": \"Authorize\","                         \
  "\"originalCommand\": \"OpenDoor\","                  \
  "\"uuid\": \"112233445566\","                         \
  "\"externalId\": \"someidsuppliedbyapp\","            \
  "\"payload\": \"Rejected\""                           \
"}"
uint8_t BJJA_LM_create_json(Json_Handle *hLocalObject,Json_Handle *hLocalTemplate,char *json_schema,char *json_data);
uint8_t BJJA_LM_json_get_token_data(Json_Handle *hLocalObject,uint8_t *ret_data,char *pKey);
uint8_t BJJA_LM_json_set_token_data(Json_Handle *hLocalObject,uint8_t *set_value,char *pKey);
uint8_t BJJA_LM_json_build(Json_Handle *hLocalObject,char *buffer,uint16_t jsonBufSize);
uint8_t BJJA_LM_destory_json(Json_Handle *hTemplate,Json_Handle *hObject);
uint8_t BJJA_LM_Json_cmd_parsing_from_BLE(char *buf);
uint8_t BJJA_LM_Json_cmd_parsing_from_MQTT_downlink_channel(char *buf);
uint8_t BJJA_LM_Json_periodic_upload_mqtt(char *buf);
/****************add by weli end for json function***********************




/*********************************************************************
* LOCAL FUNCTIONS
*/
static void multi_role_init(void);
static void multi_role_scanInit(void);
static void multi_role_advertInit(void);
static void multi_role_taskFxn(UArg a0, UArg a1);

static uint8_t multi_role_processStackMsg(ICall_Hdr *pMsg);
static uint8_t multi_role_processGATTMsg(gattMsgEvent_t *pMsg);
static void multi_role_processAppMsg(mrEvt_t *pMsg);
static void multi_role_processCharValueChangeEvt(uint8_t paramID);
static void multi_role_processGATTDiscEvent(gattMsgEvent_t *pMsg);
static void multi_role_processPasscode(mrPasscodeData_t *pData);
static void multi_role_processPairState(mrPairStateData_t* pairingEvent);
static void multi_role_processGapMsg(gapEventHdr_t *pMsg);
static void multi_role_processParamUpdate(uint16_t connHandle);
static void multi_role_processAdvEvent(mrGapAdvEventData_t *pEventData);

static void multi_role_charValueChangeCB(uint8_t paramID);
static status_t multi_role_enqueueMsg(uint8_t event, void *pData);
static void multi_role_handleKeys(uint8_t keys);
static uint16_t multi_role_getConnIndex(uint16_t connHandle);
static void multi_role_keyChangeHandler(uint8_t keys);
static uint8_t multi_role_addConnInfo(uint16_t connHandle, uint8_t *pAddr,
                                      uint8_t role);
static void multi_role_performPeriodicTask(void);
static void multi_role_clockHandler(UArg arg);
static uint8_t multi_role_clearConnListEntry(uint16_t connHandle);
#if (DEFAULT_DEV_DISC_BY_SVC_UUID == TRUE)
static void multi_role_addScanInfo(uint8_t *pAddr, uint8_t addrType);
static bool multi_role_findSvcUuid(uint16_t uuid, uint8_t *pData,
                                      uint16_t dataLen);
#endif // DEFAULT_DEV_DISC_BY_SVC_UUID
static uint8_t multi_role_removeConnInfo(uint16_t connHandle);
static void multi_role_startSvcDiscovery(void);
#ifndef Display_DISABLE_ALL
static char* multi_role_getConnAddrStr(uint16_t connHandle);
#endif
static void multi_role_advCB(uint32_t event, void *pBuf, uintptr_t arg);
static void multi_role_scanCB(uint32_t evt, void* msg, uintptr_t arg);
static void multi_role_passcodeCB(uint8_t *deviceAddr, uint16_t connHandle,
                                  uint8_t uiInputs, uint8_t uiOutputs, uint32_t numComparison);
static void multi_role_pairStateCB(uint16_t connHandle, uint8_t state,
                                   uint8_t status);
static void multi_role_updateRPA(void);


/*********************************************************************
 * Add by weli begin for function
 */
#define GATT_CLIENT_CFG_NOTIFY                  0x0001 //notify
#define GATT_CLIENT_CFG_INDICATE                0x0002 //indicate
#define BJJA_LM_OBDD_EVT_PERIOD               100
void BJJA_LM_subg_early_init();
static void BJJA_LM_OBDD_performPeriodicTask(void);
void BJJA_LM_subg_semphore_init();
void BJJA_LM_subg_createTask(void);
static void BJJA_LM_subg_taskFxn(UArg a0, UArg a1);
void BJJA_parsing_AT_cmd_send_data(uint8 *pBuf,uint8 pBuf_len);
void BJJA_parsing_AT_cmd_send_data_UART2();
uint8_t BJJA_LM_check_INGI();
uint8_t BJJA_LM_check_DOOR();
bool cansec_doNotify(uint8_t index);
bool cansec_Write2Periphearl(uint8_t index,uint16_t len,char *data);
void BJJA_LM_state_machine_heart_beat();
void BJJA_LM_1S_worker();
void BJJA_LM_read_flash();
void BJJA_LM_write_flash();
void BJJA_LM_load_default_setting();
void BJJA_LM_init();
void BJJA_LM_read_SR_flash();
void BJJA_LM_write_SR_flash();
uint8_t BJJA_ascii2hex(uint8_t val);
int8_t BJJA_LM_find_OBDII_conn_index();
void BJJA_LM_disconnect_OBDII();
void BJJA_LM_AES_init();
void GetMacAddress(uint8 *p_Address);
uint8_t check_ble();
static void BJJA_4G_JOIN();
static void BJJA_mqtt_connect(void);
void parsing_mqtt_return_cmd(uint8_t *data);
void BJJA_reconnect_4G();
void send_mqtt_test_cmd(uint8_t *mylocaldata,uint8 gps_en);
void send_mqtt_cmd(uint8_t *mylocaldata);
uint8_t BJJA_LM_read_gps();
void BJJA_LM_GPS_mode();
void BJJA_LM_4G_mode();
void BJJA_LM_enable_gps();
void BJJA_LM_disable_gps();
static void BJJA_LM_4G_HeartBeat();
void BJJA_LM_4G_early_init();
void BJJA_LM_control_door_function(uint8_t mode);
void BJJA_LM_BLE_INCOMING();
uint8_t BJJA_LM_Early_Entry_DisArm_state();
uint8_t BJJA_LM_Early_Entry_Arm_state();
void BJJA_LM_send_odo_cmd(uint8_t type);//0toyota,1 standard obdii
void BJJA_LM_send_fuel_level_cmd(uint8_t type);//0toyota,1 standard obdii
void BJJA_LM_parsing_OBDII_data(uint8 *data,uint8 len);
#define OBD_MAX_CMD 16
typedef struct
{
  uint8_t timer;
  uint8_t cmd_name[20];
  uint8_t cmd_len;
} OBD2_cmd_t;
uint8_t OBDII_current_index=0x00;
OBD2_cmd_t myOBD[OBD_MAX_CMD];
uint8_t gLastGpsLat[64]={0x00};
uint8_t gLastGpsLat_flag=0x00;
enum STATE_MACHINE {Pwr_on,Idle,Arm,Disarm,Working};
enum OBD_STATE {O_Discover,O_Connect,O_Notify,O_Running,O_Running_Late};



//const static uint8_t sign_cacert[1187]={0x2D,0x2D,0x2D,0x2D,0x2D,0x42,0x45,0x47,0x49,0x4E,0x20,0x43,0x45,0x52,0x54,0x49,0x46,0x49,0x43,0x41,0x54,0x45,0x2D,0x2D,0x2D,0x2D,0x2D,0x0A,0x4D,0x49,0x49,0x44,0x51,0x54,0x43,0x43,0x41,0x69,0x6D,0x67,0x41,0x77,0x49,0x42,0x41,0x67,0x49,0x54,0x42,0x6D,0x79,0x66,0x7A,0x35,0x6D,0x2F,0x6A,0x41,0x6F,0x35,0x34,0x76,0x42,0x34,0x69,0x6B,0x50,0x6D,0x6C,0x6A,0x5A,0x62,0x79,0x6A,0x41,0x4E,0x42,0x67,0x6B,0x71,0x68,0x6B,0x69,0x47,0x39,0x77,0x30,0x42,0x41,0x51,0x73,0x46,0x0A,0x41,0x44,0x41,0x35,0x4D,0x51,0x73,0x77,0x43,0x51,0x59,0x44,0x56,0x51,0x51,0x47,0x45,0x77,0x4A,0x56,0x55,0x7A,0x45,0x50,0x4D,0x41,0x30,0x47,0x41,0x31,0x55,0x45,0x43,0x68,0x4D,0x47,0x51,0x57,0x31,0x68,0x65,0x6D,0x39,0x75,0x4D,0x52,0x6B,0x77,0x46,0x77,0x59,0x44,0x56,0x51,0x51,0x44,0x45,0x78,0x42,0x42,0x62,0x57,0x46,0x36,0x0A,0x62,0x32,0x34,0x67,0x55,0x6D,0x39,0x76,0x64,0x43,0x42,0x44,0x51,0x53,0x41,0x78,0x4D,0x42,0x34,0x58,0x44,0x54,0x45,0x31,0x4D,0x44,0x55,0x79,0x4E,0x6A,0x41,0x77,0x4D,0x44,0x41,0x77,0x4D,0x46,0x6F,0x58,0x44,0x54,0x4D,0x34,0x4D,0x44,0x45,0x78,0x4E,0x7A,0x41,0x77,0x4D,0x44,0x41,0x77,0x4D,0x46,0x6F,0x77,0x4F,0x54,0x45,0x4C,0x0A,0x4D,0x41,0x6B,0x47,0x41,0x31,0x55,0x45,0x42,0x68,0x4D,0x43,0x56,0x56,0x4D,0x78,0x44,0x7A,0x41,0x4E,0x42,0x67,0x4E,0x56,0x42,0x41,0x6F,0x54,0x42,0x6B,0x46,0x74,0x59,0x58,0x70,0x76,0x62,0x6A,0x45,0x5A,0x4D,0x42,0x63,0x47,0x41,0x31,0x55,0x45,0x41,0x78,0x4D,0x51,0x51,0x57,0x31,0x68,0x65,0x6D,0x39,0x75,0x49,0x46,0x4A,0x76,0x0A,0x62,0x33,0x51,0x67,0x51,0x30,0x45,0x67,0x4D,0x54,0x43,0x43,0x41,0x53,0x49,0x77,0x44,0x51,0x59,0x4A,0x4B,0x6F,0x5A,0x49,0x68,0x76,0x63,0x4E,0x41,0x51,0x45,0x42,0x42,0x51,0x41,0x44,0x67,0x67,0x45,0x50,0x41,0x44,0x43,0x43,0x41,0x51,0x6F,0x43,0x67,0x67,0x45,0x42,0x41,0x4C,0x4A,0x34,0x67,0x48,0x48,0x4B,0x65,0x4E,0x58,0x6A,0x0A,0x63,0x61,0x39,0x48,0x67,0x46,0x42,0x30,0x66,0x57,0x37,0x59,0x31,0x34,0x68,0x32,0x39,0x4A,0x6C,0x6F,0x39,0x31,0x67,0x68,0x59,0x50,0x6C,0x30,0x68,0x41,0x45,0x76,0x72,0x41,0x49,0x74,0x68,0x74,0x4F,0x67,0x51,0x33,0x70,0x4F,0x73,0x71,0x54,0x51,0x4E,0x72,0x6F,0x42,0x76,0x6F,0x33,0x62,0x53,0x4D,0x67,0x48,0x46,0x7A,0x5A,0x4D,0x0A,0x39,0x4F,0x36,0x49,0x49,0x38,0x63,0x2B,0x36,0x7A,0x66,0x31,0x74,0x52,0x6E,0x34,0x53,0x57,0x69,0x77,0x33,0x74,0x65,0x35,0x64,0x6A,0x67,0x64,0x59,0x5A,0x36,0x6B,0x2F,0x6F,0x49,0x32,0x70,0x65,0x56,0x4B,0x56,0x75,0x52,0x46,0x34,0x66,0x6E,0x39,0x74,0x42,0x62,0x36,0x64,0x4E,0x71,0x63,0x6D,0x7A,0x55,0x35,0x4C,0x2F,0x71,0x77,0x0A,0x49,0x46,0x41,0x47,0x62,0x48,0x72,0x51,0x67,0x4C,0x4B,0x6D,0x2B,0x61,0x2F,0x73,0x52,0x78,0x6D,0x50,0x55,0x44,0x67,0x48,0x33,0x4B,0x4B,0x48,0x4F,0x56,0x6A,0x34,0x75,0x74,0x57,0x70,0x2B,0x55,0x68,0x6E,0x4D,0x4A,0x62,0x75,0x6C,0x48,0x68,0x65,0x62,0x34,0x6D,0x6A,0x55,0x63,0x41,0x77,0x68,0x6D,0x61,0x68,0x52,0x57,0x61,0x36,0x0A,0x56,0x4F,0x75,0x6A,0x77,0x35,0x48,0x35,0x53,0x4E,0x7A,0x2F,0x30,0x65,0x67,0x77,0x4C,0x58,0x30,0x74,0x64,0x48,0x41,0x31,0x31,0x34,0x67,0x6B,0x39,0x35,0x37,0x45,0x57,0x57,0x36,0x37,0x63,0x34,0x63,0x58,0x38,0x6A,0x4A,0x47,0x4B,0x4C,0x68,0x44,0x2B,0x72,0x63,0x64,0x71,0x73,0x71,0x30,0x38,0x70,0x38,0x6B,0x44,0x69,0x31,0x4C,0x0A,0x39,0x33,0x46,0x63,0x58,0x6D,0x6E,0x2F,0x36,0x70,0x55,0x43,0x79,0x7A,0x69,0x4B,0x72,0x6C,0x41,0x34,0x62,0x39,0x76,0x37,0x4C,0x57,0x49,0x62,0x78,0x63,0x63,0x65,0x56,0x4F,0x46,0x33,0x34,0x47,0x66,0x49,0x44,0x35,0x79,0x48,0x49,0x39,0x59,0x2F,0x51,0x43,0x42,0x2F,0x49,0x49,0x44,0x45,0x67,0x45,0x77,0x2B,0x4F,0x79,0x51,0x6D,0x0A,0x6A,0x67,0x53,0x75,0x62,0x4A,0x72,0x49,0x71,0x67,0x30,0x43,0x41,0x77,0x45,0x41,0x41,0x61,0x4E,0x43,0x4D,0x45,0x41,0x77,0x44,0x77,0x59,0x44,0x56,0x52,0x30,0x54,0x41,0x51,0x48,0x2F,0x42,0x41,0x55,0x77,0x41,0x77,0x45,0x42,0x2F,0x7A,0x41,0x4F,0x42,0x67,0x4E,0x56,0x48,0x51,0x38,0x42,0x41,0x66,0x38,0x45,0x42,0x41,0x4D,0x43,0x0A,0x41,0x59,0x59,0x77,0x48,0x51,0x59,0x44,0x56,0x52,0x30,0x4F,0x42,0x42,0x59,0x45,0x46,0x49,0x51,0x59,0x7A,0x49,0x55,0x30,0x37,0x4C,0x77,0x4D,0x6C,0x4A,0x51,0x75,0x43,0x46,0x6D,0x63,0x78,0x37,0x49,0x51,0x54,0x67,0x6F,0x49,0x4D,0x41,0x30,0x47,0x43,0x53,0x71,0x47,0x53,0x49,0x62,0x33,0x44,0x51,0x45,0x42,0x43,0x77,0x55,0x41,0x0A,0x41,0x34,0x49,0x42,0x41,0x51,0x43,0x59,0x38,0x6A,0x64,0x61,0x51,0x5A,0x43,0x68,0x47,0x73,0x56,0x32,0x55,0x53,0x67,0x67,0x4E,0x69,0x4D,0x4F,0x72,0x75,0x59,0x6F,0x75,0x36,0x72,0x34,0x6C,0x4B,0x35,0x49,0x70,0x44,0x42,0x2F,0x47,0x2F,0x77,0x6B,0x6A,0x55,0x75,0x30,0x79,0x4B,0x47,0x58,0x39,0x72,0x62,0x78,0x65,0x6E,0x44,0x49,0x0A,0x55,0x35,0x50,0x4D,0x43,0x43,0x6A,0x6A,0x6D,0x43,0x58,0x50,0x49,0x36,0x54,0x35,0x33,0x69,0x48,0x54,0x66,0x49,0x55,0x4A,0x72,0x55,0x36,0x61,0x64,0x54,0x72,0x43,0x43,0x32,0x71,0x4A,0x65,0x48,0x5A,0x45,0x52,0x78,0x68,0x6C,0x62,0x49,0x31,0x42,0x6A,0x6A,0x74,0x2F,0x6D,0x73,0x76,0x30,0x74,0x61,0x64,0x51,0x31,0x77,0x55,0x73,0x0A,0x4E,0x2B,0x67,0x44,0x53,0x36,0x33,0x70,0x59,0x61,0x41,0x43,0x62,0x76,0x58,0x79,0x38,0x4D,0x57,0x79,0x37,0x56,0x75,0x33,0x33,0x50,0x71,0x55,0x58,0x48,0x65,0x65,0x45,0x36,0x56,0x2F,0x55,0x71,0x32,0x56,0x38,0x76,0x69,0x54,0x4F,0x39,0x36,0x4C,0x58,0x46,0x76,0x4B,0x57,0x6C,0x4A,0x62,0x59,0x4B,0x38,0x55,0x39,0x30,0x76,0x76,0x0A,0x6F,0x2F,0x75,0x66,0x51,0x4A,0x56,0x74,0x4D,0x56,0x54,0x38,0x51,0x74,0x50,0x48,0x52,0x68,0x38,0x6A,0x72,0x64,0x6B,0x50,0x53,0x48,0x43,0x61,0x32,0x58,0x56,0x34,0x63,0x64,0x46,0x79,0x51,0x7A,0x52,0x31,0x62,0x6C,0x64,0x5A,0x77,0x67,0x4A,0x63,0x4A,0x6D,0x41,0x70,0x7A,0x79,0x4D,0x5A,0x46,0x6F,0x36,0x49,0x51,0x36,0x58,0x55,0x0A,0x35,0x4D,0x73,0x49,0x2B,0x79,0x4D,0x52,0x51,0x2B,0x68,0x44,0x4B,0x58,0x4A,0x69,0x6F,0x61,0x6C,0x64,0x58,0x67,0x6A,0x55,0x6B,0x4B,0x36,0x34,0x32,0x4D,0x34,0x55,0x77,0x74,0x42,0x56,0x38,0x6F,0x62,0x32,0x78,0x4A,0x4E,0x44,0x64,0x32,0x5A,0x68,0x77,0x4C,0x6E,0x6F,0x51,0x64,0x65,0x58,0x65,0x47,0x41,0x44,0x62,0x6B,0x70,0x79,0x0A,0x72,0x71,0x58,0x52,0x66,0x62,0x6F,0x51,0x6E,0x6F,0x5A,0x73,0x47,0x34,0x71,0x35,0x57,0x54,0x50,0x34,0x36,0x38,0x53,0x51,0x76,0x76,0x47,0x35,0x0A,0x2D,0x2D,0x2D,0x2D,0x2D,0x45,0x4E,0x44,0x20,0x43,0x45,0x52,0x54,0x49,0x46,0x49,0x43,0x41,0x54,0x45,0x2D,0x2D,0x2D,0x2D,0x2D};
//const static uint8_t sign_clientcert[1220]={0x2D,0x2D,0x2D,0x2D,0x2D,0x42,0x45,0x47,0x49,0x4E,0x20,0x43,0x45,0x52,0x54,0x49,0x46,0x49,0x43,0x41,0x54,0x45,0x2D,0x2D,0x2D,0x2D,0x2D,0x0A,0x4D,0x49,0x49,0x44,0x57,0x54,0x43,0x43,0x41,0x6B,0x47,0x67,0x41,0x77,0x49,0x42,0x41,0x67,0x49,0x55,0x54,0x70,0x33,0x72,0x32,0x5A,0x39,0x4B,0x45,0x45,0x69,0x65,0x6D,0x75,0x67,0x6D,0x4F,0x31,0x59,0x58,0x43,0x76,0x63,0x45,0x6E,0x76,0x30,0x77,0x44,0x51,0x59,0x4A,0x4B,0x6F,0x5A,0x49,0x68,0x76,0x63,0x4E,0x41,0x51,0x45,0x4C,0x0A,0x42,0x51,0x41,0x77,0x54,0x54,0x46,0x4C,0x4D,0x45,0x6B,0x47,0x41,0x31,0x55,0x45,0x43,0x77,0x78,0x43,0x51,0x57,0x31,0x68,0x65,0x6D,0x39,0x75,0x49,0x46,0x64,0x6C,0x59,0x69,0x42,0x54,0x5A,0x58,0x4A,0x32,0x61,0x57,0x4E,0x6C,0x63,0x79,0x42,0x50,0x50,0x55,0x46,0x74,0x59,0x58,0x70,0x76,0x62,0x69,0x35,0x6A,0x62,0x32,0x30,0x67,0x0A,0x53,0x57,0x35,0x6A,0x4C,0x69,0x42,0x4D,0x50,0x56,0x4E,0x6C,0x59,0x58,0x52,0x30,0x62,0x47,0x55,0x67,0x55,0x31,0x51,0x39,0x56,0x32,0x46,0x7A,0x61,0x47,0x6C,0x75,0x5A,0x33,0x52,0x76,0x62,0x69,0x42,0x44,0x50,0x56,0x56,0x54,0x4D,0x42,0x34,0x58,0x44,0x54,0x49,0x79,0x4D,0x54,0x41,0x78,0x4D,0x7A,0x41,0x78,0x4E,0x44,0x55,0x79,0x0A,0x4F,0x56,0x6F,0x58,0x44,0x54,0x51,0x35,0x4D,0x54,0x49,0x7A,0x4D,0x54,0x49,0x7A,0x4E,0x54,0x6B,0x31,0x4F,0x56,0x6F,0x77,0x48,0x6A,0x45,0x63,0x4D,0x42,0x6F,0x47,0x41,0x31,0x55,0x45,0x41,0x77,0x77,0x54,0x51,0x56,0x64,0x54,0x49,0x45,0x6C,0x76,0x56,0x43,0x42,0x44,0x5A,0x58,0x4A,0x30,0x61,0x57,0x5A,0x70,0x59,0x32,0x46,0x30,0x0A,0x5A,0x54,0x43,0x43,0x41,0x53,0x49,0x77,0x44,0x51,0x59,0x4A,0x4B,0x6F,0x5A,0x49,0x68,0x76,0x63,0x4E,0x41,0x51,0x45,0x42,0x42,0x51,0x41,0x44,0x67,0x67,0x45,0x50,0x41,0x44,0x43,0x43,0x41,0x51,0x6F,0x43,0x67,0x67,0x45,0x42,0x41,0x4D,0x35,0x74,0x4D,0x75,0x52,0x4B,0x41,0x42,0x50,0x55,0x4A,0x69,0x2F,0x42,0x7A,0x52,0x51,0x6D,0x0A,0x52,0x57,0x4A,0x2B,0x74,0x5A,0x6B,0x62,0x71,0x4E,0x32,0x6D,0x66,0x59,0x61,0x75,0x68,0x5A,0x74,0x70,0x6C,0x51,0x61,0x77,0x4D,0x58,0x32,0x56,0x35,0x63,0x6A,0x66,0x46,0x54,0x4A,0x59,0x6B,0x53,0x72,0x75,0x30,0x76,0x71,0x43,0x41,0x62,0x70,0x37,0x4C,0x71,0x52,0x58,0x47,0x37,0x2B,0x34,0x4F,0x32,0x2B,0x47,0x34,0x55,0x42,0x6F,0x0A,0x76,0x4F,0x61,0x64,0x32,0x38,0x62,0x38,0x2B,0x45,0x67,0x63,0x54,0x30,0x70,0x31,0x34,0x6A,0x64,0x2F,0x4D,0x30,0x31,0x51,0x70,0x5A,0x34,0x6D,0x73,0x72,0x58,0x69,0x36,0x6C,0x71,0x35,0x43,0x6D,0x79,0x66,0x42,0x46,0x34,0x58,0x42,0x4F,0x43,0x6C,0x2F,0x49,0x52,0x4B,0x77,0x79,0x2F,0x4D,0x74,0x43,0x56,0x6A,0x6F,0x51,0x6D,0x5A,0x0A,0x49,0x30,0x72,0x36,0x4C,0x32,0x37,0x34,0x66,0x66,0x51,0x53,0x54,0x70,0x46,0x36,0x71,0x56,0x44,0x71,0x65,0x54,0x39,0x79,0x79,0x68,0x64,0x66,0x69,0x71,0x69,0x56,0x70,0x6C,0x53,0x37,0x44,0x76,0x43,0x71,0x38,0x71,0x4A,0x67,0x4C,0x4B,0x76,0x30,0x78,0x31,0x58,0x51,0x52,0x36,0x36,0x61,0x70,0x6B,0x2F,0x6B,0x4C,0x6E,0x6C,0x2F,0x0A,0x64,0x38,0x2F,0x5A,0x55,0x72,0x4C,0x41,0x30,0x76,0x4B,0x45,0x34,0x35,0x71,0x77,0x38,0x36,0x66,0x46,0x78,0x61,0x76,0x75,0x49,0x71,0x62,0x73,0x73,0x62,0x70,0x54,0x43,0x78,0x31,0x47,0x4D,0x61,0x7A,0x37,0x54,0x42,0x2B,0x7A,0x66,0x7A,0x70,0x59,0x43,0x41,0x6F,0x37,0x42,0x62,0x31,0x61,0x34,0x69,0x67,0x37,0x53,0x76,0x53,0x65,0x0A,0x4D,0x48,0x4B,0x49,0x43,0x35,0x50,0x70,0x61,0x56,0x48,0x6D,0x4D,0x59,0x7A,0x61,0x69,0x2F,0x54,0x6D,0x58,0x51,0x75,0x57,0x51,0x34,0x38,0x6F,0x79,0x65,0x49,0x69,0x56,0x33,0x72,0x50,0x77,0x63,0x77,0x35,0x52,0x5A,0x34,0x4E,0x44,0x53,0x4F,0x6A,0x66,0x69,0x59,0x4A,0x45,0x52,0x4D,0x6F,0x54,0x41,0x41,0x5A,0x44,0x75,0x48,0x46,0x0A,0x76,0x4E,0x6B,0x43,0x41,0x77,0x45,0x41,0x41,0x61,0x4E,0x67,0x4D,0x46,0x34,0x77,0x48,0x77,0x59,0x44,0x56,0x52,0x30,0x6A,0x42,0x42,0x67,0x77,0x46,0x6F,0x41,0x55,0x78,0x32,0x72,0x78,0x50,0x61,0x46,0x4E,0x64,0x6F,0x78,0x76,0x59,0x47,0x4F,0x4A,0x2B,0x77,0x30,0x5A,0x73,0x38,0x4C,0x58,0x73,0x78,0x49,0x77,0x48,0x51,0x59,0x44,0x0A,0x56,0x52,0x30,0x4F,0x42,0x42,0x59,0x45,0x46,0x4E,0x38,0x79,0x72,0x6E,0x63,0x51,0x63,0x45,0x2B,0x5A,0x73,0x6B,0x38,0x6C,0x72,0x57,0x56,0x5A,0x2F,0x33,0x47,0x53,0x38,0x43,0x64,0x71,0x4D,0x41,0x77,0x47,0x41,0x31,0x55,0x64,0x45,0x77,0x45,0x42,0x2F,0x77,0x51,0x43,0x4D,0x41,0x41,0x77,0x44,0x67,0x59,0x44,0x56,0x52,0x30,0x50,0x0A,0x41,0x51,0x48,0x2F,0x42,0x41,0x51,0x44,0x41,0x67,0x65,0x41,0x4D,0x41,0x30,0x47,0x43,0x53,0x71,0x47,0x53,0x49,0x62,0x33,0x44,0x51,0x45,0x42,0x43,0x77,0x55,0x41,0x41,0x34,0x49,0x42,0x41,0x51,0x41,0x34,0x46,0x54,0x78,0x71,0x64,0x4D,0x44,0x6C,0x33,0x54,0x38,0x79,0x70,0x44,0x76,0x55,0x63,0x55,0x5A,0x74,0x6E,0x54,0x38,0x33,0x0A,0x36,0x52,0x43,0x6D,0x4D,0x64,0x47,0x57,0x41,0x76,0x42,0x55,0x57,0x4C,0x42,0x70,0x6A,0x6C,0x75,0x76,0x50,0x6E,0x4A,0x6D,0x31,0x48,0x6A,0x4D,0x73,0x61,0x77,0x51,0x4F,0x32,0x47,0x45,0x61,0x32,0x77,0x46,0x51,0x31,0x44,0x59,0x30,0x2B,0x50,0x4A,0x6E,0x44,0x38,0x4D,0x46,0x4F,0x68,0x42,0x68,0x50,0x69,0x39,0x2F,0x4E,0x69,0x45,0x0A,0x34,0x74,0x64,0x31,0x42,0x74,0x6A,0x4E,0x59,0x58,0x4B,0x67,0x44,0x33,0x76,0x63,0x4C,0x6A,0x79,0x34,0x51,0x79,0x50,0x64,0x36,0x6A,0x47,0x39,0x48,0x49,0x4F,0x65,0x41,0x54,0x37,0x4C,0x66,0x70,0x58,0x65,0x46,0x5A,0x35,0x45,0x63,0x38,0x48,0x55,0x67,0x54,0x5A,0x31,0x74,0x64,0x65,0x6D,0x31,0x56,0x51,0x31,0x6B,0x38,0x55,0x70,0x0A,0x68,0x37,0x6E,0x6D,0x32,0x4C,0x69,0x44,0x33,0x31,0x6F,0x5A,0x37,0x4E,0x4D,0x49,0x36,0x6B,0x45,0x6B,0x31,0x63,0x58,0x4B,0x69,0x2B,0x65,0x37,0x75,0x70,0x61,0x2B,0x4E,0x50,0x62,0x6D,0x33,0x50,0x51,0x6F,0x77,0x78,0x78,0x37,0x41,0x76,0x53,0x6E,0x64,0x37,0x61,0x35,0x44,0x4A,0x2B,0x38,0x2B,0x34,0x57,0x53,0x31,0x6F,0x61,0x4A,0x0A,0x6C,0x64,0x74,0x45,0x53,0x5A,0x33,0x59,0x39,0x4B,0x73,0x52,0x4F,0x77,0x36,0x70,0x4A,0x79,0x64,0x78,0x34,0x33,0x32,0x51,0x6D,0x6C,0x47,0x7A,0x2B,0x72,0x66,0x58,0x4D,0x4A,0x6C,0x57,0x66,0x51,0x75,0x58,0x49,0x6E,0x49,0x77,0x31,0x36,0x76,0x41,0x79,0x6F,0x59,0x74,0x37,0x45,0x6F,0x46,0x65,0x55,0x42,0x4C,0x65,0x6F,0x62,0x52,0x0A,0x4B,0x4D,0x49,0x30,0x6C,0x33,0x6C,0x44,0x41,0x70,0x72,0x58,0x42,0x38,0x61,0x73,0x62,0x77,0x6E,0x62,0x76,0x38,0x7A,0x47,0x62,0x65,0x52,0x38,0x71,0x56,0x33,0x6C,0x2B,0x50,0x34,0x39,0x44,0x64,0x6E,0x78,0x51,0x69,0x33,0x32,0x43,0x75,0x34,0x67,0x4E,0x2B,0x4C,0x2F,0x63,0x31,0x72,0x35,0x73,0x38,0x56,0x4C,0x0A,0x2D,0x2D,0x2D,0x2D,0x2D,0x45,0x4E,0x44,0x20,0x43,0x45,0x52,0x54,0x49,0x46,0x49,0x43,0x41,0x54,0x45,0x2D,0x2D,0x2D,0x2D,0x2D,0x0A};
//const static uint8_t sign_private[1675]={0x2D,0x2D,0x2D,0x2D,0x2D,0x42,0x45,0x47,0x49,0x4E,0x20,0x52,0x53,0x41,0x20,0x50,0x52,0x49,0x56,0x41,0x54,0x45,0x20,0x4B,0x45,0x59,0x2D,0x2D,0x2D,0x2D,0x2D,0x0A,0x4D,0x49,0x49,0x45,0x6F,0x77,0x49,0x42,0x41,0x41,0x4B,0x43,0x41,0x51,0x45,0x41,0x7A,0x6D,0x30,0x79,0x35,0x45,0x6F,0x41,0x45,0x39,0x51,0x6D,0x4C,0x38,0x48,0x4E,0x46,0x43,0x5A,0x46,0x59,0x6E,0x36,0x31,0x6D,0x52,0x75,0x6F,0x33,0x61,0x5A,0x39,0x68,0x71,0x36,0x46,0x6D,0x32,0x6D,0x56,0x42,0x72,0x41,0x78,0x66,0x5A,0x58,0x6C,0x0A,0x79,0x4E,0x38,0x56,0x4D,0x6C,0x69,0x52,0x4B,0x75,0x37,0x53,0x2B,0x6F,0x49,0x42,0x75,0x6E,0x73,0x75,0x70,0x46,0x63,0x62,0x76,0x37,0x67,0x37,0x62,0x34,0x62,0x68,0x51,0x47,0x69,0x38,0x35,0x70,0x33,0x62,0x78,0x76,0x7A,0x34,0x53,0x42,0x78,0x50,0x53,0x6E,0x58,0x69,0x4E,0x33,0x38,0x7A,0x54,0x56,0x43,0x6C,0x6E,0x69,0x61,0x79,0x0A,0x74,0x65,0x4C,0x71,0x57,0x72,0x6B,0x4B,0x62,0x4A,0x38,0x45,0x58,0x68,0x63,0x45,0x34,0x4B,0x58,0x38,0x68,0x45,0x72,0x44,0x4C,0x38,0x79,0x30,0x4A,0x57,0x4F,0x68,0x43,0x5A,0x6B,0x6A,0x53,0x76,0x6F,0x76,0x62,0x76,0x68,0x39,0x39,0x42,0x4A,0x4F,0x6B,0x58,0x71,0x70,0x55,0x4F,0x70,0x35,0x50,0x33,0x4C,0x4B,0x46,0x31,0x2B,0x4B,0x0A,0x71,0x4A,0x57,0x6D,0x56,0x4C,0x73,0x4F,0x38,0x4B,0x72,0x79,0x6F,0x6D,0x41,0x73,0x71,0x2F,0x54,0x48,0x56,0x64,0x42,0x48,0x72,0x70,0x71,0x6D,0x54,0x2B,0x51,0x75,0x65,0x58,0x39,0x33,0x7A,0x39,0x6C,0x53,0x73,0x73,0x44,0x53,0x38,0x6F,0x54,0x6A,0x6D,0x72,0x44,0x7A,0x70,0x38,0x58,0x46,0x71,0x2B,0x34,0x69,0x70,0x75,0x79,0x78,0x0A,0x75,0x6C,0x4D,0x4C,0x48,0x55,0x59,0x78,0x72,0x50,0x74,0x4D,0x48,0x37,0x4E,0x2F,0x4F,0x6C,0x67,0x49,0x43,0x6A,0x73,0x46,0x76,0x56,0x72,0x69,0x4B,0x44,0x74,0x4B,0x39,0x4A,0x34,0x77,0x63,0x6F,0x67,0x4C,0x6B,0x2B,0x6C,0x70,0x55,0x65,0x59,0x78,0x6A,0x4E,0x71,0x4C,0x39,0x4F,0x5A,0x64,0x43,0x35,0x5A,0x44,0x6A,0x79,0x6A,0x4A,0x0A,0x34,0x69,0x4A,0x58,0x65,0x73,0x2F,0x42,0x7A,0x44,0x6C,0x46,0x6E,0x67,0x30,0x4E,0x49,0x36,0x4E,0x2B,0x4A,0x67,0x6B,0x52,0x45,0x79,0x68,0x4D,0x41,0x42,0x6B,0x4F,0x34,0x63,0x57,0x38,0x32,0x51,0x49,0x44,0x41,0x51,0x41,0x42,0x41,0x6F,0x49,0x42,0x41,0x51,0x43,0x5A,0x54,0x6B,0x41,0x64,0x69,0x31,0x66,0x44,0x59,0x69,0x74,0x36,0x0A,0x44,0x46,0x52,0x69,0x51,0x6F,0x6F,0x46,0x50,0x46,0x56,0x69,0x41,0x45,0x6A,0x4A,0x56,0x48,0x79,0x6C,0x4B,0x62,0x66,0x51,0x55,0x2F,0x6C,0x35,0x6E,0x69,0x45,0x6A,0x51,0x39,0x41,0x44,0x2F,0x71,0x6D,0x66,0x57,0x6D,0x64,0x31,0x6D,0x79,0x6A,0x56,0x49,0x76,0x68,0x6C,0x70,0x6C,0x5A,0x64,0x64,0x74,0x51,0x45,0x37,0x71,0x34,0x31,0x0A,0x68,0x64,0x61,0x45,0x48,0x30,0x55,0x72,0x67,0x4E,0x46,0x59,0x56,0x30,0x65,0x4E,0x52,0x6E,0x6B,0x63,0x73,0x36,0x2F,0x74,0x78,0x32,0x6F,0x79,0x59,0x56,0x4B,0x65,0x77,0x64,0x36,0x33,0x64,0x6D,0x37,0x57,0x4D,0x64,0x61,0x73,0x46,0x4F,0x30,0x4A,0x63,0x38,0x38,0x4C,0x69,0x44,0x71,0x68,0x68,0x57,0x53,0x77,0x65,0x7A,0x62,0x50,0x0A,0x44,0x63,0x37,0x72,0x63,0x65,0x2F,0x6B,0x48,0x79,0x70,0x62,0x48,0x2F,0x46,0x71,0x38,0x71,0x32,0x6E,0x48,0x5A,0x48,0x68,0x70,0x6B,0x47,0x6C,0x2F,0x43,0x4D,0x4E,0x6D,0x30,0x2F,0x46,0x61,0x43,0x4F,0x6C,0x56,0x62,0x38,0x38,0x71,0x58,0x2B,0x76,0x42,0x6F,0x44,0x32,0x74,0x2F,0x64,0x4C,0x50,0x35,0x66,0x67,0x66,0x68,0x4E,0x76,0x0A,0x2B,0x4A,0x48,0x35,0x6E,0x4B,0x73,0x64,0x6D,0x5A,0x4E,0x54,0x34,0x6C,0x42,0x46,0x2F,0x4D,0x77,0x32,0x33,0x49,0x31,0x6A,0x4D,0x54,0x43,0x52,0x53,0x43,0x51,0x56,0x68,0x62,0x57,0x45,0x79,0x65,0x73,0x5A,0x58,0x64,0x38,0x71,0x76,0x32,0x6C,0x73,0x76,0x59,0x34,0x39,0x34,0x6E,0x58,0x44,0x6E,0x4F,0x69,0x5A,0x63,0x39,0x50,0x62,0x0A,0x75,0x5A,0x34,0x42,0x62,0x33,0x36,0x51,0x4B,0x4F,0x72,0x4A,0x6C,0x78,0x59,0x51,0x65,0x36,0x50,0x4D,0x45,0x68,0x70,0x4B,0x68,0x31,0x4A,0x47,0x52,0x43,0x48,0x4D,0x77,0x39,0x63,0x4D,0x6E,0x39,0x72,0x68,0x37,0x58,0x4F,0x72,0x70,0x68,0x39,0x58,0x39,0x79,0x6D,0x35,0x39,0x77,0x36,0x79,0x61,0x65,0x33,0x4D,0x46,0x64,0x57,0x6F,0x0A,0x6C,0x54,0x42,0x6D,0x42,0x44,0x36,0x42,0x41,0x6F,0x47,0x42,0x41,0x50,0x38,0x51,0x42,0x6C,0x71,0x66,0x47,0x50,0x54,0x57,0x54,0x56,0x61,0x61,0x2F,0x66,0x4F,0x61,0x6B,0x30,0x59,0x78,0x46,0x4F,0x4B,0x57,0x4C,0x43,0x6B,0x6B,0x6F,0x6B,0x59,0x38,0x34,0x67,0x78,0x61,0x31,0x4B,0x64,0x76,0x30,0x4D,0x43,0x45,0x73,0x66,0x6C,0x45,0x0A,0x4B,0x50,0x4F,0x4F,0x46,0x68,0x34,0x4E,0x30,0x41,0x61,0x48,0x42,0x6B,0x6D,0x59,0x48,0x30,0x35,0x57,0x39,0x69,0x43,0x39,0x53,0x42,0x2B,0x73,0x47,0x4B,0x37,0x42,0x44,0x36,0x48,0x39,0x53,0x42,0x74,0x33,0x66,0x75,0x74,0x67,0x49,0x6D,0x65,0x46,0x78,0x6E,0x6B,0x45,0x32,0x57,0x74,0x4E,0x34,0x4A,0x41,0x75,0x45,0x68,0x75,0x50,0x0A,0x51,0x45,0x73,0x6C,0x54,0x30,0x66,0x63,0x7A,0x33,0x66,0x47,0x38,0x36,0x6D,0x2B,0x7A,0x75,0x5A,0x50,0x78,0x49,0x53,0x78,0x6B,0x62,0x6A,0x31,0x43,0x4D,0x6F,0x2F,0x50,0x62,0x34,0x57,0x70,0x58,0x53,0x72,0x53,0x56,0x52,0x78,0x6F,0x64,0x55,0x4F,0x57,0x50,0x75,0x35,0x69,0x4F,0x58,0x76,0x41,0x6F,0x47,0x42,0x41,0x4D,0x38,0x76,0x0A,0x61,0x6A,0x4E,0x6D,0x52,0x4D,0x52,0x47,0x46,0x46,0x4B,0x2F,0x4B,0x51,0x39,0x6C,0x57,0x53,0x2F,0x67,0x76,0x56,0x44,0x30,0x69,0x64,0x69,0x77,0x6B,0x4F,0x32,0x4D,0x6F,0x69,0x2B,0x6F,0x52,0x4A,0x2F,0x75,0x6B,0x56,0x46,0x36,0x57,0x32,0x6A,0x65,0x6D,0x47,0x38,0x46,0x4A,0x79,0x37,0x4F,0x70,0x67,0x67,0x74,0x68,0x79,0x41,0x59,0x0A,0x32,0x58,0x4C,0x52,0x52,0x63,0x59,0x41,0x72,0x6E,0x2B,0x79,0x53,0x72,0x46,0x56,0x72,0x74,0x53,0x74,0x76,0x67,0x57,0x65,0x6C,0x69,0x73,0x2B,0x38,0x59,0x7A,0x6D,0x43,0x39,0x50,0x51,0x5A,0x46,0x51,0x4B,0x57,0x38,0x54,0x61,0x49,0x6A,0x51,0x6A,0x4B,0x6B,0x44,0x71,0x4E,0x4C,0x69,0x56,0x49,0x72,0x47,0x78,0x6C,0x52,0x66,0x31,0x0A,0x4C,0x6E,0x48,0x67,0x4C,0x48,0x67,0x4F,0x74,0x65,0x30,0x62,0x44,0x52,0x51,0x30,0x79,0x44,0x73,0x5A,0x6E,0x49,0x6F,0x76,0x34,0x38,0x45,0x50,0x50,0x70,0x5A,0x43,0x4A,0x55,0x51,0x70,0x49,0x35,0x47,0x33,0x41,0x6F,0x47,0x41,0x55,0x71,0x33,0x6A,0x49,0x57,0x55,0x4A,0x4E,0x66,0x52,0x78,0x78,0x57,0x30,0x67,0x66,0x4F,0x4C,0x53,0x0A,0x63,0x71,0x4A,0x65,0x58,0x73,0x54,0x48,0x4D,0x39,0x38,0x49,0x4B,0x7A,0x52,0x35,0x49,0x67,0x41,0x66,0x68,0x74,0x63,0x63,0x47,0x41,0x76,0x72,0x6C,0x52,0x32,0x66,0x47,0x4C,0x51,0x71,0x50,0x7A,0x76,0x43,0x2F,0x78,0x71,0x74,0x30,0x78,0x56,0x59,0x73,0x4A,0x42,0x48,0x34,0x48,0x7A,0x36,0x38,0x43,0x6C,0x64,0x4A,0x75,0x69,0x32,0x0A,0x4A,0x4A,0x42,0x78,0x32,0x31,0x56,0x30,0x38,0x74,0x2B,0x4B,0x78,0x33,0x76,0x35,0x78,0x69,0x6A,0x6F,0x51,0x58,0x78,0x52,0x47,0x75,0x75,0x55,0x4F,0x78,0x4C,0x49,0x69,0x4A,0x4E,0x6A,0x69,0x36,0x76,0x73,0x4A,0x4B,0x74,0x39,0x4F,0x4C,0x7A,0x39,0x58,0x48,0x4C,0x6E,0x42,0x51,0x78,0x36,0x62,0x44,0x59,0x68,0x7A,0x30,0x32,0x49,0x0A,0x75,0x69,0x47,0x6C,0x4B,0x4A,0x69,0x4E,0x67,0x4B,0x34,0x46,0x41,0x34,0x64,0x50,0x47,0x4F,0x6F,0x66,0x6B,0x6B,0x6B,0x43,0x67,0x59,0x41,0x4E,0x72,0x33,0x73,0x62,0x5A,0x42,0x44,0x38,0x79,0x67,0x68,0x44,0x6F,0x76,0x37,0x71,0x56,0x6D,0x35,0x36,0x76,0x43,0x53,0x6C,0x4F,0x56,0x48,0x31,0x72,0x30,0x77,0x54,0x64,0x4F,0x75,0x74,0x0A,0x72,0x44,0x62,0x45,0x50,0x62,0x54,0x35,0x70,0x64,0x52,0x74,0x36,0x2B,0x34,0x7A,0x76,0x79,0x70,0x6B,0x62,0x43,0x41,0x4A,0x67,0x45,0x42,0x68,0x76,0x57,0x4A,0x33,0x74,0x42,0x30,0x67,0x78,0x43,0x44,0x43,0x72,0x4A,0x74,0x45,0x64,0x58,0x31,0x7A,0x37,0x50,0x6F,0x56,0x55,0x76,0x46,0x6D,0x62,0x2B,0x54,0x79,0x77,0x71,0x74,0x62,0x0A,0x56,0x58,0x4F,0x62,0x48,0x59,0x67,0x4D,0x53,0x38,0x42,0x67,0x6F,0x30,0x59,0x43,0x50,0x62,0x59,0x33,0x7A,0x78,0x6F,0x59,0x6C,0x4C,0x74,0x64,0x64,0x73,0x4F,0x58,0x6F,0x42,0x41,0x76,0x36,0x67,0x44,0x59,0x5A,0x61,0x59,0x4B,0x68,0x4B,0x59,0x4A,0x53,0x56,0x72,0x4F,0x77,0x66,0x55,0x51,0x5A,0x70,0x70,0x69,0x77,0x49,0x48,0x48,0x0A,0x71,0x39,0x50,0x2B,0x70,0x77,0x4B,0x42,0x67,0x48,0x44,0x47,0x73,0x53,0x38,0x31,0x53,0x68,0x4E,0x31,0x35,0x47,0x65,0x73,0x36,0x73,0x4F,0x51,0x42,0x66,0x52,0x39,0x57,0x44,0x74,0x6E,0x55,0x2F,0x63,0x31,0x39,0x42,0x75,0x36,0x4F,0x52,0x45,0x62,0x54,0x4F,0x7A,0x6A,0x41,0x44,0x41,0x4F,0x76,0x35,0x75,0x6B,0x42,0x36,0x46,0x59,0x0A,0x6D,0x4B,0x36,0x6A,0x36,0x70,0x76,0x79,0x35,0x39,0x72,0x43,0x77,0x36,0x4A,0x32,0x51,0x73,0x32,0x65,0x49,0x49,0x48,0x48,0x71,0x4A,0x52,0x48,0x73,0x68,0x78,0x2B,0x6C,0x79,0x37,0x6E,0x62,0x58,0x33,0x50,0x72,0x6B,0x6E,0x79,0x6E,0x4A,0x75,0x62,0x73,0x36,0x72,0x5A,0x62,0x57,0x52,0x42,0x77,0x7A,0x6A,0x32,0x30,0x6E,0x72,0x72,0x0A,0x73,0x53,0x71,0x6F,0x64,0x72,0x51,0x72,0x4B,0x75,0x66,0x52,0x48,0x54,0x39,0x4C,0x47,0x51,0x43,0x50,0x53,0x68,0x6A,0x4B,0x76,0x6B,0x57,0x48,0x58,0x2F,0x48,0x43,0x32,0x68,0x71,0x37,0x38,0x2B,0x43,0x6C,0x67,0x39,0x37,0x36,0x4C,0x6F,0x74,0x47,0x4F,0x7A,0x6C,0x71,0x0A,0x2D,0x2D,0x2D,0x2D,0x2D,0x45,0x4E,0x44,0x20,0x52,0x53,0x41,0x20,0x50,0x52,0x49,0x56,0x41,0x54,0x45,0x20,0x4B,0x45,0x59,0x2D,0x2D,0x2D,0x2D,0x2D,0x0A};
static Clock_Struct BJJA_LM_OBDD_clkPeriodic;
Semaphore_Handle gSem;
uint8_t gProduceFlag=0x00;
uint8_t gProduceFlag2=0x00;
uint8_t gService_uuid[16] = {0xf2,0xc3,0xf0,0xae,0xa9,0xfa,0x15,0x8c,0x9d,0x49,0xae,0x73,0x71,0x0a,0x81,0xe7};
uint8_t gNoti_uuid[16]    = {0x9f,0x9f,0x00,0xc1,0x58,0xbd,0x32,0xb6,0x9e,0x4c,0x21,0x9c,0xc9,0xd6,0xf8,0xbe};
uint8_t gWrite_uuid[16]   = {0x9f,0x9f,0x00,0xc1,0x58,0xbd,0x32,0xb6,0x9e,0x4c,0x21,0x9c,0xc9,0xd6,0xf8,0xbe};
//uint8_t gOBDII_mac[6]={0xF5,0x88,0xE2,0x4D,0x5B,0x94};//MAC:F5:88:E2:4D:5B:94
uint8_t gACC_ON_timer_flag=0x00;
uint8_t gArm_Disarm_command=0;//set from lte-m or ble command,to notify state machine try to entry arm or disarm state
uint8_t gWorkingHeartBeat=0x00;
uint8_t WorkingHeartBeatTimer=10;
uint8_t gWaitCount=0x00;
uint8_t gWaitTimer=3;
uint8_t gMac[6]={0x00};
uint8_t gvalid=0x00;
uint8_t gConnTimeout_count=0x00;

uint8_t gPairEnable=0;
uint32_t gServiceUUID=SIMPLEPROFILE_SERV_UUID;
uint32_t gNotifyUUID=SIMPLEPROFILE_CHAR4_UUID;
uint32_t gWriteUUID=SIMPLEPROFILE_CHAR3_UUID;
uint16_t gMaxPacket=150;
uint8_t gDelayTime=150;
extern uint8 gWriteUART_Length;
extern uint8_t gPacket[10];
enum STATE_MACHINE gBJJA_LM_State_machine=Pwr_on;
enum OBD_STATE gBJJA_LM_Obd_state=O_Discover;
SubGpairing_data gSubGpairing_data[8];
BJJM_LM_flash_data gFlash_data;

uint8_t g4GStatus=TELCOMM_STATUS_4G_NON_DETECTED;
#ifdef FACTORY
static uint8_t gStopHB=0x01;
#else
static uint8_t gStopHB=0x00;
#endif
static uint8_t gCsq_val=31;
int8_t gMQTT_received_flag=0x00;
uint8_t gAuto_Ping_count=0x00;
static uint8_t first_4g_flag=0x00;
uint8_t g4G_cmd_flag=0x00;
static uint8_t g4G_connection_retry_count=0x00;
static uint8_t gAutoMode_reconnecting_count=0x00;
static uint8_t gEarly_online_mqtt_reconnect_count=0x00;
static uint8_t gEarly_online_mqtt_reconnect_flag=0x00;
static uint8_t gMQTT_total_retry_count=0;
static uint8_t g4G_detect_retry_count=0x00;
uint16_t g4G_heartbeat=0x00;
uint8_t gCurrent_Notify_id=0x00;
uint8_t gACC_ON_OFF_flag=0x00;
uint32_t gODO_meter=0x00;
uint8_t gDoor_State=0x00;
uint8_t g_previous_door_state=0;
static uint8_t gFirst_boot=0x01;
static uint8_t gSuperUserMode=0x00;
uint8_t gmyi=0;
void PRINT_DATA(char *ptr2, ...)
{
  //return;
  /*if(gmyi<=10)
  {
    gmyi++;
    return;
  }*/

  uint8 data2[255] = { 0 };
  va_list ap2;
  va_start(ap2, ptr2);
  //vsprintf(data2, ptr, ap2);
  SystemP_vsnprintf(data2, 200, ptr2, ap2);
  va_end(ap2);
  UartMessage2(data2, strlen(data2));
  //DELAY_US(1000*20);
}
void SEND_LTE_M(char *ptr, ...)
{
  //return;
  uint8 data[255] = { 0 };
  va_list ap;
  va_start(ap, ptr);
  //vsprintf(data, ptr, ap);
  SystemP_vsnprintf(data, 200, ptr, ap);
  va_end(ap);
  UartMessage(data, strlen(data));
  UartMessage2("Send:", strlen("Send:"));
  UartMessage2(data, strlen(data));
}






Task_Struct gSubgTask;
#if defined __TI_COMPILER_VERSION__
#pragma DATA_ALIGN(mrTaskStack, 8)
#else
#pragma data_alignment=8
#endif
uint8_t gSubgTaskStack[MR_TASK_STACK_SUBG_SIZE];
/*********************************************************************
 * Add by weli end
 */

/*
 *  =============================== AES begin===============================
 */


#include <ti/drivers/AESECB.h>
#include <ti/drivers/cryptoutils/cryptokey/CryptoKeyPlaintext.h>
uint8_t plaintext[] = {0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
                       0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
                       0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
                       0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51};
uint8_t keyingMaterial[16] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                              0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};
// The plaintext should be the following after the decryption operation:
// 0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
// 0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
void ecbCallback(AESECB_Handle handle,
                 int_fast16_t returnValue,
                 AESECB_Operation *operation,
                 AESECB_OperationType operationType)
{
    if (returnValue != AESECB_STATUS_SUCCESS) {
        // handle error
    }
}

void ecbEncrypt(uint8 *in,uint8 *out,uint8 len) 
{
    AESECB_Handle handle;
    AESECB_Params params;
    CryptoKey cryptoKey;
    int_fast16_t decryptionResult;
    AESECB_Operation operation;
    PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
    AESECB_Params_init(&params);
    PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
    //params.returnBehavior = AESECB_RETURN_BEHAVIOR_CALLBACK;
    //params.callbackFxn = ecbCallback;
    handle = AESECB_open(CONFIG_AESECB_0, &params);
    PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
    if (handle == NULL) {
        // handle error
      PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
    }
    PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
    CryptoKeyPlaintext_initKey(&cryptoKey, keyingMaterial, sizeof(keyingMaterial));
    PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
    AESECB_Operation_init(&operation);
    operation.key               = &cryptoKey;
    operation.input             = in;
    operation.output            = out;
    operation.inputLength       = len;

    PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
    decryptionResult = AESECB_oneStepEncrypt(handle, &operation);
    PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
    if (decryptionResult != AESECB_STATUS_SUCCESS) {
        // handle error
      PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
    }
    
    AESECB_close(handle);
}
void ecbDecrypt(uint8 *in,uint8 *out,uint8 len) 
{
    AESECB_Handle handle;
    AESECB_Params params;
    CryptoKey cryptoKey;
    int_fast16_t decryptionResult;
    AESECB_Operation operation;
    PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
    AESECB_Params_init(&params);
    PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
    //params.returnBehavior = AESECB_RETURN_BEHAVIOR_CALLBACK;
    //params.callbackFxn = ecbCallback;
    handle = AESECB_open(CONFIG_AESECB_0, &params);
    PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
    if (handle == NULL) {
        // handle error
      PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
    }
    PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
    CryptoKeyPlaintext_initKey(&cryptoKey, keyingMaterial, sizeof(keyingMaterial));
    PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
    AESECB_Operation_init(&operation);
    operation.key               = &cryptoKey;
    operation.input             = in;
    operation.output            = out;
    operation.inputLength       = len;

    PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
    decryptionResult = AESECB_oneStepDecrypt(handle, &operation);
    PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
    if (decryptionResult != AESECB_STATUS_SUCCESS) {
        // handle error
      PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
    }
    AESECB_close(handle);
}


/*
 *  =============================== AES end===============================
 */



/*
 *  =============================== Watchdog begin===============================
 */
#include <ti/drivers/Watchdog.h>
#include <ti/drivers/watchdog/WatchdogCC26XX.h>
#include <ti/drivers/Watchdog.h>

volatile bool serviceFlag = true;
volatile bool watchdogExpired = false;
Watchdog_Handle watchdogHandle;

void watchdogCallback(uintptr_t unused)
{
    /* Clear watchdog interrupt flag */
    //Watchdog_clear(watchdogHandle);

    watchdogExpired = true;

    /* Insert timeout handling code here. */
    //UartMessage("WDT Timeout",strlen("WDT Timeout"));
    //HCI_EXT_ResetSystemCmd(HCI_EXT_RESET_SYSTEM_HARD);
}
void BJJA_WDT_init()
{
  //return;
  Watchdog_Params params;
  Watchdog_init();
  Watchdog_Params_init(&params);
  params.callbackFxn = (Watchdog_Callback)watchdogCallback;
  params.resetMode = Watchdog_RESET_ON;

  watchdogHandle = Watchdog_open(CONFIG_WATCHDOG_0, &params);
  if (watchdogHandle == NULL) {
      /* Error opening Watchdog */
      while (1);
  }
  uint32_t reloadValue = Watchdog_convertMsToTicks(watchdogHandle, 10000);
  Watchdog_setReload(watchdogHandle, reloadValue);
}
void BJJA_LM_tick_wdt()
{

  //UartMessage("tick wdt2\r\n",strlen("tick wdt2\r\n"));
  //return;
  
  if(gvalid)
    Watchdog_clear(watchdogHandle);
}
/*
 *  =============================== Watchdog end===============================
 */


mrClockEventData_t periodicOBDD =
{
  .event = MR_EVT_OBDD_PERIODIC
};


/*********************************************************************
 * EXTERN FUNCTIONS
*/
extern void AssertHandler(uint8 assertCause, uint8 assertSubcause);

/*********************************************************************
* PROFILE CALLBACKS
*/

// Simple GATT Profile Callbacks
static simpleProfileCBs_t multi_role_simpleProfileCBs =
{
  multi_role_charValueChangeCB // Characteristic value change callback
};

// GAP Bond Manager Callbacks
static gapBondCBs_t multi_role_BondMgrCBs =
{
  multi_role_passcodeCB, // Passcode callback
  multi_role_pairStateCB                  // Pairing state callback
};

/*********************************************************************
* PUBLIC FUNCTIONS
*/

/*********************************************************************
 * @fn      multi_role_spin
 *
 * @brief   Spin forever
 *
 * @param   none
 */
static void multi_role_spin(void)
{
  volatile uint8_t x = 0;

  while(1)
  {
    x++;
  }
}

/*********************************************************************
* @fn      multi_role_createTask
*
* @brief   Task creation function for multi_role.
*
* @param   None.
*
* @return  None.
*/
void multi_role_createTask(void)
{
  Task_Params taskParams;

  // Configure task
  Task_Params_init(&taskParams);
  taskParams.stack = mrTaskStack;
  taskParams.stackSize = MR_TASK_STACK_SIZE;
  taskParams.priority = MR_TASK_PRIORITY;

  Task_construct(&mrTask, multi_role_taskFxn, &taskParams, NULL);
}

/*********************************************************************
* @fn      multi_role_init
*
* @brief   Called during initialization and contains application
*          specific initialization (ie. hardware initialization/setup,
*          table initialization, power up notification, etc), and
*          profile initialization/setup.
*
* @param   None.
*
* @return  None.
*/
static void multi_role_init(void)
{
  BLE_LOG_INT_TIME(0, BLE_LOG_MODULE_APP, "APP : ---- init ", MR_TASK_PRIORITY);
  // Create the menu
  //multi_role_build_menu();
  // ******************************************************************
  // N0 STACK API CALLS CAN OCCUR BEFORE THIS CALL TO ICall_registerApp
  // ******************************************************************
  // Register the current thread as an ICall dispatcher application
  // so that the application can send and receive messages.
  ICall_registerApp(&selfEntity, &syncEvent);

  // Open Display.

  //BJJA_LM_init();

  // Disable all items in the main menu
  //tbm_setItemStatus(&mrMenuMain, MR_ITEM_NONE, MR_ITEM_ALL);

  // Init two button menu
  //tbm_initTwoBtnMenu(dispHandle, &mrMenuMain, 5, multi_role_menuSwitchCb);
  //Display_printf(dispHandle, MR_ROW_SEPARATOR, 0, "====================");

  //clear out connection menu
  //tbm_setItemStatus(&mrMenuConnect, MR_ITEM_NONE, MR_ITEM_ALL);

  // Create an RTOS queue for message from profile to be sent to app.
  appMsgQueue = Util_constructQueue(&appMsg);

  // Create one-shot clock for internal periodic events.
  Util_constructClock(&clkPeriodic, multi_role_clockHandler,
                      MR_PERIODIC_EVT_PERIOD, 0, false,
                      (UArg)&periodicUpdateData);
  Util_constructClock(&BJJA_LM_OBDD_clkPeriodic, multi_role_clockHandler,
                      BJJA_LM_OBDD_EVT_PERIOD, 0, false,
                      (UArg)&periodicOBDD);

  // Init key debouncer
  Board_initKeys(multi_role_keyChangeHandler);

  // Initialize Connection List
  multi_role_clearConnListEntry(LINKDB_CONNHANDLE_ALL);

  // Set the Device Name characteristic in the GAP GATT Service
  // For more information, see the section in the User's Guide:
  // http://software-dl.ti.com/lprf/ble5stack-latest/
  GGS_SetParameter(GGS_DEVICE_NAME_ATT, GAP_DEVICE_NAME_LEN, (void *)attDeviceName);

  // Configure GAP
  {
    uint16_t paramUpdateDecision = DEFAULT_PARAM_UPDATE_REQ_DECISION;

    // Pass all parameter update requests to the app for it to decide
    GAP_SetParamValue(GAP_PARAM_LINK_UPDATE_DECISION, paramUpdateDecision);
  }

  // Set default values for Data Length Extension
  // Extended Data Length Feature is already enabled by default
  {
    // Set initial values to maximum, RX is set to max. by default(251 octets, 2120us)
    // Some brand smartphone is essentially needing 251/2120, so we set them here.
  #define APP_SUGGESTED_PDU_SIZE 251 //default is 27 octets(TX)
  #define APP_SUGGESTED_TX_TIME 2120 //default is 328us(TX)

    // This API is documented in hci.h
    // See the LE Data Length Extension section in the BLE5-Stack User's Guide for information on using this command:
    // http://software-dl.ti.com/lprf/ble5stack-latest/
    HCI_LE_WriteSuggestedDefaultDataLenCmd(APP_SUGGESTED_PDU_SIZE, APP_SUGGESTED_TX_TIME);
  }

  // Initialize GATT Client, used by GAPBondMgr to look for RPAO characteristic for network privacy
  GATT_InitClient();

  // Register to receive incoming ATT Indications/Notifications
  GATT_RegisterForInd(selfEntity);

  // Setup the GAP Bond Manager
  setBondManagerParameters();

  // Initialize GATT attributes
  GGS_AddService(GATT_ALL_SERVICES);           // GAP
  GATTServApp_AddService(GATT_ALL_SERVICES);   // GATT attributes
  //DevInfo_AddService();                        // Device Information Service
  SimpleProfile_AddService(GATT_ALL_SERVICES); // Simple GATT Profile

  // Setup the SimpleProfile Characteristic Values
  // For more information, see the GATT and GATTServApp sections in the User's Guide:
  // http://software-dl.ti.com/lprf/ble5stack-latest/
  {
    uint8_t charValue1 = 1;
    uint8_t charValue2 = 2;
    uint8_t charValue3 = 3;
    uint8_t charValue4 = 4;
    uint8_t charValue5[SIMPLEPROFILE_CHAR5_LEN] = { 1, 2, 3, 4, 5 };

    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR1, sizeof(uint8_t),
                               &charValue1);
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR2, sizeof(uint8_t),
                               &charValue2);
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR3, sizeof(uint8_t),
                               &charValue3);
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4, sizeof(uint8_t),
                               &charValue4);
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR5, SIMPLEPROFILE_CHAR5_LEN,
                               charValue5);
  }

  // Register callback with SimpleGATTprofile
  SimpleProfile_RegisterAppCBs(&multi_role_simpleProfileCBs);

  // Start Bond Manager and register callback
  VOID GAPBondMgr_Register(&multi_role_BondMgrCBs);

  // Register with GAP for HCI/Host messages. This is needed to receive HCI
  // events. For more information, see the HCI section in the User's Guide:
  // http://software-dl.ti.com/lprf/ble5stack-latest/
  GAP_RegisterForMsgs(selfEntity);

  // Register for GATT local events and ATT Responses pending for transmission
  GATT_RegisterForMsgs(selfEntity);

  BLE_LOG_INT_TIME(0, BLE_LOG_MODULE_APP, "APP : ---- call GAP_DeviceInit", GAP_PROFILE_PERIPHERAL | GAP_PROFILE_CENTRAL);
  //Initialize GAP layer for Peripheral and Central role and register to receive GAP events
  GAP_DeviceInit(GAP_PROFILE_PERIPHERAL | GAP_PROFILE_CENTRAL, selfEntity,
                 addrMode, &pRandomAddress);


}
static void BJJA_LM_main_task_Fxn()
{
  
}
/*********************************************************************
* @fn      multi_role_taskFxn
*
* @brief   Application task entry point for the multi_role.
*
* @param   a0, a1 - not used.
*
* @return  None.
*/
static void multi_role_taskFxn(UArg a0, UArg a1)
{
  // Initialize application
  multi_role_init();
  BJJA_LM_init();
  BJJA_LM_subg_early_init();

  // Application main loop
  for (;;)
  {
    uint32_t events;

    // Waits for an event to be posted associated with the calling thread.
    // Note that an event associated with a thread is posted when a
    // message is queued to the message receive queue of the thread
    events = Event_pend(syncEvent, Event_Id_NONE, MR_ALL_EVENTS,
                        ICALL_TIMEOUT_FOREVER);

    if (events)
    {
      ICall_EntityID dest;
      ICall_ServiceEnum src;
      ICall_HciExtEvt *pMsg = NULL;

      if (ICall_fetchServiceMsg(&src, &dest,
                                (void **)&pMsg) == ICALL_ERRNO_SUCCESS)
      {
        uint8_t safeToDealloc = TRUE;

        if ((src == ICALL_SERVICE_CLASS_BLE) && (dest == selfEntity))
        {
          ICall_Stack_Event *pEvt = (ICall_Stack_Event *)pMsg;

          // Check for BLE stack events first
          if (pEvt->signature != 0xffff)
          {
            // Process inter-task message
            safeToDealloc = multi_role_processStackMsg((ICall_Hdr *)pMsg);
          }
        }

        if (pMsg && safeToDealloc)
        {
          ICall_freeMsg(pMsg);
        }
      }

      // If RTOS queue is not empty, process app message.
      if (events & MR_QUEUE_EVT)
      {
        while (!Queue_empty(appMsgQueue))
        {
          mrEvt_t *pMsg = (mrEvt_t *)Util_dequeueMsg(appMsgQueue);
          if (pMsg)
          {
            // Process message.
            multi_role_processAppMsg(pMsg);

            // Free the space from the message.
            ICall_free(pMsg);
          }
        }
      }
    }
  }
}

/*********************************************************************
* @fn      multi_role_processStackMsg
*
* @brief   Process an incoming stack message.
*
* @param   pMsg - message to process
*
* @return  TRUE if safe to deallocate incoming message, FALSE otherwise.
*/
static uint8_t multi_role_processStackMsg(ICall_Hdr *pMsg)
{
  uint8_t safeToDealloc = TRUE;

  BLE_LOG_INT_INT(0, BLE_LOG_MODULE_APP, "APP : Stack msg status=%d, event=0x%x\n", pMsg->status, pMsg->event);

  switch (pMsg->event)
  {
    case GAP_MSG_EVENT:
      //multi_role_processRoleEvent((gapMultiRoleEvent_t *)pMsg);
      multi_role_processGapMsg((gapEventHdr_t*) pMsg);
      break;

    case GATT_MSG_EVENT:
      // Process GATT message
      safeToDealloc = multi_role_processGATTMsg((gattMsgEvent_t *)pMsg);
      break;

    case HCI_GAP_EVENT_EVENT:
    {
      // Process HCI message
      switch (pMsg->status)
      {
        case HCI_COMMAND_COMPLETE_EVENT_CODE:
          break;

        case HCI_BLE_HARDWARE_ERROR_EVENT_CODE:
          AssertHandler(HAL_ASSERT_CAUSE_HARDWARE_ERROR,0);
          break;

        // HCI Commands Events
        case HCI_COMMAND_STATUS_EVENT_CODE:
          {
            hciEvt_CommandStatus_t *pMyMsg = (hciEvt_CommandStatus_t *)pMsg;
            switch ( pMyMsg->cmdOpcode )
            {
              case HCI_LE_SET_PHY:
                {
                  if (pMyMsg->cmdStatus ==
                      HCI_ERROR_CODE_UNSUPPORTED_REMOTE_FEATURE)
                  {
                    ///Display_printf(dispHandle, MR_ROW_CUR_CONN, 0,
                    //        "PHY Change failure, peer does not support this");
                  }
                  else
                  {
                    //Display_printf(dispHandle, MR_ROW_CUR_CONN, 0,
                    //               "PHY Update Status: 0x%02x",
                                   //pMyMsg->cmdStatus);
                  }
                }
                break;
              case HCI_DISCONNECT:
                PRINT_DATA("event:HCI_DISCONNECT\r\n");
                break;

              default:
                {
                  PRINT_DATA("Unknown Cmd Status: 0x%04x::0x%02x",pMyMsg->cmdOpcode, pMyMsg->cmdStatus);
                }
              break;
            }
          }
          break;

        // LE Events
        case HCI_LE_EVENT_CODE:
        {
          hciEvt_BLEPhyUpdateComplete_t *pPUC
            = (hciEvt_BLEPhyUpdateComplete_t*) pMsg;

          if (pPUC->BLEEventCode == HCI_BLE_PHY_UPDATE_COMPLETE_EVENT)
          {
            if (pPUC->status != SUCCESS)
            {

              //Display_printf(dispHandle, MR_ROW_ANY_CONN, 0,
              //               "%s: PHY change failure",
              //               multi_role_getConnAddrStr(pPUC->connHandle));
            }
            else
            {
              //Display_printf(dispHandle, MR_ROW_ANY_CONN, 0,
              //               "%s: PHY updated to %s",
              //               multi_role_getConnAddrStr(pPUC->connHandle),
              // Only symmetrical PHY is supported.
              // rxPhy should be equal to txPhy.
              //               (pPUC->rxPhy == PHY_UPDATE_COMPLETE_EVENT_1M) ? "1 Mbps" :
              //               (pPUC->rxPhy == PHY_UPDATE_COMPLETE_EVENT_2M) ? "2 Mbps" :
              //               (pPUC->rxPhy == PHY_UPDATE_COMPLETE_EVENT_CODED) ? "Coded" : "Unexpected PHY Value");
            }
          }

          break;
        }

        default:
          break;
      }

      break;
    }

    case L2CAP_SIGNAL_EVENT:
      // place holder for L2CAP Connection Parameter Reply
      break;

    default:
      // Do nothing
      break;
  }

  return (safeToDealloc);
}

/*********************************************************************
 * @fn      multi_role_processGapMsg
 *
 * @brief   GAP message processing function.
 *
 * @param   pMsg - pointer to event message structure
 *
 * @return  none
 */
static void multi_role_processGapMsg(gapEventHdr_t *pMsg)
{
  switch (pMsg->opcode)
  {
    case GAP_DEVICE_INIT_DONE_EVENT:
    {
      gapDeviceInitDoneEvent_t *pPkt = (gapDeviceInitDoneEvent_t *)pMsg;

      BLE_LOG_INT_TIME(0, BLE_LOG_MODULE_APP, "APP : ---- got GAP_DEVICE_INIT_DONE_EVENT", 0);
      if(pPkt->hdr.status == SUCCESS)
      {
        // Store the system ID
        uint8_t systemId[DEVINFO_SYSTEM_ID_LEN];

        // use 6 bytes of device address for 8 bytes of system ID value
        systemId[0] = pPkt->devAddr[0];
        systemId[1] = pPkt->devAddr[1];
        systemId[2] = pPkt->devAddr[2];

        // set middle bytes to zero
        systemId[4] = 0x00;
        systemId[3] = 0x00;

        // shift three bytes up
        systemId[7] = pPkt->devAddr[5];
        systemId[6] = pPkt->devAddr[4];
        systemId[5] = pPkt->devAddr[3];

        // Set Device Info Service Parameter
        DevInfo_SetParameter(DEVINFO_SYSTEM_ID, DEVINFO_SYSTEM_ID_LEN, systemId);

        BLE_LOG_INT_INT(0, BLE_LOG_MODULE_APP, "APP : ---- start advert %d,%d\n", 0, 0);
        //Setup and start advertising
        multi_role_advertInit();

      }

      //Setup scanning
      multi_role_scanInit();

      mrMaxPduSize = pPkt->dataPktLen;

      // Enable "Discover Devices", "Set Scanning PHY", and "Set Address Type"
      // in the main menu
      //tbm_setItemStatus(&mrMenuMain, MR_ITEM_STARTDISC | MR_ITEM_ADVERTISE | MR_ITEM_PHY, MR_ITEM_NONE);

      //Display initialized state status
      //Display_printf(dispHandle, MR_ROW_NUM_CONN, 0, "Num Conns: %d", numConn);
      //Display_printf(dispHandle, MR_ROW_NON_CONN, 0, "Initialized");
      //Display_printf(dispHandle, MR_ROW_MYADDRSS, 0, "Multi-Role Address: %s",(char *)Util_convertBdAddr2Str(pPkt->devAddr));

      break;
    }

    case GAP_CONNECTING_CANCELLED_EVENT:
    {
      /*uint16_t itemsToEnable = MR_ITEM_STARTDISC| MR_ITEM_ADVERTISE | MR_ITEM_PHY;

      if (numConn > 0)
      {
        itemsToEnable |= MR_ITEM_SELECTCONN;
      }*/

      //Display_printf(dispHandle, MR_ROW_NON_CONN, 0,
      //               "Conneting attempt cancelled");

      // Enable "Discover Devices", "Connect To", and "Set Scanning PHY"
      // and disable everything else.
      //tbm_setItemStatus(&mrMenuMain,
      //                  itemsToEnable, MR_ITEM_ALL & ~itemsToEnable);

      break;
    }

    case GAP_LINK_ESTABLISHED_EVENT:
    {
      uint16_t connHandle = ((gapEstLinkReqEvent_t*) pMsg)->connectionHandle;
      uint8_t role = ((gapEstLinkReqEvent_t*) pMsg)->connRole;
      uint8_t* pAddr      = ((gapEstLinkReqEvent_t*) pMsg)->devAddr;
      uint8_t  connIndex;
      uint32_t itemsToDisable =0;// MR_ITEM_STOPDISC | MR_ITEM_CANCELCONN;
      uint8_t* pStrAddr;
      uint8_t i;
      uint8_t numConnectable = 0;

      BLE_LOG_INT_TIME(0, BLE_LOG_MODULE_APP, "APP : ---- got GAP_LINK_ESTABLISHED_EVENT", 0);
      // Add this connection info to the list
      connIndex = multi_role_addConnInfo(connHandle, pAddr, role);

      // connIndex cannot be equal to or greater than MAX_NUM_BLE_CONNS
      MULTIROLE_ASSERT(connIndex < MAX_NUM_BLE_CONNS);

      connList[connIndex].charHandle = 0;

      

      pStrAddr = (uint8_t*) Util_convertBdAddr2Str(connList[connIndex].addr);


      PRINT_DATA("Connected to %s\r\n", pStrAddr);
      PRINT_DATA("Num Conns: %d\r\n", numConn);
      PRINT_DATA("+Conntion_handler=%d,%s\r\n",connIndex,Util_convertBdAddr2Str(connList[connIndex].addr));
      multi_role_doSelectConn(connIndex);
      //Display_printf(dispHandle, MR_ROW_NON_CONN, 0, "Connected to %s", pStrAddr);
      //Display_printf(dispHandle, MR_ROW_NUM_CONN, 0, "Num Conns: %d", numConn);

      // Disable "Connect To" until another discovery is performed
      //itemsToDisable |= MR_ITEM_CONNECT;

      // If we already have maximum allowed number of connections,
      // disable device discovery and additional connection making.
      /*if (numConn >= MAX_NUM_BLE_CONNS)
      {
        itemsToDisable |= MR_ITEM_STARTDISC;
      }*/

      /*for (i = 0; i < TBM_GET_NUM_ITEM(&mrMenuConnect); i++)
      {
        if (!memcmp(TBM_GET_ACTION_DESC(&mrMenuConnect, i), pStrAddr,
            MR_ADDR_STR_SIZE))
        {
          // Disable this device from the connection choices
          tbm_setItemStatus(&mrMenuConnect, MR_ITEM_NONE, 1 << i);
        }
        else if (TBM_IS_ITEM_ACTIVE(&mrMenuConnect, i))
        {
          numConnectable++;
        }
      }*/

      // Enable/disable Main menu items properly
      //tbm_setItemStatus(&mrMenuMain,
      //                  MR_ITEM_ALL & ~(itemsToDisable), itemsToDisable);
#if 0//mark by weli
      if (numConn < MAX_NUM_BLE_CONNS)
      {
        // Start advertising since there is room for more connections
        GapAdv_enable(advHandle, GAP_ADV_ENABLE_OPTIONS_USE_MAX, 0);
      }
      else
      {
        // Stop advertising since there is no room for more connections
        GapAdv_disable(advHandle);
      }
#else

      uint8_t check_obdii_dev=0x00;
      for(i=0;i<MAX_NUM_BLE_CONNS;i++)
      {
        if(connList[i].addr[5]==gFlash_data.obdii_mac[0] && 
          connList[i].addr[4]==gFlash_data.obdii_mac[1] &&
          connList[i].addr[3]==gFlash_data.obdii_mac[2] && 
          connList[i].addr[2]==gFlash_data.obdii_mac[3] &&
          connList[i].addr[1]==gFlash_data.obdii_mac[4] && 
          connList[i].addr[0]==gFlash_data.obdii_mac[5] && connList[i].connHandle!=LINKDB_CONNHANDLE_INVALID
          )
        {
          check_obdii_dev=1;
          break;
        }
      }
      if(check_obdii_dev==0 && numConn == 1)
      {
        GapAdv_disable(advHandle);//add by weli
        PRINT_DATA("ble(phone) has been link,disable adv\r\n");
      }
      else if(check_obdii_dev==1 && numConn==2)
      {
        GapAdv_disable(advHandle);//add by weli
        PRINT_DATA("ble(phone) and OBDII has been link,disable adv\r\n");
      }
      else
      {
        PRINT_DATA("still ADV\r\n"); 
      }
      
#endif
      break;
    }

    case GAP_LINK_TERMINATED_EVENT:
    {
      uint16_t connHandle = ((gapTerminateLinkEvent_t*) pMsg)->connectionHandle;
      uint8_t connIndex;
      //uint32_t itemsToEnable = MR_ITEM_STARTDISC | MR_ITEM_ADVERTISE | MR_ITEM_PHY;
      uint8_t* pStrAddr;
      uint8_t i;
      uint8_t numConnectable = 0;

      BLE_LOG_INT_STR(0, BLE_LOG_MODULE_APP, "APP : GAP msg: status=%d, opcode=%s\n", 0, "GAP_LINK_TERMINATED_EVENT");
      // Mark this connection deleted in the connected device list.
      connIndex = multi_role_removeConnInfo(connHandle);

      // connIndex cannot be equal to or greater than MAX_NUM_BLE_CONNS
      MULTIROLE_ASSERT(connIndex < MAX_NUM_BLE_CONNS);

      pStrAddr = (uint8_t*) Util_convertBdAddr2Str(connList[connIndex].addr);

      /*Display_printf(dispHandle, MR_ROW_NON_CONN, 0, "%s is disconnected",
                     pStrAddr);
      Display_printf(dispHandle, MR_ROW_NUM_CONN, 0, "Num Conns: %d", numConn);*/
      PRINT_DATA("%s is disconnected",pStrAddr);
      PRINT_DATA("Num Conns: %d", numConn);
      //todo weli,check whether obdii disconnect,is yes ,reconnect

      /*for (i = 0; i < TBM_GET_NUM_ITEM(&mrMenuConnect); i++)
      {
        if (!memcmp(TBM_GET_ACTION_DESC(&mrMenuConnect, i), pStrAddr,
                     MR_ADDR_STR_SIZE))
        {
          // Enable this device in the connection choices
          tbm_setItemStatus(&mrMenuConnect, 1 << i, MR_ITEM_NONE);
        }

        if (TBM_IS_ITEM_ACTIVE(&mrMenuConnect, i))
        {
          numConnectable++;
        }
      }*/

      // Start advertising since there is room for more connections
      GapAdv_enable(advHandle, GAP_ADV_ENABLE_OPTIONS_USE_MAX , 0);

      if (numConn > 0)
      {
        // There still is an active connection to select
        //itemsToEnable |= MR_ITEM_SELECTCONN;
      }

      // If no active connections
      if (numConn == 0)
      {
        // Stop periodic clock
        //tbm_setItemStatus(&mrMenuMain, TBM_ITEM_NONE, TBM_ITEM_ALL);
        //tbm_setItemStatus(&mrMenuMain, MR_ITEM_NONE, MR_ITEM_ALL ^(MR_ITEM_STARTDISC | MR_ITEM_ADVERTISE | MR_ITEM_PHY));
      }

      // Enable/disable items properly.
      //tbm_setItemStatus(&mrMenuMain,
      //                  itemsToEnable, MR_ITEM_ALL & ~itemsToEnable);

      // If we are in the context which the teminated connection was associated
      // with, go to main menu.
      /*if (connHandle == mrConnHandle)
      {
        tbm_goTo(&mrMenuMain);
      }*/

      break;
    }

    case GAP_UPDATE_LINK_PARAM_REQ_EVENT:
    {
      gapUpdateLinkParamReqReply_t rsp;
      gapUpdateLinkParamReqEvent_t *pReq = (gapUpdateLinkParamReqEvent_t *)pMsg;

      rsp.connectionHandle = pReq->req.connectionHandle;
      rsp.signalIdentifier = pReq->req.signalIdentifier;

      // Only accept connection intervals with slave latency of 0
      // This is just an example of how the application can send a response
      if(pReq->req.connLatency == 0)
      {
        rsp.intervalMin = pReq->req.intervalMin;
        rsp.intervalMax = pReq->req.intervalMax;
        rsp.connLatency = pReq->req.connLatency;
        rsp.connTimeout = pReq->req.connTimeout;
        rsp.accepted = TRUE;
      }
      else
      {
        rsp.accepted = FALSE;
      }

      // Send Reply
      VOID GAP_UpdateLinkParamReqReply(&rsp);

      break;
    }

     case GAP_LINK_PARAM_UPDATE_EVENT:
      {
        gapLinkUpdateEvent_t *pPkt = (gapLinkUpdateEvent_t *)pMsg;

        // Get the address from the connection handle
        linkDBInfo_t linkInfo;
        if (linkDB_GetInfo(pPkt->connectionHandle, &linkInfo) ==  SUCCESS)
        {

          if(pPkt->status == SUCCESS)
          {
            /*Display_printf(dispHandle, MR_ROW_CUR_CONN, 0,
                          "Updated: %s, connTimeout:%d",
                           Util_convertBdAddr2Str(linkInfo.addr),
                           linkInfo.connTimeout*CONN_TIMEOUT_MS_CONVERSION);*/
          }
          else
          {
            // Display the address of the connection update failure
            /*Display_printf(dispHandle, MR_ROW_CUR_CONN, 0,
                           "Update Failed 0x%h: %s", pPkt->opcode,
                           Util_convertBdAddr2Str(linkInfo.addr));*/
          }
        }
        // Check if there are any queued parameter updates
        mrConnHandleEntry_t *connHandleEntry = (mrConnHandleEntry_t *)List_get(&paramUpdateList);
        if (connHandleEntry != NULL)
        {
          // Attempt to send queued update now
          multi_role_processParamUpdate(connHandleEntry->connHandle);

          // Free list element
          ICall_free(connHandleEntry);
        }
        break;
      }

#if defined ( NOTIFY_PARAM_UPDATE_RJCT )
     case GAP_LINK_PARAM_UPDATE_REJECT_EVENT:
     {
       linkDBInfo_t linkInfo;
       gapLinkUpdateEvent_t *pPkt = (gapLinkUpdateEvent_t *)pMsg;

       // Get the address from the connection handle
       linkDB_GetInfo(pPkt->connectionHandle, &linkInfo);

       // Display the address of the connection update failure
       /*Display_printf(dispHandle, MR_ROW_CUR_CONN, 0,
                      "Peer Device's Update Request Rejected 0x%h: %s", pPkt->opcode,
                      Util_convertBdAddr2Str(linkInfo.addr));*/

       break;
     }
#endif

    default:
      break;
  }
}

/*********************************************************************
* @fn      multi_role_scanInit
*
* @brief   Setup initial device scan settings.
*
* @return  None.
*/
static void multi_role_scanInit(void)
{
  uint8_t temp8;
  uint16_t temp16;

  // Setup scanning
  // For more information, see the GAP section in the User's Guide:
  // http://software-dl.ti.com/lprf/ble5stack-latest/

  // Register callback to process Scanner events
  GapScan_registerCb(multi_role_scanCB, NULL);

  // Set Scanner Event Mask
  GapScan_setEventMask(GAP_EVT_SCAN_ENABLED | GAP_EVT_SCAN_DISABLED |
                       GAP_EVT_ADV_REPORT);

  // Set Scan PHY parameters
  GapScan_setPhyParams(DEFAULT_SCAN_PHY, SCAN_TYPE_ACTIVE,
                       DEFAULT_SCAN_INTERVAL, DEFAULT_SCAN_WINDOW);

  // Set Advertising report fields to keep
  temp16 = ADV_RPT_FIELDS;
  GapScan_setParam(SCAN_PARAM_RPT_FIELDS, &temp16);
  // Set Scanning Primary PHY
  temp8 = DEFAULT_SCAN_PHY;
  GapScan_setParam(SCAN_PARAM_PRIM_PHYS, &temp8);
  // Set LL Duplicate Filter
  temp8 = SCANNER_DUPLICATE_FILTER;
  GapScan_setParam(SCAN_PARAM_FLT_DUP, &temp8);

  // Set PDU type filter -
  // Only 'Connectable' and 'Complete' packets are desired.
  // It doesn't matter if received packets are
  // whether Scannable or Non-Scannable, whether Directed or Undirected,
  // whether Scan_Rsp's or Advertisements, and whether Legacy or Extended.
  temp16 = SCAN_FLT_PDU_CONNECTABLE_ONLY | SCAN_FLT_PDU_COMPLETE_ONLY;
  GapScan_setParam(SCAN_PARAM_FLT_PDU_TYPE, &temp16);

  // Set initiating PHY parameters
  GapInit_setPhyParam(DEFAULT_INIT_PHY, INIT_PHYPARAM_CONN_INT_MIN,
					  INIT_PHYPARAM_MIN_CONN_INT);
  GapInit_setPhyParam(DEFAULT_INIT_PHY, INIT_PHYPARAM_CONN_INT_MAX,
					  INIT_PHYPARAM_MAX_CONN_INT);
}

/*********************************************************************
* @fn      multi_role_scanInit
*
* @brief   Setup initial advertisment and start advertising from device init.
*
* @return  None.
*/
static void multi_role_advertInit(void)
{
  uint8_t status = FAILURE;
  // Setup and start Advertising
  // For more information, see the GAP section in the User's Guide:
  // http://software-dl.ti.com/lprf/ble5stack-latest/


  BLE_LOG_INT_INT(0, BLE_LOG_MODULE_APP, "APP : ---- call GapAdv_create set=%d,%d\n", 1, 0);
  // Create Advertisement set #1 and assign handle
  GapAdv_create(&multi_role_advCB, &advParams1,
                &advHandle);

  // Load advertising data for set #1 that is statically allocated by the app
  GapAdv_loadByHandle(advHandle, GAP_ADV_DATA_TYPE_ADV,
                      sizeof(advData1), advData1);

  // Load scan response data for set #1 that is statically allocated by the app
  //GapAdv_loadByHandle(advHandle, GAP_ADV_DATA_TYPE_SCAN_RSP,
  //                    sizeof(scanResData1), scanResData1);

  // Set event mask for set #1
  GapAdv_setEventMask(advHandle,
                      GAP_ADV_EVT_MASK_START_AFTER_ENABLE |
                      GAP_ADV_EVT_MASK_END_AFTER_DISABLE |
                      GAP_ADV_EVT_MASK_SET_TERMINATED);

  BLE_LOG_INT_TIME(0, BLE_LOG_MODULE_APP, "APP : ---- GapAdv_enable", 0);
  // Enable legacy advertising for set #1
  status = GapAdv_enable(advHandle, GAP_ADV_ENABLE_OPTIONS_USE_MAX , 0);

  if(status != SUCCESS)
  {
    mrIsAdvertising = false;
    //Display_printf(dispHandle, MR_ROW_ADVERTIS, 0, "Error: Failed to Start Advertising!");
  }

  if (addrMode > ADDRMODE_RANDOM)
  {
    multi_role_updateRPA();

    // Create one-shot clock for RPA check event.
    Util_constructClock(&clkRpaRead, multi_role_clockHandler,
                        READ_RPA_PERIOD, 0, true,
                        (UArg) &argRpaRead);
  }
}

/*********************************************************************
 * @fn      multi_role_advCB
 *
 * @brief   GapAdv module callback
 *
 * @param   pMsg - message to process
 */
static void multi_role_advCB(uint32_t event, void *pBuf, uintptr_t arg)
{
  mrGapAdvEventData_t *pData = ICall_malloc(sizeof(mrGapAdvEventData_t));

  if (pData)
  {
    pData->event = event;
    pData->pBuf = pBuf;

    if(multi_role_enqueueMsg(MR_EVT_ADV, pData) != SUCCESS)
    {
      ICall_free(pData);
    }
  }
}


/*********************************************************************
* @fn      multi_role_processGATTMsg
*
* @brief   Process GATT messages and events.
*
* @return  TRUE if safe to deallocate incoming message, FALSE otherwise.
*/
static uint8_t multi_role_processGATTMsg(gattMsgEvent_t *pMsg)
{
  // Get connection index from handle
  uint8_t connIndex = multi_role_getConnIndex(pMsg->connHandle);
  MULTIROLE_ASSERT(connIndex < MAX_NUM_BLE_CONNS);

  if (pMsg->method == ATT_FLOW_CTRL_VIOLATED_EVENT)
  {
    // ATT request-response or indication-confirmation flow control is
    // violated. All subsequent ATT requests or indications will be dropped.
    // The app is informed in case it wants to drop the connection.

    // Display the opcode of the message that caused the violation.
    //Display_printf(dispHandle, MR_ROW_CUR_CONN, 0, "FC Violated: %d", pMsg->msg.flowCtrlEvt.opcode);
    PRINT_DATA("FC Violated: %d\r\n", pMsg->msg.flowCtrlEvt.opcode);
  }
  else if (pMsg->method == ATT_MTU_UPDATED_EVENT)
  {
    // MTU size updated
    PRINT_DATA("MTU Size: %d\r\n", pMsg->msg.mtuEvt.MTU);
  }
  else if ( ( pMsg->method == ATT_HANDLE_VALUE_NOTI ) )   //notify event
  {  
    PRINT_DATA("Incoming data\r\n");
    uint8_t index = 0;
    for (index = 0; index < numConn; index ++)
    {
      if(connList[index].gNotify_charHdl == pMsg->msg.handleValueNoti.handle)
      {
        //uint8_t local_data[64]={0x00};
        //sprintf(local_data,"%s",connList[index].strAddr);
        //UartMessage(local_data,strlen(local_data));//print mac address
        PRINT_DATA("+Incoming=%s,%d,%s\r\n",
          Util_convertBdAddr2Str(connList[index].addr),pMsg->msg.handleValueNoti.len,
          pMsg->msg.handleValueNoti.pValue);
#if 1
        BJJA_LM_parsing_OBDII_data(pMsg->msg.handleValueNoti.pValue,pMsg->msg.handleValueNoti.len);
#else //this is test mode
        uint8_t mytest[32]={0x00};
        //toyota odometer setting
        sprintf(mytest,"7E80561280070E3");
        BJJA_LM_parsing_OBDII_data(mytest,strlen(mytest));
        sprintf(mytest,">7E80561280070E3");
        BJJA_LM_parsing_OBDII_data(mytest,strlen(mytest));
        sprintf(mytest,"\r\n>7E80561280070E3\r\n");
        BJJA_LM_parsing_OBDII_data(mytest,strlen(mytest));

        //standard obdii odometer
        sprintf(mytest,"7E80641A600066095");
        BJJA_LM_parsing_OBDII_data(mytest,strlen(mytest));
        sprintf(mytest,">7E80641A600066095");
        BJJA_LM_parsing_OBDII_data(mytest,strlen(mytest));
        sprintf(mytest,"\r\n>7E80641A600066095\r\n");
        BJJA_LM_parsing_OBDII_data(mytest,strlen(mytest));

        //standard obdii fuel level
        sprintf(mytest,"7E803412FB6");
        BJJA_LM_parsing_OBDII_data(mytest,strlen(mytest));
        sprintf(mytest,">7E803412FB6");
        BJJA_LM_parsing_OBDII_data(mytest,strlen(mytest));
        sprintf(mytest,"\r\n>7E803412FB6\r\n");
        BJJA_LM_parsing_OBDII_data(mytest,strlen(mytest));

#endif

      }
    }  
    //UartMessage(pMsg->msg.handleValueNoti.pValue, pMsg->msg.handleValueNoti.len);
    /*if(gDisconn_mode==0)
    {
        multi_role_doDisconnect(index);
      PRINT_DISPLAY("this is a oneshot mode,do disconnect\r\n");
    }*/
  }



  // Messages from GATT server
  if (linkDB_Up(pMsg->connHandle))
  {
    if ((pMsg->method == ATT_READ_RSP)   ||
        ((pMsg->method == ATT_ERROR_RSP) &&
         (pMsg->msg.errorRsp.reqOpcode == ATT_READ_REQ)))
    {
      if (pMsg->method == ATT_ERROR_RSP)
      {
        //Display_printf(dispHandle, MR_ROW_CUR_CONN, 0, "Read Error %d", pMsg->msg.errorRsp.errCode);
      }
      else
      {
        // After a successful read, display the read value
        //Display_printf(dispHandle, MR_ROW_CUR_CONN, 0, "Read rsp: %d", pMsg->msg.readRsp.pValue[0]);
      }

    }
    else if ((pMsg->method == ATT_WRITE_RSP)  ||
             ((pMsg->method == ATT_ERROR_RSP) &&
              (pMsg->msg.errorRsp.reqOpcode == ATT_WRITE_REQ)))
    {

      if (pMsg->method == ATT_ERROR_RSP)
      {
        //Display_printf(dispHandle, MR_ROW_CUR_CONN, 0, "Write Error %d", pMsg->msg.errorRsp.errCode);
      }
      else
      {
        // After a succesful write, display the value that was written and
        // increment value
        //Display_printf(dispHandle, MR_ROW_CUR_CONN, 0, "Write sent: %d", charVal);
      }

      //tbm_goTo(&mrMenuPerConn);
    }
    else if (connList[connIndex].discState != BLE_DISC_STATE_IDLE)
    {
      multi_role_processGATTDiscEvent(pMsg);
    }
  } // Else - in case a GATT message came after a connection has dropped, ignore it.

  // Free message payload. Needed only for ATT Protocol messages
  GATT_bm_free(&pMsg->msg, pMsg->method);

  // It's safe to free the incoming message
  return (TRUE);
}

/*********************************************************************
 * @fn		multi_role_processParamUpdate
 *
 * @brief	Process connection parameters update
 *
 * @param	connHandle - connection handle to update
 *
 * @return	None.
 */
static void multi_role_processParamUpdate(uint16_t connHandle)
{
  gapUpdateLinkParamReq_t req;
  uint8_t connIndex;

  req.connectionHandle = connHandle;
#ifdef DEFAULT_SEND_PARAM_UPDATE_REQ
  req.connLatency = DEFAULT_DESIRED_SLAVE_LATENCY;
  req.connTimeout = DEFAULT_DESIRED_CONN_TIMEOUT;
  req.intervalMin = DEFAULT_DESIRED_MIN_CONN_INTERVAL;
  req.intervalMax = DEFAULT_DESIRED_MAX_CONN_INTERVAL;
#endif

  connIndex = multi_role_getConnIndex(connHandle);
  MULTIROLE_ASSERT(connIndex < MAX_NUM_BLE_CONNS);

  // Deconstruct the clock object
  Clock_destruct(connList[connIndex].pUpdateClock);
  // Free clock struct, only in case it is not NULL
  if (connList[connIndex].pUpdateClock != NULL)
  {
	ICall_free(connList[connIndex].pUpdateClock);
	connList[connIndex].pUpdateClock = NULL;
  }
  // Free ParamUpdateEventData, only in case it is not NULL
  if (connList[connIndex].pParamUpdateEventData != NULL)
  {
    ICall_free(connList[connIndex].pParamUpdateEventData);
  }

  // Send parameter update
  bStatus_t status = GAP_UpdateLinkParamReq(&req);

  // If there is an ongoing update, queue this for when the udpate completes
  if (status == bleAlreadyInRequestedMode)
  {
    mrConnHandleEntry_t *connHandleEntry = ICall_malloc(sizeof(mrConnHandleEntry_t));
    if (connHandleEntry)
    {
      connHandleEntry->connHandle = connHandle;

      List_put(&paramUpdateList, (List_Elem *)connHandleEntry);
    }
  }
}

/*********************************************************************
* @fn      multi_role_processAppMsg
*
* @brief   Process an incoming callback from a profile.
*
* @param   pMsg - message to process
*
* @return  None.
*/
static void multi_role_processAppMsg(mrEvt_t *pMsg)
{
  bool safeToDealloc = TRUE;

  if (pMsg->event <= APP_EVT_EVENT_MAX)
  {
    BLE_LOG_INT_STR(0, BLE_LOG_MODULE_APP, "APP : App msg status=%d, event=%s\n", 0, appEventStrings[pMsg->event]);
  }
  else
  {
    BLE_LOG_INT_INT(0, BLE_LOG_MODULE_APP, "APP : App msg status=%d, event=0x%x\n", 0, pMsg->event);
  }

  switch (pMsg->event)
  {
    case MR_EVT_CHAR_CHANGE:
    {
      multi_role_processCharValueChangeEvt(*(uint8_t*)(pMsg->pData));
      break;
    }

    case MR_EVT_KEY_CHANGE:
    {
      multi_role_handleKeys(*(uint8_t *)(pMsg->pData));
      break;
    }

    case MR_EVT_ADV_REPORT:
    {
      GapScan_Evt_AdvRpt_t* pAdvRpt = (GapScan_Evt_AdvRpt_t*) (pMsg->pData);
      PRINT_DATA("incoming: %s\r\n",Util_convertBdAddr2Str(pAdvRpt->addr));
      //PRINT_DATA("mac:%x-%x-%x-%x-%x-%x\r\n",pAdvRpt->addr[0],pAdvRpt->addr[1],
      //  pAdvRpt->addr[2],pAdvRpt->addr[3],pAdvRpt->addr[4],pAdvRpt->addr[5]);
#if (DEFAULT_DEV_DISC_BY_SVC_UUID == TRUE)
      //if (multi_role_findSvcUuid(SIMPLEPROFILE_SERV_UUID,
      //                           pAdvRpt->pData, pAdvRpt->dataLen))
      if(pAdvRpt->addr[5]==gFlash_data.obdii_mac[0] && pAdvRpt->addr[4]==gFlash_data.obdii_mac[1] &&
        pAdvRpt->addr[3]==gFlash_data.obdii_mac[2] && pAdvRpt->addr[2]==gFlash_data.obdii_mac[3] &&
        pAdvRpt->addr[1]==gFlash_data.obdii_mac[4] &&pAdvRpt->addr[0]==gFlash_data.obdii_mac[5])//MAC:F5:88:E2:4D:5B:94
      {
        multi_role_addScanInfo(pAdvRpt->addr, pAdvRpt->addrType);
        PRINT_DATA("Discovered: %s",Util_convertBdAddr2Str(pAdvRpt->addr));
        /*Display_printf(dispHandle, MR_ROW_CUR_CONN, 0, "Discovered: %s",
                       Util_convertBdAddr2Str(pAdvRpt->addr));*/
        if(gBJJA_LM_Obd_state==O_Discover)
        {
          //gBJJA_LM_State_machine=2;
          gBJJA_LM_Obd_state=O_Connect;
          gWaitCount=1;
          gConnTimeout_count=1;

        }
      }
#else // !DEFAULT_DEV_DISC_BY_SVC_UUID
      /*Display_printf(dispHandle, MR_ROW_CUR_CONN, 0, "Discovered: %s",
                     Util_convertBdAddr2Str(pAdvRpt->addr));*/
#endif // DEFAULT_DEV_DISC_BY_SVC_UUID

      // Free scan payload data
      if (pAdvRpt->pData != NULL)
      {
        ICall_free(pAdvRpt->pData);
      }
      break;
    }

    case MR_EVT_SCAN_ENABLED:
    {
      // Disable everything but "Stop Discovering" on the menu
      /*tbm_setItemStatus(&mrMenuMain, MR_ITEM_STOPDISC,
                       (MR_ITEM_ALL & ~MR_ITEM_STOPDISC));
      Display_printf(dispHandle, MR_ROW_NON_CONN, 0, "Discovering...");*/

      break;
    }

    case MR_EVT_SCAN_DISABLED:
    {
      uint8_t numReport;
      uint8_t i;
      static uint8_t* pAddrs = NULL;
      uint8_t* pAddrTemp;
      //uint16_t itemsToEnable = MR_ITEM_STARTDISC | MR_ITEM_ADVERTISE | MR_ITEM_PHY;
#if (DEFAULT_DEV_DISC_BY_SVC_UUID == TRUE)
      numReport = numScanRes;
#else // !DEFAULT_DEV_DISC_BY_SVC_UUID
      GapScan_Evt_AdvRpt_t advRpt;

      numReport = ((GapScan_Evt_End_t*) (pMsg->pData))->numReport;
#endif // DEFAULT_DEV_DISC_BY_SVC_UUID

      /*Display_printf(dispHandle, MR_ROW_NON_CONN, 0,
                     "%d devices discovered", numReport);*/

      if (numReport > 0)
      {
        // Also enable "Connect to"
        //itemsToEnable |= MR_ITEM_CONNECT;
      }

      if (numConn > 0)
      {
        // Also enable "Work with"
       // itemsToEnable |= MR_ITEM_SELECTCONN;
      }

      // Enable "Discover Devices", "Set Scanning PHY", and possibly
      // "Connect to" and/or "Work with".
      // Disable "Stop Discovering".
      //tbm_setItemStatus(&mrMenuMain, itemsToEnable, MR_ITEM_STOPDISC);
      if (pAddrs != NULL)
      {
        ICall_free(pAddrs);
      }
      // Allocate buffer to display addresses
      pAddrs = ICall_malloc(numReport * MR_ADDR_STR_SIZE);
      if (pAddrs == NULL)
      {
        numReport = 0;
      }

      //TBM_SET_NUM_ITEM(&mrMenuConnect, numReport);

      if (pAddrs != NULL)
      {
        pAddrTemp = pAddrs;
        for (i = 0; i < numReport; i++, pAddrTemp += MR_ADDR_STR_SIZE)
        {
  #if (DEFAULT_DEV_DISC_BY_SVC_UUID == TRUE)
          // Get the address from the list, convert it to string, and
          // copy the string to the address buffer
          memcpy(pAddrTemp, Util_convertBdAddr2Str(scanList[i].addr),
                 MR_ADDR_STR_SIZE);
  #else // !DEFAULT_DEV_DISC_BY_SVC_UUID
          // Get the address from the report, convert it to string, and
          // copy the string to the address buffer
          GapScan_getAdvReport(i, &advRpt);
          memcpy(pAddrTemp, Util_convertBdAddr2Str(advRpt.addr),
                 MR_ADDR_STR_SIZE);
  #endif // DEFAULT_DEV_DISC_BY_SVC_UUID

          // Assign the string to the corresponding action description of the menu
         // TBM_SET_ACTION_DESC(&mrMenuConnect, i, pAddrTemp);
         // tbm_setItemStatus(&mrMenuConnect, (1 << i) , TBM_ITEM_NONE);
        }

        // Disable any non-active scan results
        /*for(; i < DEFAULT_MAX_SCAN_RES; i++)
        {
          tbm_setItemStatus(&mrMenuConnect, TBM_ITEM_NONE, (1 << i));
        }*/
      }
      break;
    }

    case MR_EVT_SVC_DISC:
    {
      multi_role_startSvcDiscovery();
      break;
    }

    case MR_EVT_ADV:
    {
      multi_role_processAdvEvent((mrGapAdvEventData_t*)(pMsg->pData));
      break;
    }

    case MR_EVT_PAIRING_STATE:
    {
      multi_role_processPairState((mrPairStateData_t*)(pMsg->pData));
      break;
    }

    case MR_EVT_PASSCODE_NEEDED:
    {
      multi_role_processPasscode((mrPasscodeData_t*)(pMsg->pData));
      break;
    }

    case MR_EVT_SEND_PARAM_UPDATE:
    {
      // Extract connection handle from data
      uint16_t locConnHandle = *(uint16_t *)(((mrClockEventData_t *)pMsg->pData)->data);
      multi_role_processParamUpdate(locConnHandle);
      safeToDealloc = FALSE;
      break;
    }
    case BJJA_LM_Notify_DATA_EVT:
    {
      BJJA_LM_BLE_INCOMING();
      break;
    }
    case MR_EVT_PERIODIC:
    {
      multi_role_performPeriodicTask();
      break;
    }
    case MR_EVT_OBDD_PERIODIC:
    {
      BJJA_LM_OBDD_performPeriodicTask();
      break;
    }
    case SBP_UART_INCOMING_EVT:          //add by weli
    {
        //bjja_lm_uart_data_t myData = ((mrClockEventData_t *)pMsg->pData)->data
        //PRINT_DATA("get data:%s,len:%d\r\n",((bjja_lm_uart_data_t*)pMsg->pData).pBuf,((bjja_lm_uart_data_t*)pMsg->pData).data_len);
        BJJA_parsing_AT_cmd_send_data(((bjja_lm_uart_data_t*)pMsg->pData)->pBuf,((bjja_lm_uart_data_t*)pMsg->pData)->data_len);
        //ICall_free(pData);
        break;
    }
    case SBP_UART2_INCOMING_EVT:          //add by weli
    {
        //UartMessage(serialBuffer, gSerialLen);
        BJJA_parsing_AT_cmd_send_data_UART2();
        break;
    }

    case MR_EVT_READ_RPA:
    {
      multi_role_updateRPA();
      break;
    }

    case MR_EVT_INSUFFICIENT_MEM:
    {
      // We are running out of memory.
      //Display_printf(dispHandle, MR_ROW_ANY_CONN, 0, "Insufficient Memory");

      // We might be in the middle of scanning, try stopping it.
      GapScan_disable();
      break;
    }

    default:
      // Do nothing.
      break;
  }

  if ((safeToDealloc == TRUE) && (pMsg->pData != NULL))
  {
    ICall_free(pMsg->pData);
  }
}

/*********************************************************************
 * @fn      multi_role_processAdvEvent
 *
 * @brief   Process advertising event in app context
 *
 * @param   pEventData
 */
static void multi_role_processAdvEvent(mrGapAdvEventData_t *pEventData)
{
  switch (pEventData->event)
  {
    case GAP_EVT_ADV_START_AFTER_ENABLE:
      BLE_LOG_INT_TIME(0, BLE_LOG_MODULE_APP, "APP : ---- GAP_EVT_ADV_START_AFTER_ENABLE", 0);
      mrIsAdvertising = true;
      //Display_printf(dispHandle, MR_ROW_ADVERTIS, 0, "Adv Set %d Enabled",
      //               *(uint8_t *)(pEventData->pBuf));
      break;

    case GAP_EVT_ADV_END_AFTER_DISABLE:
      mrIsAdvertising = false;
      //Display_printf(dispHandle, MR_ROW_ADVERTIS, 0, "Adv Set %d Disabled",
      //               *(uint8_t *)(pEventData->pBuf));
      break;

    case GAP_EVT_ADV_START:
      //Display_printf(dispHandle, MR_ROW_ADVERTIS, 0, "Adv Started %d Enabled",
      //               *(uint8_t *)(pEventData->pBuf));
      break;

    case GAP_EVT_ADV_END:
      //Display_printf(dispHandle, MR_ROW_ADVERTIS, 0, "Adv Ended %d Disabled",
      ///               *(uint8_t *)(pEventData->pBuf));
      break;

    case GAP_EVT_ADV_SET_TERMINATED:
    {
      mrIsAdvertising = false;
#ifndef Display_DISABLE_ALL
      GapAdv_setTerm_t *advSetTerm = (GapAdv_setTerm_t *)(pEventData->pBuf);
#endif
      //Display_printf(dispHandle, MR_ROW_ADVERTIS, 0, "Adv Set %d disabled after conn %d",
      //               advSetTerm->handle, advSetTerm->connHandle );
    }
    break;

    case GAP_EVT_SCAN_REQ_RECEIVED:
      break;

    case GAP_EVT_INSUFFICIENT_MEMORY:
      break;

    default:
      break;
  }

  // All events have associated memory to free except the insufficient memory
  // event
  if (pEventData->event != GAP_EVT_INSUFFICIENT_MEMORY)
  {
    ICall_free(pEventData->pBuf);
  }
}

#if (DEFAULT_DEV_DISC_BY_SVC_UUID == TRUE)
/*********************************************************************
 * @fn      multi_role_findSvcUuid
 *
 * @brief   Find a given UUID in an advertiser's service UUID list.
 *
 * @return  TRUE if service UUID found
 */
static bool multi_role_findSvcUuid(uint16_t uuid, uint8_t *pData,
                                      uint16_t dataLen)
{
  uint8_t adLen;
  uint8_t adType;
  uint8_t *pEnd;

  if (dataLen > 0)
  {
    pEnd = pData + dataLen - 1;

    // While end of data not reached
    while (pData < pEnd)
    {
      // Get length of next AD item
      adLen = *pData++;
      if (adLen > 0)
      {
        adType = *pData;

        // If AD type is for 16-bit service UUID
        if ((adType == GAP_ADTYPE_16BIT_MORE) ||
            (adType == GAP_ADTYPE_16BIT_COMPLETE))
        {
          pData++;
          adLen--;

          // For each UUID in list
          while (adLen >= 2 && pData < pEnd)
          {
            // Check for match
            if ((pData[0] == LO_UINT16(uuid)) && (pData[1] == HI_UINT16(uuid)))
            {
              // Match found
              return TRUE;
            }

            // Go to next
            pData += 2;
            adLen -= 2;
          }

          // Handle possible erroneous extra byte in UUID list
          if (adLen == 1)
          {
            pData++;
          }
        }
        else
        {
          // Go to next item
          pData += adLen;
        }
      }
    }
  }

  // Match not found
  return FALSE;
}

/*********************************************************************
 * @fn      multi_role_addScanInfo
 *
 * @brief   Add a device to the scanned device list
 *
 * @return  none
 */
static void multi_role_addScanInfo(uint8_t *pAddr, uint8_t addrType)
{
  uint8_t i;

  // If result count not at max
  if (numScanRes < DEFAULT_MAX_SCAN_RES)
  {
    // Check if device is already in scan results
    for (i = 0; i < numScanRes; i++)
    {
      if (memcmp(pAddr, scanList[i].addr , B_ADDR_LEN) == 0)
      {
        return;
      }
    }

    // Add addr to scan result list
    memcpy(scanList[numScanRes].addr, pAddr, B_ADDR_LEN);
    scanList[numScanRes].addrType = addrType;

    // Increment scan result count
    numScanRes++;
  }
}
#endif // DEFAULT_DEV_DISC_BY_SVC_UUID

/*********************************************************************
 * @fn      multi_role_scanCB
 *
 * @brief   Callback called by GapScan module
 *
 * @param   evt - event
 * @param   msg - message coming with the event
 * @param   arg - user argument
 *
 * @return  none
 */
void multi_role_scanCB(uint32_t evt, void* pMsg, uintptr_t arg)
{
  uint8_t event;

  if (evt & GAP_EVT_ADV_REPORT)
  {
    event = MR_EVT_ADV_REPORT;
  }
  else if (evt & GAP_EVT_SCAN_ENABLED)
  {
    event = MR_EVT_SCAN_ENABLED;
  }
  else if (evt & GAP_EVT_SCAN_DISABLED)
  {
    event = MR_EVT_SCAN_DISABLED;
  }
  else if (evt & GAP_EVT_INSUFFICIENT_MEMORY)
  {
    event = MR_EVT_INSUFFICIENT_MEM;
  }
  else
  {
    return;
  }

  if(multi_role_enqueueMsg(event, pMsg) != SUCCESS)
  {
    ICall_free(pMsg);
  }

}

/*********************************************************************
* @fn      multi_role_charValueChangeCB
*
* @brief   Callback from Simple Profile indicating a characteristic
*          value change.
*
* @param   paramID - parameter ID of the value that was changed.
*
* @return  None.
*/
static void multi_role_charValueChangeCB(uint8_t paramID)
{
  uint8_t *pData;

  // Allocate space for the event data.
  if ((pData = ICall_malloc(sizeof(uint8_t))))
  {
    *pData = paramID;

    // Queue the event.
    if(multi_role_enqueueMsg(MR_EVT_CHAR_CHANGE, pData) != SUCCESS)
    {
      ICall_free(pData);
    }
  }
}

/*********************************************************************
 * @fn      multi_role_enqueueMsg
 *
 * @brief   Creates a message and puts the message in RTOS queue.
 *
 * @param   event - message event.
 * @param   state - message state.
 * @param   pData - message data pointer.
 *
 * @return  TRUE or FALSE
 */
static status_t multi_role_enqueueMsg(uint8_t event, void *pData)
{
  uint8_t success;
  mrEvt_t *pMsg = ICall_malloc(sizeof(mrEvt_t));

  // Create dynamic pointer to message.
  if (pMsg)
  {
    pMsg->event = event;
    pMsg->pData = pData;

    // Enqueue the message.
    success = Util_enqueueMsg(appMsgQueue, syncEvent, (uint8_t *)pMsg);
    return (success) ? SUCCESS : FAILURE;
  }

  return(bleMemAllocError);
}

/*********************************************************************
 * @fn      multi_role_processCharValueChangeEvt
 *
 * @brief   Process a pending Simple Profile characteristic value change
 *          event.
 *
 * @param   paramID - parameter ID of the value that was changed.
 */
static void multi_role_processCharValueChangeEvt(uint8_t paramId)
{
  uint8_t charValue3[SIMPLEPROFILE_CHAR3_LEN]={0};
  //uint8_t newValue;

  switch(paramId)
  {
    /*case SIMPLEPROFILE_CHAR1:
      SimpleProfile_GetParameter(SIMPLEPROFILE_CHAR1, &newValue);

      //Display_printf(dispHandle, MR_ROW_CHARSTAT, 0, "Char 1: %d", (uint16_t)newValue);
      break;*/

    case SIMPLEPROFILE_CHAR3:
      SimpleProfile_GetParameter(SIMPLEPROFILE_CHAR3, charValue3);
      if(BJJA_LM_Json_cmd_parsing_from_BLE(charValue3)==0)
      {
        PRINT_DATA("Json command by BLE\r\n");
      }
      else if(strncmp(charValue3,"AT+ARM",strlen("AT+ARM"))==0)
      {
        /*
        //SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen("thx for all cansec fans,Weli,amy is baga.") ,"thx for all cansec fans,Weli,amy is baga.");//weli add
        gArm_Disarm_command=1;
        PRINT_DATA("from BLE set ARM\r\n");
        gCurrent_Notify_id=5;
        multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
        */
        if(BJJA_LM_Early_Entry_Arm_state())
        {
          PRINT_DATA("from BLE set ARM OK\r\n");
          //gArm_Disarm_command=1;
          //SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen("OK+ARM") ,"OK+ARM");//weli add
          gCurrent_Notify_id=5;
          multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
        }
        else
        {
          PRINT_DATA("from BLE set ARM FAIL\r\n");
          //SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen("FAIL+ARM") ,"FAIL+ARM");//weli add
          gCurrent_Notify_id=6;
          multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
        }
      }
      else if(strncmp(charValue3,"AT+DISARM",strlen("AT+DISARM"))==0)
      {
        /*
        //SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen("thx for all cansec fans,Weli,amy is baga.") ,"thx for all cansec fans,Weli,amy is baga.");//weli add
        gArm_Disarm_command=2;
        PRINT_DATA("from BLE set DISARM\r\n");
        gCurrent_Notify_id=6;
        multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
        */
        if(BJJA_LM_Early_Entry_DisArm_state())
        {
          PRINT_DATA("from BLE set DISARM OK\r\n");
          //gArm_Disarm_command=2;
          //SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen("OK+DISARM") ,"OK+DISARM");//weli add
          gCurrent_Notify_id=7;
          multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
        }
        else
        {
          PRINT_DATA("from BLE set DISARM FAIL\r\n");
          //SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen("FAIL+DISARM") ,"FAIL+DISARM");//weli add
          gCurrent_Notify_id=8;
          multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
        }
      }
      else if(strncmp(charValue3,"AT+OpenDR=",strlen("AT+OpenDR="))==0)
      {
        //parsing token
        //check mac address
        //control door.
        uint8_t random_number[32]={0x00};
        uint8_t mac_addr[16]={0x00};
        uint8_t user_id[32]={0x00};
        uint8_t door_state=0;
        char * pch;
        //printf ("Splitting string \"%s\" into tokens:\n",str);
        char *delim = ",";
        pch = strtok(charValue3,delim);
        if (pch != NULL)
        {
          sprintf(random_number,"%s",pch);
          pch = strtok(NULL,delim);
          if(pch!=NULL)
          {
            sprintf(mac_addr,"%s",pch);
            pch = strtok(NULL,delim);
            if(pch!=NULL)
            {
              sprintf(user_id,"%s",pch);
              pch = strtok(NULL,delim);
              if(pch!=NULL)
              {
                  door_state = atoi(pch);
                  //PRINT_DATA("Door control mac:%s,user:%s,door:%d\r\n",mac_addr,user_id,door_state);
                  if(door_state==0)
                    gCurrent_Notify_id=1;//door lock
                  else
                    gCurrent_Notify_id=2;//door unlock
                  multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);

              }
            }
          }
        }
      }
      else if(strncmp(charValue3,"AT+DRMode=?",strlen("AT+DRMode=?"))==0)
      {
        gCurrent_Notify_id=4;
        multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
      }
      else if(strncmp(charValue3,"AT+DRMode=",strlen("AT+DRMode="))==0)
      {
        if(charValue3[10]=='0')
          gFlash_data.door_mode = 0;
        else
          gFlash_data.door_mode = 1;
        gCurrent_Notify_id=3;
        multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
      }
      else if(strncmp(charValue3,"AT+SUEN=?",strlen("AT+SUEN=?"))==0)
      {
        gCurrent_Notify_id=9;
        multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
      }
      else if(strncmp(charValue3,"AT+SUEN=",strlen("AT+SUEN="))==0)
      {
        //gSuperUserMode
        uint8_t tmp_mac[6]={0x00};
        uint8_t i=0;
        if(strlen(charValue3)>=20)
        {
          for(i=0;i<6;i++)
          {
            tmp_mac[i] = (BJJA_ascii2hex(charValue3[(8+(i*2))])<<4) | BJJA_ascii2hex(charValue3[(8+((i*2)+1))]);
          }
          for(i=0;i<6;i++)
          {
            if(gMac[i]!= tmp_mac[i])
              break;
          }
          if(i>=6)
          {
            gSuperUserMode=1;
            gCurrent_Notify_id=11;
            multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
          }
          else
          {
            gCurrent_Notify_id=12;
            multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
          }
        }
          
      }
      else if(strncmp(charValue3,"AT+SUDIS",strlen("AT+SUDIS"))==0)
      {
        gSuperUserMode=0;
        gCurrent_Notify_id=13;
        multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
      }
      else if(strncmp(charValue3,"AT+SRD=",strlen("AT+SRD="))==0)
      {
        if(gSuperUserMode)
        {
          //AT+SRD=0404020303030302//delete data
          if(strlen(charValue3)>=23)
          {
            uint8_t find_index=0,i=0,j=0;
            uint8_t new_S[8]={0x00};
            // compare address not match 
            for(i=0;i<8;i++)
            {
              //gSubGpairing_data[find_index].S_code[i] = (BJJA_ascii2hex(serialBuffer2[(6+(i*2))])<<4) | BJJA_ascii2hex(serialBuffer2[(6+((i*2)+1))]);
              new_S[i] = (BJJA_ascii2hex(charValue3[(7+(i*2))])<<4) | BJJA_ascii2hex(charValue3[(7+((i*2)+1))]);
            }
            for(i=0;i<8;i++)
            {
              if(gSubGpairing_data[i].enable=='1')
              {
                find_index=0;
                for(j=0;j<8;j++)
                {
                  if(new_S[j]==gSubGpairing_data[i].S_code[j])
                  {
                    find_index++;
                  }
                }
                if(find_index>=8)
                {
                  break;
                }
              }
            }
            if(find_index>=8)
            {
               //clear data
              gSubGpairing_data[i].enable='0';
              for(j=0;j<8;j++)
              {
                  gSubGpairing_data[i].S_code[j] = 0x00;
              }
              //BJJA_LM_write_SR_flash();
              PRINT_DATA("OK+SRD\r\n");
              gCurrent_Notify_id=21;
              multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
            }
            else
            {
              PRINT_DATA("FAIL+SRD:NOT FOUND\r\n");
              gCurrent_Notify_id=22;
              multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
            }
          }
          else
          {
            PRINT_DATA("FAIL+SRD\r\n");
            gCurrent_Notify_id=23;
            multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
          }




        }
        else
        {
          gCurrent_Notify_id=10;
          multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
        }
      }
      else if(strncmp(charValue3,"AT+SR=?",strlen("AT+SR=?"))==0)
      {
        if(gSuperUserMode)
        {
          gCurrent_Notify_id=14;
          multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
        }
        else
        {
          gCurrent_Notify_id=10;
          multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
        }
      }
      else if(strncmp(charValue3,"AT+SR=",strlen("AT+SR="))==0)
      {
        if(gSuperUserMode)
        {

          if(strlen(charValue3)>=22)
          {
            uint8_t find_index=0,i=0,j=0;
            uint8_t new_S[8]={0x00};
            // compare address not match 
            for(i=0;i<8;i++)
            {
              //gSubGpairing_data[find_index].S_code[i] = (BJJA_ascii2hex(serialBuffer2[(6+(i*2))])<<4) | BJJA_ascii2hex(serialBuffer2[(6+((i*2)+1))]);
              new_S[i] = (BJJA_ascii2hex(charValue3[(6+(i*2))])<<4) | BJJA_ascii2hex(charValue3[(6+((i*2)+1))]);
            }
            for(i=0;i<8;i++)
            {
              if(gSubGpairing_data[i].enable=='1')
              {
                find_index=0;
                for(j=0;j<8;j++)
                {
                  if(new_S[j]==gSubGpairing_data[i].S_code[j])
                  {
                    find_index++;
                  }
                }
                if(find_index>=8)
                {
                  break;
                }
              }
              
            }

            if(find_index>=8)
            {
              //show fail,has exist
              PRINT_DATA("FAIL+SR:DATA IS EXIST\r\n");
              gCurrent_Notify_id=18;
              multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
            }
            else
            {
              //write data
              //FIND EMPTY ENABLE FLAG
              find_index=0;
              for(i=0;i<8;i++)
              {
                if(gSubGpairing_data[i].enable!='1')
                {
                  break;
                }
              }
              gSubGpairing_data[i].enable='1';
              for(j=0;j<8;j++)
              {
                //gSubGpairing_data[find_index].S_code[i] = (BJJA_ascii2hex(serialBuffer2[(6+(i*2))])<<4) | BJJA_ascii2hex(serialBuffer2[(6+((i*2)+1))]);
                gSubGpairing_data[i].S_code[j] = new_S[j];
              }
              PRINT_DATA("OK+SR\r\n");
              gCurrent_Notify_id=20;
              multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
              
            }
          
          }
          else
          {
            PRINT_DATA("FAIL+SR\r\n");
            gCurrent_Notify_id=19;
            multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
          }






        }
        else
        {
          gCurrent_Notify_id=10;
          multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
        }
      }
      else if(strncmp(charValue3,"AT+OBDMAC=?",strlen("AT+OBDMAC=?"))==0)
      {
        if(gSuperUserMode)
        {
          gCurrent_Notify_id=15;
          multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
        }
        else
        {
          gCurrent_Notify_id=10;
          multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
        }
      }
      else if(strncmp(charValue3,"AT+OBDMAC=",strlen("AT+OBDMAC="))==0)
      {
        if(gSuperUserMode)
        {
          //AT+OBDMAC=F588E24D5B94
          if(strlen(charValue3)>=22)
          {
            for(uint8_t i=0;i<6;i++)
            {
              gFlash_data.obdii_mac[i] = (BJJA_ascii2hex(charValue3[(10+(i*2))])<<4) | BJJA_ascii2hex(charValue3[(10+((i*2)+1))]);
            }
            PRINT_DATA("OK+OBDMAC\r\n");
            gCurrent_Notify_id=16;
          }
          else
          {
            PRINT_DATA("FAIL+OBDMAC\r\n");
            gCurrent_Notify_id=17;
          }
          multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
        }
        else
        {
          gCurrent_Notify_id=10;
          multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
        }
      }

      else
      {
        UartMessage2(charValue3,/*strlen(charValue3)*/gWriteUART_Length);  
      }
      break;

    default:
      // should not reach here!
      break;
  }
}

/*********************************************************************
 * @fn      multi_role_performPeriodicTask
 *
 * @brief   Perform a periodic application task. This function gets called
 *          every five seconds (SP_PERIODIC_EVT_PERIOD). In this example,
 *          the value of the third characteristic in the SimpleGATTProfile
 *          service is retrieved from the profile, and then copied into the
 *          value of the the fourth characteristic.
 *
 * @param   None.
 *
 * @return  None.
 */
static void multi_role_performPeriodicTask(void)
{
#if 0 //20220719 remove by weli
  uint8_t valueToCopy;

  // Call to retrieve the value of the third characteristic in the profile
  if (SimpleProfile_GetParameter(SIMPLEPROFILE_CHAR3, &valueToCopy) == SUCCESS)
  {
    // Call to set that value of the fourth characteristic in the profile.
    // Note that if notifications of the fourth characteristic have been
    // enabled by a GATT client device, then a notification will be sent
    // every time this function is called.
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4, sizeof(uint8_t),
                               &valueToCopy);
  }
#endif
  //GPIO_write(CONFIG_INGI,0);
  BJJA_LM_state_machine_heart_beat();
  BJJA_LM_1S_worker();


  Util_startClock(&clkPeriodic);
  
}

/*********************************************************************
 * @fn      multi_role_updateRPA
 *
 * @brief   Read the current RPA from the stack and update display
 *          if the RPA has changed.
 *
 * @param   None.
 *
 * @return  None.
 */
static void multi_role_updateRPA(void)
{
  uint8_t* pRpaNew;

  // Read the current RPA.
  pRpaNew = GAP_GetDevAddress(FALSE);

  if (memcmp(pRpaNew, rpa, B_ADDR_LEN))
  {
    // If the RPA has changed, update the display
    //Display_printf(dispHandle, MR_ROW_RPA, 0, "RP Addr: %s",
    //               Util_convertBdAddr2Str(pRpaNew));
    memcpy(rpa, pRpaNew, B_ADDR_LEN);
  }
}

/*********************************************************************
 * @fn      multi_role_clockHandler
 *
 * @brief   Handler function for clock timeouts.
 *
 * @param   arg - event type
 *
 * @return  None.
 */
static void multi_role_clockHandler(UArg arg)
{
  mrClockEventData_t *pData = (mrClockEventData_t *)arg;

  if (pData->event == MR_EVT_PERIODIC)
  {
    // Start the next period
    //Util_startClock(&clkPeriodic);

    // Send message to perform periodic task
    multi_role_enqueueMsg(MR_EVT_PERIODIC, NULL);
  }
  else if (pData->event == MR_EVT_OBDD_PERIODIC)
  {
    // Start the next period
    Util_startClock(&BJJA_LM_OBDD_clkPeriodic);

    // Send message to perform periodic task
    multi_role_enqueueMsg(MR_EVT_OBDD_PERIODIC, NULL);
  }
  else if (pData->event == MR_EVT_READ_RPA)
  {
    // Start the next period
    Util_startClock(&clkRpaRead);

    // Send message to read the current RPA
    multi_role_enqueueMsg(MR_EVT_READ_RPA, NULL);
  }
  else if (pData->event == MR_EVT_SEND_PARAM_UPDATE)
  {
    // Send message to app
    multi_role_enqueueMsg(MR_EVT_SEND_PARAM_UPDATE, pData);
  }
}

/*********************************************************************
* @fn      multi_role_keyChangeHandler
*
* @brief   Key event handler function
*
* @param   a0 - ignored
*
* @return  none
*/
static void multi_role_keyChangeHandler(uint8_t keys)
{
  uint8_t *pValue = ICall_malloc(sizeof(uint8_t));

  if (pValue)
  {
    *pValue = keys;

    multi_role_enqueueMsg(MR_EVT_KEY_CHANGE, pValue);
  }
}

/*********************************************************************
* @fn      multi_role_handleKeys
*
* @brief   Handles all key events for this device.
*
* @param   keys - bit field for key events. Valid entries:
*                 HAL_KEY_SW_2
*                 HAL_KEY_SW_1
*
* @return  none
*/
static void multi_role_handleKeys(uint8_t keys)
{
  uint32_t rtnVal = 0;
  if (keys & KEY_LEFT)
  {
    // Check if the key is still pressed
    if (PIN_getInputValue(CONFIG_PIN_BTN1) == 0)
    {
      //tbm_buttonLeft();
    }
  }
  else if (keys & KEY_RIGHT)
  {
    // Check if the key is still pressed
    rtnVal = PIN_getInputValue(CONFIG_PIN_BTN2);
    if (rtnVal == 0)
    {
      //tbm_buttonRight();
    }
  }
}

/*********************************************************************
* @fn      multi_role_processGATTDiscEvent
*
* @brief   Process GATT discovery event
*
* @param   pMsg - pointer to discovery event stack message
*
* @return  none
*/
static void multi_role_processGATTDiscEvent(gattMsgEvent_t *pMsg)
{
  PRINT_DATA("%s,line:%d\r\n",__FUNCTION__,__LINE__);
  uint8_t connIndex = multi_role_getConnIndex(pMsg->connHandle);
  MULTIROLE_ASSERT(connIndex < MAX_NUM_BLE_CONNS);

  if (connList[connIndex].discState == BLE_DISC_STATE_MTU)
  {
    // MTU size response received, discover simple service
    if (pMsg->method == ATT_EXCHANGE_MTU_RSP)
    {
#if 0
      uint8_t uuid[ATT_BT_UUID_SIZE] = { LO_UINT16(SIMPLEPROFILE_SERV_UUID),
                                         HI_UINT16(SIMPLEPROFILE_SERV_UUID) };

      connList[connIndex].discState = BLE_DISC_STATE_SVC;

      // Discovery simple service
      VOID GATT_DiscPrimaryServiceByUUID(pMsg->connHandle, uuid,
                                         ATT_BT_UUID_SIZE, selfEntity);
#else
      //service uuid 128bit
      uint8_t uuid[ATT_UUID_SIZE] ={0x00};
      for(uint8_t i=0;i<ATT_UUID_SIZE;i++)
      {
        uuid[i] = gService_uuid[i];
      }
      connList[connIndex].discState= BLE_DISC_STATE_SVC;
      // Discovery of simple service
      VOID GATT_DiscPrimaryServiceByUUID(pMsg->connHandle, uuid, ATT_UUID_SIZE,
                                         selfEntity); 
#endif
    }
    PRINT_DATA("%s,line:%d\r\n",__FUNCTION__,__LINE__);
  }
  else if (connList[connIndex].discState == BLE_DISC_STATE_SVC)
  {
    PRINT_DATA("%s,line:%d\r\n",__FUNCTION__,__LINE__);
    // Service found, store handles
    if (pMsg->method == ATT_FIND_BY_TYPE_VALUE_RSP &&
        pMsg->msg.findByTypeValueRsp.numInfo > 0)
    {
      PRINT_DATA("%s,line:%d\r\n",__FUNCTION__,__LINE__);
      svcStartHdl = ATT_ATTR_HANDLE(pMsg->msg.findByTypeValueRsp.pHandlesInfo, 0);
      svcEndHdl = ATT_GRP_END_HANDLE(pMsg->msg.findByTypeValueRsp.pHandlesInfo, 0);
    }
    PRINT_DATA("%s,line:%d\r\n",__FUNCTION__,__LINE__);
    // If procedure complete
    if (((pMsg->method == ATT_FIND_BY_TYPE_VALUE_RSP) &&
         (pMsg->hdr.status == bleProcedureComplete))  ||
        (pMsg->method == ATT_ERROR_RSP))
    {
      if (svcStartHdl != 0)
      {
        PRINT_DATA("%s,line:%d\r\n",__FUNCTION__,__LINE__);
        attReadByTypeReq_t req;

        // Discover characteristic
        connList[connIndex].discState = BLE_DISC_STATE_CHAR;

        req.startHandle = svcStartHdl;
        req.endHandle = svcEndHdl;
        /*req.type.len = ATT_BT_UUID_SIZE;
        req.type.uuid[0] = LO_UINT16(SIMPLEPROFILE_CHAR1_UUID);
        req.type.uuid[1] = HI_UINT16(SIMPLEPROFILE_CHAR1_UUID);*/

        //weli 128bit
        req.type.len = ATT_UUID_SIZE;
        for(uint8_t i=0;i<ATT_UUID_SIZE;i++)
        {
          req.type.uuid[i] =gNoti_uuid[i];
        }

        VOID GATT_DiscCharsByUUID(pMsg->connHandle, &req, selfEntity);
      }
    }
  }
  else if (connList[connIndex].discState == BLE_DISC_STATE_CHAR)
  {
    // Characteristic found, store handle
    if ((pMsg->method == ATT_READ_BY_TYPE_RSP) &&
        (pMsg->msg.readByTypeRsp.numPairs > 0))
    {
      uint8_t connIndex = multi_role_getConnIndex(mrConnHandle);

      // connIndex cannot be equal to or greater than MAX_NUM_BLE_CONNS
      MULTIROLE_ASSERT(connIndex < MAX_NUM_BLE_CONNS);

      // Store the handle of the simpleprofile characteristic 1 value
#if 0
      connList[connIndex].charHandle
        = BUILD_UINT16(pMsg->msg.readByTypeRsp.pDataList[3],
                       pMsg->msg.readByTypeRsp.pDataList[4]);
#endif
      //Display_printf(dispHandle, MR_ROW_CUR_CONN, 0, "Simple Svc Found");


      PRINT_DATA("%s:%d\r\n",__FUNCTION__,__LINE__);
      uint8_t *pairs = &pMsg->msg.readByTypeRsp.pDataList[3];
      uint16_t numPairs = pMsg->msg.readByTypeRsp.numPairs;
      uint16_t pairLen = pMsg->msg.readByTypeRsp.len;
      uint16_t tmp_charHdl=0;
      PRINT_DATA("%s:%d->len:%d,numPairs->%d\r\n",__FUNCTION__,__LINE__,pairLen,numPairs);

      for (uint16_t idx = 0; idx < numPairs; ++idx, pairs += pairLen)
      {
        uint16_t handle = *(uint16_t *)&pairs[0];
        uint16_t uuid = *(uint16_t *)&pairs[2];
        tmp_charHdl = handle;
        uint8_t i=0;
        for(i=0;i<ATT_UUID_SIZE;i++)
        {
          if(pairs[2+i]==gWrite_uuid[i])
            continue;
          else
            break;
        }
        if(i==ATT_UUID_SIZE)
        {
          PRINT_DATA("find the write char handler 128bits\r\n");
          //discInfo[connIndex].charHdl = tmp_charHdl;//20200519 fixed abnormal bug old version
          connList[connIndex].gNotify_charHdl = tmp_charHdl;//20200519 fixed abnormal bug
        }
        for(i=0;i<ATT_UUID_SIZE;i++)
        {
          if(pairs[2+i]==gNoti_uuid[i])
            continue;
          else
            break;
        }
        if(i==ATT_UUID_SIZE)
        {
          PRINT_DATA("find the notify char handler 128bits\r\n");
          //discInfo[connIndex].gNotify_charHdl = tmp_charHdl;//20200519 fixed abnormal bug old version
          connList[connIndex].charHandle = tmp_charHdl;//20200519 fixed abnormal bug
        }
      }
      PRINT_DATA("%s:%d\r\n",__FUNCTION__,__LINE__);

      // Now we can use GATT Read/Write
      PRINT_DATA("Simple Svc Found\r\n");
      //tbm_setItemStatus(&mrMenuPerConn,
      //                  MR_ITEM_GATTREAD | MR_ITEM_GATTWRITE, MR_ITEM_NONE);
      gBJJA_LM_Obd_state=O_Notify;
      gWaitCount=1;
      gConnTimeout_count=1;
    }

    connList[connIndex].discState = BLE_DISC_STATE_IDLE;
  }
}

/*********************************************************************
* @fn      multi_role_getConnIndex
*
* @brief   Translates connection handle to index
*
* @param   connHandle - the connection handle
*
 * @return  the index of the entry that has the given connection handle.
 *          if there is no match, MAX_NUM_BLE_CONNS will be returned.
*/
static uint16_t multi_role_getConnIndex(uint16_t connHandle)
{
  uint8_t i;
  // Loop through connection
  for (i = 0; i < MAX_NUM_BLE_CONNS; i++)
  {
    // If matching connection handle found
    if (connList[i].connHandle == connHandle)
    {
      return i;
    }
  }

  // Not found if we got here
  return(MAX_NUM_BLE_CONNS);
}

#ifndef Display_DISABLE_ALL
/*********************************************************************
 * @fn      multi_role_getConnAddrStr
 *
 * @brief   Return, in string form, the address of the peer associated with
 *          the connHandle.
 *
 * @return  A null-terminated string of the address.
 *          if there is no match, NULL will be returned.
 */
static char* multi_role_getConnAddrStr(uint16_t connHandle)
{
  uint8_t i;

  for (i = 0; i < MAX_NUM_BLE_CONNS; i++)
  {
    if (connList[i].connHandle == connHandle)
    {
      return Util_convertBdAddr2Str(connList[i].addr);
    }
  }

  return NULL;
}
#endif

/*********************************************************************
 * @fn      multi_role_clearConnListEntry
 *
 * @brief   clear device list by connHandle
 *
 * @return  SUCCESS if connHandle found valid index or bleInvalidRange
 *          if index wasn't found. LINKDB_CONNHANDLE_ALL will always succeed.
 */
static uint8_t multi_role_clearConnListEntry(uint16_t connHandle)
{
  uint8_t i;
  // Set to invalid connection index initially
  uint8_t connIndex = MAX_NUM_BLE_CONNS;

  if(connHandle != LINKDB_CONNHANDLE_ALL)
  {
    connIndex = multi_role_getConnIndex(connHandle);
    // Get connection index from handle
    if(connIndex >= MAX_NUM_BLE_CONNS)
    {
      return bleInvalidRange;
    }
  }

  // Clear specific handle or all handles
  for(i = 0; i < MAX_NUM_BLE_CONNS; i++)
  {
    if((connIndex == i) || (connHandle == LINKDB_CONNHANDLE_ALL))
    {
      connList[i].connHandle = LINKDB_CONNHANDLE_INVALID;
      connList[i].charHandle = 0;
      connList[i].discState  =  0;
    }
  }

  return SUCCESS;
}


/************************************************************************
* @fn      multi_role_pairStateCB
*
* @param   connHandle - the connection handle
*
* @param   state - pairing state
*
* @param   status - status of pairing state
*
* @return  none
*/
static void multi_role_pairStateCB(uint16_t connHandle, uint8_t state,
                                   uint8_t status)
{
  mrPairStateData_t *pData = ICall_malloc(sizeof(mrPairStateData_t));

  // Allocate space for the event data.
  if (pData)
  {
    pData->state = state;
    pData->connHandle = connHandle;
    pData->status = status;

    // Queue the event.
    if (multi_role_enqueueMsg(MR_EVT_PAIRING_STATE, pData) != SUCCESS)
    {
      ICall_free(pData);
    }
  }
}

/*********************************************************************
* @fn      multi_role_passcodeCB
*
* @brief   Passcode callback.
*
* @param   deviceAddr - pointer to device address
*
* @param   connHandle - the connection handle
*
* @param   uiInputs - pairing User Interface Inputs
*
* @param   uiOutputs - pairing User Interface Outputs
*
* @param   numComparison - numeric Comparison 20 bits
*
* @return  none
*/
static void multi_role_passcodeCB(uint8_t *deviceAddr, uint16_t connHandle,
                                  uint8_t uiInputs, uint8_t uiOutputs,
                                  uint32_t numComparison)
{
  mrPasscodeData_t *pData = ICall_malloc(sizeof(mrPasscodeData_t));

  // Allocate space for the passcode event.
  if (pData)
  {
    pData->connHandle = connHandle;
    memcpy(pData->deviceAddr, deviceAddr, B_ADDR_LEN);
    pData->uiInputs = uiInputs;
    pData->uiOutputs = uiOutputs;
    pData->numComparison = numComparison;

    // Enqueue the event.
    if (multi_role_enqueueMsg(MR_EVT_PASSCODE_NEEDED, pData) != SUCCESS)
    {
      ICall_free(pData);
    }
  }
}

/*********************************************************************
* @fn      multi_role_processPairState
*
* @brief   Process the new paring state.
*
* @param   pairingEvent - pairing event received from the stack
*
* @return  none
*/
static void multi_role_processPairState(mrPairStateData_t *pPairData)
{
  uint8_t state = pPairData->state;
  uint8_t status = pPairData->status;

  switch (state)
  {
    case GAPBOND_PAIRING_STATE_STARTED:
      //Display_printf(dispHandle, MR_ROW_SECURITY, 0, "Pairing started");
      break;

    case GAPBOND_PAIRING_STATE_COMPLETE:
      if (status == SUCCESS)
      {
        linkDBInfo_t linkInfo;

        //Display_printf(dispHandle, MR_ROW_SECURITY, 0, "Pairing success");

        if (linkDB_GetInfo(pPairData->connHandle, &linkInfo) == SUCCESS)
        {
          // If the peer was using private address, update with ID address
          if ((linkInfo.addrType == ADDRTYPE_PUBLIC_ID ||
               linkInfo.addrType == ADDRTYPE_RANDOM_ID) &&
              !Util_isBufSet(linkInfo.addrPriv, 0, B_ADDR_LEN))

          {
            // Update the address of the peer to the ID address
            //Display_printf(dispHandle, MR_ROW_NON_CONN, 0, "Addr updated: %s",
            //               Util_convertBdAddr2Str(linkInfo.addr));

            // Update the connection list with the ID address
            uint8_t i = multi_role_getConnIndex(pPairData->connHandle);

            MULTIROLE_ASSERT(i < MAX_NUM_BLE_CONNS);
            memcpy(connList[i].addr, linkInfo.addr, B_ADDR_LEN);
          }
        }
      }
      else
      {
        //Display_printf(dispHandle, MR_ROW_SECURITY, 0, "Pairing fail: %d", status);
      }
      break;

    case GAPBOND_PAIRING_STATE_ENCRYPTED:
      if (status == SUCCESS)
      {
        //Display_printf(dispHandle, MR_ROW_SECURITY, 0, "Encryption success");
      }
      else
      {
///        Display_printf(dispHandle, MR_ROW_SECURITY, 0, "Encryption failed: %d", status);
      }
      break;

    case GAPBOND_PAIRING_STATE_BOND_SAVED:
      if (status == SUCCESS)
      {
        //Display_printf(dispHandle, MR_ROW_SECURITY, 0, "Bond save success");
      }
      else
      {
       // Display_printf(dispHandle, MR_ROW_SECURITY, 0, "Bond save failed: %d", status);
      }

      break;

    default:
      break;
  }
}

/*********************************************************************
* @fn      multi_role_processPasscode
*
* @brief   Process the Passcode request.
*
* @return  none
*/
static void multi_role_processPasscode(mrPasscodeData_t *pData)
{
  // Display passcode to user
  if (pData->uiOutputs != 0)
  {
   // Display_printf(dispHandle, MR_ROW_SECURITY, 0, "Passcode: %d",
   //                B_APP_DEFAULT_PASSCODE);
  }

  // Send passcode response
  GAPBondMgr_PasscodeRsp(pData->connHandle, SUCCESS,
                         B_APP_DEFAULT_PASSCODE);
}

/*********************************************************************
 * @fn      multi_role_startSvcDiscovery
 *
 * @brief   Start service discovery.
 *
 * @return  none
 */
static void multi_role_startSvcDiscovery(void)
{
  uint8_t connIndex = multi_role_getConnIndex(mrConnHandle);

  // connIndex cannot be equal to or greater than MAX_NUM_BLE_CONNS
  MULTIROLE_ASSERT(connIndex < MAX_NUM_BLE_CONNS);

  attExchangeMTUReq_t req;

  // Initialize cached handles
  svcStartHdl = svcEndHdl = 0;

  connList[connIndex].discState = BLE_DISC_STATE_MTU;

  // Discover GATT Server's Rx MTU size
  req.clientRxMTU = mrMaxPduSize - L2CAP_HDR_SIZE;

  // ATT MTU size should be set to the minimum of the Client Rx MTU
  // and Server Rx MTU values
  VOID GATT_ExchangeMTU(mrConnHandle, &req, selfEntity);
  PRINT_DATA("%s,%d\r\n",__FUNCTION__,__LINE__);
}

/*********************************************************************
* @fn      multi_role_addConnInfo
*
* @brief   add a new connection to the index-to-connHandle map
*
* @param   connHandle - the connection handle
*
* @param   addr - pointer to device address
*
* @return  index of connection handle
*/
static uint8_t multi_role_addConnInfo(uint16_t connHandle, uint8_t *pAddr,
                                      uint8_t role)
{
  uint8_t i;

  for (i = 0; i < MAX_NUM_BLE_CONNS; i++)
  {
    if (connList[i].connHandle == LINKDB_CONNHANDLE_INVALID)
    {
      // Found available entry to put a new connection info in
      connList[i].connHandle = connHandle;
      memcpy(connList[i].addr, pAddr, B_ADDR_LEN);
      numConn++;

#ifdef DEFAULT_SEND_PARAM_UPDATE_REQ
      // If a peripheral, start the clock to send a connection parameter update
      if(role == GAP_PROFILE_PERIPHERAL)
      {
        // Allocate data to send through clock handler
        connList[i].pParamUpdateEventData = ICall_malloc(sizeof(mrClockEventData_t) +
                                                         sizeof(uint16_t));
        if(connList[i].pParamUpdateEventData)
        {
          // Set clock data
          connList[i].pParamUpdateEventData->event = MR_EVT_SEND_PARAM_UPDATE;
          *((uint16_t *)connList[i].pParamUpdateEventData->data) = connHandle;

          // Create a clock object and start
          connList[i].pUpdateClock
            = (Clock_Struct*) ICall_malloc(sizeof(Clock_Struct));

          if (connList[i].pUpdateClock)
          {
            Util_constructClock(connList[i].pUpdateClock,
                                multi_role_clockHandler,
                                SEND_PARAM_UPDATE_DELAY, 0, true,
                                (UArg) connList[i].pParamUpdateEventData);
          }
          else
          {
            // Clean up
            ICall_free(connList[i].pParamUpdateEventData);
          }
        }
        else
        {
          // Memory allocation failed
          MULTIROLE_ASSERT(false);
        }
      }
#endif

      break;
    }
  }

  return i;
}

/*********************************************************************
 * @fn      multi_role_clearPendingParamUpdate
 *
 * @brief   clean pending param update request in the paramUpdateList list
 *
 * @param   connHandle - connection handle to clean
 *
 * @return  none
 */
void multi_role_clearPendingParamUpdate(uint16_t connHandle)
{
  List_Elem *curr;

  for (curr = List_head(&paramUpdateList); curr != NULL; curr = List_next(curr)) 
  {
    if (((mrConnHandleEntry_t *)curr)->connHandle == connHandle)
    {
      List_remove(&paramUpdateList, curr);
    }
  }
}

/*********************************************************************
 * @fn      multi_role_removeConnInfo
 *
 * @brief   Remove a device from the connected device list
 *
 * @return  index of the connected device list entry where the new connection
 *          info is removed from.
 *          if connHandle is not found, MAX_NUM_BLE_CONNS will be returned.
 */
static uint8_t multi_role_removeConnInfo(uint16_t connHandle)
{
  uint8_t connIndex = multi_role_getConnIndex(connHandle);

  if(connIndex < MAX_NUM_BLE_CONNS)
  {
    Clock_Struct* pUpdateClock = connList[connIndex].pUpdateClock;

    if (pUpdateClock != NULL)
    {
      // Stop and destruct the RTOS clock if it's still alive
      if (Util_isActive(pUpdateClock))
      {
        Util_stopClock(pUpdateClock);
      }

      // Destruct the clock object
      Clock_destruct(pUpdateClock);
      // Free clock struct
      ICall_free(pUpdateClock);
      // Free ParamUpdateEventData
      ICall_free(connList[connIndex].pParamUpdateEventData);
    }
    // Clear pending update requests from paramUpdateList
    multi_role_clearPendingParamUpdate(connHandle);
    // Clear Connection List Entry
    multi_role_clearConnListEntry(connHandle);
    numConn--;
  }

  return connIndex;
}

/*********************************************************************
* @fn      multi_role_doDiscoverDevices
*
* @brief   Respond to user input to start scanning
*
* @param   index - not used
*
* @return  TRUE since there is no callback to use this value
*/
bool multi_role_doDiscoverDevices(uint8_t index)
{
  (void) index;
  PRINT_DATA("%s,%d\r\n",__FUNCTION__,__LINE__);

#if (DEFAULT_DEV_DISC_BY_SVC_UUID == TRUE)
  // Scanning for DEFAULT_SCAN_DURATION x 10 ms.
  // The stack does not need to record advertising reports
  // since the application will filter them by Service UUID and save.

  // Reset number of scan results to 0 before starting scan
  numScanRes = 0;
  GapScan_enable(0, DEFAULT_SCAN_DURATION, 0);
#else // !DEFAULT_DEV_DISC_BY_SVC_UUID
  // Scanning for DEFAULT_SCAN_DURATION x 10 ms.
  // Let the stack record the advertising reports as many as up to DEFAULT_MAX_SCAN_RES.
  GapScan_enable(0, DEFAULT_SCAN_DURATION, DEFAULT_MAX_SCAN_RES);
#endif // DEFAULT_DEV_DISC_BY_SVC_UUID
  // Enable only "Stop Discovering" and disable all others in the main menu
  //tbm_setItemStatus(&mrMenuMain, MR_ITEM_STOPDISC,
  //                  (MR_ITEM_ALL & ~MR_ITEM_STOPDISC));

  return (true);
}

/*********************************************************************
 * @fn      multi_role_doStopDiscovering
 *
 * @brief   Stop on-going scanning
 *
 * @param   index - item index from the menu
 *
 * @return  always true
 */
bool multi_role_doStopDiscovering(uint8_t index)
{
  (void) index;

  GapScan_disable();

  return (true);
}

/*********************************************************************
 * @fn      multi_role_doCancelConnecting
 *
 * @brief   Cancel on-going connection attempt
 *
 * @param   index - item index from the menu
 *
 * @return  always true
 */
bool multi_role_doCancelConnecting(uint8_t index)
{
  (void) index;

  GapInit_cancelConnect();

  return (true);
}

/*********************************************************************
* @fn      multi_role_doConnect
*
* @brief   Respond to user input to form a connection
*
* @param   index - index as selected from the mrMenuConnect
*
* @return  TRUE since there is no callback to use this value
*/
bool multi_role_doConnect(uint8_t index)
{
  PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
  // Temporarily disable advertising
  GapAdv_disable(advHandle);

#if (DEFAULT_DEV_DISC_BY_SVC_UUID == TRUE)
  /*GapInit_connect(scanList[index].addrType & MASK_ADDRTYPE_ID,
                  scanList[index].addr, mrInitPhy, 0);*/
  GapInit_connect(gOBDII_dev.addrType & MASK_ADDRTYPE_ID,
                  gOBDII_dev.addr, mrInitPhy, 0);
  PRINT_DATA("OBDII dev type:%d,addr:%2x-%2x-%2x-%2x-%2x-%2x\r\n",
    gOBDII_dev.addrType,gOBDII_dev.addr[0],gOBDII_dev.addr[1],gOBDII_dev.addr[2],gOBDII_dev.addr[3],gOBDII_dev.addr[4],gOBDII_dev.addr[5]);
  
  //PRINT_DATA("OBDII dev type:%d,addr:%2x-%2x-%2x-%2x-%2x-%2x\r\n",
  //  scanList[index].addrType,scanList[index].addr[0],scanList[index].addr[1],scanList[index].addr[2],scanList[index].addr[3],scanList[index].addr[4],scanList[index].addr[5]);
#else // !DEFAULT_DEV_DISC_BY_SVC_UUID
  GapScan_Evt_AdvRpt_t advRpt;

  GapScan_getAdvReport(index, &advRpt);

  GapInit_connect(advRpt.addrType & MASK_ADDRTYPE_ID,
                  advRpt.addr, mrInitPhy, 0);
#endif // DEFAULT_DEV_DISC_BY_SVC_UUID

  // Re-enable advertising
  GapAdv_enable(advHandle, GAP_ADV_ENABLE_OPTIONS_USE_MAX , 0);

  // Enable only "Cancel Connecting" and disable all others in the main menu
  //tbm_setItemStatus(&mrMenuMain, MR_ITEM_CANCELCONN,
  //                  (MR_ITEM_ALL & ~MR_ITEM_CANCELCONN));

  //Display_printf(dispHandle, MR_ROW_NON_CONN, 0, "Connecting...");

  //tbm_goTo(&mrMenuMain);//

  return (true);
}

/*********************************************************************
 * @fn      multi_role_doSelectConn
 *
 * @brief   Select a connection to communicate with
 *
 * @param   index - item index from the menu
 *
 * @return  always true
 */
bool multi_role_doSelectConn(uint8_t index)
{
  //uint32_t itemsToDisable = MR_ITEM_NONE;

  // index cannot be equal to or greater than MAX_NUM_BLE_CONNS
  MULTIROLE_ASSERT(index < MAX_NUM_BLE_CONNS);

  mrConnHandle  = connList[index].connHandle;

  if (connList[index].charHandle == 0)
  {
    // Initiate service discovery
    multi_role_enqueueMsg(MR_EVT_SVC_DISC, NULL);

    // Diable GATT Read/Write until simple service is found
    //itemsToDisable = MR_ITEM_GATTREAD | MR_ITEM_GATTWRITE;
  }

  // Set the menu title and go to this connection's context
 // TBM_SET_TITLE(&mrMenuPerConn, TBM_GET_ACTION_DESC(&mrMenuSelectConn, index));

  //tbm_setItemStatus(&mrMenuPerConn, MR_ITEM_NONE, itemsToDisable);

  // Clear non-connection-related message
  //Display_clearLine(dispHandle, MR_ROW_NON_CONN);

  //tbm_goTo(&mrMenuPerConn);

  return (true);
}

/*********************************************************************
 * @fn      multi_role_doGattRead
 *
 * @brief   GATT Read
 *
 * @param   index - item index from the menu
 *
 * @return  always true
 */
bool multi_role_doGattRead(uint8_t index)
{
  attReadReq_t req;
  uint8_t connIndex = multi_role_getConnIndex(mrConnHandle);

  // connIndex cannot be equal to or greater than MAX_NUM_BLE_CONNS
  MULTIROLE_ASSERT(connIndex < MAX_NUM_BLE_CONNS);

  req.handle = connList[connIndex].charHandle;
  GATT_ReadCharValue(mrConnHandle, &req, selfEntity);

  return (true);
}

/*********************************************************************
 * @fn      multi_role_doGattWrite
 *
 * @brief   GATT Write
 *
 * @param   index - item index from the menu
 *
 * @return  always true
 */
bool multi_role_doGattWrite(uint8_t index)
{
  status_t status;
  uint8_t charVals[4] = { 0x00, 0x55, 0xAA, 0xFF }; // Should be consistent with
                                                    // those in scMenuGattWrite
  attWriteReq_t req;

  req.pValue = GATT_bm_alloc(mrConnHandle, ATT_WRITE_REQ, 1, NULL);

  if ( req.pValue != NULL )
  {
    uint8_t connIndex = multi_role_getConnIndex(mrConnHandle);

    // connIndex cannot be equal to or greater than MAX_NUM_BLE_CONNS
    MULTIROLE_ASSERT(connIndex < MAX_NUM_BLE_CONNS);

    req.handle = connList[connIndex].charHandle;
    req.len = 1;
    charVal = charVals[index];
    req.pValue[0] = charVal;
    req.sig = 0;
    req.cmd = 0;

    status = GATT_WriteCharValue(mrConnHandle, &req, selfEntity);
    if ( status != SUCCESS )
    {
      GATT_bm_free((gattMsg_t *)&req, ATT_WRITE_REQ);
    }
  }

  return (true);
}

/*********************************************************************
* @fn      multi_role_doConnUpdate
*
* @brief   Respond to user input to do a connection update
*
* @param   index - index as selected from the mrMenuConnUpdate
*
* @return  TRUE since there is no callback to use this value
*/
bool multi_role_doConnUpdate(uint8_t index)
{
  gapUpdateLinkParamReq_t params;

  (void) index; //may need to get the real connHandle?

  params.connectionHandle = mrConnHandle;
  params.intervalMin = DEFAULT_UPDATE_MIN_CONN_INTERVAL;
  params.intervalMax = DEFAULT_UPDATE_MAX_CONN_INTERVAL;
  params.connLatency = DEFAULT_UPDATE_SLAVE_LATENCY;

  linkDBInfo_t linkInfo;
  if (linkDB_GetInfo(mrConnHandle, &linkInfo) == SUCCESS)
  {
    if (linkInfo.connTimeout == DEFAULT_UPDATE_CONN_TIMEOUT)
    {
      params.connTimeout = DEFAULT_UPDATE_CONN_TIMEOUT + 200;
    }
    else
    {
      params.connTimeout = DEFAULT_UPDATE_CONN_TIMEOUT;
    }

    GAP_UpdateLinkParamReq(&params);

    //Display_printf(dispHandle, MR_ROW_CUR_CONN, 0, "Param update Request:connTimeout =%d",
    //                params.connTimeout*CONN_TIMEOUT_MS_CONVERSION);
  }
  else
  {

    //Display_printf(dispHandle, MR_ROW_CUR_CONN, 0,
     //              "update :%s, Unable to find link information",
   //                Util_convertBdAddr2Str(linkInfo.addr));
  }

  return (true);
}

/*********************************************************************
 * @fn      multi_role_doConnPhy
 *
 * @brief   Set Connection PHY preference.
 *
 * @param   index - item number in MRMenu_connPhy list
 *
 * @return  always true
 */
bool multi_role_doConnPhy(uint8_t index)
{
  // Set Phy Preference on the current connection. Apply the same value
  // for RX and TX. For more information, see the LE 2M PHY section in the User's Guide:
  // http://software-dl.ti.com/lprf/ble5stack-latest/
  // Note PHYs are already enabled by default in build_config.opt in stack project.
  //HCI_LE_SetPhyCmd(mrConnHandle, 0, MRMenu_connPhy[index].value, MRMenu_connPhy[index].value, 0);

  ///Display_printf(dispHandle, MR_ROW_CUR_CONN, 0, "Connection PHY preference: %s",
     //            TBM_GET_ACTION_DESC(&mrMenuConnPhy, index));

  return (true);
}

/*********************************************************************
 * @fn      multi_role_doSetInitPhy
 *
 * @brief   Set initialize PHY preference.
 *
 * @param   index - item number in MRMenu_initPhy list
 *
 * @return  always true
 */
bool multi_role_doSetInitPhy(uint8_t index)
{
  //mrInitPhy = MRMenu_initPhy[index].value;
  //Display_printf(dispHandle, MR_ROW_CUR_CONN, 0, "Initialize PHY preference: %s",
  //               TBM_GET_ACTION_DESC(&mrMenuInitPhy, index));

  return (true);
}

/*********************************************************************
 * @fn      multi_role_doSetScanPhy
 *
 * @brief   Set PHYs for scanning.
 *
 * @param   index - item number in MRMenu_scanPhy list
 *
 * @return  always true
 */
bool multi_role_doSetScanPhy(uint8_t index)
{
  // Set scanning primary PHY
  //GapScan_setParam(SCAN_PARAM_PRIM_PHYS, &MRMenu_scanPhy[index].value);

//  Display_printf(dispHandle, MR_ROW_NON_CONN, 0, "Primary Scan PHY: %s",
 //                TBM_GET_ACTION_DESC(&mrMenuScanPhy, index));

  return (true);
}

/*********************************************************************
 * @fn      multi_role_doSetAdvPhy
 *
 * @brief   Set advertise PHY preference.
 *
 * @param   index - item number in MRMenu_advPhy list
 *
 * @return  always true
 */
bool multi_role_doSetAdvPhy(uint8_t index)
{
  uint16_t props;
  GapAdv_primaryPHY_t phy;
  bool isAdvActive = mrIsAdvertising;
  /*
  switch (MRMenu_advPhy[index].value)
  {
    case MR_ADV_LEGACY_PHY_1_MBPS:
        props = GAP_ADV_PROP_CONNECTABLE | GAP_ADV_PROP_SCANNABLE | GAP_ADV_PROP_LEGACY;
        phy = GAP_ADV_PRIM_PHY_1_MBPS;     
    break;
    case MR_ADV_EXT_PHY_1_MBPS:
        props = GAP_ADV_PROP_CONNECTABLE;
        phy = GAP_ADV_PRIM_PHY_1_MBPS;
    break;
    case MR_ADV_EXT_PHY_CODED:
        props = GAP_ADV_PROP_CONNECTABLE;
        phy = GAP_ADV_PRIM_PHY_CODED_S2;
    break;
    default:
        return (false);
  }
  if (isAdvActive)
  {
    // Turn off advertising
    GapAdv_disable(advHandle);
  }
  GapAdv_setParam(advHandle,GAP_ADV_PARAM_PROPS,&props);
  GapAdv_setParam(advHandle,GAP_ADV_PARAM_PRIMARY_PHY,&phy);
  GapAdv_setParam(advHandle,GAP_ADV_PARAM_SECONDARY_PHY,&phy);
  if (isAdvActive)
  {
    // Turn on advertising
    GapAdv_enable(advHandle, GAP_ADV_ENABLE_OPTIONS_USE_MAX , 0);
  }
  
 // Display_printf(dispHandle, MR_ROW_CUR_CONN, 0, "Advertise PHY preference: %s",
  //               TBM_GET_ACTION_DESC(&mrMenuAdvPhy, index));
*/
  return (true);
}

/*********************************************************************
* @fn      multi_role_doDisconnect
*
* @brief   Respond to user input to terminate a connection
*
* @param   index - index as selected from the mrMenuConnUpdate
*
* @return  always true
*/
bool multi_role_doDisconnect(uint8_t index)
{
  (void) index;

  // Disconnect
  GAP_TerminateLinkReq(mrConnHandle, HCI_DISCONNECT_REMOTE_USER_TERM);

  return (true);
}

/*********************************************************************
* @fn      multi_role_doAdvertise
*
* @brief   Respond to user input to terminate a connection
*
* @param   index - index as selected from the mrMenuConnUpdate
*
* @return  always true
*/
bool multi_role_doAdvertise(uint8_t index)
{
  (void) index;

  // If we're currently advertising
  if (mrIsAdvertising)
  {
    // Turn off advertising
    GapAdv_disable(advHandle);
  }
  // If we're not currently advertising
  else
  {
    if (numConn < MAX_NUM_BLE_CONNS)
    {
      // Start advertising since there is room for more connections
      GapAdv_enable(advHandle, GAP_ADV_ENABLE_OPTIONS_USE_MAX , 0);
    }
    else
    {
   //   Display_printf(dispHandle, MR_ROW_ADVERTIS, 0,
   //                  "At Maximum Connection Limit, Cannot Enable Advertisment");
    }
  }

  return (true);
}


/*********************************************************************
*********************************************************************/
void BJJA_LM_subg_early_init()
{
  //Display_printf(dispHandle, MR_ROW_ADVERTIS, 0, "[weli]%s-begin\n", __FUNCTION__);
  BJJA_LM_Sub1G_init();
 // Display_printf(dispHandle, MR_ROW_ADVERTIS, 0, "[weli]%s-end\n", __FUNCTION__);
  Util_startClock(&BJJA_LM_OBDD_clkPeriodic);
  Util_startClock(&clkPeriodic);//check INGI whether ON

  
  if(gBJJA_LM_State_machine==Pwr_on)
    gBJJA_LM_State_machine=Idle;
  
  
}
void BJJA_LM_subg_semphore_init()
{
  Semaphore_Params semParams;
  Semaphore_Params_init(&semParams);
  gSem = Semaphore_create(0, &semParams, NULL); /* Memory allocated in here */
  if (gSem == NULL) /* Check if the handle is valid */
  {
   // Display_printf(dispHandle, MR_ROW_ADVERTIS, 0, "[weli]%s-Semaphore could not be created\n", __FUNCTION__);
  }
  //Semaphore_pend(gSem,BIOS_WAIT_FOREVER); //pending
  //Semaphore_post(gSem);//unlock 
}
uint8_t obdd_timer_flag=0x00;
static void BJJA_LM_OBDD_performPeriodicTask(void)
{
  if(gBJJA_LM_Obd_state==O_Running && OBDII_current_index<OBD_MAX_CMD)
  {
    obdd_timer_flag++;
    if(obdd_timer_flag%(myOBD[OBDII_current_index].timer+1)==0)
    {
      PRINT_DATA(">>>>%d,%s\r\n",myOBD[OBDII_current_index].cmd_len,myOBD[OBDII_current_index].cmd_name);
      cansec_Write2Periphearl(0,myOBD[OBDII_current_index].cmd_len,myOBD[OBDII_current_index].cmd_name);
      OBDII_current_index++;
      obdd_timer_flag=0;
    }
  }
  BJJA_LM_tick_wdt();
}
void BJJA_LM_subg_createTask(void)
{
  Task_Params taskParams;

  // Configure task
  Task_Params_init(&taskParams);
  taskParams.stack = gSubgTaskStack;
  taskParams.stackSize = MR_TASK_STACK_SUBG_SIZE;
  taskParams.priority = MR_TASK_PRIORITY;//0low,

  Task_construct(&gSubgTask, BJJA_LM_subg_taskFxn, &taskParams, NULL);
}
static void BJJA_LM_subg_taskFxn(UArg a0, UArg a1)
{
  BJJA_LM_subg_semphore_init();
  for (;;)
  {
    Semaphore_pend(gSem,BIOS_WAIT_FOREVER); //pending
    //Display_printf(dispHandle, MR_ROW_ADVERTIS, 0, "[weli]%s-begin\n", __FUNCTION__);
    BJJA_LM_early_send_cmd();
    //Display_printf(dispHandle, MR_ROW_ADVERTIS, 0, "[weli]%s-end\n", __FUNCTION__);
  }
}
void Weli_UartChangeCB()
{
  //SimplePeripheral_enqueueMsg(SBP_UART_INCOMING_EVT, NULL);
  if(gProduceFlag==0)
  {
    if(get_queue()==0)
    {
      bjja_lm_uart_data_t *pData = ICall_malloc(sizeof(bjja_lm_uart_data_t));
      pData->data_len = gSerialLen;
      //pData->pBuf = serialBuffer
      uint8_t i=0;
      for(i=0;i<gSerialLen;i++)
        pData->pBuf[i] = serialBuffer[i];
      //PRINT_DATA("trigger get_queue:%d,%s\r\n",gSerialLen,serialBuffer);
      //multi_role_enqueueMsg(SBP_UART_INCOMING_EVT,NULL);
      if (multi_role_enqueueMsg(SBP_UART_INCOMING_EVT, pData) != SUCCESS)
      {
        ICall_free(pData);
      }
      /*for(i=0;i<gSerialLen;i++)
      {
        //if(strncmp("+QGPSLOC: ",pch,strlen("+QGPSLOC: "))==0)//GPS LAT
        if(serialBuffer[i]=='+' && serialBuffer[i+1]=='Q' && serialBuffer[i+2]=='G'
          && serialBuffer[i+3]=='P' && serialBuffer[i+4]=='S'
          && serialBuffer[i+5]=='L' && serialBuffer[i+6]=='O'
          && serialBuffer[i+7]=='C' && serialBuffer[i+8]==':') 
          {
            gLastGpsLat_flag=0;
          } 
      }*/
      
    }
  }
}
void clear_uart()
{
#if 1
  VOID memset(serialBuffer, 0, sizeof(serialBuffer));
  //sprintf(serialBuffer,"");
    gSerialLen=0x00;
    gProduceFlag=0;
  Weli_UartChangeCB();
#endif
}
void BJJA_parsing_AT_cmd_send_data(uint8 *pBuf,uint8 pBuf_len)
{
  gProduceFlag=1;
  //if(gEnableLog)
  //UartMessage("LTE-M>>",7);
  //UartMessage(serialBuffer,gSerialLen);
  PRINT_DATA("from LTE-M>>%d>>%s\r\n",pBuf_len,pBuf);
  /*if(strncmp(serialBuffer,"AT+SKEY=?",strlen("AT+SKEY=?"))==0)
  {
    uint8_t data[24]={0x00};
    sprintf(data,"OK+SKEY=%d\r\n",gPasskey);
    UartMessage(data,strlen(data));
  }*/
  char *my_ret;
  my_ret = strstr(pBuf,"+QMTRECV:");
  if(my_ret!=NULL)
  {
    parsing_mqtt_return_cmd(my_ret);
    gProduceFlag=0;
    return;
  }

  //split token
  char * pch;
  //printf ("Splitting string \"%s\" into tokens:\n",str);
  char *delim = "\n";
  pch = strtok(pBuf,delim);
  while (pch != NULL)
  {
    //BJJA_write_data_LOG("\r\n>>4G>>",strlen("\r\n>>4G>>"));
    PRINT_DATA(pch);
    //BJJA_write_data_LOG("<<4G<<\r\n",strlen("<<4G<<\r\n"));
    //if(strncmp(gUART4_rx_buffer,"AT+4G_TEST",10)==0)
    if(strncmp("+CSQ:",pch,5)==0)
    {
      char * str_val;
      char *dot = ",";
      str_val = strtok(pch+5,dot);
      if(str_val!=NULL)
      {
        gCsq_val = atoi(str_val);
        PRINT_DATA("CSQ:%d\r\n",gCsq_val);
      }
    }
    else if(strncmp("+QMTOPEN: 0,0",pch,strlen("+QMTOPEN: 0,0"))==0)//AT+QMTOPEN OK
    {
      PRINT_DATA("MQTT early init OK\r\n");
      SEND_LTE_M("AT+QMTCONN=0,\"%02x%02x%02x%02x%02x%02x\"\r\n",gMac[0],gMac[1],gMac[2],gMac[3],gMac[4],gMac[5]);
      //SEND_LTE_M("AT+QMTCONN=0,\"%s\"\r\n",/*gFlash_data.mqtt_user*/"HiwOk5pXCdBy9drsR6bD");
    }
    else if(strncmp("+QMTCONN: 0,0,0",pch,strlen("+QMTCONN: 0,0,0"))==0)//AT+QMTOPEN OK
    {
      PRINT_DATA("MQTT Init OK,change Status:TELCOMM_STATUS_ONLINE\r\n");
      SEND_LTE_M("AT+QMTSUB=0,1,\"/AVIS/%02x%02x%02x%02x%02x%02x/downlink\",0\r\n",gMac[0],gMac[1],gMac[2],gMac[3],gMac[4],gMac[5]);
      g4GStatus = TELCOMM_STATUS_ONLINE;//direct online
    }
    else if(strncmp("+CREG: ",pch,strlen("+CREG: "))==0)//AT+CREG
    {
      if(pch[9]=='1' || pch[9]=='5')
      {
        if(g4GStatus == TELCOMM_STATUS_4G_DETECTED_OK)
        {
            PRINT_DATA("change to early online\r\n");
            g4GStatus = TELCOMM_STATUS_EARLY_ONLINE;
        }
        else
        {
          PRINT_DATA("4G get operator OK\r\n");
        }
        g4G_detect_retry_count=0;
        
      }
      else
      {
        g4G_detect_retry_count++;
        PRINT_DATA("4G get operator fail:%d<5\r\n",g4G_detect_retry_count);
        if(g4G_connection_retry_count>5)
        {
          g4GStatus = TELCOMM_STATUS_OFFLINE;
          PRINT_DATA("4G get operator fail todo action\r\n");
        }
        
      }
      
    }
    /*else if(strncmp("+QGPSLOC: ",pch,strlen("+QGPSLOC: "))==0)//GPS LAT
    {
      gLastGpsLat_flag=0;
      //todo
      uint8_t gps_string_1[32]={0x00};
      uint8_t gps_string_2[32]={0x00};

      char * str_val;
      char *dot = ",";
      str_val = strtok(pch,dot);
      if(str_val!=NULL)
      {
        str_val = strtok(NULL,dot);
        if(str_val!=NULL)
        {
          sprintf(gps_string_1,"%s",str_val);
          str_val = strtok(NULL,dot);
          if(str_val!=NULL)
          {
            sprintf(gps_string_2,"%s",str_val);
            PRINT_DATA("get gps data:%s,%s\r\n",gps_string_1,gps_string_2);
            
            uint8_t i=0;
            for(i=0;i<32;i++)
            {
              if(gps_string_1[i]=='N')//2238.3620N
                gps_string_1[i]=0x00;//remove 'N'
              if(gps_string_2[i]=='E')//12021.4064E
                gps_string_1[i]=0x00;//remove 'E'
            }
            float val1 = atof(gps_string_1);
            float val2 = atof(gps_string_2);
            int16_t ival1 = val1/100;
            int16_t ival2 = val2/100;
            val1 = ((val1-(ival1*100))/60)+ival1;
            val2 = ((val2-(ival2*100))/60)+ival2;
            uint8_t test_data[64]={0x00};
            sprintf(test_data,"gps final:%4.4f,%4.4f,/100 %d,%d\r\n",val1,val2,ival1,ival2);
            //PRINT_DATA("gps raw:%4.4f,%4.4f,/100 %d,%d\r\n",val1,val2,ival1,ival2);
            PRINT_DATA(test_data);
            



            //sprintf(gLastGpsLat,"%s,%s",gps_string_1,gps_string_2);//2238.3620N,12021.4064E
            sprintf(gLastGpsLat,"%fN,%fE",val1,val2);//2238.3620N,12021.4064E

          } 
        }
      }
    }
    else if(strncmp("+CME ERROR: 505",pch,strlen("+CME ERROR: 505"))==0)//NO GPS
    {
      sprintf(gLastGpsLat,"ERR:GPS_NOT_ENABLE\r\n");
      BJJA_LM_enable_gps();
    }
    else if(strncmp("+CME ERROR: 516",pch,strlen("+CME ERROR: 516"))==0)//NO GPS
    {
      sprintf(gLastGpsLat,"ERR:GPS_NOT_FIXED\r\n");
    }*/
    else if(strncmp("+CME ERROR: 13",pch,strlen("+CME ERROR: 13"))==0)//AT+CCID
    {
      g4GStatus = TELCOMM_STATUS_4G_DETECTED_FAIL;
      PRINT_DATA("SIM detected FAIL\r\n");
    }
    else if(strncmp("+CCID:",pch,strlen("+CCID:"))==0)//AT+CCID
    {
      g4GStatus = TELCOMM_STATUS_4G_DETECTED_OK;
      PRINT_DATA("SIM detected OK\r\n");
    }
    /*else if(strncmp("+QIACT:",pch,strlen("+QIACT:"))==0)//AT+QIACT
    {
      //gAutoMode_reconnecting_count=0;//reset auto mode max retry count
      if(g4GStatus==TELCOMM_STATUS_ONLINE)
      {
        PRINT_DATA("already state machine is ONLINE\r\n");
      }
      else
      {
        PRINT_DATA("change to early online\r\n");
        g4GStatus = TELCOMM_STATUS_EARLY_ONLINE;
      }
      PRINT_DATA("4G get PDP OK\r\n");
      gPDP_waiting_count=0;
    }*/
    else if(strncmp("+QPING: 561",pch,strlen("+QPING: 561"))==0)//AT+QPING fail
    {
      PRINT_DATA("4G ping fail now is offline\r\n");
      //+QPING: 561
      g4GStatus = TELCOMM_STATUS_OFFLINE;
    }
    
    else if(strncmp("+QMTPUBEX: 1,0,0",pch,strlen("+QMTPUBEX: 1,0,0"))==0)
    {
      //gLED_Auto_off=0;//clear turn off led blue flag ,so led blue will keep on
      //gMQTT_received_flag--;
      PRINT_DATA("received MQTT OK message\r\n");
      gAutoMode_reconnecting_count=0;//reset auto mode max retry count
    }
    else if(strncmp("+QMTPUB: 0,0,0",pch,strlen("+QMTPUB: 0,0,0"))==0)
    {
      //gLED_Auto_off=0;//clear turn off led blue flag ,so led blue will keep on
      //gMQTT_received_flag--;
      PRINT_DATA("received MQTT OK message\r\n");
      gAutoMode_reconnecting_count=0;//reset auto mode max retry count
    }
    else if(strncmp("ERROR",pch,strlen("ERROR"))==0)//for fix issue 388
    {
      //gMQTT_received_flag=10;
      if(gAutoMode_reconnecting_count>0)
      {
        gAutoMode_reconnecting_count++;
        PRINT_DATA("MQTT send ERROR count:%d\r\n",gAutoMode_reconnecting_count);
        if(gAutoMode_reconnecting_count>5)
        {
          PRINT_DATA("Reconnect 4G because MQTT send ERROR\r\n");
          gAutoMode_reconnecting_count =0;
          BJJA_reconnect_4G();
        }
      }
    }
    else if(strncmp("+QMTSTAT: 0,1",pch,13)==0)
    {//Bug #1992[IS006_OBDII]QMTSTAT issue.
      PRINT_DATA("detected mqtt offline reconnect");
#if 1
      BJJA_mqtt_connect();
#else
      gAutoMode_reconnecting_count =0;
      BJJA_reconnect_4G();
#endif
    }
    else if(strncmp("+QMTSTAT: 1,1",pch,13)==0)
    {
      PRINT_DATA("detected mqtt offline reconnect");
      BJJA_mqtt_connect();//if return +QMTSTAT: 1,1 restart mqtt_connet
      //HAL_NVIC_SystemReset();
    }
    else if(strncmp("+QMTRECV:",pch,9)==0)
    {
#if 0
      //[Weli]received data:+QMTRECV: 1,0,"/flow4g/867160044718165/downlink/","12345678901234567890"
      //sprintf(gLogTmp,"[Weli]received data:%s\r\n",pch);
      //BJJA_write_data_LOG(gLogTmp,strlen(gLogTmp));
      char * sub_token;
      //printf ("Splitting string \"%s\" into tokens:\n",str);
      char *delim_split = "\"";
      sub_token = strtok(pch,delim_split);
      uint8_t my_test_count=0;
      while (sub_token != NULL)
      {
        sub_token = strtok(NULL,delim_split);
        if(my_test_count==2)
        {
          //sprintf(gLogTmp,"[Weli]split %d:%s\r\n",my_test_count,sub_token);
          //BJJA_write_data_LOG(gLogTmp,strlen(gLogTmp));
          parsing_mqtt_return_cmd(sub_token);
          break;
        }
        my_test_count++;
      }
#else
      /*uint8_t i=0;
      uint8_t my_test_count=0;
      for(i=0;i<strlen(pch);i++)
      {
        if(pch[i]=='\"')
          my_test_count++;
        if(my_test_count==3)
          parsing_mqtt_return_cmd(pch+(i+1));
      }*/
      //parsing_mqtt_return_cmd(pch);
#endif
    }
    pch = strtok(NULL,delim);
  }
  //if(gCsq_val>11)
  if(g4GStatus==TELCOMM_STATUS_EARLY_ONLINE)
  {
    g4G_connection_retry_count=0;
    if(first_4g_flag==0/* && gIMEI[0]!=0*/)
    {
      first_4g_flag=1;
      BJJA_mqtt_connect();
    }

  }
  
  //memset(gUART2_rx_buffer,0x00,sizeof(gUART2_rx_buffer));
  //gUART2_index=0;
  g4G_cmd_flag=0;
  if(g4G_connection_retry_count>=200)
  {
    g4GStatus = TELCOMM_STATUS_OFFLINE;
    PRINT_DATA("4G connection fail\r\n");
  }
  if(g4G_connection_retry_count>=1)
    g4G_connection_retry_count++;
  if(gCsq_val>=99)
  {
    //g4GStatus = TELCOMM_STATUS_OFFLINE;
    PRINT_DATA("4G CSQ value too weak\r\n");
  }

  /*if(g4GStatus==TELCOMM_STATUS_CONNECTING && gPDP_waiting_count>0)//when get PDP over 30 seconds,re connect 4G modem
  {
    gPDP_waiting_count++;
    if(gPDP_waiting_count>=5)
    {
      gPDP_waiting_count=0;
      BJJA_reconnect_4G();
      PRINT_DATA("gPDP_waiting_count>=5\r\n");
    }
    
  }*/
  PRINT_DATA("\r\n[Weli]gMQTT_received_flag:[%d]\r\n",gMQTT_received_flag);
  memset(pBuf,0x00,sizeof(pBuf));

  clear_uart();
}
/*************************THIS IS FOR UART2 BEGIN***********************/
void Weli_UartChangeCB2()
{
  //SimplePeripheral_enqueueMsg(SBP_UART_INCOMING_EVT, NULL);
  if(gProduceFlag2==0)
  {
    if(get_queue2()==0)
    {
      multi_role_enqueueMsg(SBP_UART2_INCOMING_EVT,NULL);
    }
  }
}
void clear_uart2()
{
#if 1
  VOID memset(serialBuffer2, 0, sizeof(serialBuffer2));
  //sprintf(serialBuffer,"");
    gSerialLen2=0x00;
    gProduceFlag2=0;
  Weli_UartChangeCB2();
#endif
}
void BJJA_LM_parsing_gps(char *data)
{
  //PRINT_DATA("get GPS data:%s\r\n",gps_ret);
  
  char* Message_ID = strtok(data,",");
  char* Time = strtok(NULL,",");
  char* Data_Valid = strtok(NULL,",");
  char* Raw_Latitude = strtok(NULL,",");
  char* N_S = strtok(NULL,",");
  char* Raw_Longitude = strtok(NULL,",");
  char* E_W = strtok(NULL,",");
  if(strncmp(Data_Valid,"A",strlen("A"))==0)
  {
    float val1 = atof(Raw_Latitude);
    float val2 = atof(Raw_Longitude);
    int16_t ival1 = val1/100;
    int16_t ival2 = val2/100;
    val1 = ((val1-(ival1*100))/60)+ival1;
    val2 = ((val2-(ival2*100))/60)+ival2;
    /*uint8_t test_data[64]={0x00};
    sprintf(test_data,"gps final:%4.4f,%4.4f,/100 %d,%d\r\n",val1,val2,ival1,ival2);
    //PRINT_DATA("gps raw:%4.4f,%4.4f,/100 %d,%d\r\n",val1,val2,ival1,ival2);
    PRINT_DATA(test_data);*/
    //sprintf(gLastGpsLat,"%s,%s",gps_string_1,gps_string_2);//2238.3620N,12021.4064E
    sprintf(gLastGpsLat,"%fN,%fE",val1,val2);//2238.3620N,12021.4064E
    //PRINT_DATA("Find gps location:%s,%s\r\n",Raw_Latitude,Raw_Longitude);
    PRINT_DATA("Find gps location:%s\r\n",gLastGpsLat);
    //gLastGpsLat_flag=0;
  }
}
void BJJA_parsing_AT_cmd_send_data_UART2()
{
  gProduceFlag2=1;
  //if(gEnableLog)
  //  UartMessage2(serialBuffer2,gSerialLen2);
  if(strstr(serialBuffer2, "$")!=NULL)
  {
    if(gLastGpsLat_flag==0)
    {
      PRINT_DATA("not ready to parsing gps\r\n");
    }
    else
    {
      //PRINT_DATA("originalGPS:%s\r\n",serialBuffer2);
      //PRINT_DATA("start_ready to parsing gps \r\n");
      //gLastGpsLat_flag=0;
      char *gps_ret;
      gps_ret = strstr(serialBuffer2, "$GPRMC,");
      //$GNRMC,062337.000,A,2237.43276,N,12021.67264,E,0.00,268.84,180423,,,A*7E
      if(gps_ret!=NULL)
      {
        BJJA_LM_parsing_gps(gps_ret);
      }
      else
      {
        gps_ret = strstr(serialBuffer2, "$GNRMC,");
        if(gps_ret!=NULL)
        {
          BJJA_LM_parsing_gps(gps_ret);
        }
      }
    }
    /*char * pch;
    //printf ("Splitting string \"%s\" into tokens:\n",str);
    char *delim = "\n";
    pch = strtok(serialBuffer2,delim);
    while (pch != NULL)
    {
      if(strncmp("$GNRMC,",pch,7)==0 ||strncmp("$GPRMC,",pch,7)==0)
      {
        PRINT_DATA("get GPS data:%s\r\n",pch);
      }
    }*/
  }
  else if(strncmp(serialBuffer2,"AT+FACTORY",strlen("AT+FACTORY"))==0)
  {
    gStopHB=1;
    PRINT_DATA("OK+FACTORY\r\n");
  }
  else if(strncmp(serialBuffer2,"AT+GPS=1",strlen("AT+GPS=1"))==0)
  {
    gLastGpsLat_flag=1;
    PRINT_DATA("OK+GPS\r\n");
  }
  else if(strncmp(serialBuffer2,"AT+GPS=0",strlen("AT+GPS=0"))==0)
  {
    gLastGpsLat_flag=0;
    BJJA_LM_GPS_OFF();
    PRINT_DATA("OK+GPS\r\n");
  }
  else if(strncmp(serialBuffer2,"AT+NOTI",strlen("AT+NOTI"))==0)
  {
    cansec_doNotify(0);
    PRINT_DATA("OK+NOTI\r\n");
  }
  else if (strncmp(serialBuffer2,"AT+WSS=",strlen("AT+WSS="))==0)
  {
    //AT+WSS=1ac61f99da1696e73d3898eae2b82f43
    for(uint8_t i=0;i<16;i++)
    {
      gFlash_data.sn[i] = (BJJA_ascii2hex(serialBuffer2[(7+(i*2))])<<4) | BJJA_ascii2hex(serialBuffer2[(7+((i*2)+1))]);
    }
    PRINT_DATA("OK+WSS\r\n");
    BJJA_LM_write_flash();
  }
  else if (strncmp(serialBuffer2,"ATT",strlen("ATT"))==0)
  {
    cansec_Write2Periphearl(0,6,"01A6\r\n");
    PRINT_DATA("OK+ATT\r\n");
  }
  else if (strncmp(serialBuffer2,"AT+SEND",strlen("AT+SEND"))==0)
  {
    cansec_Write2Periphearl(0,5,"ATZ\r\n");
    PRINT_DATA("OK+SEND\r\n");
  }
  else if (strncmp(serialBuffer2,"AT+ARM",strlen("AT+ARM"))==0)
  {
    gArm_Disarm_command=1;
    PRINT_DATA("OK+ARM\r\n");
  }
  else if (strncmp(serialBuffer2,"AT+DISARM",strlen("AT+DISARM"))==0)
  {
    gArm_Disarm_command=2;
    PRINT_DATA("OK+DISARM\r\n");
  }
  else if (strncmp(serialBuffer2,"AT+OBDMAC=?",strlen("AT+OBDMAC=?"))==0)
  {
    PRINT_DATA("OK+OBDMAC:%02x%02x%02x%02x%02x%02x\r\n",gFlash_data.obdii_mac[0],
      gFlash_data.obdii_mac[1],gFlash_data.obdii_mac[2],
      gFlash_data.obdii_mac[3],gFlash_data.obdii_mac[4],
      gFlash_data.obdii_mac[5]);
  }
  else if (strncmp(serialBuffer2,"AT+OBDMAC=",strlen("AT+OBDMAC="))==0)
  {
    //AT+OBDMAC=F588E24D5B94
    if(gSerialLen2>=24)
    {
      for(uint8_t i=0;i<6;i++)
      {
        gFlash_data.obdii_mac[i] = (BJJA_ascii2hex(serialBuffer2[(10+(i*2))])<<4) | BJJA_ascii2hex(serialBuffer2[(10+((i*2)+1))]);
      }
      PRINT_DATA("OK+OBDMAC\r\n");
      BJJA_LM_write_flash();
    }
    else
    {
      PRINT_DATA("FAIL+OBDMAC\r\n");
    }
    
  }
  else if (strncmp(serialBuffer2,"AT+SRD=",strlen("AT+SRD="))==0)
  {
    //AT+SRD=0404020303030302//delete data
    
    if(gSerialLen2>=25)
    {
      uint8_t find_index=0,i=0,j=0;
      uint8_t new_S[8]={0x00};
      // compare address not match 
      for(i=0;i<8;i++)
      {
        //gSubGpairing_data[find_index].S_code[i] = (BJJA_ascii2hex(serialBuffer2[(6+(i*2))])<<4) | BJJA_ascii2hex(serialBuffer2[(6+((i*2)+1))]);
        new_S[i] = (BJJA_ascii2hex(serialBuffer2[(7+(i*2))])<<4) | BJJA_ascii2hex(serialBuffer2[(7+((i*2)+1))]);
      }
      for(i=0;i<8;i++)
      {
        if(gSubGpairing_data[i].enable=='1')
        {
          find_index=0;
          for(j=0;j<8;j++)
          {
            if(new_S[j]==gSubGpairing_data[i].S_code[j])
            {
              find_index++;
            }
          }
          if(find_index>=8)
          {
            break;
          }
        }
      }
      if(find_index>=8)
      {
         //clear data
        gSubGpairing_data[i].enable='0';
        for(j=0;j<8;j++)
        {
            gSubGpairing_data[i].S_code[j] = 0x00;
        }
        BJJA_LM_write_SR_flash();
        PRINT_DATA("OK+SRD\r\n");
      }
      else
      {
        PRINT_DATA("FAIL+SR:NOT FOUND\r\n");
      }
    }
    else
    {
      PRINT_DATA("FAIL+SR\r\n");
    }
  }
  else if (strncmp(serialBuffer2,"AT+SR=?",strlen("AT+SR=?"))==0)
  {
    //AT+SR=0404020303030302//add data
    PRINT_DATA("OK+SR:\r\n");
    uint8_t i=0;
    for(i=0;i<8;i++)
    {
      if(gSubGpairing_data[i].enable=='1')
      {
        PRINT_DATA("[%d]%02x%02x%02x%02x%02x%02x%02x%02x\r\n",i,
          gSubGpairing_data[i].S_code[0],gSubGpairing_data[i].S_code[1],
          gSubGpairing_data[i].S_code[2],gSubGpairing_data[i].S_code[3],
          gSubGpairing_data[i].S_code[4],gSubGpairing_data[i].S_code[5],
          gSubGpairing_data[i].S_code[6],gSubGpairing_data[i].S_code[7]);
      }
    }
  }
  else if (strncmp(serialBuffer2,"AT+SR=",strlen("AT+SR="))==0)
  {
    //
    if(gSerialLen2>=24)
    {
      uint8_t find_index=0,i=0,j=0;
      uint8_t new_S[8]={0x00};
      // compare address not match 
      for(i=0;i<8;i++)
      {
        //gSubGpairing_data[find_index].S_code[i] = (BJJA_ascii2hex(serialBuffer2[(6+(i*2))])<<4) | BJJA_ascii2hex(serialBuffer2[(6+((i*2)+1))]);
        new_S[i] = (BJJA_ascii2hex(serialBuffer2[(6+(i*2))])<<4) | BJJA_ascii2hex(serialBuffer2[(6+((i*2)+1))]);
      }
      for(i=0;i<8;i++)
      {
        if(gSubGpairing_data[i].enable=='1')
        {
          find_index=0;
          for(j=0;j<8;j++)
          {
            if(new_S[j]==gSubGpairing_data[i].S_code[j])
            {
              find_index++;
            }
          }
          if(find_index>=8)
          {
            break;
          }
        }
        
      }

      if(find_index>=8)
      {
        //show fail,has exist
        PRINT_DATA("FAIL+SR:DATA IS EXIST\r\n");
      }
      else
      {
        //write data
        //FIND EMPTY ENABLE FLAG
        find_index=0;
        for(i=0;i<8;i++)
        {
          if(gSubGpairing_data[i].enable!='1')
          {
            break;
          }
        }
        gSubGpairing_data[i].enable='1';
        for(j=0;j<8;j++)
        {
          //gSubGpairing_data[find_index].S_code[i] = (BJJA_ascii2hex(serialBuffer2[(6+(i*2))])<<4) | BJJA_ascii2hex(serialBuffer2[(6+((i*2)+1))]);
          gSubGpairing_data[i].S_code[j] = new_S[j];
        }
        PRINT_DATA("OK+SR\r\n");
        BJJA_LM_write_SR_flash();
      }
    
    }
    else
    {
      PRINT_DATA("FAIL+SR\r\n");
    }
  }
  /*else if (strncmp(serialBuffer2,"AT+GPS",strlen("AT+GPS"))==0)
  {
    BJJA_LM_GPS_mode();
    while(1)
    {
      SEND_LTE_M("AT+QGPSLOC=0\r\n");
      
      Task_sleep(1000000 / Clock_tickPeriod); BJJA_LM_tick_wdt();
      Task_sleep(1000000 / Clock_tickPeriod); BJJA_LM_tick_wdt();
      Task_sleep(1000000 / Clock_tickPeriod); BJJA_LM_tick_wdt();
      Task_sleep(1000000 / Clock_tickPeriod); BJJA_LM_tick_wdt();
      Task_sleep(1000000 / Clock_tickPeriod); BJJA_LM_tick_wdt();
    }
  }*/
  else if (strncmp(serialBuffer2,"AT+LOCKMODE=",strlen("AT+LOCKMODE="))==0)
  {
    if(serialBuffer2[12]=='0')
      gFlash_data.door_mode=0;
    else
      gFlash_data.door_mode=1;
    BJJA_LM_write_flash();
  }
  else if (strncmp(serialBuffer2,"AT+LOCK=",strlen("AT+LOCK="))==0)
  {
    if(serialBuffer2[8]=='0')
      BJJA_LM_control_door_function(0);//lock test
    else
      BJJA_LM_control_door_function(1);//unlock test
  }
#if 0
  else if (strncmp(serialBuffer2,"AT+SIGNCA1",strlen("AT+SIGNCA1"))==0)
  {
    PRINT_DATA("Write CA1 file");
    //AT+QFUPL="cacert.pem",1187,100
    SEND_LTE_M("AT+QFUPL=\"cacert.pem\",%d,100\r\n",sizeof(sign_cacert));
    DELAY_US(100*1000);
    UartMessage(sign_cacert,sizeof(sign_cacert));

    PRINT_DATA("Write CA1 file DONE");

  }
  else if (strncmp(serialBuffer2,"AT+SIGNCA2",strlen("AT+SIGNCA2"))==0)
  {
    PRINT_DATA("Write CA2 file");

    //AT+QFUPL="client.pem",1220,100
    SEND_LTE_M("AT+QFUPL=\"client.pem\",%d,100\r\n",sizeof(sign_clientcert));
    DELAY_US(100*1000);
    UartMessage(sign_clientcert,sizeof(sign_clientcert));

    PRINT_DATA("Write CA2 file DONE");

  }
  else if (strncmp(serialBuffer2,"AT+SIGNCA3",strlen("AT+SIGNCA3"))==0)
  {
    PRINT_DATA("Write CA3 file");
    //AT+QFUPL="user_key.pem",1675,100
    SEND_LTE_M("AT+QFUPL=\"user_key.pem\",%d,100\r\n",sizeof(sign_private));
    DELAY_US(100*1000);
    UartMessage(sign_private,sizeof(sign_private));

    PRINT_DATA("Write CA3 file DONE");

  }
#endif
  else if (strncmp(serialBuffer2,"AT+MQ",strlen("AT+MQ"))==0)
  {
    uint8_t test_data[128]={0x00};
    
    //sprintf(test_data,"AT+QMTOPEN=0,\"a22gkkuykn8yvq-ats.iot.ap-northeast-1.amazonaws.com\",8883\r\n");
    sprintf(test_data,"AT+QMTOPEN=0,\"a3toi86b2jby7e-ats.iot.eu-west-1.amazonaws.com\",8883\r\n");//this is for AVIS 20230307
    //AT+QMTOPEN=0,"a22gkkuykn8yvq-ats.iot.ap-northeast-1.amazonaws.com",8883//
    UartMessage(test_data,strlen(test_data));
    PRINT_DATA("Send:%s",test_data);
  }
  else if (strncmp(serialBuffer2,"AT+4G=",strlen("AT+4G="))==0)
  {
    PRINT_DATA("Send to 4G:[%s]\r\n",serialBuffer2+6);
    UartMessage(serialBuffer2+6,gSerialLen2-6);
  }
  else if(strncmp(serialBuffer2,"AT+4G_MQTT_PORT=?",strlen("AT+4G_MQTT_PORT=?"))==0)
  {
    PRINT_DATA("OK+4G_MQTT_PORT:%d\r\n",gFlash_data.mqtt_port);
  }
  else if(strncmp(serialBuffer2,"AT+4G_MQTT_PORT=",strlen("AT+4G_MQTT_PORT="))==0)
  {
    //split token
    char * pch;
    //printf ("Splitting string \"%s\" into tokens:\n",str);
    char *delim = "=";
    pch = strtok(serialBuffer2,delim);
    if (pch != NULL)
    {
      pch = strtok(NULL,delim);
      gFlash_data.mqtt_port = atoi(pch);
      BJJA_LM_write_flash();
      PRINT_DATA("OK+4G_MQTT_PORT\r\n");
    }
    else
    {
      PRINT_DATA("FAIL+4G_MQTT_PORT\r\n");
    }
  }
  else if(strncmp(serialBuffer2,"AT+4G_MQTT_URL=?",strlen("AT+4G_MQTT_URL=?"))==0)
  {
    PRINT_DATA("OK+4G_MQTT_URL:%s\r\n",gFlash_data.mqtt_url);
  }
  else if(strncmp(serialBuffer2,"AT+4G_MQTT_URL=",strlen("AT+4G_MQTT_URL="))==0)
  {
    //split token
    char * pch;
    //printf ("Splitting string \"%s\" into tokens:\n",str);
    char *delim = "=";
    pch = strtok(serialBuffer2,delim);
    if (pch != NULL)
    {
      pch = strtok(NULL,delim);
      
      sprintf(gFlash_data.mqtt_url,"%s",pch);
      BJJA_LM_write_flash();
      PRINT_DATA("OK+4G_MQTT_URL\r\n");
    }
    else
    {
      PRINT_DATA("FAIL+4G_MQTT_URL\r\n");
    }
  }
  //if(g_IsConnected)
  else
  {
#if 0
    uint16_t current_index=0;
    while(current_index<gSerialLen2)
    {
      //uint8_t data[32]={0x00};
      //sprintf(data,">>index:%d,Len:%d,Delay:%d\r\n",current_index,gSerialLen,gDelayTime);
      //UartMessage(data,strlen(data));
      if((gSerialLen - current_index)>=gMaxPacket )
      {
        //UartMessage("111\r\n",strlen("111\r\n"));
        SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,gMaxPacket ,serialBuffer2+current_index);
      }
      else
      {
        //UartMessage("222\r\n",strlen("111\r\n"));
        SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,(gSerialLen2 - current_index) ,serialBuffer2+current_index);
      }
      current_index+=gMaxPacket;
      DELAY_US(1000*gDelayTime);//20ms
    }
#else
    //PRINT_DATA("[%s]\r\n",serialBuffer2);
    /*char * pch;
    //printf ("Splitting string \"%s\" into tokens:\n",str);
    char *delim = "\n";
    pch = strtok(serialBuffer2,delim);
    while (pch != NULL)
    {
      if(strncmp("$GNRMC:",pch,7)==0 ||strncmp("$GPRMC:",pch,7)==0)
      {
        PRINT_DATA("get GPS data:%s\r\n",pch);
      }
    }*/
#endif
  }
  clear_uart2();
}
uint8_t BJJA_LM_check_INGI()
{
  uint8_t INGITimes=5;
  while(INGITimes--)
  {
    DELAY_US(1000*20);
    if(GPIO_read(CONFIG_INGI)==0)//weli:INGI DIO3 low active
      return 0;   
  }
  return 1; 
}

uint8_t BJJA_LM_check_DOOR()
{
  uint8_t DOORTimes=5;
  while(DOORTimes--)
  {
    DELAY_US(1000*20);
    if(GPIO_read(CONFIG_DOOR))//weli:door open is pull down
    {
      gDoor_State=0;
      return 0;   
    }
  }
  gDoor_State=1;
  return 1; 
}
bool cansec_doNotify(uint8_t index)
{
  bStatus_t status = FAILURE;
  // If characteristic has been discovered
  if (connList[index].charHandle != 0)
  {
    PRINT_DATA("cansec_doNotify connHandleMap[%d].connHandle=%x\r\n",index,connList[index].connHandle);
    attWriteReq_t req;
    // Allocate GATT write request
    req.pValue = GATT_bm_alloc(connList[index].connHandle, ATT_WRITE_REQ, 2, NULL);
    // If successfully allocated
    if (req.pValue != NULL)
    {
      // Fill up request
      req.len = 2;
      req.pValue[0] = LO_UINT16(GATT_CLIENT_CFG_NOTIFY);
      req.pValue[1] = HI_UINT16(GATT_CLIENT_CFG_NOTIFY);
      req.sig = 0;
      req.cmd = 0;
      req.handle = connList[index].gNotify_charHdl+1;//CCC = Nofity characteristic+1
      PRINT_DATA("CCC:%x,index:%d\r\n",req.handle,index);

      // Send GATT write to controller
      status = GATT_WriteCharValue(connList[index].connHandle, &req, selfEntity);

      // If not sucessfully sent
      if ( status != SUCCESS )
      {
        // Free write request as the controller will not
        GATT_bm_free((gattMsg_t *)&req, ATT_WRITE_REQ);
        PRINT_DATA("subcribe Notify fail:%d\r\n",status);
        BJJA_LM_disconnect_OBDII();
      }
    }
    if (status == SUCCESS)
    {
      PRINT_DATA("subcribe Notify success\r\n");
      gBJJA_LM_Obd_state=O_Running;
      gConnTimeout_count=0;
      OBDII_current_index=0;
    }
  }

  return TRUE;
}
bool cansec_Write2Periphearl(uint8_t index,uint16_t len,char *data)
{
  int8_t val = BJJA_LM_find_OBDII_conn_index();
  if(val!=-1)
    index = val;
  else
  {
    PRINT_DATA("obdii mac not found,reconnect\r\n");
    BJJA_LM_disconnect_OBDII();
  }
  bStatus_t status = FAILURE;
  // If characteristic has been discovered
  if (connList[index].charHandle != 0)
  {
    // Do a write
    attWriteReq_t req;

    // Allocate GATT write request
    req.pValue = GATT_bm_alloc(connList[index].connHandle, ATT_WRITE_REQ, len, NULL);
    // If successfully allocated
    if (req.pValue != NULL)
    {
      // Fill up request
      req.handle = connList[index].charHandle;
      req.len = len;
      //sprintf(req.pValue,"%s",serialBuffer);
      for(uint16_t i=0;i<len;i++)
        req.pValue[i] = data[i];
      req.sig = 0;
      req.cmd = 0;

      // Send GATT write to controller
      status = GATT_WriteCharValue(connList[index].connHandle, &req, selfEntity);

      // If not sucessfully sent
      if ( status != SUCCESS )
      {
        // Free write request as the controller will not
        GATT_bm_free((gattMsg_t *)&req, ATT_WRITE_REQ);
        PRINT_DATA("Write data fail\r\n");
        BJJA_LM_disconnect_OBDII();

      }
      else
      {
        PRINT_DATA("Write data SUCCESS\r\n");
      }
    }
    else
    {
      PRINT_DATA("Write data fail:%d\r\n",__LINE__);
      BJJA_LM_disconnect_OBDII();
    }
  }
  else
  {
    PRINT_DATA("Write data fail:%d\r\n",__LINE__);
    BJJA_LM_disconnect_OBDII();
  }
  return TRUE;
}
int8_t BJJA_LM_find_OBDII_conn_index()
{
  uint8_t ret=-1;
  for(uint8_t i=0;i<MAX_NUM_BLE_CONNS;i++)
  {
    if(connList[i].addr[5]==gFlash_data.obdii_mac[0] && 
      connList[i].addr[4]==gFlash_data.obdii_mac[1] &&
      connList[i].addr[3]==gFlash_data.obdii_mac[2] && 
      connList[i].addr[2]==gFlash_data.obdii_mac[3] &&
      connList[i].addr[1]==gFlash_data.obdii_mac[4] && 
      connList[i].addr[0]==gFlash_data.obdii_mac[5] && 
      connList[i].connHandle!=LINKDB_CONNHANDLE_INVALID
      )
    {
      ret=i;
      break;
    }
  }
  return ret;
}
uint8_t BJJA_LM_check_OBDII_is_online()
{
  uint8_t ret=0x00;
  uint8_t i=0;
  for(i=0;i<MAX_NUM_BLE_CONNS;i++)
  {
    if(connList[i].addr[5]==gFlash_data.obdii_mac[0] && 
      connList[i].addr[4]==gFlash_data.obdii_mac[1] &&
      connList[i].addr[3]==gFlash_data.obdii_mac[2] && 
      connList[i].addr[2]==gFlash_data.obdii_mac[3] &&
      connList[i].addr[1]==gFlash_data.obdii_mac[4] && 
      connList[i].addr[0]==gFlash_data.obdii_mac[5] && connList[i].connHandle!=LINKDB_CONNHANDLE_INVALID
      )
    {
      mrConnHandle  = connList[i].connHandle;
      return 1;
    }
  }
  return 0;
}
uint8_t obd_cmd_flag=0x00;
void BJJA_LM_Working_state_running()
{
  PRINT_DATA("gBJJA_LM_Obd_state=%d,gWaitCount:%d,gWaitTimer:%d\r\n",gBJJA_LM_Obd_state,gWaitCount,gWaitTimer);
  if(gACC_ON_timer_flag>=3)
  {
    
    if(gBJJA_LM_Obd_state==O_Discover)
    {
      //multi_role_doDiscoverDevices(0);
      //gConnTimeout_count=0;
      gBJJA_LM_Obd_state=O_Connect;
      gConnTimeout_count=1;
    }
    else if(gBJJA_LM_Obd_state==O_Connect)
    {
      //waiting for few seconds
      if(gWaitCount>0 && gWaitCount<gWaitTimer)
      {
        gWaitCount++;
      }
      //else if(gWaitCount>0 && gWaitCount>=gWaitTimer)
      else
      {
        multi_role_doConnect(0);//this is from scanlist,scanlist always is obdii mac address
        //當connect失敗時,怎麼recovery,怎麼reconnect.
        gWaitCount=0;
      }
      if(gConnTimeout_count>=1)
      {
        gConnTimeout_count++;
        if(gConnTimeout_count>=30)
        {
          PRINT_DATA("connection over 30seconds,reconnect\r\n");
          BJJA_LM_disconnect_OBDII(); 
          gConnTimeout_count=0;
          gBJJA_LM_Obd_state=O_Discover;
        }
      }
      
    }
    else if(gBJJA_LM_Obd_state==O_Notify)
    {
      if(gWaitCount>0 && gWaitCount<gWaitTimer)
      {
        gWaitCount++;
      }
      else
      {
        //notify device
        //todo:warning index is MAC address of OBDII
        int8_t val = BJJA_LM_find_OBDII_conn_index();
        if(val!=-1)
        {
          PRINT_DATA("OBDII device index:%d\r\n",val);
          cansec_doNotify(val);//need mactch obdii mac
          //notify fail how to recovery,key word subcribe Notify fail:
          gWaitCount=0;  
        }
        else
        {
          PRINT_DATA("not found OBDII device index,roll back to discover\r\n");
          gBJJA_LM_Obd_state=O_Discover;
        }
      }
      if(gConnTimeout_count>=1)
      {
        gConnTimeout_count++;
        if(gConnTimeout_count>=30)
        {
          PRINT_DATA("notify over 30seconds,reconnect\r\n");
          BJJA_LM_disconnect_OBDII(); 
          gConnTimeout_count=0;
          gBJJA_LM_Obd_state=O_Discover;
        }
      }
      
    }
    else if(gBJJA_LM_Obd_state==O_Running)
    {
      //todo OBDII init command first time.
      /*
      ATZ
      STI
      TI
      ATD
      ATD0
      ATE0
      ATSP6
      ATH1
      ATM0
      ATS0
      ATAT1
      ATAL
      ATST96
      0100
      */
      
      /*cansec_Write2Periphearl(0,4,"ATZ\r\n");DELAY_US(1000*1000);
      cansec_Write2Periphearl(0,5,"STI\r\n");DELAY_US(1000*400);
      cansec_Write2Periphearl(0,4,"TI\r\n");DELAY_US(1000*400);
      cansec_Write2Periphearl(0,5,"ATD\r\n");DELAY_US(1000*400);
      cansec_Write2Periphearl(0,6,"ATD0\r\n");DELAY_US(1000*400);
      cansec_Write2Periphearl(0,6,"ATE0\r\n");DELAY_US(1000*400);
      cansec_Write2Periphearl(0,7,"ATSP6\r\n");DELAY_US(1000*400);
      cansec_Write2Periphearl(0,6,"ATH1\r\n");DELAY_US(1000*400);
      cansec_Write2Periphearl(0,6,"ATM0\r\n");DELAY_US(1000*400);
      cansec_Write2Periphearl(0,6,"ATS0\r\n");DELAY_US(1000*400);
      cansec_Write2Periphearl(0,7,"ATAT1\r\n");DELAY_US(1000*400);
      cansec_Write2Periphearl(0,6,"ATAL\r\n");DELAY_US(1000*400);
      cansec_Write2Periphearl(0,8,"ATST96\r\n");*/
      if(OBDII_current_index>=OBD_MAX_CMD)
      {
        gBJJA_LM_Obd_state=O_Running_Late;
      }
      
      
    }
    else if(gBJJA_LM_Obd_state==O_Running_Late)
    {
      //when obdii disconnect,write data to obdii will cause fail,and then it will reconnct to obdii
      gWorkingHeartBeat++;
      if(gWorkingHeartBeat>=WorkingHeartBeatTimer)//per 10seconds
      {
        if(obd_cmd_flag)
        {
          //cansec_Write2Periphearl(0,6,"01A6\r\n");
          BJJA_LM_send_odo_cmd(1);
          obd_cmd_flag=0;
        }
        else
        {
          //cansec_Write2Periphearl(0,6,"0100\r\n");  
          BJJA_LM_send_fuel_level_cmd(1);
          obd_cmd_flag=1;
        }
        
        //DELAY_US(1000*500);//20ms
        
        //gFlash_data.ODO_meter=3250;
        //gFlash_data.SOC=32;
        //PRINT_DATA("Odometer:%dkm,fuel level:%d%\r\n",gFlash_data.ODO_meter,gFlash_data.SOC);
        
        gWorkingHeartBeat=0;
      }
    }

  }
  else if(gACC_ON_timer_flag==0)
  {
    gBJJA_LM_State_machine=Idle;
    //todo:notify lte-m and ble
    PRINT_DATA("Entry Idle state,notify ble and Lte-M1\r\n");
  }
}
void BJJA_LM_entry_Working_state()
{
  if(gACC_ON_timer_flag>=3)
  {
    gBJJA_LM_State_machine=Working;
  }
  /*else if(gACC_ON_timer_flag==0 && (!(gBJJA_LM_State_machine==Disarm) ||!(gBJJA_LM_State_machine==Arm)))
  {
    gBJJA_LM_State_machine=Idle;
    PRINT_DATA("Entry Idle state,notify ble and Lte-M2\r\n");
  }*/
}
void BJJA_LM_Entry_Idle_state()
{
  //todo
  //check INGI all off
  if(gACC_ON_timer_flag==0)
  {
    //disconnect obdii device
    BJJA_LM_disconnect_OBDII();
    gBJJA_LM_State_machine=Idle;
    PRINT_DATA("Entry Idle state,notify ble and Lte-M3\r\n");
    //todo:weli notify ltem and ble
  }
  else
  {
    //do nothing;
  }
}
uint8_t BJJA_LM_Early_Entry_DisArm_state()
{
  if(gACC_ON_timer_flag==0)
    return 1;
  else
    return 0;
}
void BJJA_LM_Entry_DisArm_state()
{
  if(gArm_Disarm_command!=0)
  {
    if(gArm_Disarm_command==2)//ask entry disarm state
    {
      //check INGI all off
      if(gACC_ON_timer_flag==0)
      {
        //disconnect obdii device
        BJJA_LM_disconnect_OBDII();
        
        //send disarm state to sub1g
        //aa 13 04 04 02 03 03 03 03 01//enable all S OFF
        gPacket[0]=0xaa;
        gPacket[1]=0x03;
        gPacket[2]=0x04;
        gPacket[3]=0x04;
        gPacket[4]=0x02;
        gPacket[5]=0x03;
        gPacket[6]=0x03;
        gPacket[7]=0x03;
        gPacket[8]=0x03;
        gPacket[9]=0x01;
        Semaphore_post(gSem);//unlock 
        gBJJA_LM_State_machine=Disarm;

        //todo notify to lte-m and ble
        PRINT_DATA("entry disarm-state OK\r\n");
        gACC_ON_OFF_flag=4;
      }
      else
      {
        //todo notify to lte-m and ble
        PRINT_DATA("entry disarm-state fail\r\n");
      }
      gArm_Disarm_command=0;
    }
    

  }
}
uint8_t BJJA_LM_Early_Entry_Arm_state()
{
  if(gACC_ON_timer_flag==0 && BJJA_LM_check_DOOR()==0)//INGI OFF & DOOR off
    return 1;
  else
    return 0;
}
void BJJA_LM_Entry_Arm_state()
{
  if(gArm_Disarm_command!=0)
  {
    if(gArm_Disarm_command==1)//ask entry arm state
    {
      if(gACC_ON_timer_flag==0 && BJJA_LM_check_DOOR()==0)//INGI OFF & DOOR off
      {
        //disconnect obdii device
        BJJA_LM_disconnect_OBDII();
        //send arm state to sub1g
        //aa 02 04 04 02 03 03 03 03 01//enable all S ON
        gPacket[0]=0xaa;
        gPacket[1]=0x02;
        gPacket[2]=0x04;
        gPacket[3]=0x04;
        gPacket[4]=0x02;
        gPacket[5]=0x03;
        gPacket[6]=0x03;
        gPacket[7]=0x03;
        gPacket[8]=0x03;
        gPacket[9]=0x01;
        Semaphore_post(gSem);//unlock 
        gBJJA_LM_State_machine=Arm;

        //todo notify to lte-m and ble
        PRINT_DATA("entry arm-state OK:%d\r\n",gBJJA_LM_State_machine);
        gACC_ON_OFF_flag=3;
      }
      else
      {
        //todo notify to lte-m and ble
        PRINT_DATA("entry arm-state fail\r\n");
      }
      gArm_Disarm_command=0;
    }
    

  }
}
void BJJA_LM_state_machine_heart_beat()
{
  BJJA_LM_tick_wdt();
  if(BJJA_LM_check_INGI()==1)
  {
    if(gACC_ON_timer_flag<=250)
      gACC_ON_timer_flag++;
    if(gACC_ON_timer_flag==1)
      gACC_ON_OFF_flag=1;


    // run do scan OBDII
    UartMessage2("acc on ",strlen("acc on ")); //weli mark
    
    /*if(gBJJA_LM_State_machine==0)//if acc on,connect to dbdii
    {
      gBJJA_LM_State_machine=1;
      multi_role_doDiscoverDevices(0);  
    }*/
  }
  else
  {
    if(gACC_ON_timer_flag>0)
      gACC_ON_OFF_flag=2;
    gACC_ON_timer_flag=0;
    UartMessage2("acc off ",strlen("acc off ")); //weli mark
  }
#if 1
  if(BJJA_LM_check_DOOR()==1)
  {
    // run do scan OBDII
    UartMessage2("door open ",strlen("door open ")); 
  }
  else
  {
    UartMessage2("door close ",strlen("door close ")); 
  }
  if(gDoor_State != g_previous_door_state)
  {
    UartMessage2(",door state change ",strlen(",door state change ")); 
    gACC_ON_OFF_flag=5;
  }
  g_previous_door_state = gDoor_State;
  PRINT_DATA("current state machine:");
  switch(gBJJA_LM_State_machine)
  {
    case Pwr_on:
      PRINT_DATA("Pwr_on:%d",gBJJA_LM_State_machine);
      break;
    case Idle:
      PRINT_DATA("Idle:%d",gBJJA_LM_State_machine);
      break;
    case Arm:
      PRINT_DATA("Arm:%d",gBJJA_LM_State_machine);
      break;
    case Disarm:
      PRINT_DATA("Disarm:%d",gBJJA_LM_State_machine);
      break;
    case Working:
      PRINT_DATA("Working:%d",gBJJA_LM_State_machine);
      break;
  }
  PRINT_DATA(" gArm_Disarm_command:%d gACC_ON_timer_flag:%d\r\n",gArm_Disarm_command,gACC_ON_timer_flag);
#endif
  if(gBJJA_LM_State_machine==Idle)
  {
    BJJA_LM_Entry_DisArm_state();
    BJJA_LM_Entry_Arm_state();
    BJJA_LM_entry_Working_state();
  }
  else if(gBJJA_LM_State_machine==Arm)
  {
    BJJA_LM_Entry_Arm_state();
    BJJA_LM_Entry_DisArm_state();
  }
  else if(gBJJA_LM_State_machine==Disarm)
  {
    BJJA_LM_Entry_DisArm_state();
    BJJA_LM_Entry_Arm_state();
    BJJA_LM_entry_Working_state();
  }
  else if(gBJJA_LM_State_machine==Working)
  {
    BJJA_LM_Working_state_running();
    BJJA_LM_Entry_Idle_state();
  }
  if(gArm_Disarm_command!=0)
  {
    PRINT_DATA("todo:state machine not match\r\n");
    //todo notify msg by ble and lte-m
  }

  /*if(gBJJA_LM_State_machine==2)
  {
    gBJJA_LM_State_machine=3;
    PRINT_DATA("doconnect\r\n");
    multi_role_doConnect(0);
  }*/
  BJJA_LM_4G_HeartBeat();

}
void BJJA_LM_read_flash()
{
  osal_snv_read(SNV_ID_BJJA_FLASH, sizeof(gFlash_data), &gFlash_data);
  //PRINT_DATA("sing:%c%c%c\r\n",gFlash_data.sign[0],gFlash_data.sign[1],gFlash_data.sign[2]);
  PRINT_DATA("OBDII MAC:%02x%02x%02x%02x%02x%02x\r\n",gFlash_data.obdii_mac[0],
      gFlash_data.obdii_mac[1],gFlash_data.obdii_mac[2],
      gFlash_data.obdii_mac[3],gFlash_data.obdii_mac[4],
      gFlash_data.obdii_mac[5]);
}
void BJJA_LM_write_flash()
{
  osal_snv_write(SNV_ID_BJJA_FLASH, sizeof(gFlash_data),&gFlash_data);
}
void BJJA_LM_load_default_setting()
{
  osal_snv_read(SNV_ID_BJJA_FLASH, sizeof(gFlash_data), &gFlash_data);
  if(gFlash_data.sign[0]=='A' && gFlash_data.sign[1]=='M' && gFlash_data.sign[2]=='Y')
  {
    PRINT_DATA("data has exist\r\n");
    return;
  }
  else
  {
    PRINT_DATA("reload new data\r\n");
    gFlash_data.sign[0]='A';
    gFlash_data.sign[1]='M';
    gFlash_data.sign[2]='Y';
    gFlash_data.last_statemachine=Idle;
    gFlash_data.periodic_timer=30;
    gFlash_data.door_mode=0;


    gFlash_data.mqtt_port=8883;
#if 0 //normal MQTT mode
    sprintf(gFlash_data.mqtt_url,"%s","rf-idh.com");
    gFlash_data.mqtt_authority=1883;
#else  //AWS IoT Core mode
    sprintf(gFlash_data.mqtt_url,"%s","a3toi86b2jby7e-ats.iot.eu-west-1.amazonaws.com");
    gFlash_data.mqtt_authority=8883;
#endif
    sprintf(gFlash_data.mqtt_user,"%s","HiwOk5pXCdBy9drsR6bD");
    sprintf(gFlash_data.mqtt_passwd,"%s","bar");
    //write save data end in here
    //osal_snv_write(SNV_ID_BJJA_FLASH, BUF_OF_BJJA_SAVE_LEN, (uint8 *)data);
    osal_snv_write(SNV_ID_BJJA_FLASH, sizeof(gFlash_data),&gFlash_data);
  }
}
void BJJA_LM_read_SR_flash()
{
  osal_snv_read(SNV_ID_BJJA_SAVE, sizeof(gSubGpairing_data[8]),&gSubGpairing_data);
  uint8_t i=0;
  for(i=0;i<8;i++)
  {
    if(gSubGpairing_data[i].enable=='1')
    {
      PRINT_DATA("[%d]%02x%02x%02x%02x%02x%02x%02x%02x\r\n",i,
        gSubGpairing_data[i].S_code[0],gSubGpairing_data[i].S_code[1],
        gSubGpairing_data[i].S_code[2],gSubGpairing_data[i].S_code[3],
        gSubGpairing_data[i].S_code[4],gSubGpairing_data[i].S_code[5],
        gSubGpairing_data[i].S_code[6],gSubGpairing_data[i].S_code[7]);
    }
  }
}
void BJJA_LM_write_SR_flash()
{
  osal_snv_write(SNV_ID_BJJA_SAVE, sizeof(gSubGpairing_data[8]),&gSubGpairing_data);
}
uint8_t BJJA_LM_check_Factory_mode()
{
  uint8_t i=0;
  for(i=0;i<10;i++)
  {
    PRINT_DATA("factory check:%d\r\n",i);
    if(GPIO_read(CONFIG_ENG_BTN)==1)//weli:INGI DIO3 low active
      return 0;   
    DELAY_US(1000*300);
    
  }
  return 1; 
}
BJJA_LM_GPS_OFF()
{
  GPIO_write(GPS_PWR_EN,0);
}
BJJA_LM_GPS_init()
{
  //GPS_nRESET default low
  //keep low

  //GPS_STANDBY default low
  //keep low

  //GPS_PWR_EN default low
  //keep HIGH

  GPIO_write(GPS_nRESET,0);
  GPIO_write(GPS_STANDBY,0);
  GPIO_write(GPS_PWR_EN,1);

}
void BJJA_LM_init()
{
  BJJA_WDT_init();
  Board_initUser();
  
  Board_initUser2();
  //UartMessage2("Hello world\r\n",strlen("Hello world\r\n"));
  PRINT_DATA("Ver:v1.1.3,Build Time:%s\r\n",__TIME__);
  BJJA_LM_load_default_setting();
  
  BJJA_LM_read_flash();
  
  BJJA_LM_read_SR_flash();
  
  gOBDII_dev.addrType=1;
  gOBDII_dev.addr[0] = gFlash_data.obdii_mac[5];
  gOBDII_dev.addr[1] = gFlash_data.obdii_mac[4];
  gOBDII_dev.addr[2] = gFlash_data.obdii_mac[3];
  gOBDII_dev.addr[3] = gFlash_data.obdii_mac[2];
  gOBDII_dev.addr[4] = gFlash_data.obdii_mac[1];
  gOBDII_dev.addr[5] = gFlash_data.obdii_mac[0];


  //BJJA_LM_AES_init();//test ok
  
  check_ble();
  
  PRINT_DATA("MAC=%02x:%02x:%02x:%02x:%02x:%02x\r\n",gMac[0],gMac[1],gMac[2],gMac[3],gMac[4],gMac[5]);

  GPIO_write(GPIO_3V8_EN,1);
  PRINT_DATA("Enable 3V8\r\n");
  if(BJJA_LM_check_Factory_mode()==1)
  {
    PRINT_DATA("Entry factory mode:\r\n");
    gStopHB=1;
  }
  BJJA_LM_GPS_init();
  DELAY_US(1000*100);

  GPIO_write(GPIO_4G_PWR,1);
  DELAY_US(1000*100);
  GPIO_write(GPIO_4G_PWR,0);
  DELAY_US(1000*300);
  GPIO_write(GPIO_4G_RST,1);
  DELAY_US(1000*100);
  GPIO_write(GPIO_4G_RST,0);
  PRINT_DATA("Enable LTE-M module\r\n");

  sprintf(gFlash_data.user_name,"weli");
  

  
  
  GPIO_write(GPIO_LOCK,0);//pull down car door lock pin
  GPIO_write(GPIO_UNLOCK,0);//pull down car door unlock pin
  

  /*UartMessage("AT\r\n",4);
  DELAY_US(1000*100);
  UartMessage("AT\r\n",4);
  DELAY_US(1000*100);
  UartMessage("AT\r\n",4);
  DELAY_US(1000*100);
  UartMessage("AT\r\n",4);
  DELAY_US(1000*100);
  UartMessage("AT\r\n",4);
  DELAY_US(1000*100);
  UartMessage("AT\r\n",4);
  DELAY_US(1000*100);*/

  //BJJA_LM_4G_early_init();
  //BJJA_4G_JOIN();


#if 0
  myOBD[0].timer=10;
  myOBD[0].cmd_len=5;
  sprintf(myOBD[0].cmd_name,"ATZ\r\n");

  myOBD[1].timer=10;
  myOBD[1].cmd_len=5;
  sprintf(myOBD[1].cmd_name,"STI\r\n");

  myOBD[2].timer=10;
  myOBD[2].cmd_len=4;
  sprintf(myOBD[2].cmd_name,"TI\r\n");

  myOBD[3].timer=10;
  myOBD[3].cmd_len=5;
  sprintf(myOBD[3].cmd_name,"ATD\r\n");

  myOBD[4].timer=10;
  myOBD[4].cmd_len=6;
  sprintf(myOBD[4].cmd_name,"ATD0\r\n");

  myOBD[5].timer=10;
  myOBD[5].cmd_len=6;
  sprintf(myOBD[5].cmd_name,"ATE0\r\n");

  myOBD[6].timer=10;
  myOBD[6].cmd_len=7;
  sprintf(myOBD[6].cmd_name,"ATSP6\r\n");

  myOBD[7].timer=10;
  myOBD[7].cmd_len=6;
  sprintf(myOBD[7].cmd_name,"ATH1\r\n");

  myOBD[8].timer=10;
  myOBD[8].cmd_len=6;
  sprintf(myOBD[8].cmd_name,"ATM0\r\n");

  myOBD[9].timer=10;
  myOBD[9].cmd_len=6;
  sprintf(myOBD[9].cmd_name,"ATS0\r\n");

  myOBD[10].timer=10;
  myOBD[10].cmd_len=7;
  sprintf(myOBD[10].cmd_name,"ATAT1\r\n");

  myOBD[11].timer=10;
  myOBD[11].cmd_len=6;
  sprintf(myOBD[11].cmd_name,"ATAL\r\n");

  myOBD[12].timer=10;
  myOBD[12].cmd_len=8;
  sprintf(myOBD[12].cmd_name,"ATST96\r\n");
#else
  /*
  ATE0
  STI
  VTI
  ATD
  ATD0
  ATE0
  ATSP0
  ATE0
  ATH1
  ATM0
  ATS0
  ATAT1
  ATAL
  ATST64
  0100
  ATDPN
  */
  myOBD[0].timer=10;
  myOBD[0].cmd_len=5;
  sprintf(myOBD[0].cmd_name,"ATZ\r\n");

  myOBD[1].timer=10;
  myOBD[1].cmd_len=6;
  sprintf(myOBD[1].cmd_name,"ATE0\r\n");

  myOBD[2].timer=10;
  myOBD[2].cmd_len=5;
  sprintf(myOBD[2].cmd_name,"STI\r\n");

  myOBD[3].timer=10;
  myOBD[3].cmd_len=5;
  sprintf(myOBD[3].cmd_name,"VTI\r\n");

  myOBD[4].timer=10;
  myOBD[4].cmd_len=5;
  sprintf(myOBD[4].cmd_name,"ATD\r\n");

  myOBD[5].timer=10;
  myOBD[5].cmd_len=6;
  sprintf(myOBD[5].cmd_name,"ATD0\r\n");

  myOBD[6].timer=10;
  myOBD[6].cmd_len=6;
  sprintf(myOBD[6].cmd_name,"ATE0\r\n");

  myOBD[7].timer=10;
  myOBD[7].cmd_len=7;
  sprintf(myOBD[7].cmd_name,"ATSP0\r\n");

  myOBD[8].timer=10;
  myOBD[8].cmd_len=6;
  sprintf(myOBD[8].cmd_name,"ATE0\r\n");

  myOBD[9].timer=10;
  myOBD[9].cmd_len=6;
  sprintf(myOBD[9].cmd_name,"ATH1\r\n");

  myOBD[10].timer=10;
  myOBD[10].cmd_len=6;
  sprintf(myOBD[10].cmd_name,"ATM0\r\n");

  myOBD[11].timer=10;
  myOBD[11].cmd_len=6;
  sprintf(myOBD[11].cmd_name,"ATS0\r\n");

  myOBD[12].timer=10;
  myOBD[12].cmd_len=7;
  sprintf(myOBD[12].cmd_name,"ATST1\r\n");

  myOBD[13].timer=10;
  myOBD[13].cmd_len=6;
  sprintf(myOBD[13].cmd_name,"ATAL\r\n");

  myOBD[14].timer=10;
  myOBD[14].cmd_len=8;
  sprintf(myOBD[14].cmd_name,"ATST64\r\n");

  myOBD[15].timer=10;
  myOBD[15].cmd_len=7;
  sprintf(myOBD[15].cmd_name,"ATDPN\r\n");
#endif
}
void GetMacAddress(uint8 *p_Address)
{
    uint32 Mac0 = HWREG(FCFG1_BASE + FCFG1_O_MAC_BLE_0);
    uint32 Mac1 = HWREG(FCFG1_BASE + FCFG1_O_MAC_BLE_1);

    p_Address[5] = Mac0;
    p_Address[4] = Mac0 >> 8;
    p_Address[3] = Mac0 >> 16;
    p_Address[2] = Mac0 >> 24;
    p_Address[1] = Mac1;
    p_Address[0] = Mac1 >> 8;
}
uint8_t check_ble()
{
  
  //PRINT_DATA("-------------\r\n");
  //for(uint8_t i=0;i<16;i++)
  //  PRINT_DATA("%2x",gFlash_data.sn[i]);
  //PRINT_DATA("\r\n");
  GetMacAddress(gMac);
  //gvalid=1;
  //return 1;

  //http://testprotect.com/appendix/AEScalc
  //cf21513c0d0a1203251220030144ff01//key
  //fca89beca98afca89beca98aaabbccdd//mac 
  //decryption:1ac61f99da1696e73d3898eae2b82f43
  //AT+WSS=1ac61f99da1696e73d3898eae2b82f43
  uint8_t key[16] = {0xcf, 0x21, 0x51, 0x3c, 0x0d, 0x0a, 0x12, 0x03,
                              0x25, 0x12, 0x20, 0x03, 0x01, 0x44, 0xff, 0x01};
  uint8_t out[16]={0x00};
  AESECB_Handle handle;
  AESECB_Params params;
  CryptoKey cryptoKey;
  int_fast16_t decryptionResult;
  AESECB_Operation operation;
  //PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
  AESECB_Params_init(&params);
  //PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
  //params.returnBehavior = AESECB_RETURN_BEHAVIOR_CALLBACK;
  //params.callbackFxn = ecbCallback;
  handle = AESECB_open(CONFIG_AESECB_0, &params);
  //PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
  if (handle == NULL) {
      // handle error
    //PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
  }
  //PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
  CryptoKeyPlaintext_initKey(&cryptoKey, key, sizeof(key));
  //PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
  AESECB_Operation_init(&operation);
  operation.key               = &cryptoKey;
  operation.input             = gFlash_data.sn;
  operation.output            = out;
  operation.inputLength       = sizeof(out);

  //PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
  decryptionResult = AESECB_oneStepDecrypt(handle, &operation);
  //PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
  if (decryptionResult != AESECB_STATUS_SUCCESS) {
      // handle error
    //PRINT_DATA("%s-%d\r\n",__FUNCTION__,__LINE__);
  }
  AESECB_close(handle);
  //for(uint8_t i=0;i<16;i++)
  //  PRINT_DATA("%2x",out[i]);
  uint8_t chk_flag=0;
  for(uint8_t i=0;i<6;i++)
  {
    if((gMac[i] == out[i]) &&(out[i] == out[6+i]))
      chk_flag++;
  }
  if(chk_flag>=6)
  {
    gvalid=1;
    PRINT_DATA("OK\r\n");

  }
  else
  {
    PRINT_DATA("##%d#\r\n",chk_flag);
    gvalid=0;
  }
  return gvalid;

   
}
uint8_t BJJA_ascii2hex(uint8_t val)
{
  //PRINT_DATA(">%c\r\n",val);
  if(val>='0' && val<='9')
    return val-'0';
  if(val>='a' && val<='f')
    return ((val-'a')+10);
  if(val>='A' && val<='F')
    return ((val-'A')+10);
  else return 0x00;
}
void BJJA_LM_disconnect_OBDII()
{
  if(BJJA_LM_check_OBDII_is_online())
  {
    multi_role_doDisconnect(0);
    PRINT_DATA("OBDII online,request disconnect\r\n");
  }
  else
  {
    PRINT_DATA("OBDII has been offline\r\n");
  }
  gBJJA_LM_Obd_state=O_Discover;
  gConnTimeout_count=0;
}
void BJJA_LM_AES_init()
{
  PRINT_DATA("raw data\r\n");
  for(uint8_t i=0;i<sizeof(plaintext);i++)
    PRINT_DATA("%02x ",plaintext[i]);
  PRINT_DATA("\r\n");
  uint8_t encrypt_data[32]={0x00};
  uint8_t decrypt_data[32]={0x00};
  ecbEncrypt(plaintext,encrypt_data,32);
  PRINT_DATA("encrypt data\r\n");
  for(uint8_t i=0;i<sizeof(encrypt_data);i++)
    PRINT_DATA("%02x ",encrypt_data[i]);
  PRINT_DATA("\r\n");
  ecbDecrypt(encrypt_data,decrypt_data,32);
  PRINT_DATA("decrypt data\r\n");
  for(uint8_t i=0;i<sizeof(decrypt_data);i++)
    PRINT_DATA("%02x ",decrypt_data[i]);
  PRINT_DATA("\r\n");

}
static void BJJA_LM_4G_HeartBeat()
{
  g4G_heartbeat++;
  if(g4G_heartbeat>=5 && g4GStatus!=TELCOMM_STATUS_ONLINE)
  {
    PRINT_DATA("4G quick heartbeat\r\n");
    BJJA_4G_JOIN();
    g4G_heartbeat=0;
  }
  else if(g4G_heartbeat>=10/*10*/)//per 10s tick again
  {
    PRINT_DATA("4G normal heartbeat\r\n");
    BJJA_4G_JOIN();
    g4G_heartbeat=0;
  }
}
uint8_t gPING_count=0;
static void BJJA_4G_JOIN()
{
  //return;//debug mode
  if(gStopHB)
  {
    PRINT_DATA("4G skip heartbeat,factory mode\r\n");
    return;
  }
  if(TELCOMM_STATUS_4G_NON_DETECTED==g4GStatus)
  {
    BJJA_LM_4G_early_init();
    DELAY_US(50*1000);
    PRINT_DATA("4G SIM card detecting\r\n");
    UartMessage("AT+CCID\r\n",strlen("AT+CCID\r\n"));
    DELAY_US(10*1000);
  }
  /*else if(TELCOMM_STATUS_4G_DETECTED_FAIL==g4GStatus)
  {
    BJJA_write_data_LOG("4G SIM card detecting\r\n",strlen("4G SIM card detecting\r\n"));
    HAL_UART_Transmit(&huart2,"AT+CCID\r\n",strlen("AT+CCID\r\n"),100);
    HAL_Delay(10);
  }*/
  /*else if(g4GStatus==TELCOMM_STATUS_4G_DETECTED_OPERATOR_OK)
  {
    HAL_UART_Transmit(&huart2,"AT+QIACT=1\r\n",strlen("AT+QIACT=1\r\n"),100);
    BJJA_write_data_LOG("AT+QIACT=1\r\n",strlen("AT+QIACT=1\r\n"));
    g4GStatus=TELCOMM_STATUS_CONNECTING;

  }*/
  else if(g4GStatus==TELCOMM_STATUS_4G_DETECTED_OK || g4GStatus==TELCOMM_STATUS_ONLINE ||g4GStatus==TELCOMM_STATUS_CONNECTING || g4GStatus == TELCOMM_STATUS_EARLY_ONLINE)
  {
    //parsing_4G_return_data();
    
    //if(g4GStatus == TELCOMM_STATUS_EARLY_ONLINE)//weli mark
    //if(g4GStatus == TELCOMM_STATUS_ONLINE)
    //  send_mqtt_test_cmd("AT+PING\r\n");
    PRINT_DATA("4G heartbeat\r\n");
    /*UartMessage("AT+GSN\r\n",strlen("AT+GSN\r\n"));
    PRINT_DATA("AT+GSN\r\n");
    DELAY_US(50*1000);BJJA_LM_tick_wdt();*/
    if(gEarly_online_mqtt_reconnect_flag)
    {
      gEarly_online_mqtt_reconnect_flag=0;
      PRINT_DATA("reconnect MQTT\r\n");
      BJJA_mqtt_connect();
    }
    else
    {
      if(g4GStatus == TELCOMM_STATUS_EARLY_ONLINE)
      {
        //如果在early online,一直沒連上MQTT,在經過3次(30s)heart beat時間,就重連
        if(gEarly_online_mqtt_reconnect_count>2)
        {
          gEarly_online_mqtt_reconnect_flag=1;
        }
        else
        {
          gEarly_online_mqtt_reconnect_count++;
        }
        PRINT_DATA("mqtt_reconnect_count:%d,mqtt_reconnect_flag:%d,gMQTT_total_retry_count:%d\r\n",gEarly_online_mqtt_reconnect_count,gEarly_online_mqtt_reconnect_flag,gMQTT_total_retry_count);
      }
      else
      {
        gEarly_online_mqtt_reconnect_flag=0;
        gEarly_online_mqtt_reconnect_count=0;
        gMQTT_total_retry_count=0;
      }
        UartMessage("AT+CREG?\r\n",strlen("AT+CREG?\r\n"));
        PRINT_DATA("AT+CREG?\r\n");
        DELAY_US(50*1000);BJJA_LM_tick_wdt();
        /*UartMessage("AT+QIACT?\r\n",strlen("AT+QIACT?\r\n"));
        PRINT_DATA("AT+QIACT?\r\n");
        DELAY_US(50*1000);BJJA_LM_tick_wdt();*/
        UartMessage("AT+CSQ\r\n",strlen("AT+CSQ\r\n"));
        PRINT_DATA("AT+CSQ\r\n");  
    }
    
    
    PRINT_DATA("4G heartbeat done\r\n");
  }
  else if(g4GStatus == TELCOMM_STATUS_OFFLINE)
  {
    PRINT_DATA("reconnect 4G from heartbeat\r\n");
    BJJA_reconnect_4G();
  }
  if(g4GStatus==TELCOMM_STATUS_ONLINE)
  {
    if(gPING_count>=12)//10seconds*12=120seconds
    {
      SEND_LTE_M("AT+QPING=1,\"8.8.8.8\"\r\n");
      gPING_count=0;
    }
    else
    {
      gPING_count++;
    }
  }
  PRINT_DATA("4G current state machine:");
  if(g4GStatus==TELCOMM_STATUS_OFFLINE)
    PRINT_DATA("TELCOMM_STATUS_OFFLINE\r\n");
  else if(g4GStatus==TELCOMM_STATUS_ONLINE)
    PRINT_DATA("TELCOMM_STATUS_ONLINE\r\n");
  else if(g4GStatus==TELCOMM_STATUS_CONNECTING)
    PRINT_DATA("TELCOMM_STATUS_CONNECTING\r\n");
  else if(g4GStatus==TELCOMM_STATUS_4G_NON_DETECTED)
    PRINT_DATA("TELCOMM_STATUS_4G_NON_DETECTED\r\n");
  else if(g4GStatus==TELCOMM_STATUS_4G_DETECTED_OK)
    PRINT_DATA("TELCOMM_STATUS_4G_DETECTED_OK\r\n");
  else if(g4GStatus==TELCOMM_STATUS_4G_DETECTED_FAIL)
    PRINT_DATA("TELCOMM_STATUS_4G_DETECTED_FAIL\r\n");
  else if(g4GStatus==TELCOMM_STATUS_EARLY_ONLINE)
    PRINT_DATA("TELCOMM_STATUS_EARLY_ONLINE\r\n");
}
void BJJA_reconnect_4G()
{
  gAuto_Ping_count=0x00;
  gMQTT_received_flag=0;
  g4G_connection_retry_count=1;
  g4GStatus = TELCOMM_STATUS_4G_NON_DETECTED;
  SEND_LTE_M("AT+QMTCLOSE=0\r\n");
  BJJA_LM_tick_wdt();
  DELAY_US(1000*1000);
  SEND_LTE_M("AT+CFUN=1,1\r\n");
  //HAL_Delay(1000*10);//wait for 4G init
  uint8_t i=0;
  for(i=0;i<20;i++)
  {
    BJJA_LM_tick_wdt();
    DELAY_US(500*1000);
    BJJA_LM_tick_wdt();
  }
  
  PRINT_DATA("reconnect 4G module\r\n");
  first_4g_flag=0;
  gCsq_val=31;
}

static void BJJA_mqtt_connect()
{
  PRINT_DATA("TODO:%s:line:%d\r\n",__FUNCTION__,__LINE__);
  if(gMQTT_total_retry_count>4)
  {
    //如果連線5次都拿不到MQTT的連線,就重新初始化4G
    PRINT_DATA("MQTT reconnect>4,do re-init 4G module\r\n");
    g4GStatus = TELCOMM_STATUS_OFFLINE;
    gEarly_online_mqtt_reconnect_flag=0;
    gEarly_online_mqtt_reconnect_count=0;
    gMQTT_total_retry_count=0;

  }
  else
  {
    DELAY_US(300*1000);
    SEND_LTE_M("AT+QMTOPEN=0,\"%s\",%d\r\n",/*gFlash_data.mqtt_url*/"a3toi86b2jby7e-ats.iot.eu-west-1.amazonaws.com",gFlash_data.mqtt_port);
    DELAY_US(300*1000);
    gMQTT_total_retry_count++;
  }

  
}
void parsing_mqtt_return_cmd(uint8_t *data)
{
  PRINT_DATA("TODO:%s:line:%d\r\n",__FUNCTION__,__LINE__);
  PRINT_DATA("MQTT DATA:[%s]\r\n",data);
  char *pch = strstr(data,"{");
  char my_data[512]={0x00};
  uint8_t i=0;
  for(i=0;i<strlen(pch);i++)
  {
    if(pch[i]=='}')
    {
      break;
    }
  }
  memcpy(my_data,pch,i+1);
  PRINT_DATA("pre-process DATA:[%s]\r\n",my_data);

#if 0
  if(strncmp(data,"AT+Door=",strlen("AT+Door="))==0)
  {
    if(data[8]=='0')
    {
      BJJA_LM_control_door_function(0);
      send_mqtt_cmd("OK+Door:0\r\n");
    }
    else
    {
      BJJA_LM_control_door_function(1);
      send_mqtt_cmd("OK+Door:1\r\n");
    }
  }
  else if(strncmp(data,"AT+ARM",strlen("AT+ARM"))==0)
  {
    
    if(BJJA_LM_Early_Entry_Arm_state())
    {
      PRINT_DATA("from MQTT set ARM OK\r\n");
      gArm_Disarm_command=1;
      send_mqtt_cmd("OK+ARM\r\n");
    }
    else
    {
      PRINT_DATA("from MQTT set ARM FAIL\r\n");
      send_mqtt_cmd("FAIL+ARM\r\n");
    }
  }
  else if(strncmp(data,"AT+DISARM",strlen("AT+DISARM"))==0)
  {
    if(BJJA_LM_Early_Entry_DisArm_state())
    {
      PRINT_DATA("from MQTT set DISARM OK\r\n");
      gArm_Disarm_command=2;
      send_mqtt_cmd("OK+DISARM\r\n");
    }
    else
    {
      PRINT_DATA("from MQTT set DISARM FAIL\r\n");
      send_mqtt_cmd("FAIL+DISARM\r\n");
    }
  }
  /*else if(strncmp(data,"AT+DRMode=?",strlen("AT+DRMode=?"))==0)
  {
    gCurrent_Notify_id=4;
    multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
  }
  else if(strncmp(data,"AT+DRMode=",strlen("AT+DRMode="))==0)
  {
    if(charValue3[10]=='0')
      gFlash_data.door_mode = 0;
    else
      gFlash_data.door_mode = 1;
    gCurrent_Notify_id=3;
    multi_role_enqueueMsg(BJJA_LM_Notify_DATA_EVT,NULL);
  }*/
  else if(strncmp(data,"AT+GetStatus=?",strlen("AT+GetStatus=?"))==0)
  {
    uint8_t msg[128]={0x00};
    sprintf(msg,"OK+GetStatus:%s,%d,%d,%d,%s\r\n",/*"weli",12300,32,0,"N,E"*/gFlash_data.user_name,gFlash_data.SOC,gFlash_data.ODO_meter,gDoor_State,gLastGpsLat);
    send_mqtt_cmd(msg);
  }
  else if(strncmp(data,"AT+ChangeUploadInterval=?",strlen("AT+ChangeUploadInterval=?"))==0)
  {
    uint8_t tmp_result[40]={0x00};
    sprintf(tmp_result,"OK+ChangeUploadInterval:%d\r\n",gFlash_data.periodic_timer);
    send_mqtt_cmd(tmp_result);
  }
  else if(strncmp(data,"AT+ChangeUploadInterval=",strlen("AT+ChangeUploadInterval="))==0)
  {
    uint16_t val = atoi(data+strlen("AT+ChangeUploadInterval="));
    if(val>=20 && val<=7200)
    {
      gFlash_data.periodic_timer=val;
      BJJA_LM_write_flash();  
      send_mqtt_cmd("OK+ChangeUploadInterval\r\n");
    }
    else
    {
      send_mqtt_cmd("FAIL+ChangeUploadInterval\r\n"); 
    }
  }
#else
  BJJA_LM_Json_cmd_parsing_from_MQTT_downlink_channel(my_data);
  

  
  

#endif

}
void BJJA_LM_enable_gps()
{
  //SEND_LTE_M("AT+QGPS=1\r\n");
}
void BJJA_LM_disable_gps()
{
  //SEND_LTE_M("AT+QGPSEND\r\n");
}
void BJJA_LM_4G_mode()
{
  SEND_LTE_M("AT+QGPSCFG=\"priority\",1\r\n");//4G mode
}
void BJJA_LM_GPS_mode()
{
  //SEND_LTE_M("AT+QGPSCFG=\"priority\",0\r\n");//gps mode
}
uint8_t BJJA_LM_read_gps()
{
#if 0
  BJJA_LM_GPS_mode();
  //gLastGpsLat
  gLastGpsLat_flag=1;
  uint8_t i=0;
  for(i=0;i<8;i++)
  {
    SEND_LTE_M("AT+QGPSLOC=0\r\n");
    
    Task_sleep(1000000 / Clock_tickPeriod); BJJA_LM_tick_wdt();
    if(gLastGpsLat_flag==0)
    {
      PRINT_DATA("got gps location\r\n");
      break;
    }
    Task_sleep(1000000 / Clock_tickPeriod); BJJA_LM_tick_wdt();
    Task_sleep(1000000 / Clock_tickPeriod); BJJA_LM_tick_wdt();
    Task_sleep(1000000 / Clock_tickPeriod); BJJA_LM_tick_wdt();
    Task_sleep(1000000 / Clock_tickPeriod); BJJA_LM_tick_wdt();
    //SEND_LTE_M("AT+QGPSCFG=\"priority\"\r\n");

    
  }
  BJJA_LM_4G_mode();
  /*if(gLastGpsLat_flag)
  {
    sprintf(gLastGpsLat,"NO_GPS\r\n");
  }*/
#else
  gLastGpsLat_flag=1;
  PRINT_DATA("start_parsing gps\r\n");
#endif
  return gLastGpsLat_flag;
}
void send_mqtt_cmd(uint8_t *mylocaldata)
{
  gAutoMode_reconnecting_count++;
  PRINT_DATA("%s-send mqtt data\r\n",__FUNCTION__);
  SEND_LTE_M("AT+QMTPUB=0,0,0,0,\"/AVIS/%02x%02x%02x%02x%02x%02x/uplink\",%d\r\n",gMac[0],gMac[1],gMac[2],gMac[3],gMac[4],gMac[5],strlen(mylocaldata));
  DELAY_US(30*1000);
  SEND_LTE_M("%s",mylocaldata);
  
}
void send_mqtt_test_cmd(uint8_t *mylocaldata,uint8 gps_en)
{
  PRINT_DATA("TODO:%s:line:%d\r\n",__FUNCTION__,__LINE__);
#if 1
  //if(gps_en)
  if(1)//add by weli 20221121 customer require,when acc off,still upload gps
  {
    BJJA_LM_read_gps();
  }
  if(strlen(gLastGpsLat)==0)
  {
    snprintf(gLastGpsLat,sizeof(gLastGpsLat),"%dN,%dE",0,0);
  }
  uint8_t msg[128]={0x00};
  if(gps_en)
    sprintf(msg,"+EVT_STATUS:%s,%d,%d,%d,%s\r\n",/*"weli",12300,32,0,"N,E"*/gFlash_data.user_name,gFlash_data.SOC,gFlash_data.ODO_meter,gDoor_State,gLastGpsLat);
  else
    sprintf(msg,"+EVT_HB:%s,%d,%d,%d,%s\r\n",/*"weli",12300,32,0,"N,E"*/gFlash_data.user_name,gFlash_data.SOC,gFlash_data.ODO_meter,gDoor_State,gLastGpsLat);
  //PRINT_DATA("send msg:%s\r\n",msg);
  //memset(msg,0x00,sizeof(msg));
  //snprintf(mylog_s,"UPLOAD:%s\r\n",gLastGpsLat);
  
  //snprintf(mylog_s,"GPS:%s\r\n",gLastGpsLat);

#if 0
  SEND_LTE_M("AT+QMTPUB=0,0,0,0,\"/AVIS/%02x%02x%02x%02x%02x%02x/uplink\",%d\r\n",gMac[0],gMac[1],gMac[2],gMac[3],gMac[4],gMac[5],/*strlen(mylocaldata)*/strlen(msg));
  DELAY_US(30*1000);
  SEND_LTE_M("%s",msg/*mylocaldata*/);
#else
    BJJA_LM_Json_periodic_upload_mqtt(msg);
#endif
  //SEND_LTE_M(mylog_s);
#endif
  PRINT_DATA("TODO:%s:line:%d\r\n",__FUNCTION__,__LINE__);
}
void BJJA_LM_AWS_IoT_init()
{
  DELAY_US(50*1000);
  SEND_LTE_M("AT+QMTCFG=\"ssl\",0,1,2\r\n");DELAY_US(50*1000);
#if 1
  /*SEND_LTE_M("AT+QSSLCFG=\"cacert\",2,\"cacert_avis.pem\"\r\n");DELAY_US(50*1000);
  SEND_LTE_M("AT+QSSLCFG=\"clientcert\",2,\"client_avis.pem\"\r\n");DELAY_US(50*1000);
  SEND_LTE_M("AT+QSSLCFG=\"clientkey\",2,\"user_key_avis.pem\"\r\n");DELAY_US(50*1000);*/
  SEND_LTE_M("AT+QSSLCFG=\"cacert\",2,\"cacert_avis_all.pem\"\r\n");DELAY_US(50*1000);
  SEND_LTE_M("AT+QSSLCFG=\"clientcert\",2,\"client_avis_all.pem\"\r\n");DELAY_US(50*1000);
  SEND_LTE_M("AT+QSSLCFG=\"clientkey\",2,\"user_key_avis_all.pem\"\r\n");DELAY_US(50*1000);
#else
  SEND_LTE_M("AT+QSSLCFG=\"cacert\",2,\"cacert.pem\"\r\n");DELAY_US(50*1000);
  SEND_LTE_M("AT+QSSLCFG=\"clientcert\",2,\"client.pem\"\r\n");DELAY_US(50*1000);
  SEND_LTE_M("AT+QSSLCFG=\"clientkey\",2,\"user_key.pem\"\r\n");DELAY_US(50*1000);
#endif
  SEND_LTE_M("AT+QSSLCFG=\"seclevel\",2,2\r\n");DELAY_US(50*1000);
  SEND_LTE_M("AT+QSSLCFG=\"sslversion\",2,4\r\n");DELAY_US(50*1000);
  SEND_LTE_M("AT+QSSLCFG=\"ciphersuite\",2,0XFFFF\r\n");DELAY_US(50*1000);
  SEND_LTE_M("AT+QSSLCFG=\"ignorelocaltime\",2,1\r\n");DELAY_US(50*1000);
  DELAY_US(50*1000);
}
void BJJA_LM_4G_early_init()
{
  
  PRINT_DATA("4G early_init\r\n");

  SEND_LTE_M("AT+QMTCLOSE=0\r\n");
  BJJA_LM_tick_wdt();
  DELAY_US(1000*1000);
  BJJA_LM_tick_wdt();
  SEND_LTE_M("AT+CFUN=1,1\r\n");

  uint8_t i=0;
  for(i=0;i<20;i++)
  {
    BJJA_LM_tick_wdt();
    DELAY_US(500*1000);
    BJJA_LM_tick_wdt();
  }
  
  BJJA_LM_4G_mode();
  /*DELAY_US(50*1000);
  BJJA_LM_enable_gps();//todo:test,need remove
  DELAY_US(50*1000);*/

  

  BJJA_LM_AWS_IoT_init();
  SEND_LTE_M("AT+QMTCFG=\"keepalive\",0,3600\r\n");
  DELAY_US(50*1000);
  SEND_LTE_M("AT+QMTCFG=\"timeout\",0,60,10,1\r\n");
  DELAY_US(50*1000);



}
uint16_t gTimer_count_for_periodic_upload=0x00;
void BJJA_LM_1S_worker()
{
  
  if(gTimer_count_for_periodic_upload>gFlash_data.periodic_timer)
  {
    if(g4GStatus == TELCOMM_STATUS_ONLINE && gACC_ON_timer_flag!=0)
    {
      send_mqtt_test_cmd("WAKEUP\r\n",1);  //read gps
    }
    else if(g4GStatus == TELCOMM_STATUS_ONLINE)
    {
      send_mqtt_test_cmd("WAKEUP\r\n",0);  //acc off periodic upload 20221118
    }
    gTimer_count_for_periodic_upload=0;
  }
  else
  {
    
    gTimer_count_for_periodic_upload++;
  }
  if(g4GStatus == TELCOMM_STATUS_ONLINE && gFirst_boot)
  {
    gFirst_boot=0;
    uint8_t mydata[64]={0x00};
    //sprintf(mydata,"+EVT_MB_POWER_ON:%s,%d,%d,%d\r\n",gFlash_data.user_name,gFlash_data.SOC,gFlash_data.ODO_meter,gDoor_State);
#if 0    
    send_mqtt_cmd(mydata);
#else
    //BJJA_LM_Json_periodic_upload_mqtt(mydata);
#endif
  }
#if 1
  if(g4GStatus == TELCOMM_STATUS_ONLINE && gACC_ON_OFF_flag>0)
  {
    uint8_t mydata[64]={0x00};
    if(gACC_ON_timer_flag==1)
      sprintf(mydata,"+EVT_ACC_ON:%s,%d,%d,%d\r\n",gFlash_data.user_name,gFlash_data.SOC,gFlash_data.ODO_meter,gDoor_State);
    else if(gACC_ON_OFF_flag==2)
      sprintf(mydata,"+EVT_ACC_OFF:%s,%d,%d,%d\r\n",gFlash_data.user_name,gFlash_data.SOC,gFlash_data.ODO_meter,gDoor_State);
    else if(gACC_ON_OFF_flag==3)//update ARM state
      sprintf(mydata,"+EVT_ARM:%s,%d,%d,%d\r\n",gFlash_data.user_name,gFlash_data.SOC,gFlash_data.ODO_meter,gDoor_State);
    else if(gACC_ON_OFF_flag==4)//update DISARM state
      sprintf(mydata,"+EVT_DISARM:%s,%d,%d,%d\r\n",gFlash_data.user_name,gFlash_data.SOC,gFlash_data.ODO_meter,gDoor_State);
    else if(gACC_ON_OFF_flag==5)//update DISARM state
      sprintf(mydata,"+EVT_DOOR:%s,%d,%d,%d\r\n",gFlash_data.user_name,gFlash_data.SOC,gFlash_data.ODO_meter,gDoor_State);
#if 0    
    send_mqtt_cmd(mydata);
#else
    BJJA_LM_Json_periodic_upload_mqtt(mydata);
#endif
    gACC_ON_OFF_flag=0;
  }
#endif
}
void BJJA_LM_control_door_function(uint8_t mode)
{
  if(mode==0)//lock mode
  {
    GPIO_write(GPIO_LOCK,1);
    if(gFlash_data.door_mode==0)
    {
        Task_sleep(750000 / Clock_tickPeriod);//us  
    }
    else
    {
        Task_sleep(1000000 / Clock_tickPeriod);//us   
        BJJA_LM_tick_wdt();
        Task_sleep(1000000 / Clock_tickPeriod);//us   
        BJJA_LM_tick_wdt();
        Task_sleep(1000000 / Clock_tickPeriod);//us   
        BJJA_LM_tick_wdt();
        Task_sleep(1000000 / Clock_tickPeriod);//us   
        BJJA_LM_tick_wdt();
        Task_sleep(1000000 / Clock_tickPeriod);//us   
        BJJA_LM_tick_wdt();
    }
    GPIO_write(GPIO_LOCK,0);
  }
  else//unlock mode
  {
    GPIO_write(GPIO_UNLOCK,1);
    if(gFlash_data.door_mode==0)
    {
        Task_sleep(750000 / Clock_tickPeriod);//us  
    }
    else
    {
        Task_sleep(1000000 / Clock_tickPeriod);//us   
        BJJA_LM_tick_wdt();
        Task_sleep(1000000 / Clock_tickPeriod);//us   
        BJJA_LM_tick_wdt();
        Task_sleep(1000000 / Clock_tickPeriod);//us   
        BJJA_LM_tick_wdt();
        Task_sleep(1000000 / Clock_tickPeriod);//us   
        BJJA_LM_tick_wdt();
        Task_sleep(1000000 / Clock_tickPeriod);//us   
        BJJA_LM_tick_wdt();
    }
    GPIO_write(GPIO_UNLOCK,0);
  }
}
void BJJA_LM_BLE_INCOMING()
{
  uint8_t mytmp[32]={0x00};
  if(gCurrent_Notify_id==1 || gCurrent_Notify_id==2)
  {
    sprintf(mytmp,"OK+OpenDR:%d",gCurrent_Notify_id-1);
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(mytmp) ,mytmp);
    BJJA_LM_control_door_function(gCurrent_Notify_id-1);
  }
  else if(gCurrent_Notify_id==3)
  {
    sprintf(mytmp,"OK+DRMode:%d",gFlash_data.door_mode);
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(mytmp) ,mytmp);

    BJJA_LM_write_flash();
  }
  else if(gCurrent_Notify_id==4)
  {
    sprintf(mytmp,"OK+DRMode:%d",gFlash_data.door_mode);
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(mytmp) ,mytmp);
  }
  else if(gCurrent_Notify_id==5)
  {
    sprintf(mytmp,"OK+ARM");
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(mytmp) ,mytmp);
    //DELAY_US(1000*100);
    gArm_Disarm_command=1;
  }
  else if(gCurrent_Notify_id==6)
  {
    sprintf(mytmp,"FAIL+ARM");
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(mytmp) ,mytmp);
  }
  else if(gCurrent_Notify_id==7)
  {
    sprintf(mytmp,"OK+DISARM");
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(mytmp) ,mytmp);
    //DELAY_US(1000*100);
    gArm_Disarm_command=2;
  }
  else if(gCurrent_Notify_id==8)
  {
    sprintf(mytmp,"FAIL+DISARM");
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(mytmp) ,mytmp);
  }
  else if(gCurrent_Notify_id==9)
  {
    sprintf(mytmp,"OK+SUEN:%d",gSuperUserMode);
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(mytmp) ,mytmp);
  }
  else if(gCurrent_Notify_id==10)
  {
    sprintf(mytmp,"FAIL+PERMISSION_DENIED");
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(mytmp) ,mytmp);
  }
  else if(gCurrent_Notify_id==11)
  {
    sprintf(mytmp,"OK+SUEN");
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(mytmp) ,mytmp);
  }
  else if(gCurrent_Notify_id==12)
  {
    sprintf(mytmp,"FAIL+SUEN");
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(mytmp) ,mytmp);
  }
  else if(gCurrent_Notify_id==13)
  {
    sprintf(mytmp,"OK+SUDIS");
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(mytmp) ,mytmp);
  }
  else if(gCurrent_Notify_id==14)
  {
    sprintf(mytmp,"OK+SR:\r\n");
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(mytmp) ,mytmp);
    DELAY_US(1000*30);
    uint8_t i=0;
    for(i=0;i<8;i++)
    {
      if(gSubGpairing_data[i].enable=='1')
      {
        sprintf(mytmp,"[%d]%02x%02x%02x%02x%02x%02x%02x%02x\r\n",i,
          gSubGpairing_data[i].S_code[0],gSubGpairing_data[i].S_code[1],
          gSubGpairing_data[i].S_code[2],gSubGpairing_data[i].S_code[3],
          gSubGpairing_data[i].S_code[4],gSubGpairing_data[i].S_code[5],
          gSubGpairing_data[i].S_code[6],gSubGpairing_data[i].S_code[7]);
        SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(mytmp) ,mytmp);
        DELAY_US(1000*50);
      }
    }
  }
  else if(gCurrent_Notify_id==15)//get OBD MAC
  {
    sprintf(mytmp,"OK+OBDMAC:%02x%02x%02x%02x%02x%02x\r\n",gFlash_data.obdii_mac[0],
      gFlash_data.obdii_mac[1],gFlash_data.obdii_mac[2],
      gFlash_data.obdii_mac[3],gFlash_data.obdii_mac[4],
      gFlash_data.obdii_mac[5]);
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(mytmp) ,mytmp);
  }
  else if(gCurrent_Notify_id==16)//set OBD MAC OK
  {
    BJJA_LM_write_flash();
    sprintf(mytmp,"OK+OBDMAC");
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(mytmp) ,mytmp);
  }
  else if(gCurrent_Notify_id==17)//set OBD MAC FAIL
  {
    sprintf(mytmp,"FAIL+OBDMAC");
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(mytmp) ,mytmp);
  }
  else if(gCurrent_Notify_id==18)//AT+SR DATA IS EXIST
  {
    sprintf(mytmp,"FAIL+SR:DATA_IS_EXIST");
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(mytmp) ,mytmp);
  }
  else if(gCurrent_Notify_id==19)//SR FAIL
  {
    sprintf(mytmp,"FAIL+SR");
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(mytmp) ,mytmp);
  }
  else if(gCurrent_Notify_id==20)//AT+SR DATA IS EXIST
  {
    BJJA_LM_write_SR_flash();
    sprintf(mytmp,"OK+SR");
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(mytmp) ,mytmp);
  }
  else if(gCurrent_Notify_id==21)//AT+SRD OK
  {
    BJJA_LM_write_SR_flash();
    sprintf(mytmp,"OK+SRD");
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(mytmp) ,mytmp);
  }
  else if(gCurrent_Notify_id==22)//SRD NOT FOUND
  {
    sprintf(mytmp,"FAIL+SRD:NOT_FOUND");
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(mytmp) ,mytmp);
  }
  else if(gCurrent_Notify_id==23)//SRD FAIL
  {
    sprintf(mytmp,"FAIL+SRD");
    SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(mytmp) ,mytmp);
  }
  gCurrent_Notify_id=0;
}
/************************* THIS IS FOR UART2 END ***********************/

void BJJA_LM_send_odo_cmd(uint8_t type)//0toyota,1 standard obdii
{
  if(type)
  {
    cansec_Write2Periphearl(0,6,"01A6\r\n");
  }
  else//toyota format
  {
    cansec_Write2Periphearl(0,7,"21281\r\n");
  }
}
void BJJA_LM_send_fuel_level_cmd(uint8_t type)//0toyota,1 standard obdii
{
  if(type)
  {
    cansec_Write2Periphearl(0,6,"012F\r\n");  
  }
  else//toyota format
  {
    //no fuel level
  }
  
}
void BJJA_LM_parsing_OBDII_data(uint8 *data,uint8 len)
{
  PRINT_DATA("from:%s,data:%s,len:%d\r\n",__FUNCTION__,data,len);
  uint8_t i=0,x=0,y=0,z=0,a=0;
  uint32_t odo_meter=0x00;
  uint8_t fuel_level=0x00;
  uint8_t ret=0;
  //todo if toyota
  //T:21281
  //R:7E80561280070E3
  //7E8(ID)-05(len)-61(mode)-28(PID)-0070E3(DATA)
  //(00(hex)*2^16)+(70(hex)*2^8)+(E3)
  //=28672+227=28899KM

  if(len>=15)//this is toyota odometer
  {
    for(i=0;i<len-8;i++)
    {
      if(data[0]=='7' && data[1]=='E' && data[2]=='8' &&
        data[3]=='0' && data[4]=='5' && data[5]=='6' && data[6]=='1'
        && data[7]=='2' && data[8]=='8')
      {
        ret=1;
        x = ((BJJA_ascii2hex(data[9])<<4) |(BJJA_ascii2hex(data[10])));
        y = ((BJJA_ascii2hex(data[11])<<4) |(BJJA_ascii2hex(data[12])));
        z = ((BJJA_ascii2hex(data[13])<<4) |(BJJA_ascii2hex(data[14])));
        odo_meter = (x*65536)+(y*256)+z;
        PRINT_DATA("toyota:%d,%d,%d,odo:%d\r\n",x,y,z,odo_meter);
        gFlash_data.ODO_meter = odo_meter;
        //gFlash_data.fuel_level = fuel_level;
        BJJA_LM_write_flash();
      }
    }
  }
  //--------------------------
  //todo standard obdii
  //T:012F fuel level
  //R:7E803412FB6
  //7E8(ID)-03(len)-41(mode)-2F(PID)-B6(DATA)
  //B6=182(dec)
  //100/255*182=71%
  //--------------------------
  if(ret==0 && len>=11)
  {
    for(i=0;i<len-8;i++)
    {
      if(data[0]=='7' && data[1]=='E' && data[2]=='8' &&
        data[3]=='0' &&data[4]=='3' &&data[5]=='4' &&data[6]=='1'
        &&data[7]=='2' &&data[8]=='F')
      {
        x = ((BJJA_ascii2hex(data[9])<<4) |(BJJA_ascii2hex(data[10])));
        ret=1;
        fuel_level = ((int)((float)(0.39*x)));
        PRINT_DATA("standard OBDII fuel level:%d,fuel level:%d\r\n",x,fuel_level);
        gFlash_data.SOC = fuel_level;
        BJJA_LM_write_flash();
      }
    }
  }

  //--------------------------
  //todo standard obdii
  //T:012F fuel level
  //R:7E803412FB6
  //7E8(ID)-03(len)-41(mode)-2F(PID)-B6(DATA)
  //B6=182(dec)
  //100/255*182=71%
  //--------------------------
  //T:01A6 odo meter
  //R:7E80641A600066095
  //7E8(ID)-06(len)-41(mode)-A6(PID)-00066095(DATA)
  //65536*6 + 256*96 + 149 = 393216+24576+149=4117941/10 =41794KM
  if(len>=17)//this is toyota odometer
  {
    for(i=0;i<len-8;i++)
    {
      if(data[0]=='7' && data[1]=='E' && data[2]=='8' &&
        data[3]=='0' &&data[4]=='6' &&data[5]=='4' &&data[6]=='1'
        &&data[7]=='A' &&data[8]=='6')
      {
        ret=1;
        x = ((BJJA_ascii2hex(data[11])<<4) |(BJJA_ascii2hex(data[12])));
        y = ((BJJA_ascii2hex(data[13])<<4) |(BJJA_ascii2hex(data[14])));
        z = ((BJJA_ascii2hex(data[15])<<4) |(BJJA_ascii2hex(data[16])));
        odo_meter = ((x*65536)+(y*256)+z)/10;
        PRINT_DATA("standard OBDII:%d,%d,%d,odo:%d\r\n",x,y,z,odo_meter);
        gFlash_data.ODO_meter = odo_meter;
        //gFlash_data.fuel_level = fuel_level;
        BJJA_LM_write_flash();
      }
    }
  }
}
/**************add by weli begin for json function***********************/
uint8_t BJJA_LM_create_json(Json_Handle *hLocalObject,Json_Handle *hLocalTemplate,char *json_schema,char *json_data)
{
    int16_t retVal;
    /* create a template from a buffer containing a template */
    retVal = Json_createTemplate(hLocalTemplate, json_schema,
            strlen(json_schema));
    if (retVal != 0) {
        PRINT_DATA("Error creating the JSON template\r\n");
        return 0;
    }
    else {
        PRINT_DATA("JSON template created\r\n");
    }

    /* create a default-sized JSON object from the template */
    retVal = Json_createObject(hLocalObject, *hLocalTemplate, 0);
    if (retVal != 0) 
    {
        PRINT_DATA("Error creating JSON object\r\n");
        return 0;
    }
    else 
    {
        PRINT_DATA("JSON object created from template\r\n");
    }

    /* parse EXAMPLE_JSONBUF */
    retVal = Json_parse(*hLocalObject, json_data, strlen(json_data));
    if (retVal != 0) 
    {
        PRINT_DATA("Error parsing the JSON buffer\r\n");
        return 0;
    }
    else 
    {
        PRINT_DATA("JSON buffer parsed\r\n");
    }


    return 1;

}
uint8_t BJJA_LM_json_get_token_data(Json_Handle *hLocalObject,uint8_t *ret_data,char *pKey)
{
    void *valueBuf;
    int16_t retVal;
    uint16_t valueSize;
    uint16_t jsonBufSize;
    retVal = Json_getValue(*hLocalObject, pKey, NULL, &valueSize);
    if (retVal != 0) 
    {
        //Display_printf(display, 0, 0, "Error getting the %s buffer size",pKey);
        return 0;
    }
    else 
    {
        //Display_printf(display, 0, 0, "%s buffer size: %d\n", pKey, valueSize);
    }

    valueBuf = calloc(1, valueSize + 1);
    retVal = Json_getValue(*hLocalObject, pKey, valueBuf, &valueSize);
    if (retVal != 0) 
    {
        //Display_printf(display, 0, 0, "Error getting the %s value",pKey);
        return 0;
    }
    else 
    {
        //Display_printf(display, 0, 0, "JSON buffer parsed, %s: %s\n",pKey,valueBuf);
    }
    memcpy(ret_data,valueBuf,valueSize);
    free(valueBuf);
    return 1;

}
uint8_t BJJA_LM_json_set_token_data(Json_Handle *hLocalObject,uint8_t *set_value,char *pKey)
{
    int16_t retVal;
    //retVal = Json_getValue(hObject, "\"age\"", &age, &valueSize);
    retVal = Json_setValue(*hLocalObject, pKey, set_value, strlen(set_value));
    if (retVal != 0) 
    {
        PRINT_DATA("Error setting the age value\r\n");
        return 0;
    }
    return 1;

}
uint8_t BJJA_LM_json_build(Json_Handle *hLocalObject,char *buffer,uint16_t jsonBufSize)
{
    int16_t retVal = Json_build(*hLocalObject, buffer, &jsonBufSize);
    if (retVal != 0) 
    {
        PRINT_DATA("Error serializing JSON data\r\n");
        return 0;
    }
    else 
    {
        PRINT_DATA("serialized data:\n%s\r\n", buffer);
        return 1;
    }
}
uint8_t BJJA_LM_destory_json(Json_Handle *hTemplate,Json_Handle *hObject)
{
    int16_t retVal =0;
    /* done, cleanup */
    retVal = Json_destroyObject(*hObject);

    retVal = Json_destroyTemplate(*hTemplate);

    PRINT_DATA("Finished JSON example\r\n");
    return 1;
}
void BJJA_LM_OpenDoor()
{
  //BJJA_LM_control_door_function(0);//lock test
  BJJA_LM_control_door_function(1);//unlock test
}
void BJJA_LM_OpenDoorAndDisarm()
{
  //door unlock
  BJJA_LM_control_door_function(1);//unlock test
  //disarm
  gArm_Disarm_command=2;//disarm
}
void BJJA_LM_CloseDoor()
{
  BJJA_LM_control_door_function(0);//lock test
  //BJJA_LM_control_door_function(1);//unlock test
}
void BJJA_LM_Disarm()
{
  //disarm
  gArm_Disarm_command=2;//disarm
}
void BJJA_LM_Arm()
{
  //disarm
  gArm_Disarm_command=1;//arm
}
uint8_t BJJA_LM_Json_cmd_parsing_from_MQTT_downlink_channel(char *buf)
{
  uint16_t jsonBufSize=1024;
  static char jsonBuf[1024];  /* max string to hold serialized JSON buffer */
  Json_Handle hTemplate;
  Json_Handle hObject;
  BJJA_LM_create_json(&hObject,&hTemplate,MQTT_DOWNLINK_SCHEMA,buf);
  char token_data[255]={0x00};

  Json_Handle hTemplate_response;
  Json_Handle hObject_response;  
  uint8_t my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"command\"");
  if(my_ret)
  {
    if(strncmp(token_data,"GetStatus",strlen("GetStatus"))==0)
    {
      BJJA_LM_create_json(&hObject_response,&hTemplate_response,RESPONSE_SCHEMA_UPLINK,EXAMPLE_OF_RESPONSE_UPLINK);

      BJJA_LM_json_set_token_data(&hObject_response,"GetStatusResponse","\"command\"");  
      memset(token_data,0x00,sizeof(token_data));
      
      my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"commandId\"");
      if(my_ret)
      {
        BJJA_LM_json_set_token_data(&hObject_response,token_data,"\"commandId\"");
      }
      memset(token_data,0x00,sizeof(token_data));

      my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"uuid\"");
      if(my_ret)
      {
        BJJA_LM_json_set_token_data(&hObject_response,token_data,"\"uuid\"");
      }
      memset(token_data,0x00,sizeof(token_data));

      my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"externalId\"");
      if(my_ret)
      {
        BJJA_LM_json_set_token_data(&hObject_response,token_data,"\"externalId\"");
      }
      memset(token_data,0x00,sizeof(token_data));
      sprintf(token_data,"%d,%d,%d,%s",/*"weli",12300,32,0,"N,E"*/gFlash_data.SOC,gFlash_data.ODO_meter,gDoor_State,gLastGpsLat);
      BJJA_LM_json_set_token_data(&hObject_response,token_data,"\"payload\"");

      
      BJJA_LM_json_build(&hObject_response,jsonBuf,jsonBufSize);
      BJJA_LM_destory_json(&hTemplate,&hObject);
      BJJA_LM_destory_json(&hTemplate_response,&hObject_response);
      send_mqtt_cmd(jsonBuf);
      return 0;
    }
    else if(strncmp(token_data,"OpenDoor",strlen("OpenDoor"))==0 ||
            strncmp(token_data,"OpenDoorAndDisarm",strlen("OpenDoorAndDisarm"))==0 ||
            strncmp(token_data,"CloseDoor",strlen("CloseDoor"))==0 ||
            strncmp(token_data,"Disarm",strlen("Disarm"))==0 ||
            strncmp(token_data,"Arm",strlen("Arm"))==0 )
    {
      uint8_t ret=0;
      BJJA_LM_create_json(&hObject_response,&hTemplate_response,MQTT_PERIODIC_UPLOAD_SCHEMA,EXAMPLE_OF_RESPONSE);
      if(strncmp(token_data,"OpenDoorAndDisarm",strlen("OpenDoorAndDisarm"))==0)
      {
        BJJA_LM_json_set_token_data(&hObject_response,"OpenDoorAndDisarmResponse","\"command\"");  
        BJJA_LM_OpenDoorAndDisarm();
      }
      else if(strncmp(token_data,"OpenDoor",strlen("OpenDoor"))==0)
      {
        BJJA_LM_json_set_token_data(&hObject_response,"OpenDoorResponse","\"command\"");  
        BJJA_LM_OpenDoor();
        
      }
      else if(strncmp(token_data,"CloseDoor",strlen("CloseDoor"))==0)
      {
        BJJA_LM_json_set_token_data(&hObject_response,"CloseDoorResponse","\"command\"");  
        BJJA_LM_CloseDoor();
      }
      else if(strncmp(token_data,"Disarm",strlen("Disarm"))==0)
      {
        BJJA_LM_json_set_token_data(&hObject_response,"DisarmResponse","\"command\"");  
        BJJA_LM_Disarm();
      }
      else if(strncmp(token_data,"Arm",strlen("Arm"))==0)
      {
        BJJA_LM_json_set_token_data(&hObject_response,"ArmResponse","\"command\"");  
        if(BJJA_LM_check_INGI()==1)
          ret=1;
        else
          BJJA_LM_Arm();
      }
      
      memset(token_data,0x00,sizeof(token_data));
      
      my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"commandId\"");
      if(my_ret)
      {
        BJJA_LM_json_set_token_data(&hObject_response,token_data,"\"commandId\"");
      }
      memset(token_data,0x00,sizeof(token_data));
      if(ret)
        BJJA_LM_json_set_token_data(&hObject_response,"Rejected","\"payload\"");
      else
        BJJA_LM_json_set_token_data(&hObject_response,"Accepted","\"payload\"");

      
      BJJA_LM_json_build(&hObject_response,jsonBuf,jsonBufSize);
      BJJA_LM_destory_json(&hTemplate,&hObject);
      BJJA_LM_destory_json(&hTemplate_response,&hObject_response);

      

      send_mqtt_cmd(jsonBuf);
      return 0;
    }
    else if(strncmp(token_data,"Authorize",strlen("Authorize"))==0)
    {
      memset(token_data,0x00,sizeof(token_data));
      my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"payload\"");
      if(my_ret)
      {
        if(strncmp(token_data,"Accepted",strlen("Accepted"))==0)
        {
          //todo send request command to MQTT broker.
          my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"originalCommand\"");
          if(my_ret)
          {
            if(strncmp(token_data,"OpenDoorResponse",strlen("OpenDoorResponse"))==0)
            {
              BJJA_LM_OpenDoor();
            }
            else if(strncmp(token_data,"OpenDoorAndDisarmResponse",strlen("OpenDoorAndDisarmResponse"))==0)
            {
              BJJA_LM_OpenDoorAndDisarm();
            }
            else if(strncmp(token_data,"CloseDoorResponse",strlen("CloseDoorResponse"))==0)
            {
              BJJA_LM_CloseDoor();
            }
            else if(strncmp(token_data,"DisarmResponse",strlen("DisarmResponse"))==0)
            {
              BJJA_LM_Disarm();
            }
            else if(strncmp(token_data,"ArmResponse",strlen("ArmResponse"))==0)
            {
              BJJA_LM_Arm();
            }
          }

        }
        else if(strncmp(token_data,"Rejected",strlen("Rejected"))==0)
        {
          ;
        }
      }
      //set to ble
      SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(buf) ,buf);
      BJJA_LM_destory_json(&hTemplate,&hObject);
      //BJJA_LM_destory_json(&hTemplate_response,&hObject_response);
      return 0;
    }//end of Authorize

#if 0
    else if(strncmp(token_data,"Authorize",strlen("Authorize"))==0)
    {
      memset(token_data,0x00,sizeof(token_data));
      my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"payload\"");
      if(my_ret)
      {
        if(strncmp(token_data,"Accepted",strlen("Accepted"))==0)
        {
          //todo send request command to MQTT broker.
          BJJA_LM_create_json(&hObject_response,&hTemplate_response,MQTT_UPLINK_REQUEST_SCHEMA,EXAMPLE_OF_RESPONSE);
          BJJA_LM_json_set_token_data(&hObject_response,Util_convertBdAddr2Str(connList[0].addr),"\"uuid\"");  
          memset(token_data,0x00,sizeof(token_data));

          my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"originalCommand\"");
          if(my_ret)
          {
            BJJA_LM_json_set_token_data(&hObject_response,token_data,"\"command\"");
          }
          memset(token_data,0x00,sizeof(token_data));

          my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"externalId\"");
          if(my_ret)
          {
            BJJA_LM_json_set_token_data(&hObject_response,token_data,"\"externalId\"");
          }
          memset(token_data,0x00,sizeof(token_data));

          my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"commandId\"");
          if(my_ret)
          {
            BJJA_LM_json_set_token_data(&hObject_response,token_data,"\"commandId\"");
          }
          memset(token_data,0x00,sizeof(token_data));

          //todo original command.
          BJJA_LM_json_build(&hObject_response,jsonBuf,jsonBufSize);
          send_mqtt_cmd(jsonBuf);

        }
        else if(strncmp(token_data,"Rejected",strlen("Rejected"))==0)
        {
          //send ble command to smart phone
          BJJA_LM_create_json(&hObject_response,&hTemplate_response,BLE_RESPONSE_SCHEMA,EXAMPLE_OF_RESPONSE);
          BJJA_LM_json_set_token_data(&hObject_response,token_data,"\"payload\"");
          memset(token_data,0x00,sizeof(token_data));
      
          my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"commandId\"");
          if(my_ret)
          {
            BJJA_LM_json_set_token_data(&hObject_response,token_data,"\"commandId\"");
          }
          memset(token_data,0x00,sizeof(token_data));

          my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"originalCommand\"");
          if(my_ret)
          {
            char cmd_response[64]={0x00};
            sprintf(cmd_response,"%sResponse",token_data);
            BJJA_LM_json_set_token_data(&hObject_response,cmd_response,"\"command\"");
          }
          memset(token_data,0x00,sizeof(token_data));
          BJJA_LM_json_build(&hObject_response,jsonBuf,jsonBufSize);
          //send to ble
          SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(jsonBuf) ,jsonBuf);
        }
      }
      BJJA_LM_destory_json(&hTemplate,&hObject);
      BJJA_LM_destory_json(&hTemplate_response,&hObject_response);
      return 0;
    }//end of Authorize
    else if(strncmp(token_data,"OpenDoorResponse",strlen("OpenDoorResponse"))==0)
    {
      memset(token_data,0x00,sizeof(token_data));
      my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"payload\"");
      if(my_ret)
      {
        if(strncmp(token_data,"Accepted",strlen("Accepted"))==0)
        {
           //todo door open
          //BJJA_LM_control_door_function(0);//lock test
          BJJA_LM_control_door_function(1);//unlock test
        }
        else if(strncmp(token_data,"Rejected",strlen("Rejected"))==0)
        {
          //rejected
        }
        PRINT_DATA("send to BLE:%s\r\n",buf);
        SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(buf) ,buf);
      }
      BJJA_LM_destory_json(&hTemplate,&hObject);
      BJJA_LM_destory_json(&hTemplate_response,&hObject_response);
      return 0;
    }//end of OpenDoorResponse
    else if(strncmp(token_data,"CloseDoorResponse",strlen("CloseDoorResponse"))==0)
    {
      memset(token_data,0x00,sizeof(token_data));
      my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"payload\"");
      if(my_ret)
      {
        if(strncmp(token_data,"Accepted",strlen("Accepted"))==0)
        {
           //door lock
          BJJA_LM_control_door_function(0);//lock test
          //BJJA_LM_control_door_function(1);//unlock test
        }
        else if(strncmp(token_data,"Rejected",strlen("Rejected"))==0)
        {
          //rejected
        }
        PRINT_DATA("send to BLE:%s\r\n",buf);
        SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(buf) ,buf);
      }
      BJJA_LM_destory_json(&hTemplate,&hObject);
      BJJA_LM_destory_json(&hTemplate_response,&hObject_response);
      return 0;
    }//end of CloseDoorResponse
    else if(strncmp(token_data,"OpenDoorAndDisarmResponse",strlen("OpenDoorAndDisarmResponse"))==0)
    {
      memset(token_data,0x00,sizeof(token_data));
      my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"payload\"");
      if(my_ret)
      {
        if(strncmp(token_data,"Accepted",strlen("Accepted"))==0)
        {
           //door unlock
          BJJA_LM_control_door_function(1);//unlock test
          //disarm
          gArm_Disarm_command=2;//disarm

        }
        else if(strncmp(token_data,"Rejected",strlen("Rejected"))==0)
        {
          //rejected
        }
        PRINT_DATA("send to BLE:%s\r\n",buf);
        SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(buf) ,buf);
      }
      BJJA_LM_destory_json(&hTemplate,&hObject);
      BJJA_LM_destory_json(&hTemplate_response,&hObject_response);
      return 0;
    }//end of OpenDoorAndDisarmReponse
    else if(strncmp(token_data,"DisarmResponse",strlen("DisarmResponse"))==0)
    {
      memset(token_data,0x00,sizeof(token_data));
      my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"payload\"");
      if(my_ret)
      {
        if(strncmp(token_data,"Accepted",strlen("Accepted"))==0)
        {
          //disarm
          gArm_Disarm_command=2;//disarm

        }
        else if(strncmp(token_data,"Rejected",strlen("Rejected"))==0)
        {
          //rejected
        }
        PRINT_DATA("send to BLE:%s\r\n",buf);
        SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(buf) ,buf);
      }
      BJJA_LM_destory_json(&hTemplate,&hObject);
      BJJA_LM_destory_json(&hTemplate_response,&hObject_response);
      return 0;
    }//end of DisarmReponse
    else if(strncmp(token_data,"ArmResponse",strlen("ArmResponse"))==0)
    {
      memset(token_data,0x00,sizeof(token_data));
      my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"payload\"");
      if(my_ret)
      {
        if(strncmp(token_data,"Accepted",strlen("Accepted"))==0)
        {
          //disarm
          gArm_Disarm_command=1;//arm

        }
        else if(strncmp(token_data,"Rejected",strlen("Rejected"))==0)
        {
          //rejected
        }
        PRINT_DATA("send to BLE:%s\r\n",buf);
        SimpleProfile_SetParameter(SIMPLEPROFILE_CHAR4,strlen(buf) ,buf);
      }
      BJJA_LM_destory_json(&hTemplate,&hObject);
      BJJA_LM_destory_json(&hTemplate_response,&hObject_response);
      return 0;
    }//end of ArmReponse
#endif


  }
  return 1;
}
uint8_t BJJA_LM_Json_periodic_upload_mqtt(char *buf)
{
  uint16_t jsonBufSize=1024;
  static char jsonBuf[1024];  /* max string to hold serialized JSON buffer */
  Json_Handle hTemplate;
  Json_Handle hObject;
  BJJA_LM_create_json(&hObject,&hTemplate,MQTT_PERIODIC_UPLOAD_SCHEMA,EXAMPLE_OF_MQTT_PERIODIC_UPLOAD);
  char token_data[255]={0x00};
  BJJA_LM_json_set_token_data(&hObject,buf,"\"payload\"");  
  BJJA_LM_json_build(&hObject,jsonBuf,jsonBufSize);
  BJJA_LM_destory_json(&hTemplate,&hObject);
  send_mqtt_cmd(jsonBuf);
  return 0;
}
uint8_t BJJA_LM_Json_cmd_parsing_from_BLE(char *buf)
{
  uint16_t jsonBufSize=1024;
  static char jsonBuf[1024];  /* max string to hold serialized JSON buffer */
  Json_Handle hTemplate;
  Json_Handle hObject;
  BJJA_LM_create_json(&hObject,&hTemplate,AUTHORIZE_SCHEMA,buf);
  char token_data[255]={0x00};
#if 0
  Json_Handle hTemplate_authorize;
  Json_Handle hObject_authorize;
  BJJA_LM_create_json(&hObject_authorize,&hTemplate_authorize,AUTHORIZE_SCHEMA,EXAMPLE_OF_AUTHORIZE);


  uint8_t my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"command\"");
  if(my_ret)
  {
    BJJA_LM_json_set_token_data(&hObject_authorize,"Authorize","\"command\"");  
    BJJA_LM_json_set_token_data(&hObject_authorize,Util_convertBdAddr2Str(connList[0].addr),"\"uuid\"");  

    BJJA_LM_json_set_token_data(&hObject_authorize,token_data,"\"originalCommand\"");  
    memset(token_data,0x00,sizeof(token_data));

    my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"commandId\"");
    if(my_ret)
    {
      BJJA_LM_json_set_token_data(&hObject_authorize,token_data,"\"commandId\"");  
    }
    memset(token_data,0x00,sizeof(token_data));
    
    my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"externalId\"");
    if(my_ret)
    {
      BJJA_LM_json_set_token_data(&hObject_authorize,token_data,"\"externalId\"");  
    }
    memset(token_data,0x00,sizeof(token_data));  

    BJJA_LM_json_build(&hObject_authorize,jsonBuf,jsonBufSize);
    BJJA_LM_destory_json(&hTemplate,&hObject);
    BJJA_LM_destory_json(&hTemplate_authorize,&hObject_authorize);
    send_mqtt_cmd(jsonBuf);
    return 0;
  }
#else
  uint8_t my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"command\"");
  if(my_ret && strncmp(token_data,"Authorize",strlen("Authorize"))==0)
  {
    BJJA_LM_json_set_token_data(&hObject,Util_convertBdAddr2Str(connList[0].addr),"\"uuid\"");  
    BJJA_LM_json_build(&hObject,jsonBuf,jsonBufSize);
    BJJA_LM_destory_json(&hTemplate,&hObject);
    send_mqtt_cmd(jsonBuf);
    return 0;
  }
#endif
  return 1;
  

#if 0
    if(strncmp(token_data,"GetStatus",strlen("GetStatus"))==0)
    {
      BJJA_LM_json_set_token_data(&hObject_response,"GetStatusResponse","\"command\"");  
      memset(token_data,0x00,sizeof(token_data));
      
      my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"commandId\"");
      if(my_ret)
      {
        BJJA_LM_json_set_token_data(&hObject_response,token_data,"\"commandId\"");
      }
      memset(token_data,0x00,sizeof(token_data));

      my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"uuid\"");
      if(my_ret)
      {
        BJJA_LM_json_set_token_data(&hObject_response,token_data,"\"uuid\"");
      }
      memset(token_data,0x00,sizeof(token_data));

      my_ret = BJJA_LM_json_get_token_data(&hObject,token_data,"\"externalId\"");
      if(my_ret)
      {
        BJJA_LM_json_set_token_data(&hObject_response,token_data,"\"externalId\"");
      }
      memset(token_data,0x00,sizeof(token_data));
      sprintf(token_data,"%d,%d,%d,%s",/*"weli",12300,32,0,"N,E"*/gFlash_data.SOC,gFlash_data.ODO_meter,gDoor_State,gLastGpsLat);
      BJJA_LM_json_set_token_data(&hObject_response,token_data,"\"payload\"");

      
      BJJA_LM_json_build(&hObject_response,jsonBuf,jsonBufSize);
      BJJA_LM_destory_json(&hTemplate,&hObject);
      BJJA_LM_destory_json(&hTemplate_response,&hObject_response);
      send_mqtt_cmd(jsonBuf);
    } 
  }
#endif
  
}
/**************add by weli begin for json function***********************/
