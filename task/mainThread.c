/*
 * Copyright (c) 2015-2019, Texas Instruments Incorporated
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

/********************************************************************************
 *  INCLUDES
 */
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

/* POSIX Header files */
#include <pthread.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART.h>
#include <ti/drivers/rf/RF.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/PIN.h>

#include <ti/display/Display.h>
#include <ti/display/DisplayUart.h>

/* Example/Board Header files */
#include "Board.h"

/* POSIX Header files */
#include <semaphore.h>

/* Driverlib Header files */
#include DeviceFamily_constructPath(driverlib/rf_prop_mailbox.h)

/* Application Header files */
#include "smartrf_settings/smartrf_settings.h"


/********************************************************************************
 *  Macros
 */

/* Packet TX Configuration */
#define PAYLOAD_LENGTH      1

#define MAX_NUM_RX_BYTES    1000   // Maximum RX bytes to receive in one go
#define MAX_NUM_TX_BYTES    1000

#define PACKET_INTERVAL     500000  /* Set packet interval to 500000us or 500ms */


/********************************************************************************
 *  GLOBAL VARIABLES
 */
sem_t EventSend;
Display_Handle display = NULL;

/********************************************************************************
 *  LOCAL VARIABLES
 */
static RF_Object rfObject;
static RF_Handle rfHandle;

//static PIN_Handle RATPinHandle;
//static PIN_State RATPinState;

uint8_t wantedRxBytes;            // Number of bytes received so far
uint8_t rxBuf[MAX_NUM_RX_BYTES];   // Receive buffer
uint8_t txBuf[MAX_NUM_TX_BYTES];    // Transmit buffer

uint8_t eventtype;
uint32_t txTimestamp;

/********************************************************************************
 *  EXTERNAL VARIABLES
 */

/********************************************************************************
 *  Callback
 */
//static void TrigerHandle(uint_least8_t index){
//
//    // 读取RAT当前值，指定5ms之后发送
//    txTimestamp = RF_getCurrentTime() + RF_convertMsToRatTicks(5);
//    // 编码事件
//    eventtype = 0x01;
//    // 发送射频发送命令
//    RFRAT_Config();
//
//    sem_post(&EventSend);
//
//}

static void eventSave (UART_Handle handle, void *rxBuf, size_t size)
{
    /* 读取RAT当前值，指定5ms之后发送  */
    txTimestamp = RF_getCurrentTime() + RF_convertMsToRatTicks(5);
    /* Make sure we received all expected bytes这里注释掉了，所以通过串口接收到的数据没有经过校验，其实是有风险的 */
    if (size == wantedRxBytes) {
    /* Copy bytes from RX buffer to TX buffer */
    size_t i;
    for( i= 0; i <= size; i++){
        txBuf[i] = ((uint8_t*)rxBuf)[i];
    }
    UART_write(handle, txBuf, 1);
    GPIO_toggle(Board_GPIO_LED_BLUE);
    sem_post(&EventSend);
    }
    else {
        while(1);
        // Handle error or call to UART_readCancel()
    }
}

/********************************************************************************
 *  LOCAL FUNCTIONS
 */
static void RF_Config(){
    RF_Params rfParams;
    RF_Params_init(&rfParams);

    rfHandle = RF_open(&rfObject, &RF_prop, (RF_RadioSetup*)&RF_cmdPropRadioDivSetup, &rfParams);

    /* Set the frequency */
    RF_postCmd(rfHandle, (RF_Op*)&RF_cmdFs, RF_PriorityNormal, NULL, 0);
}

static void RFRAT_Config(){

    RF_cmdPropTx.startTrigger.triggerType = TRIG_ABSTIME;
    RF_cmdPropTx.startTime = txTimestamp;
    RF_cmdPropTx.pktLen = PAYLOAD_LENGTH;
    RF_cmdPropTx.pPkt = txBuf;

}

static UART_Handle Uart_open()
{
    UART_Handle uart;
    UART_Params params;
    UART_init();
    /* Create a UART with data processing off. */
    UART_Params_init(&params);
    params.baudRate = 115200;
    params.readMode = UART_MODE_CALLBACK;//UART_MODE_CALLBACK||UART_MODE_BLOCKING
    params.readDataMode = UART_DATA_BINARY;
    params.readCallback = eventSave;
//    params.writeMode = UART_MODE_CALLBACK;
    params.writeDataMode = UART_DATA_BINARY;
//    params.writeCallback = writeCallback;

    uart = UART_open(Board_UART0, &params);

    if (uart == NULL) {
        /* UART_open() failed */
        while (1);
    }
    return uart;
}


