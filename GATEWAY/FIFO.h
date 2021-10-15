/*
 * FIFO.h
 *
 *  Created on: 7 oct. 2021
 *      Author: Missael Maciel
 */

#ifndef FIFO_H_
#define FIFO_H_

#include <stdint.h>

uint8_t Init_FIFO(uint16_t length);

uint8_t Fifo_Push(void* push_data, uint16_t length);

uint8_t Fifo_Pop(void* pop_data, uint16_t lenght);

uint32_t Get_Elements_Count(void);


#endif /* FIFO_H_ */
