
#include "hal.h"
#include <stdint.h>
#include "stm32f3xx_hal_rcc.h"
#include "stm32f3xx_hal_rcc_ex.h"
#include "stm32f3_hal_lowlevel.h"
#include "stm32f3xx_hal_gpio.h"
#include "stm32f3xx_hal_adc.h"
#include "adc.h"
#define ADC_READ_TIMEOUT 5000

static ADC_HandleTypeDef myadc;

void init_ADC_pin(void)
{
	GPIO_InitTypeDef gpio;
	gpio.Pin = GPIO_PIN_14;
	gpio.Mode = GPIO_MODE_ANALOG;
	gpio.Pull = GPIO_NOPULL;
	gpio.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOB, &gpio);
}

adc_return_t init_ADC(void)
{
	__HAL_RCC_ADC34_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	init_ADC_pin();
	myadc.Instance = ADC4;

	myadc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4; //div by 4 rec for 12 and 10 bit
	myadc.Init.Resolution = ADC_RESOLUTION_12B;
	myadc.Init.DataAlign = ADC_DATAALIGN_RIGHT; //lsb at bit 0
	myadc.Init.ScanConvMode = ADC_SCAN_ENABLE; //just one channel needed, so no scanning
	myadc.Init.EOCSelection = DISABLE;//ADC_EOC_SINGLE_CONV; //does this matter with one channel?
	myadc.Init.LowPowerAutoWait = DISABLE;
	myadc.Init.ContinuousConvMode = ENABLE; //single mode now
	myadc.Init.NbrOfConversion = 1; //does this matter for single channel?
	myadc.Init.DiscontinuousConvMode = DISABLE; //discarded with continuous mode
	myadc.Init.NbrOfDiscConversion = 1; //discarded without Discont mode
	myadc.Init.ExternalTrigConv = ADC_SOFTWARE_START; //start ADC with software, not ext trigger
	myadc.Init.ExternalTrigConvEdge = 0; //discarded with software start
	myadc.Init.DMAContinuousRequests = DISABLE;
	myadc.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN; //overwrite if data not read before next conversion is done

	if (HAL_ADC_Init(&myadc) != HAL_OK) {
		//send error message
		return ADC_RET_ADC_INIT;
	}

	ADC_ChannelConfTypeDef channel;
	channel.Channel = ADC_CHANNEL_4; //PB14
	channel.Rank = ADC_REGULAR_RANK_1; //single conversion, does this matter?
	channel.SamplingTime = ADC_SAMPLETIME_19CYCLES_5;
	channel.SingleDiff = ADC_SINGLE_ENDED;
	channel.OffsetNumber = ADC_OFFSET_NONE;
	channel.Offset = 0;

	if (HAL_ADC_ConfigChannel(&myadc, &channel) != HAL_OK) {
		//send error
		return ADC_RET_CHANNEL_INIT;
	}

	if (HAL_ADC_Start(&myadc) != HAL_OK) {
			return ADC_RET_ADC_START;
	}

	return 0;
}

void adc_delay(void)
{
	volatile uint32_t i;
	for (i = 0; i < 50000; i++);
}

adc_return_t read_ADC(uint16_t *val)
{

	if (HAL_ADC_PollForConversion(&myadc, ADC_READ_TIMEOUT) == HAL_OK) {
		//good to read
		*val = HAL_ADC_GetValue(&myadc);
	} else {
		//error
		return ADC_RET_ADC_TIMEOUT;
	}

	return 0;
}
