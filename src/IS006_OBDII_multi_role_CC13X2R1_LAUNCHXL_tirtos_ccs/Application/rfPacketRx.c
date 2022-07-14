/*
 * Copyright (c) 2019, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/***** Includes *****/
/* Standard C Libraries */
#include <stdlib.h>

/* TI Drivers */
#include <ti/drivers/rf/RF.h>
#include <ti/drivers/PIN.h>

/* Driverlib Header files */
#include DeviceFamily_constructPath(driverlib/rf_prop_mailbox.h)

/* Board Header files */
#include "ti_drivers_config.h"

/* Application Header files */
#include "RFQueue.h"
#include <bjja_lm_utility.h>

/***** Defines *****/

/* Packet RX Configuration */
#define DATA_ENTRY_HEADER_SIZE 8  /* Constant header size of a Generic Data Entry */
#define MAX_LENGTH             10 /* Max length byte the radio will accept */
#define NUM_DATA_ENTRIES       2  /* NOTE: Only two data entries supported at the moment */
#define NUM_APPENDED_BYTES     2  /* The Data Entries data field will contain:
                                   * 1 Header byte (RF_cmdPropRx.rxConf.bIncludeHdr = 0x1)
                                   * Max 30 payload bytes
                                   * 1 status byte (RF_cmdPropRx.rxConf.bAppendStatus = 0x1) */


/********add by weli begin**************/

uint8_t gAck_SubG=0x00;
uint8_t gPacket[MAX_LENGTH];
void BJJA_LM_entryTX();
void BJJA_LM_entryRX();
void BJJA_LM_early_send_cmd();
uint8_t BJJA_LM_late_send_cmd(uint8_t freq);
void BJJA_LM_Sub1G_init();
static RF_CmdHandle rf_cmdhandle;
/********add by weli begin**************/
/***** Prototypes *****/
static void callback(RF_Handle h, RF_CmdHandle ch, RF_EventMask e);

/***** Variable declarations *****/
static RF_Object rfObject;
static RF_Handle rfHandle;

/* Pin driver handle */
static PIN_Handle ledPinHandle;
static PIN_State ledPinState;

/* Buffer which contains all Data Entries for receiving data.
 * Pragmas are needed to make sure this buffer is 4 byte aligned (requirement from the RF Core) */
#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_ALIGN (rxDataEntryBuffer, 4);
static uint8_t
rxDataEntryBuffer[RF_QUEUE_DATA_ENTRY_BUFFER_SIZE(NUM_DATA_ENTRIES,
                                                  MAX_LENGTH,
                                                  NUM_APPENDED_BYTES)];
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma data_alignment = 4
static uint8_t
rxDataEntryBuffer[RF_QUEUE_DATA_ENTRY_BUFFER_SIZE(NUM_DATA_ENTRIES,
                                                  MAX_LENGTH,
                                                  NUM_APPENDED_BYTES)];
#elif defined(__GNUC__)
static uint8_t
rxDataEntryBuffer[RF_QUEUE_DATA_ENTRY_BUFFER_SIZE(NUM_DATA_ENTRIES,
                                                  MAX_LENGTH,
                                                  NUM_APPENDED_BYTES)]
                                                  __attribute__((aligned(4)));
#else
#error This compiler is not supported.
#endif

/* Receive dataQueue for RF Core to fill in data */
static dataQueue_t dataQueue;
static rfc_dataEntryGeneral_t* currentDataEntry;
static uint8_t packetLength;
static uint8_t* packetDataPointer;


static uint8_t packet[MAX_LENGTH + NUM_APPENDED_BYTES - 1]; /* The length byte is stored in a separate variable */

/*
 * Application LED pin configuration table:
 *   - All LEDs board LEDs are off.
 */

/***** Function definitions *****/

