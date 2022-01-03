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

static PIN_Handle RATPinHandle;
static PIN_State RATPinState;

static uint8_t eventtype;
static uint32_t txTimestamp;

/********************************************************************************
 *  EXTERNAL VARIABLES
 */
static void RFRAT_Config();

/********************************************************************************
 *  Callback
 */
static void TrigerHandle(uint_least8_t index){

    // Set a time in the near future (2ms)
    txTimestamp = RF_getCurrentTime() + RF_convertMsToRatTicks(10);
    // 编码事件
    eventtype = 0x01;
    // 射频发送
    RFRAT_Config();


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
    RF_cmdPropTx.pPkt = &eventtype;

}


/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{

    /* Call driver init functions */
    GPIO_init();
    Display_init();

    /* Initialize display */
    display = Display_open(Display_Type_UART,NULL); //TODO display输出有bug
    if (display == NULL) {
        /* UART_open() failed */
        while (1);
    }

    /* Initialize semaphore */
    sem_init(&EventSend, 0, 0);

    /* Initialize RF Core */
    RF_Config();

    Display_printf(display, 0, 0, "\r TrigerBox cc1310 ready!\r\n");

    #if (Triger == 1)
    /* Register interrupt for the Board_GPIO_TRIGER1_IN (trigger) */
    GPIO_setCallback(Board_GPIO_TRIGER1_IN, TrigerHandle);
    GPIO_enableInt(Board_GPIO_TRIGER1_IN);
    #endif

    /* led on to indicate the system is ready! */
    GPIO_write(Board_GPIO_LED_BLUE,CC1310_LAUNCHXL_PIN_LED_ON);

    while (1) {

        /* Send packet */
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
                        while(1);
                }

                GPIO_toggle(Board_GPIO_LED_BLUE);
    }
}
