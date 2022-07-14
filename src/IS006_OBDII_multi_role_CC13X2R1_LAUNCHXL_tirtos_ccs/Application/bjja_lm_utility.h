#ifndef BJJA_LM_UTILITY_H
#define BJJA_LM_UTILITY_H

#include "bjja_lm_subg_radio_config.h"
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#define DELAY_US(i)      (Task_sleep(((i) * 1) / Clock_tickPeriod))
extern void BJJA_LM_Sub1G_init();
extern void BJJA_LM_early_send_cmd();
extern void BJJA_LM_usSleep(uint16_t value);
#endif