void BJJA_LM_Sub1G_init()
{
    RF_Params rfParams;
    RF_Params_init(&rfParams);



    if( RFQueue_defineQueue(&dataQueue,
                            rxDataEntryBuffer,
                            sizeof(rxDataEntryBuffer),
                            NUM_DATA_ENTRIES,
                            MAX_LENGTH + NUM_APPENDED_BYTES))
    {
        /* Failed to allocate space for all data entries */
        while(1);
    }

    /* Modify CMD_PROP_RX command for application needs */
    /* Set the Data Entity queue for received data */
    RF_cmdPropRx.pQueue = &dataQueue;
    /* Discard ignored packets from Rx queue */
    RF_cmdPropRx.rxConf.bAutoFlushIgnored = 1;
    /* Discard packets with CRC error from Rx queue */
    RF_cmdPropRx.rxConf.bAutoFlushCrcErr = 1;
    /* Implement packet length filtering to avoid PROP_ERROR_RXBUF */
    RF_cmdPropRx.maxPktLen = MAX_LENGTH;
    //RF_cmdPropRx.pktConf.bRepeatOk = 1;
    //RF_cmdPropRx.pktConf.bRepeatNok = 1;


    /************************add by weli tx*******************************/
    RF_cmdPropTx.pktLen = MAX_LENGTH;
    RF_cmdPropTx.pPkt = gPacket;
    RF_cmdPropTx.startTrigger.triggerType = TRIG_NOW;
    RF_cmdPropRadioDivSetup.txPower=0x04C0;//-20dbm
    /************************add by weli tx*******************************/

    /* Request access to the radio */
#if defined(DeviceFamily_CC26X0R2)
    rfHandle = RF_open(&rfObject, &RF_prop, (RF_RadioSetup*)&RF_cmdPropRadioSetup, &rfParams);
#else
    rfHandle = RF_open(&rfObject, &RF_prop, (RF_RadioSetup*)&RF_cmdPropRadioDivSetup, &rfParams);
#endif// DeviceFamily_CC26X0R2

    

    /*while(1)
    {
        BJJA_LM_early_send_cmd();
        DELAY_US(5);
    }*/
    
    
}
uint32_t old_time=0x00;
void callback(RF_Handle h, RF_CmdHandle ch, RF_EventMask e)
{
    if (e & RF_EventRxEntryDone)
    {
        /* Toggle pin to indicate RX */
        //PIN_setOutputValue(ledPinHandle, CONFIG_PIN_RLED,
        //                   !PIN_getOutputValue(CONFIG_PIN_RLED));
    

        /* Get current unhandled data entry */
        currentDataEntry = RFQueue_getDataEntry();
#if 1
        /* Handle the packet data, located at &currentDataEntry->data:
         * - Length is the first byte with the current configuration
         * - Data starts from the second byte */
        packetLength      = *(uint8_t*)(&currentDataEntry->data);
        packetDataPointer = (uint8_t*)(&currentDataEntry->data + 1);

        /* Copy the payload + the status byte to the packet variable */
        //weli because packetLength is data of first byte in old datastructure
        packet[0]= packetLength;
        memcpy(packet+1, packetDataPointer, /*(packetLength + 1)*/10);

#endif
        //uint32_t new_time =
        RFQueue_nextEntry();
        
        if(packet[0]==0xaa && packet[1]==0xfd)
        {
            gAck_SubG=1;
            //if(new_time-old_time>=4000*50)
                ;//PIN_setOutputValue(ledPinHandle, CONFIG_PIN_RLED,!PIN_getOutputValue(CONFIG_PIN_RLED));
        }
        //old_time = new_time;
        //RF_yield(rfHandle);
        //RF_cancelCmd(h,ch,1);

    }
}
void BJJA_LM_early_send_cmd()
{
    
    uint8_t i=0;
    uint8_t ret=0;
    for(i=0;i<15;i++)//S1
    {
        if(BJJA_LM_late_send_cmd(0))//868.34
            break;
        if(BJJA_LM_late_send_cmd(1))//868.30
            break;
        if(BJJA_LM_late_send_cmd(2))//868.26
            break;
    }
    /*for(i=0;i<15;i++)//S2
    {
        if(BJJA_LM_late_send_cmd(0))//868.26
            break;
        if(BJJA_LM_late_send_cmd(1))//868.30
            break;
        if(BJJA_LM_late_send_cmd(2))//868.34
            break;
    }
    for(i=0;i<15;i++)//S3
    {
        if(BJJA_LM_late_send_cmd(0))//868.26
            break;
        if(BJJA_LM_late_send_cmd(1))//868.30
            break;
        if(BJJA_LM_late_send_cmd(2))//868.34
            break;
    }*/
}

