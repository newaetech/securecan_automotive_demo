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
#include <string.h>
#include "can.h"
#include "adc.h"

static void print_can_error(char *pstring, can_return_t canError);
static void print_adc_error(char *str, adc_return_t err);

const uint8_t Kenc[] = {0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C};
const uint8_t Kauth[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
const uint8_t IV[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

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

typedef struct can_input {
	uint32_t msgnum;
	uint32_t baseid;
	uint8_t data[4];
} can_input;

typedef struct seccan_packet {
	uint32_t msgnum;
	uint32_t baseid;
	uint8_t payload[8];
} seccan_packet;

//all good, I think
void get_can_packet(seccan_packet *out, can_input *in)
{
	uint8_t nonce_enc[16] = {0};
	uint8_t nonce_auth[16] = {0};

	nonce_enc[0] = (in->msgnum >> 16) & 0xFF;
	nonce_enc[1] = (in->msgnum >> 8) & 0xFF;
	nonce_enc[2] = in->msgnum & 0xFF;
	nonce_enc[3] = (in->baseid >> 8) & 0xFF;
	nonce_enc[4] = (in->baseid & 0xFF);

	memcpy(nonce_auth, nonce_enc, 5); //copy nonce over to enc
	//nonce_enc all done
	memcpy(nonce_auth + 12, in->data, 4); //copy data over

	//do XORing with IV for auth
	int i = 0;
	for (i = 0; i < 16; i++) {
		nonce_auth[i] ^= IV[i];
	}

	aes_indep_key(Kenc);
	aes_indep_enc(nonce_enc);

	aes_indep_key(Kauth);
	aes_indep_enc(nonce_auth);

	for (i = 8; i < 12; i++) {
		nonce_enc[i] ^= in->data[i - 8];
	}
	for (i = 12; i < 16; i++) {
		nonce_enc[i] ^= nonce_auth[i - 12];
	}

	memcpy(out->payload, nonce_enc + 8, 8);
	out->baseid = in->baseid;
	out->msgnum = in->msgnum;
}

int decrypt_can_packet(can_input *out, seccan_packet *in)
{
	out->baseid = in->baseid;
	out->msgnum = in->msgnum;
	uint8_t nonce_enc[16] = {0};
	uint8_t nonce_auth[16] = {0};
	//first need output from Kenc AES, so first steps are the same

	nonce_enc[0] = (in->msgnum >> 16) & 0xFF;
	nonce_enc[1] = (in->msgnum >> 8) & 0xFF;
	nonce_enc[2] = in->msgnum & 0xFF;
	nonce_enc[3] = (in->baseid >> 8) & 0xFF;
	nonce_enc[4] = (in->baseid & 0xFF);

	memcpy(nonce_auth, nonce_enc, 5); //copy nonce over to enc

	aes_indep_key(Kenc);
	aes_indep_enc(nonce_enc);

	//can now get data by XORing output of Kenc AES with data part of packet
	int i = 0;
	for (; i < 4; i++) {
		out->data[i] = in->payload[i] ^ nonce_enc[i + 8];
	}

	//now check authentication by running through Kauth procedure
	memcpy(nonce_auth + 12, out->data, 4); //copy data over

	for (i = 0; i < 16; i++) {
		nonce_auth[i] ^= IV[i];
	}

	aes_indep_key(Kauth);
	aes_indep_enc(nonce_auth);

	for (i = 0; i < 4; i++) {
		nonce_auth[i] ^= nonce_enc[i + 12];
	}

	//returns 0 if auth matches, nonzero if auth doesn't match
	return memcmp(nonce_auth, in->payload + 4, 4);
}

void send_can_packet(seccan_packet *packet)
{
	can_return_t rval = 0;
	uint32_t ext_id = (packet->baseid) & 0x7FF;
	ext_id |= (packet->msgnum << 11) & 0x1FFFF800;

	if (rval = write_can(ext_id, packet->payload, 8), rval < 0) {
		print_can_error("Tx ERROR:", rval);
	}
}

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
			packet->baseid = ext_id & 0x7FF;
			packet->msgnum = (ext_id >> 11) & 0x3FFFF;
			return 0;
		}
	}
}

int main(void) {
	can_return_t rval;
	uint32_t read_address;
	adc_return_t adcerr;
	int tick = 0;

	can_input my_data = {
				.msgnum = 0x456,
				.baseid = 0x2D0,
				.data = {0xDE, 0xAD, 0xBE, 0xEF}
	};

	can_input their_data = {0};
	seccan_packet packet = {0};

	platform_init();
	init_uart();
	MX_CAN_Init();

	trigger_setup();

	aes_indep_init();
	get_can_packet(&packet, &my_data);
	decrypt_can_packet(&their_data, &packet);

	send_string("Starting...\n");

	for (volatile unsigned int i = 0; i < 10000; i++)
		;

	if (rval < 0) {
		print_can_error("Tx ERROR:", rval);
	}

	adcerr = init_ADC();
	if (adcerr < 0) {
		print_adc_error("Error initializing ADC", adcerr);
	}

	while (1) {

		//
		if (!read_can_packet(&packet)) {
			//we got a packet, we'll just take the data from their
			//packet, increment the bottom char and send it back

			decrypt_can_packet(&their_data, &packet);
			their_data.data[0]++;
			memcpy(my_data.data, their_data.data, 4);
			my_data.msgnum++;

			get_can_packet(&packet, &my_data);
			send_can_packet(&packet);
		}

		for (volatile unsigned int j = 0; j < 50; j++) {
			for (volatile unsigned int i = 0; i < 10000; i++)
				;
		}
	}
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
