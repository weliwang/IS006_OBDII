#ifndef BJJA_LM_UTILITY_H
#define BJJA_LM_UTILITY_H

#include "bjja_lm_subg_radio_config.h"
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#define DELAY_US(i)      (Task_sleep(((i) * 1) / Clock_tickPeriod))

typedef struct
{
  uint8_t enable;
  uint8_t S_code[8];

} SubGpairing_data;

typedef struct
{
  uint8_t sign[3];
  uint8_t obdii_mac[6];
  uint8_t sn[16];
  uint16_t periodic_timer;//periodic upload timer unit:s
  uint8_t last_statemachine;
  uint8_t mqtt_url[64];
  uint16_t mqtt_port;
  uint8_t mqtt_authority;
  uint8_t mqtt_user[32];
  uint8_t mqtt_passwd[32];
  uint8_t door_mode;
} BJJM_LM_flash_data;


extern void BJJA_LM_Sub1G_init();
extern void BJJA_LM_early_send_cmd();
extern void BJJA_LM_usSleep(uint16_t value);
extern void BJJA_LM_tick_wdt();
extern SubGpairing_data gSubGpairing_data[8];
#endif