uint8_t BJJA_LM_late_send_cmd(uint8_t freq)
{
    uint8_t ret=0x00;
    if(freq==0)
    {
        RF_cmdFs.fractFreq = 0x5700;//868.34
    }
    else if(freq==1)
    {
        RF_cmdFs.fractFreq = 0x4CCD;//868.30
    }
    else
    {
        RF_cmdFs.fractFreq = 0x429A;//868.26
    }
    /* Set the frequency */
    RF_postCmd(rfHandle, (RF_Op*)&RF_cmdFs, RF_PriorityNormal, NULL, 0);
    BJJA_LM_entryTX();
    //DELAY_US(3000);
    BJJA_LM_entryRX();
    DELAY_US(3000);
    if(gAck_SubG)
    {
        gAck_SubG=0;
        ret=1;
    }
    return ret;
    
}
void BJJA_LM_entryTX()
{
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
    RF_EventMask terminationReason = RF_runCmd(rfHandle, (RF_Op*)&RF_cmdPropTx,
                                                   RF_PriorityNormal, NULL, 0);

        switch(terminationReason)
        {
            case RF_EventLastCmdDone:
                // A stand-alone radio operation command or the last radio
                // operation command in a chain finished.
                break;
            case RF_EventCmdCancelled:
                // Command cancelled before it was started; it can be caused
            // by RF_cancelCmd() or RF_flushCmd().
                break;
            case RF_EventCmdAborted:
                // Abrupt command termination caused by RF_cancelCmd() or
                // RF_flushCmd().
                break;
            case RF_EventCmdStopped:
                // Graceful command termination caused by RF_cancelCmd() or
                // RF_flushCmd().
                break;
            default:
                // Uncaught error event
                while(1);
        }

        uint32_t cmdStatus = ((volatile RF_Op*)&RF_cmdPropTx)->status;
        switch(cmdStatus)
        {
            case PROP_DONE_OK:
                // Packet transmitted successfully
                break;
            case PROP_DONE_STOPPED:
                // received CMD_STOP while transmitting packet and finished
                // transmitting packet
                break;
            case PROP_DONE_ABORT:
                // Received CMD_ABORT while transmitting packet
                break;
            case PROP_ERROR_PAR:
                // Observed illegal parameter
                break;
            case PROP_ERROR_NO_SETUP:
                // Command sent without setting up the radio in a supported
                // mode using CMD_PROP_RADIO_SETUP or CMD_RADIO_SETUP
                break;
            case PROP_ERROR_NO_FS:
                // Command sent without the synthesizer being programmed
                break;
            case PROP_ERROR_TXUNF:
                // TX underflow observed during operation
                break;
            default:
                // Uncaught error event - these could come from the
                // pool of states defined in rf_mailbox.h
                //while(1);
        }

        //PIN_setOutputValue(ledPinHandle, CONFIG_PIN_0,!PIN_getOutputValue(CONFIG_PIN_0));

        /* Power down the radio */
        //RF_yield(rfHandle);
}
void BJJA_LM_entryRX()
{
    RF_cmdPropRx.startTrigger.triggerType = TRIG_NOW;
    //RF_cmdPropRx.endTrigger.triggerType = TRIG_ABSTIME;
    //RF_cmdPropRx.endTime= RF_getCurrentTime() + (4000*100);//weli 4000 is 1ms
    RF_cmdPropRx.endTrigger.triggerType = TRIG_REL_SUBMIT;
    RF_cmdPropRx.endTime = (uint32_t)(4000000*0.5f);//100ms *0.1f=100ms
    /* Enter RX mode and stay forever in RX */
    RF_EventMask terminationReason = RF_runCmd(rfHandle, (RF_Op*)&RF_cmdPropRx,
                                               RF_PriorityNormal, &callback,
                                               RF_EventRxEntryDone);
    RF_yield(rfHandle);
    switch(terminationReason)
    {
        case RF_EventLastCmdDone:
            // A stand-alone radio operation command or the last radio
            // operation command in a chain finished.
            break;
        case RF_EventCmdCancelled:
            // Command cancelled before it was started; it can be caused
            // by RF_cancelCmd() or RF_flushCmd().
            break;
        case RF_EventCmdAborted:
            // Abrupt command termination caused by RF_cancelCmd() or
            // RF_flushCmd().
            break;
        case RF_EventCmdStopped:
            // Graceful command termination caused by RF_cancelCmd() or
            // RF_flushCmd().
            break;
        default:
            // Uncaught error event
            //while(1);
    }

    uint32_t cmdStatus = ((volatile RF_Op*)&RF_cmdPropRx)->status;
    switch(cmdStatus)
    {
        case PROP_DONE_OK:
            // Packet received with CRC OK
            break;
        case PROP_DONE_RXERR:
            // Packet received with CRC error
            break;
        case PROP_DONE_RXTIMEOUT:
            // Observed end trigger while in sync search
            break;
        case PROP_DONE_BREAK:
            // Observed end trigger while receiving packet when the command is
            // configured with endType set to 1
            break;
        case PROP_DONE_ENDED:
            // Received packet after having observed the end trigger; if the
            // command is configured with endType set to 0, the end trigger
            // will not terminate an ongoing reception
            break;
        case PROP_DONE_STOPPED:
            // received CMD_STOP after command started and, if sync found,
            // packet is received
            break;
        case PROP_DONE_ABORT:
            // Received CMD_ABORT after command started
            break;
        case PROP_ERROR_RXBUF:
            // No RX buffer large enough for the received data available at
            // the start of a packet
            break;
        case PROP_ERROR_RXFULL:
            // Out of RX buffer space during reception in a partial read
            break;
        case PROP_ERROR_PAR:
            // Observed illegal parameter
            break;
        case PROP_ERROR_NO_SETUP:
            // Command sent without setting up the radio in a supported
            // mode using CMD_PROP_RADIO_SETUP or CMD_RADIO_SETUP
            break;
        case PROP_ERROR_NO_FS:
            // Command sent without the synthesizer being programmed
            break;
        case PROP_ERROR_RXOVF:
            // RX overflow observed during operation
            break;
        default:
            // Uncaught error event - these could come from the
            // pool of states defined in rf_mailbox.h
            //while(1);
        
    }

    /* Power down the radio */
    //RF_yield(rfHandle);
}
