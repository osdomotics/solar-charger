/*
 * Copyright (c) 2009, Tim Nolan (www.timnolan.com)
 *               2014, Ralf Schlatterbeck Open Source Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/**
 * \addtogroup Peak Power Tracking Solar Charger
 *
 * This software implements Tim Nolan's Peak Power Tracking Solar
 * Charger. Originally designed with an Arduino Duemilanove Board it was
 * ported to Contiki by Ralf Schlatterbeck and rewritten in large parts.
 * The original soft- and hardware of the project is open source "free
 * of any restiction for anyone to use". I'm putting this under the
 * 3-clause BSD license (see above) to be compatible with other Contiki
 * code.
 *
 * The original author, Tim Nolan writes: "All I ask is that if you use
 * any of my hardware or software or ideas from this project is that you
 * give me credit and add a link to my website www.timnolan.com to your
 * documentation. Thank you."
 *
 * @{
 */

/**
 * \file
 *      Peak Power Tracking Solar Charger
 * \author
 *      Tim Nolan (www.timnolan.com)
 *      Ralf Schlatterbeck <rsc@runtux.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"
#include "erbium.h"
#include "er-coap-13.h"
#include "Arduino.h"
#include "ppt.h"

#define DEBUG 1
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/* PIN definitions */
#define PIN_ADC_SOL_AMPS   5  /* Solar Ampere */
#define PIN_ADC_SOL_VOLTS  4  /* Solar Volt */
#define PIN_ADC_BAT_VOLTS  6  /* Battery Volt */
#define PIN_PWM            2  /* PWM output step-down converter */
#define PIN_PWM_ENABLE     3  /* Enable MOSFETs via MOSFET driver */

/* ADC averaging */
#define ADC_AVG_NUM 8

/*
 * Integer arithmetics for ampere/voltage values, define a multiplier
 * for each depending on the hardware, this makes calibration in
 * software possible. The ADC is 10bit, 0-1023.
 *
 * V: 1023 equiv 1.6V at input pin
 *    Voltage divider 10k / 560
 *    1023 equiv 1600 * (10000 + 560) / 560 = 30171
 * A: R = 0.005 Ohm, Amplification 100
 *    U = I * R * 100 = I * 0.005 * 100 = I * 0.5
 *    1023 equiv 3200 mA
 * Calculation is in long to avoid overflows
 */
#define SCALE_SOL_VOLT_MUL  29670L
#define SCALE_SOL_VOLT_DIV   1023L
#define SCALE_BAT_VOLT_MUL  29748L
#define SCALE_BAT_VOLT_DIV   1023L
#define SCALE_SOL_AMPS_MUL   2805L
#define SCALE_SOL_AMPS_DIV   1023L

/* Timer definitions */
#define TIMER         3
#define TIMER_CHANNEL HWT_CHANNEL_A

/* Period of the mainloop, wait-time between two invocations */
#define LOOP_PERIOD (CLOCK_SECOND / 8)

/* PWM duty-cycle definitions in percent */
#define PWM_MAX   100
#define PWM_MIN    60
#define PWM_START  90
#define PWM_INC     1

/* Charger state-machine */
#define CHARGER_OFF   0
#define CHARGER_ON    1
#define CHARGER_BULK  2
#define CHARGER_FLOAT 3

#define mosfets_on()  digitalWrite (PIN_PWM_ENABLE, HIGH)
#define mosfets_off() digitalWrite (PIN_PWM_ENABLE, LOW)

/* Global variables available to contiki resources */
uint16_t sol_milliampere = 0;
uint16_t sol_millivolt   = 0;
uint16_t bat_millivolt   = 0;
uint32_t sol_milliwatt   = 0;

/* Static variables (moduel global) */
/*
 * PWM settings: pwm_max is the max. dutycycle which we know after
 * initializing the timer. Since the MOSFET driver chip has a
 * charge-pump and needs pwm, we set this 1 smaller than the maximum,
 * the maximum would set the pin to continuous high.
 */
static uint16_t pwm_max   = 0;
static uint16_t pwm_min   = 0;
static uint16_t pwm_start = 0;
static uint16_t pwm       = 0;

static uint8_t  charger_state = CHARGER_OFF;

PROCESS(ppt, "Peak Power Tracking Solar Charger");

AUTOSTART_PROCESSES (&ppt);

/**
 * \brief Averaged read of analog input
 * \param channel: The analog input to read from
 * \return averaged input value over several reads
 */
static uint16_t read_adc (uint8_t channel)
{
  uint16_t sum = 0;
  uint8_t i = 0;

  adc_setup (ADC_DEFAULT, channel);
  for (i=0; i<ADC_AVG_NUM; i++) {
    sum += adc_read ();
    clock_delay_usec (50);
  }
  adc_fin ();
  return sum / ADC_AVG_NUM;
}

/**
 * \brief Read analog inputs, battery/solar voltage, solar amps
 *        compute solar watts.
 */
static void read_analog_inputs (void)
{
  /* Multiplier / divisor constants are long, so this is long arithmetics */
  sol_milliampere = read_adc (PIN_ADC_SOL_AMPS)
                  * SCALE_SOL_AMPS_MUL / SCALE_SOL_AMPS_DIV
                  ;
  sol_millivolt   = read_adc (PIN_ADC_SOL_VOLTS)
                  * SCALE_SOL_VOLT_MUL / SCALE_SOL_VOLT_DIV
                  ;
  bat_millivolt   = read_adc (PIN_ADC_BAT_VOLTS)
                  * SCALE_BAT_VOLT_MUL / SCALE_BAT_VOLT_DIV
                  ;
  sol_milliwatt = (long)sol_milliampere * (long)sol_millivolt / 1000L;
}

PROCESS_THREAD (ppt, ev, data)
{
  static struct etimer et;
  PROCESS_BEGIN ();

  rest_init_engine ();
  //rest_activate_resource (&resource_info);
  rest_activate_resource (&resource_solar_current);
  rest_activate_resource (&resource_solar_voltage);
  rest_activate_resource (&resource_battery_voltage);
  rest_activate_resource (&resource_solar_power);
  adc_init ();
  /* 20µs cycle time for timer, fast pwm mode, ICR */
  hwtimer_pwm_ini (TIMER, 20, HWT_PWM_FAST, 0);
  hwtimer_pwm_enable (TIMER, TIMER_CHANNEL);
  pwm_max = hwtimer_pwm_max_ticks (TIMER) - 1;
  /* Do long arithmetics here, the duty-cycle can be up to 0xFFFF */
  pwm_min   = (pwm_max + 1L) * PWM_MIN   / 100L;
  pwm_start = (pwm_max + 1L) * PWM_START / 100L;
  pwm       = pwm_start;
  hwtimer_set_pwm (TIMER, TIMER_CHANNEL, 0);
  pinMode (PIN_ADC_SOL_AMPS,  INPUT);
  pinMode (PIN_ADC_SOL_VOLTS, INPUT);
  pinMode (PIN_ADC_BAT_VOLTS, INPUT);
  pinMode (PIN_PWM_ENABLE,    OUTPUT);
  pinMode (PIN_PWM,           OUTPUT);
  mosfets_on ();
  charger_state = CHARGER_ON;
  etimer_set (&et, LOOP_PERIOD);

  while(1) {
    PROCESS_WAIT_EVENT();
    read_analog_inputs ();
    etimer_reset (&et);
  } /* while (1) */

  PROCESS_END();
}

/*
 * VI settings, see coding style
 * ex:ts=8:et:sw=2
 */

/** @} */

