#include <stdio.h>
#include "ti/drivers/uart/UARTCC26XX.h"
//#include "Board.h"
#include "ti_drivers_config.h"
#include <string.h>
#include <icall.h>

#define UART_BUF_LEN2    128
#define Serial_BUF_LEN2  128
//#define SEND_BUF_LEN    70
void bjja_lm_uart_config2();
void UARTRecvCallback2 (UART_Handle handle, void *buf, size_t count);
void UartMessage2(uint8_t *str, int size);

extern uint8_t mcu_uart_rx_buffer2[UART_BUF_LEN2];
extern uint8_t serialBuffer2[Serial_BUF_LEN2];
extern uint16_t gSerialLen2;
extern void Board_initUser2(void);
extern void Board_DeinitUart2(void);
extern UART_Handle Uarthandle2;
void add_queue2(uint16_t len);
uint8_t get_queue2();