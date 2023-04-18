#include <ti/drivers/Power.h>
#include "bjja_lm_uart2.h"
#include <ti/drivers/pin/PINCC26XX.h>
#include "multi_role.h"

//#define UART_BUF_LEN 250
UART_Handle Uarthandle2;

uint8_t mcu_uart_rx_buffer2[UART_BUF_LEN2];
uint8_t serialBuffer2[Serial_BUF_LEN2];
struct _DATA_BUF
{
  uint8_t data[Serial_BUF_LEN2];
  uint16_t data_len;
}Node2[10];
uint8_t gCurrentHeader2=0x00;
uint8_t gCurrentTail2=0x00;
uint16_t gSerialLen2 = 0x00;
//uint16_t serialBufferOffset,buffer_tail;
static uint8_t gUart_flag2 = 0x00;
void Board_initUser2(void)
{
    if (gUart_flag2 == 0x00)
    {
        bjja_lm_uart_config2();
        gUart_flag2 = 0x01;
    }
}
void Board_DeinitUart2(void)
{
    if (gUart_flag2 == 0x01)
    {
        gUart_flag2 = 0x00;
    }
    else
    {
        return;
    }
    UART_Params uartParams2;

    UART_close(Uarthandle2);

    /* Call driver init functions */
    UART_init();

    /* Set UART to default parameters. */
    UART_Params_init(&uartParams2);

    Uarthandle2 = UART_open(CONFIG_UART_0, &uartParams2);

    if (Uarthandle2 == NULL)
    {
        /* UART_open() failed */
        while (1)
            ;
    }

    UART_close(Uarthandle2);
}

void bjja_lm_uart_config2()
{
    UART_Params uartParams2;
    //char buff[10];

    // Initialize the UART driver.
    UART_init();

    // Create a UART with data processing off.
    UART_Params_init(&uartParams2);
    uartParams2.writeDataMode = UART_DATA_BINARY;
    uartParams2.readDataMode = UART_DATA_BINARY;
    uartParams2.readReturnMode = UART_RETURN_FULL;
    uartParams2.readEcho = UART_ECHO_OFF;
    uartParams2.dataLength = UART_LEN_8;
    uartParams2.stopBits = UART_STOP_ONE;
    uartParams2.readCallback = UARTRecvCallback2;
    uartParams2.readMode = UART_MODE_CALLBACK;
    uartParams2.readDataMode = UART_DATA_TEXT;
    uartParams2.readReturnMode = UART_RETURN_NEWLINE;

    uartParams2.baudRate = 9600;
    // Open an instance of the UART drivers
    Uarthandle2 = UART_open(CONFIG_UART_0, &uartParams2);
    if (Uarthandle2 == NULL)
    {
        // UART_open() failed
        while (1)
            ;
    }

    UART_control(Uarthandle2, UARTCC26XX_CMD_RETURN_PARTIAL_ENABLE, NULL);
    UART_read(Uarthandle2, mcu_uart_rx_buffer2, UART_BUF_LEN2 - 1);
}

void UARTRecvCallback2(UART_Handle handle, void *buf, size_t count)
{
    if (count > 0)
    {
		  add_queue2(count);
		  Weli_UartChangeCB2();
    }

    UART_read(Uarthandle2, mcu_uart_rx_buffer2, UART_BUF_LEN2 - 1);
}

void UartMessage2(uint8_t *str, int size)
{
  UART_write(Uarthandle2, str, size);
}
void add_queue2(uint16_t len)
{
  Node2[gCurrentHeader2].data_len = len;
  uint16_t i=0;
  for(i=0;i<len;i++)
  {
    Node2[gCurrentHeader2].data[i] = mcu_uart_rx_buffer2[i];
  }
  gCurrentHeader2++;
  if(gCurrentHeader2>=10)
  {
    gCurrentHeader2=0;
  }
}
uint8_t get_queue2()
{
  if(gCurrentHeader2 == gCurrentTail2)
  {
    return 1;
  }
  gSerialLen2 = Node2[gCurrentTail2].data_len;
  //VOID memset(serialBuffer, 0, sizeof(serialBuffer));
  uint16_t i=0;
  for(i=0;i<gSerialLen2;i++)
  {
    serialBuffer2[i] = Node2[gCurrentTail2].data[i];
  }
  gCurrentTail2++;
  if(gCurrentTail2>=10)
  {
    gCurrentTail2=0;
  }
  return 0;
}

