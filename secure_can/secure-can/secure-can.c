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
#include "stm32f3_hal_lowlevel.h"
#include "stm32f3_hal.h"
#include "stm32f3xx_hal_gpio.h"
#include "stm32f3xx_hal_rcc.h"
#include "stm32f3xx_hal_tim.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "can.h"
#include "adc.h"

static void print_can_error(char *pstring, can_return_t canError);
static void print_adc_error(char *str, adc_return_t err);

uint8_t Kenc[] = {0x21, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C};
uint8_t Kauth[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
uint8_t IV[] = {0x01, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

void send_string(char *pString) {
	do {
		putch(*pString++);
	} while (*pString);
	return;
}

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

//encrypts the data in in and puts the finished product in out
void encrypt_can_packet(seccan_packet *out, can_input *in)
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

//decrypts in, putting the baseid, msgnum, and data into out
//returns 0 upon successful authentication of the message
//and nonzero for unsuccessful authentication of the message
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

//sends the info in packet over the CAN bus
void send_can_packet(seccan_packet *packet)
{
	can_return_t rval = 0;
	uint32_t ext_id = (packet->baseid) & 0x7FF;
	ext_id |= (packet->msgnum << 11) & 0x1FFFF800;

	if (rval = write_can(ext_id, packet->payload, 8), rval < 0) {
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
			packet->baseid = ext_id & 0x7FF;
			packet->msgnum = (ext_id >> 11) & 0x3FFFF;
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


void setup_led(void)
{
	__HAL_RCC_GPIOC_CLK_ENABLE();

	GPIO_InitTypeDef GpioInit;
	GpioInit.Pin       = GPIO_PIN_15;
	GpioInit.Mode      = GPIO_MODE_OUTPUT_PP;
	GpioInit.Pull      = GPIO_NOPULL;
	GpioInit.Speed     = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOC, &GpioInit);

	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_15, RESET);

	GpioInit.Pin       = GPIO_PIN_13;
	GpioInit.Mode      = GPIO_MODE_INPUT;
	GpioInit.Pull      = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOC, &GpioInit);
}



static TIM_OC_InitTypeDef pwm;

static TIM_HandleTypeDef tim;
int setup_PWM(void)
{
	HAL_StatusTypeDef rtn;
	__HAL_RCC_TIM1_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();

	GPIO_InitTypeDef GpioInit;
	GpioInit.Pin       = GPIO_PIN_11;
	GpioInit.Mode      = GPIO_MODE_AF_PP;
	GpioInit.Pull      = GPIO_NOPULL;
	GpioInit.Speed     = GPIO_SPEED_FREQ_HIGH;
	GpioInit.Alternate = GPIO_AF11_TIM1;
	HAL_GPIO_Init(GPIOA, &GpioInit);


	tim.Instance = TIM1;
	//tim.Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED1;tim.Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED1;
	tim.Init.CounterMode = TIM_COUNTERMODE_DOWN;
	tim.Init.Prescaler = 0;
	tim.Init.Period = 0xFFFF;
	tim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	tim.Init.RepetitionCounter = 0x0;
	tim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;



	pwm.OCMode = TIM_OCMODE_PWM1;
	pwm.Pulse = 0;
	pwm.OCPolarity = TIM_OCPOLARITY_HIGH;
	pwm.OCNPolarity = TIM_OCNPOLARITY_HIGH;
	pwm.OCFastMode = TIM_OCFAST_ENABLE; //s;nic
	pwm.OCIdleState = TIM_OCIDLESTATE_RESET;
	pwm.OCNIdleState = TIM_OCNIDLESTATE_RESET;

	tim.Channel = HAL_TIM_ACTIVE_CHANNEL_4;

	rtn = HAL_TIM_PWM_Init(&tim);

	rtn = HAL_TIM_PWM_ConfigChannel(&tim, &pwm, TIM_CHANNEL_4);
	rtn = HAL_TIM_PWM_Start(&tim, TIM_CHANNEL_4);

	if (rtn != HAL_OK) {
		return -1;
	}

	return 0;
}

//0 = 0%
//65535 = 100%
void change_PWM(uint16_t dcycle)
{
	pwm.Pulse = dcycle;
	//pwm.Pulse++;
	//HAL_TIM_PWM_Stop(&tim, TIM_CHANNEL_4);
	HAL_TIM_PWM_ConfigChannel(&tim, &pwm, TIM_CHANNEL_4);
	HAL_TIM_PWM_Start(&tim, TIM_CHANNEL_4);
}

void master_stm_loop(void)
{
	can_input my_data = {
				.msgnum = 0x456,
				.baseid = 0x2DD,
				.data = {0x12, 0x34, 0x56, 0x78}
	};
	can_input their_data;
	seccan_packet packet;
	setup_led();
	setup_PWM();
	while (1) {
		encrypt_can_packet(&packet, &my_data);
		send_can_packet(&packet);

		my_data.msgnum++;
		uint32_t timeout = 0;


		while (timeout++ < 50) {
			if (!read_can_packet(&packet)) {
				if (decrypt_can_packet(&their_data, &packet))
					break;
				uint16_t voltage = their_data.data[0] | (their_data.data[1] << 8);
				change_PWM(voltage << 4);
				if (voltage > 1800)
					//turn LED on
					HAL_GPIO_WritePin(GPIOC, GPIO_PIN_15, SET);
				else
					//turn LED off
					HAL_GPIO_WritePin(GPIOC, GPIO_PIN_15, RESET);
				break;
			}
		}
	}
}

void adc_stm_loop(void)
{
	adc_return_t adcerr;
	char master_msg[4] = {0x12, 0x34, 0x56, 0x78};
	adcerr = init_ADC();
	if (adcerr < 0) {
		print_adc_error("Error initializing ADC", adcerr);
	}
	can_input my_data = {
				.msgnum = 0x0,
				.baseid = 0x200,
				.data = {0x12, 0x34, 0x56, 0x78}
	};
	can_input their_data;
	seccan_packet packet;

	while(1) {

		//wait for packet
		while (read_can_packet(&packet));

		if (!decrypt_can_packet(&their_data, &packet)) {
			if (!memcmp(their_data.data, master_msg, 4)) {

				//packet all good, so start doing adc
				uint16_t adc_value = 0;
				if (adcerr = read_ADC(&adc_value), adcerr == 0) {
					my_data.data[0] = (adc_value) & 0xFF;
					my_data.data[1] = (adc_value >> 8);
					encrypt_can_packet(&packet, &my_data);
					send_can_packet(&packet);
					my_data.msgnum++;
				} else {
					print_adc_error("", adcerr);
				}
			}
		}

	}
}

int main(void) {


	setup();
	send_string("Starting...\n");

	for (volatile unsigned int i = 0; i < 10000; i++)
		;

	master_stm_loop();
	//adc_stm_loop();
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
