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


#include "hal.h"
#include "seccan.h"
#include "simpleserial.h"
#include <stdint.h>
#include <string.h>
#include "can.h"
#include "adc.h"
#include "pwm.h"
#include "aes-independant.h"

//#define CAN_TWO_WAY 1

static void print_can_error(char *pstring, can_return_t canError);
static void print_adc_error(char *str, adc_return_t err);

void send_string(char *pString) {
	do {
		putch(*pString++);
	} while (*pString);
	return;
}

//sends the info in packet over the CAN bus
void send_can_packet(seccan_packet *packet)
{
	can_return_t rval = 0;
	if (rval = write_can(packet->ext_id, packet->payload, 8), rval < 0) {
		print_can_error("Tx ERROR:", rval);
	}
}

//reads a message from the CAN bus and moves the data into packet
int read_can_packet(seccan_packet *packet)
{
	can_return_t rval = 0;
	uint32_t ext_id = 0;

	if (rval = read_can(packet->payload, &ext_id, 8), rval < 0) {
		print_can_error("Rx ERROR:", rval);
		return -1;
	} else {
		if (rval != 8) {
			send_string("Msg length too short");
			return -1;
		} else {

			return 0;
		}
	}
}

void setup(void)
{
	platform_init();
	init_uart();
	MX_CAN_Init();

	trigger_setup();
	aes_indep_init();
}

//loop for the STM that controls the motor
void master_stm_loop(void)
{
	can_input their_data;
	seccan_packet packet;
	setup_PWM();
	while (1) {
#ifdef CAN_TWO_WAY
		can_input my_data = {
					.msgnum = 0x456,
					.baseid = 0x2DD,
					.data = {0x12, 0x34, 0x56, 0x78}
		};
		encrypt_can_packet(&packet, &my_data);
		send_can_packet(&packet);

		my_data.msgnum++;
		uint32_t timeout = 0;

		while (timeout++ < 50) {
#endif
			if (!read_can_packet(&packet)) {
				if (decrypt_can_packet(&their_data, &packet))
					break;
				uint16_t voltage = their_data.data[0] | (their_data.data[1] << 8);
				change_PWM(voltage << 4);
			}
#ifdef CAN_TWO_WAY
		}
#endif
	}
}

//loop for the ADC STM32
void adc_stm_loop(void)
{
	adc_return_t adcerr;

	adcerr = init_ADC();
	if (adcerr < 0) {
		print_adc_error("Error initializing ADC", adcerr);
	}
	can_input my_data = {
				.msgnum = 0x0,
				.baseid = 0x200,
				.data = {0x12, 0x34, 0x56, 0x78}
	};

	seccan_packet packet;

	while(1) {
#ifdef CAN_TWO_WAY
		char master_msg[4] = {0x12, 0x34, 0x56, 0x78};
		can_input their_data;
		//wait for packet
		while (read_can_packet(&packet));

		if (!decrypt_can_packet(&their_data, &packet)) {
			if (!memcmp(their_data.data, master_msg, 4)) {
#endif

				//packet all good, so start doing adc
				uint16_t adc_value = 0;
				if (adcerr = read_ADC(&adc_value), adcerr == 0) {
					for (volatile unsigned int i = 0; i < 5000; i++);
					my_data.data[0] = (adc_value) & 0xFF;
					my_data.data[1] = (adc_value >> 8);
					encrypt_can_packet(&packet, &my_data);
					send_can_packet(&packet);
					my_data.msgnum++;
				} else {
					print_adc_error("", adcerr);
				}
#ifdef CAN_TWO_WAY
			}
		}
#endif

	}
}

int main(void) {
	setup();
	send_string("Starting...\n");

	//master_stm_loop();
	adc_stm_loop();
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
