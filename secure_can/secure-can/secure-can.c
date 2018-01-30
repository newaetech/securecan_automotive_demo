/*
 This file is part of the ChipWhisperer Example Targets
 Copyright (C) 2012-2017 NewAE Technology Inc.

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "aes-independant.h"
#include "hal.h"
#include "simpleserial.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "can.h"
#include "adc.h"

static void print_can_error(char *pstring, can_return_t canError);
static void print_adc_error(char *str, adc_return_t err);

uint8_t get_key(uint8_t* k) {
	aes_indep_key(k);
	return 0x00;
}

uint8_t get_pt(uint8_t* pt) {
	trigger_high();
	aes_indep_enc(pt); /* encrypting the data block */
	trigger_low();
	simpleserial_put('r', 16, pt);
	return 0x00;
}

uint8_t reset(uint8_t* x) {
	// Reset key here if needed
	return 0x00;
}

void send_string(char *pString) {
	do {
		putch(*pString++);
	} while (*pString);
	return;
}

void send_adc(uint16_t val) {
	char buf[50];
	sprintf(buf, "ADC: %u\n", (unsigned int)val);
	send_string(buf);
}

/*****************************************************************************/
/**
 @brief Prints a 8 bit hex number to the serial port as ascii.
 @param value - The value to print.
 **/
/*****************************************************************************/
void print_number(uint8_t value) {
	uint8_t temp = ((value >> 4) & 0x0f);
	if (temp >= 0x0a) {
		temp += 0x41 - 0x0a;
	} else {
		temp += 0x30;
	}
	putch(temp);

	temp = (value & 0x0f);
	if (temp >= 0x0a) {
		temp += 0x41 - 0x0a;
	} else {
		temp += 0x30;
	}
	putch(temp);
}

#define M_DELAY(x)

int main(void) {
	can_return_t rval;
	uint8_t tmp[KEY_LENGTH] = { DEFAULT_KEY };
	uint8_t write_data[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	uint8_t read_data[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	uint32_t read_address;
	adc_return_t adcerr;
	int tick = 0;

	platform_init();
	init_uart();
	MX_CAN_Init();

	trigger_setup();

	aes_indep_init();
	aes_indep_key(tmp);

	send_string("Starting...\n");

	for (volatile unsigned int i = 0; i < 10000; i++)
		;

	rval = write_can(0xab, write_data, 7);
	if (rval < 0) {
		print_can_error("Tx ERROR:", rval);
	}

	adcerr = init_ADC();
	if (adcerr < 0) {
		print_adc_error("Error initializing ADC", adcerr);
	}

	while (1) {
		uint16_t adc_reading = 0;

		adcerr = read_ADC(&adc_reading);
		if (adcerr < 0) {
			print_adc_error("Error initializing ADC", adcerr);
		}
		send_adc(adc_reading);
		send_adc(adc_reading);
		send_adc(adc_reading);

		//add adc stuff here
		write_data[1]++;

		send_string("Tick [");
		print_number(tick++);
		send_string("] Compiled:"__TIME__"\n");
		write_can(0x1fabcdef, write_data, 7);
		if (rval < 0) {
			print_can_error("Tx ERROR:", rval);
		}

		for (volatile unsigned int j = 0; j < 50; j++) {
			for (volatile unsigned int i = 0; i < 10000; i++)
				;
		}
		rval = read_can(read_data, &read_address, 8);
		if (rval > 0) {
			write_data[1] = 0x50;
			send_string("Received from: ");
			for (int i = sizeof(uint32_t); i > 0; i--) {
				uint8_t print_data;
				print_data = (uint8_t) ((read_address >> (8 * (i - 1))
						& 0x000000ff));
				print_number(print_data);
			}
			send_string(":\n");

			for (int i = 0; i < rval; i++) {
				print_number(read_data[i]);
				send_string(" ");
			}
			send_string("\n");
		} else {
			if (rval != CAN_RET_TIMEOUT) {
				print_can_error("Rx ERROR:", rval);
			}
		}
	}

	simpleserial_init();
	simpleserial_addcmd('k', 16, get_key);
	simpleserial_addcmd('p', 16, get_pt);
	simpleserial_addcmd('x', 0, reset);
	while (1)
		simpleserial_get();
}

static void print_can_error(char *pstring, can_return_t canError) {
	switch (canError) {
	case CAN_RET_TIMEOUT:
		send_string("CAN_RET_TIMEOUT \n");
		break;
	case CAN_RET_BUSY:
		send_string("CAN_RET_BUSY\n");
		break;
	case CAN_RET_ERROR:
		send_string("CAN_RET_ERROR\n");
		break;
	default:
		send_string("CAN_RET_ERROR_UNKNOWN\n");
	}
	return;
}

static void print_adc_error(char *str, adc_return_t err) {
	switch(err) {
	case ADC_RET_ADC_INIT:
			send_string("ADC Init");
			break;
	case ADC_RET_CHANNEL_INIT:
		send_string("ADC channel Init");
			break;
	case ADC_RET_ADC_START:
		send_string("ADC Start");
		break;
	case ADC_RET_ADC_TIMEOUT:
		send_string("ADC Timeout");
		break;
	case ADC_RET_ADC_STOP:
		send_string("ADC stop");
		break;
	default:
		send_string("Unknown adc error");

	}
}
