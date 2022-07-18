#include <ti/drivers/Power.h>
#include "bjja_lm_uart.h"
#include <ti/drivers/pin/PINCC26XX.h>
#include "multi_role.h"

//#define UART_BUF_LEN 250
UART_Handle Uarthandle;

uint8_t gBaudrate=9;
uint8_t mcu_uart_rx_buffer[UART_BUF_LEN];
uint8_t serialBuffer[Serial_BUF_LEN];
struct _DATA_BUF
{
  uint8_t data[Serial_BUF_LEN];
  uint16_t data_len;
}Node[10];
uint8_t gCurrentHeader=0x00;
uint8_t gCurrentTail=0x00;
uint16_t gSerialLen = 0x00;
//uint16_t serialBufferOffset,buffer_tail;
static uint8_t gUart_flag = 0x00;
void Board_initUser(void)
{
    if (gUart_flag == 0x00)
    {
        bjja_lm_uart_config();
        gUart_flag = 0x01;
    }
}
void Board_DeinitUart(void)
{
    if (gUart_flag == 0x01)
    {
        gUart_flag = 0x00;
    }
    else
    {
        return;
    }
    UART_Params uartParams;

    UART_close(Uarthandle);

    /* Call driver init functions */
    UART_init();

    /* Set UART to default parameters. */
    UART_Params_init(&uartParams);

    Uarthandle = UART_open(CONFIG_DISPLAY_UART, &uartParams);

    if (Uarthandle == NULL)
    {
        /* UART_open() failed */
        while (1)
            ;
    }

    UART_close(Uarthandle);
}
uint32_t BJJA_index_to_baudrate(uint8_t index)
{
  uint32_t ret=0;
  switch(index)
  {
    case 0:
      ret = 2400;
      break;
    case 1:
      ret = 4800;
      break;
    case 2:
      ret = 9600;
      break;
    case 3:
      ret = 14400;
      break;
    case 4:
      ret = 19200;
      break;
    case 5:
      ret = 38400;
      break;
    case 6:
      ret = 43000;
      break;
    case 7:
      ret = 57600;
      break;
    case 8:
      ret = 76800;
      break;
    case 9:
      ret = 115200;
      break;
    case 10:
      ret = 128000;
      break;
    case 11:
      ret = 230400;
      break;
    case 12:
      ret = 256000;
      break;
    case 13:
      ret = 460800;
      break;
    case 14:
      ret = 921600;
      break;
    case 15:
      ret = 1382400;
      break;
  }
  return ret;
}
void bjja_lm_uart_config()
{
    UART_Params uartParams;
    //char buff[10];

    // Initialize the UART driver.
    UART_init();

    // Create a UART with data processing off.
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_BINARY;
    uartParams.readDataMode = UART_DATA_BINARY;
    uartParams.readReturnMode = UART_RETURN_FULL;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.dataLength = UART_LEN_8;
    uartParams.stopBits = UART_STOP_ONE;
    uartParams.readCallback = UARTRecvCallback;
    uartParams.readMode = UART_MODE_CALLBACK;
    uartParams.readDataMode = UART_DATA_TEXT;
    uartParams.readReturnMode = UART_RETURN_NEWLINE;

    uartParams.baudRate = 115200;
    //uartParams.baudRate = BJJA_index_to_baudrate(gBaudrate);

    // Open an instance of the UART drivers
    Uarthandle = UART_open(CONFIG_DISPLAY_UART, &uartParams);
    if (Uarthandle == NULL)
    {
        // UART_open() failed
        while (1)
            ;
    }

    UART_control(Uarthandle, UARTCC26XX_CMD_RETURN_PARTIAL_ENABLE, NULL);
    UART_read(Uarthandle, mcu_uart_rx_buffer, UART_BUF_LEN - 1);
    //serialBufferOffset = 0;
    //buffer_tail = 0;
}

void UARTRecvCallback(UART_Handle handle, void *buf, size_t count)
{
    if (count > 0)
    {
        /*for (uint16_t i = 0; i < count; i++)
        {
            //copy one byte to data buffer
            //serialBuffer[serialBufferOffset] = mcu_uart_rx_buffer[i];
            //update offset
            //serialBufferOffset = circular_add(serialBufferOffset,1);
            serialBuffer[i] = mcu_uart_rx_buffer[i];
            //serialBufferOffset = i+1;
            //UartMessage("1",1);
        }
        gSerialLen = count;
        //SimpleObserver_UartChangeCB();
        Weli_UartChangeCB();
        //UartMessage(serialBuffer,count);
		*/
		add_queue(count);
		Weli_UartChangeCB();
    }

    UART_read(Uarthandle, mcu_uart_rx_buffer, UART_BUF_LEN - 1);
}

/*void UARTRecvCallback (UART_Handle handle, void *buf, size_t count)
 {
 if(count>0)
 {
 for (uint8 i = 0; i < count; i++)
 {
 //copy one byte to data buffer
 serialBuffer[serialBufferOffset] = mcu_uart_rx_buffer[i];
 //update offset
 serialBufferOffset = circular_add(serialBufferOffset,1);
 }
 Command_deal(mcu_uart_rx_buffer);
 }
 UART_read(Uarthandle, mcu_uart_rx_buffer, UART_BUF_LEN-1);
 }*/

void UartMessage(uint8_t *str, int size)
{
    //UART_write(Uarthandle, "out:", 4);
    UART_write(Uarthandle, str, size);
    //UART_write(Uarthandle, "\r\n", 2);
}
void add_queue(uint16_t len)
{
  Node[gCurrentHeader].data_len = len;
  uint16_t i=0;
  for(i=0;i<len;i++)
  {
    Node[gCurrentHeader].data[i] = mcu_uart_rx_buffer[i];
  }
  gCurrentHeader++;
  if(gCurrentHeader>=10)
  {
    gCurrentHeader=0;
  }
}
uint8_t get_queue()
{
  if(gCurrentHeader == gCurrentTail)
  {
    return 1;
  }
  gSerialLen = Node[gCurrentTail].data_len;
  //VOID memset(serialBuffer, 0, sizeof(serialBuffer));
  uint16_t i=0;
  for(i=0;i<gSerialLen;i++)
  {
    serialBuffer[i] = Node[gCurrentTail].data[i];
  }
  gCurrentTail++;
  if(gCurrentTail>=10)
  {
    gCurrentTail=0;
  }
  return 0;
}

