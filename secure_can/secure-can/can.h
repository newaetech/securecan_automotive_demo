/*
 * can.h
 *
 *  Created on: Jan 26, 2018
 *      Author: User
 */

#ifndef SECURE_CAN_CAN_H_
#define SECURE_CAN_CAN_H_

typedef enum
{
   CAN_RET_BAD_ADDRESS   = -1,
   CAN_RET_TOO_MUCH_DATA = -2,
   CAN_RET_TIMEOUT       = -3,
   CAN_RET_BUSY          = -4,
   CAN_RET_ERROR         = -5,
   CAN_RET_ERROR_UNKNOWN = -6
}can_return_t;

void MX_CAN_Init(void);
can_return_t write_can(uint32_t address, uint8_t *pdata, int length);
can_return_t read_can(uint8_t *pdata, uint32_t *pAddress, int length);

#endif /* SECURE_CAN_CAN_H_ */
