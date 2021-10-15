/*
 * can_rtos.h
 *
 *  Created on: 14 oct 2021
 *      Author: Isaac
 */

#ifndef CAN_RTOS_H_
#define CAN_RTOS_H_

#include "fsl_debug_console.h"
#include "fsl_flexcan.h"
#include "FreeRTOS.h"
#include "task.h"

/***************************
 * Definitions
 **************************/


#define EXAMPLE_CAN            CAN0
#define EXAMPLE_CAN_CLK_SOURCE (kFLEXCAN_ClkSrc1)
#define EXAMPLE_CAN_CLK_FREQ   CLOCK_GetFreq(kCLOCK_BusClk)
/* Set USE_IMPROVED_TIMING_CONFIG macro to use api to calculates the improved CAN / CAN FD timing values. */
#define USE_IMPROVED_TIMING_CONFIG 	(0U)
#define DLC                        	(8)
#define CAN_BUFFERS					(16)

/* Fix MISRA_C-2012 Rule 17.7. */
#define LOG_INFO (void)PRINTF

/***************************
 * Prototypes
 **************************/

/*!
 * @brief FlexCan modude in RTOS initialize function.
 */
void init_flexcan_rtos(void);

uint8_t set_Rx_Buffer(uint32_t id);

uint8_t set_Tx_Buffer(uint32_t id, uint8_t dlc);

bool flexcan_Rx_Non_Blocking(uint8_t buffer, uint32_t *word0, uint32_t *word1);
void flexcan_Rx_Blocking(uint8_t buffer, uint32_t *word0, uint32_t *word1);

bool flexcan_Tx(uint8_t buffer, uint32_t word0, uint32_t word1);


#endif /* CAN_RTOS_H_ */
