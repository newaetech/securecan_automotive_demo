/*
 * adc.h

 *
 *  Created on: Jan 30, 2018
 *      Author: Alex Dewar
 */

#include <stdint.h>
#ifndef SECURE_CAN_ADC_H_
#define SECURE_CAN_ADC_H_



typedef enum {
	ADC_RET_OK = 0,
	ADC_RET_ADC_INIT = -1,
	ADC_RET_CHANNEL_INIT = -2,
	ADC_RET_ADC_START = -3,
	ADC_RET_ADC_TIMEOUT = -4,
	ADC_RET_ADC_STOP = -5,
	ADC_RET_PIN_INIT = -6
} adc_return_t;

adc_return_t init_ADC(void);
adc_return_t read_ADC(uint16_t *val);

#endif /* SECURE_CAN_ADC_H_ */
