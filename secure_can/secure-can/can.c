/*
 * can.c
 *
 *  Created on: Jan 26, 2018
 *      Author: User
 */
#include "hal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "stm32f3xx_hal_can.h"
#include "stm32f3xx_hal_rcc.h"
#include "stm32f3xx_hal_rcc_ex.h"
#include "stm32f3_hal_lowlevel.h"
#include "stm32f3xx_hal_gpio.h"
#include "stm32f3xx_hal_rcc.h"
#include "stm32f3xx_hal_rcc_ex.h"
#include "can.h"

static void Error_Handler(void);

static CAN_HandleTypeDef myhcan;
static CanTxMsgTypeDef  msg;
static CanRxMsgTypeDef  RxMessage;

/* CAN init function */
void MX_CAN_Init(void)
{
   HAL_StatusTypeDef rval;
   CAN_FilterConfTypeDef  sFilterConfig;

   myhcan.Instance = CAN;
   myhcan.Init.Prescaler = 8;
   myhcan.Init.Mode = CAN_MODE_NORMAL;
   myhcan.Init.SJW = CAN_SJW_1TQ;
   myhcan.Init.BS1 = CAN_BS1_2TQ;
   myhcan.Init.BS2 = CAN_BS2_1TQ;
   myhcan.Init.TTCM = DISABLE;
   myhcan.Init.ABOM = DISABLE;
   myhcan.Init.AWUM = DISABLE;
   myhcan.Init.NART = DISABLE;
   myhcan.Init.RFLM = DISABLE;
   myhcan.Init.TXFP = DISABLE;

  rval = HAL_CAN_Init(&myhcan);

  if (rval != HAL_OK)
  {
    Error_Handler();
  }

  /*##-2- Configure the CAN Filter ###########################################*/
  sFilterConfig.FilterNumber = 0;
  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  sFilterConfig.FilterIdHigh = 0x0000;
  sFilterConfig.FilterIdLow = 0x0000;
  sFilterConfig.FilterMaskIdHigh = 0x0000;
  sFilterConfig.FilterMaskIdLow = 0x0000;
  sFilterConfig.FilterFIFOAssignment = 0;
  sFilterConfig.FilterActivation = ENABLE;
  sFilterConfig.BankNumber = 14;

  if (HAL_CAN_ConfigFilter(&myhcan, &sFilterConfig) != HAL_OK)
  {
    /* Filter configuration Error */
    Error_Handler();
  }

  msg.StdId = 0x124;//1BC;
  msg.ExtId = 0x12345ABA;
  msg.IDE = CAN_ID_EXT;
  msg.RTR = CAN_RTR_DATA;

  myhcan.pTxMsg = &msg;
  myhcan.pRxMsg = &RxMessage;


}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
static void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler */
  /* User can add his own implementation to report the HAL error return state */
  while(1)
  {
  }
  /* USER CODE END Error_Handler */
}

void HAL_CAN_MspInit(CAN_HandleTypeDef* hcan)
{
   GPIO_InitTypeDef GPIO_InitStruct;
   /* Peripheral clock enable */
   __HAL_RCC_CAN1_CLK_ENABLE();
   __HAL_RCC_GPIOB_CLK_ENABLE();

   /**CAN GPIO Configuration
      PB8     ------> CAN_RX
      PB9     ------> CAN_TX
    */
   GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
   GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
   GPIO_InitStruct.Alternate = GPIO_AF9_CAN;
   HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

static can_return_t can_return_error(HAL_StatusTypeDef canError)
{
   can_return_t r_error;
   switch(canError)
   {
      case HAL_TIMEOUT:
         r_error = CAN_RET_TIMEOUT;
         break;
      case HAL_BUSY:
         r_error = CAN_RET_BUSY;
         break;
      case HAL_ERROR:
         r_error = CAN_RET_ERROR;
         break;
      default:
         r_error = CAN_RET_ERROR_UNKNOWN;
   }
   return r_error;
}

can_return_t write_can(uint32_t address, uint8_t *pdata, int length)
{
   HAL_StatusTypeDef canError;
   int index;

   if(length > 8)
   {
      return(CAN_RET_TOO_MUCH_DATA);
   }
   msg.DLC = length;
   for(index = 0; index <= length; index++)
   {
      if(index >= 8)
      {
         break;
      }
      msg.Data[index] = *pdata++;
   }
   if(address <= 0x7FF)
   {
      msg.StdId = address;
      msg.IDE = CAN_ID_STD;
   }else if (address <= 0x1FFFFFFF)
   {
      msg.ExtId = address;
      msg.IDE = CAN_ID_EXT;
   }else
   {
      return(CAN_RET_BAD_ADDRESS);
   }

   // Transmit the data.
   canError = HAL_CAN_Transmit(&myhcan, 100);

   if(canError == HAL_OK)
   {
      return(index);
   }
   return(can_return_error(canError));
}

can_return_t read_can(uint8_t *pdata, uint32_t *pAddress, int length)
{
   HAL_StatusTypeDef canError;

   if(length < 8)
   {
      return(CAN_RET_TOO_MUCH_DATA);
   }
   canError = HAL_CAN_Receive(&myhcan,CAN_FIFO0,0);

   if(canError == HAL_OK)
   {
      memcpy(pdata, RxMessage.Data, 8);
      if(RxMessage.IDE == CAN_ID_EXT)
      {
         *pAddress = RxMessage.ExtId;
      }else
      {
         *pAddress = RxMessage.StdId;
      }
      return((int)RxMessage.DLC);
   }
   return(can_return_error(canError));
}
