
/** \addtogroup hal */
/** @{*/
/* mbed Microcontroller Library
 * Copyright (c) 2015 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <sys/platform.h>
#include "lp_ticker_api.h"
#include <drivers/rtc/adi_rtc.h>
#include <drivers/pwr/adi_pwr.h>

#ifdef DEVICE_LOWPOWERTIMER

// define base LF clock frequncy in Hz
#define LFCLK_FREQUENCY_HZ		32768
// RTC prescaler for the LF clock
#define RTC_PRESCALER         	0

/* time for each tick of the LF clock in us */
#define TIME_US_PER_TICK 	((float)1000000/(float)(LFCLK_FREQUENCY_HZ>>RTC_PRESCALER))

// The number of clock ticks it takes to set & enable the alarm
#define TICKS_TO_ENABLE_ALARM 50

static unsigned char rtc1_memory[ADI_RTC_MEMORY_SIZE];
static ADI_RTC_HANDLE hRTC1_Device;


/**
 * \defgroup hal_LpTicker Low Power Ticker Functions
 * @{
 */
/**
 * Local RTC 1 ISR callback function.
 *
 */
void rtc1_Callback (void *pCBParam, uint32_t nEvent, void *EventArg)
{
    /* process RTC interrupts (cleared by driver) */
    if (ADI_RTC_ALARM_INT & nEvent) {
        lp_ticker_irq_handler();
    }
}


/** Get low power ticker's data
 *
 * @return The low power ticker data
 */
/*
const ticker_data_t* get_lp_ticker_data()
{
}*/


/* HAL lp ticker */

/** Initialize the low power ticker
 *
 */
void lp_ticker_init()
{
    // select LF clock
    adi_pwr_SetLFClockMux(ADI_CLOCK_MUX_LFCLK_LFXTAL);
    adi_pwr_EnableClockSource(ADI_CLOCK_SOURCE_LFXTAL,true);

    // open the rtc device
    adi_rtc_Open(1, rtc1_memory, ADI_RTC_MEMORY_SIZE, &hRTC1_Device);

    // set the prescaler value
    adi_rtc_SetPreScale(hRTC1_Device, RTC_PRESCALER);

    // register call back for events
    adi_rtc_RegisterCallback(hRTC1_Device, rtc1_Callback, hRTC1_Device);

    // reset the RTC counter
    adi_rtc_SetCount(hRTC1_Device, 0);

    // set the trim value
    adi_rtc_SetTrim(hRTC1_Device, ADI_RTC_TRIM_INTERVAL_14, ADI_RTC_TRIM_1, ADI_RTC_TRIM_SUB);

    adi_rtc_EnableTrim(hRTC1_Device, true);

    // enable the RTC
    adi_rtc_Enable(hRTC1_Device, true);
}

/** Read the current counter
 *
 * @return The current timer's counter value in microseconds
 */
uint32_t lp_ticker_read()
{
    uint32_t count;
    float t;

    // get current count
    adi_rtc_GetCount(hRTC1_Device, &count);

    // convert ticks to us
    t = (float)count * TIME_US_PER_TICK;

    count = (uint32_t)t;

    return count;
}

/** Set interrupt for specified timestamp
 *
 * @param timestamp The time in microseconds to be set
 */
void lp_ticker_set_interrupt(timestamp_t timestamp)
{
    uint32_t rtcCount, set_time;
    uint32_t cnt0, cnt1;

    // compute the tick value based on the given alarm time
    set_time = (uint32_t)((float)(timestamp) / TIME_US_PER_TICK);

    // get current count
    adi_rtc_GetCount(hRTC1_Device, &rtcCount);

    // alarm value needs to be greater than the current time
    // if already expired, call user ISR immediately
    if (set_time <= rtcCount) {
        rtc1_Callback(NULL, ADI_RTC_ALARM_INT, NULL);
        return;
    } else if (set_time <= rtcCount + TICKS_TO_ENABLE_ALARM) {
        // otherwise if the alarm time is less than the current RTC count + the time
        // it takes to enable the alarm, just wait until the desired number of counts
        // has expired rather than using the interrupt, then call the user ISR directly.
        do {
            adi_rtc_GetCount(hRTC1_Device, &rtcCount);
        } while (rtcCount < set_time);

        rtc1_Callback(NULL, ADI_RTC_ALARM_INT, NULL);
        return;
    }

    adi_rtc_GetCount(hRTC1_Device, &cnt0);

    // set the alarm
    adi_rtc_EnableInterrupts(hRTC1_Device, ADI_RTC_ALARM_INT, true);
    adi_rtc_SetAlarm(hRTC1_Device, set_time);
    adi_rtc_EnableAlarm(hRTC1_Device,true);

    adi_rtc_GetCount(hRTC1_Device, &cnt1);
}

/** Disable low power ticker interrupt
 *
 */
void lp_ticker_disable_interrupt()
{
    // disable alarm and interrupts
    adi_rtc_EnableAlarm(hRTC1_Device,false);
    adi_rtc_EnableInterrupts(hRTC1_Device, ADI_RTC_ALARM_INT, false);
}

/** Clear the low power ticker interrupt
 *
 */
void lp_ticker_clear_interrupt()
{
    return;
}

#endif
/**@}*/