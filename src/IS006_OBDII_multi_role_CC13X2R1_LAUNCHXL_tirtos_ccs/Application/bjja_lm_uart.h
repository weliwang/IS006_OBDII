#include <stdio.h>
#include "ti/drivers/uart/UARTCC26XX.h"
//#include "Board.h"
#include "ti_drivers_config.h"
#include <string.h>
#include <icall.h>

#define UART_BUF_LEN    64
#define Serial_BUF_LEN  64
//#define SEND_BUF_LEN    70
void bjja_lm_uart_config();
void UARTRecvCallback (UART_Handle handle, void *buf, size_t count);
void UartMessage(uint8_t *str, int size);
//uint16 circular_add(uint16 x, uint16 y);
uint16 circular_diff(uint16 offset, uint16 tail);

extern uint8_t mcu_uart_rx_buffer[UART_BUF_LEN];
extern uint8_t serialBuffer[Serial_BUF_LEN];
//extern uint8_t Send_buffer[SEND_BUF_LEN];
extern uint16_t serialBufferOffset,buffer_tail;
extern uint16_t gSerialLen;
extern void Command_deal(unsigned char *rx_buffer);
extern void SimpleObserver_UartChangeCB(uint8 keys);
extern void Board_initUser(void);
extern void Board_DeinitUart(void);
extern uint8_t gBaudrate;
extern UART_Handle Uarthandle;
void add_queue(uint16_t len);
uint8_t get_queue();
extern void PRINT_DATA(char *ptr, ...);