/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{

    UART_Handle handle;
    /* Call driver init functions */
    GPIO_init();
//    Display_init();
//
//    /* Initialize display */
//    display = Display_open(Display_Type_HOST,NULL); //TODO display输出有bug
//    if (display == NULL) {
//        /* UART_open() failed */
//        while (1);
//    }

    handle = Uart_open();
    /* Initialize semaphore */
    sem_init(&EventSend, 0, 0);
    /* Initialize RF Core */
    RF_Config();
    RFRAT_Config();//TODO:这里配置了射频传输的内容，传输时间并不是通过地址幅值的，当传输时间改变时是否会改变这里的值
//    Display_printf(display, 0, 0, "\r TrigerBox cc1310 ready!\r\n");

    wantedRxBytes = 1;
//    /* 8-3编码器触发标签 */
//    #if (Triger == 1)
//    /* Register interrupt for the Board_GPIO_TRIGER1_IN (trigger) */
//    GPIO_setCallback(Board_GPIO_TRIGER1_IN, TrigerHandle);
//    GPIO_enableInt(Board_GPIO_TRIGER1_IN);
//    #endif

    /* led on to indicate the system is ready! */
    GPIO_write(Board_GPIO_LED_BLUE,CC1310_LAUNCHXL_PIN_LED_ON);

    while (1) {
        UART_read(handle, rxBuf, 1);
//        UART_write(handle, rxBuf, 1); /*这里时怀疑串口以回调模式读取数据会影响射频的发送，故改成了串口阻塞模式*/
//        GPIO_toggle(Board_GPIO_LED_BLUE);
        sem_wait(&EventSend);
        /* 发送射频发送命令 */
//        /* Create packet with incrementing sequence number and random payload */
//        packet[0] = (uint8_t)(seqNumber >> 8);
//        packet[1] = (uint8_t)(seqNumber++);
//        uint8_t i;
//        for (i = 2; i < PAYLOAD_LENGTH; i++)
//        {
//            packet[i] = rand();
//        }

//        /* Send packet */
//        RF_EventMask terminationReason = RF_runCmd(rfHandle, (RF_Op*)&RF_cmdPropTx,
//                                                   RF_PriorityNormal, NULL, 0);
//
//        switch(terminationReason)
//        {
//            case RF_EventLastCmdDone:
//                // A stand-alone radio operation command or the last radio
//                // operation command in a chain finished.
//                break;
//            case RF_EventCmdCancelled:
//                // Command cancelled before it was started; it can be caused
//            // by RF_cancelCmd() or RF_flushCmd().
//                break;
//            case RF_EventCmdAborted:
//                // Abrupt command termination caused by RF_cancelCmd() or
//                // RF_flushCmd().
//                break;
//            case RF_EventCmdStopped:
//                // Graceful command termination caused by RF_cancelCmd() or
//                // RF_flushCmd().
//                break;
//            default:
//                // Uncaught error event
//                while(1);
//        }
//
//        uint32_t cmdStatus = ((volatile RF_Op*)&RF_cmdPropTx)->status;
//        switch(cmdStatus)
//        {
//            case PROP_DONE_OK:
//                // Packet transmitted successfully
//                break;
//            case PROP_DONE_STOPPED:
//                // received CMD_STOP while transmitting packet and finished
//                // transmitting packet
//                break;
//            case PROP_DONE_ABORT:
//                // Received CMD_ABORT while transmitting packet
//                break;
//            case PROP_ERROR_PAR:
//                // Observed illegal parameter
//                break;
//            case PROP_ERROR_NO_SETUP:
//                // Command sent without setting up the radio in a supported
//                // mode using CMD_PROP_RADIO_SETUP or CMD_RADIO_SETUP
//                break;
//            case PROP_ERROR_NO_FS:
//                // Command sent without the synthesizer being programmed
//                break;
//            case PROP_ERROR_TXUNF:
//                // TX underflow observed during operation
//                break;
//            default:
//                // Uncaught error event - these could come from the
//                // pool of states defined in rf_mailbox.h
//                while(1);
//        }
//    /* Power down the radio */
//    RF_yield(rfHandle);
//
//    #ifdef POWER_MEASUREMENT
//    /* Sleep for PACKET_INTERVAL s */
//    sleep(PACKET_INTERVAL);
//    #else
//    /* Sleep for PACKET_INTERVAL us */
//    usleep(PACKET_INTERVAL);
//    #endif
    }
}
