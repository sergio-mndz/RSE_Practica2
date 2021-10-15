/*
 * FIFO.c
 *
 *  Created on: 7 oct. 2021
 *      Author: Missael Maciel
 */

#include <string.h>
#include "FIFO.h"
#include "FreeRTOS.h"

static void *FifoBuffer;
static uint8_t *PushPtr = 0;
static uint8_t *PopPtr = 0;
static uint16_t IndexMask;
static uint32_t ElementCount;
static uint32_t TotalFifoSize;
static uint32_t PushIdx;
static uint32_t PopIdx;


/*Init FIFO
 * Allocate buffer*/
uint8_t Init_FIFO(uint16_t length){
	uint8_t alloc_result;

	FifoBuffer = pvPortMalloc(length);
	if(FifoBuffer != NULL){
		alloc_result = pdPASS;
	}else{
		alloc_result = pdFAIL;
	}
	memset(FifoBuffer,0,length);
	PushPtr = (uint8_t*)FifoBuffer;
	PopPtr = (uint8_t*)FifoBuffer;
	TotalFifoSize = length;
	ElementCount = 0;
	PushIdx = 0;
	PopIdx = 0;
	IndexMask = length;
	return alloc_result;
}


uint8_t Fifo_Push(void *push_data, uint16_t length){
	uint16_t count;
	uint8_t push_result;
	uint16_t *data;

	data = (uint16_t*)push_data;
	if((TotalFifoSize - ElementCount)>=length){
		for(count=0;count<length;count++){
			PushPtr[PushIdx%IndexMask] = data[count];
			if(PushIdx==PopIdx){
				if(PopIdx==IndexMask){
					PopIdx==0;
				} else {
					PopIdx++;
				}
			}
			if(PushIdx==IndexMask){
				PushIdx=0;
			} else {
				PushIdx++;
			}

			ElementCount++;
		}
		push_result = pdPASS;
	}else{
		push_result = pdFAIL;
	}
	return push_result;
}



uint8_t Fifo_Pop(void* pop_data, uint16_t lenght){
	uint16_t count;
	uint16_t *data;
	uint8_t pop_result;

	data = (uint16_t*)pop_data;
	if(ElementCount >= lenght){
		for(count=0;count<lenght;count++){
			data[count] = PopPtr[PopIdx%IndexMask];
			if(PopIdx%IndexMask==0){
				PopIdx=0;
			} else {
				PopIdx++;
			}
			ElementCount--;
		}
		pop_result = pdPASS;
	}else{
		pop_result = pdFAIL;
	}
	return pop_result;
}


uint32_t Get_Elements_Count(void){
	return ElementCount;
}



