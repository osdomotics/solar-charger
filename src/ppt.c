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

#include "ppt.h"

#define DEBUG 1
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/* PIN definitions */
#define PIN_ADC_SOL_AMPS   /* Solar Ampere */
#define PIN_ADC_SOL_VOLTS  /* Solar Volt */
#define PIN_ADC_BAT_VOLTS  /* Battery Volt */
#define PIN_PWM            /* PWM output step-down converter */
#define PIN_PWM_ENABLE     /* Enable MOSFETs via MOSFET driver */

/* Period of the mainloop, wait-time between two invocations */
#define LOOP_PERIOD (CLOCK_SECOND / 8)

#define mosfets_on()  digitalWrite (PIN_PWM_ENABLE, HIGH)
#define mosfets_off() digitalWrite (PIN_PWM_ENABLE, LOW)

/* Global variables available to contiki resources */
uint16_t sol_milliampere = 0;
uint16_t sol_millivolt   = 0;
uint16_t bat_millivolt   = 0;
uint32_t sol_milliwatt   = 0;

PROCESS(ppt, "Peak Power Tracking Solar Charger");

AUTOSTART_PROCESSES (&ppt);

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
  etimer_set (&et, LOOP_PERIOD);

  while(1) {
    PROCESS_WAIT_EVENT();
    etimer_reset (&et);
  } /* while (1) */

  PROCESS_END();
}

/*
 * VI settings, see coding style
 * ex:ts=8:et:sw=2
 */

/** @} */

