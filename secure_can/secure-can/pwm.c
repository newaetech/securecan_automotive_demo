#include "pwm.h"
#include "stm32f3xx_hal_tim.h"
#include "stm32f3xx_hal_gpio.h"
#include "stm32f3xx_hal_rcc.h"

static TIM_OC_InitTypeDef pwm;

static TIM_HandleTypeDef tim;
//initializes a PWM signal on pin A11 using timer 1.
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

//changes the duty cycle of the PWM signal
void change_PWM(uint16_t dcycle)
{
	pwm.Pulse = dcycle;
	HAL_TIM_PWM_ConfigChannel(&tim, &pwm, TIM_CHANNEL_4);
	HAL_TIM_PWM_Start(&tim, TIM_CHANNEL_4);
}
