/*
 * pwm.h
 *
 *  Created on: Feb 5, 2018
 *      Author: Alex
 */

#ifndef SECURE_CAN_PWM_H_
#define SECURE_CAN_PWM_H_
#include <stdint.h>

//initializes PWM on PA11 with 0% duty cycle
//returns -1 upon error, 0 upon success
int setup_PWM(void);

//changes the duty cycle of the PA11 PWM signal
//0 = 0%, 65535 = 100%
void change_PWM(uint16_t dcycle);


#endif /* SECURE_CAN_PWM_H_ */
