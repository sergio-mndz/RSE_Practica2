/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2021 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "FreeRTOS.h"
#include "task.h"
#include "can_rtos.h"


/***************************
 * Definitions
 **************************/

/* Message IDs*/
#define KEEP_ALIVE		0x100U
#define BATERY_LEVEL	0x25U
#define REPORT_FREQ		0x55U
#define CMD_ACTUATOR	0x80U


/***************************
 * Prototypes
 **************************/

void CAN_KA_task(void* Args);
void CAN_Battery_task(void* Args);
void CAN_Report_Freq_task(void* Args);
void CAN_CMD_ACT_task(void* Args);

/***************************
 * Variables
 **************************/

uint8_t bufferKA, bufferBat, bufferFreq, bufferAct;

//bool rx_freq = false;
TickType_t report_Period = pdMS_TO_TICKS( 10000 ); //Periodo de 10 segundos

/***************************
 * Code
 **************************/


/*!
 * @brief Main function
 */
int main(void)
{
    /* Initialize board hardware. */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitDebugConsole();
    SystemCoreClockUpdate();

    init_flexcan_rtos();

    bufferFreq = set_Rx_Buffer((uint32_t)REPORT_FREQ);
    bufferAct = set_Rx_Buffer((uint32_t)CMD_ACTUATOR);

    bufferKA = set_Tx_Buffer((uint32_t)KEEP_ALIVE, 1);
    bufferBat = set_Tx_Buffer((uint32_t)BATERY_LEVEL, 2);

    if( xTaskCreate(
    		CAN_KA_task,
			"CAN KA Task",
			(configMINIMAL_STACK_SIZE*3),
			NULL,
			(14),
			NULL
			) != pdPASS )
    {
    	LOG_INFO("Failed to create CAN_KA_task");
    	while(1);
    }

    if( xTaskCreate(
    		CAN_Battery_task,
			"CAN Battery Task",
			(configMINIMAL_STACK_SIZE*3),
			NULL,
			(13),
			NULL
			) != pdPASS )
        {
        	LOG_INFO("Failed to create CAN_Battery_task");
        	while(1);
        }
    if( xTaskCreate(
    		CAN_Report_Freq_task,
			"CAN Freq Task",
			(configMINIMAL_STACK_SIZE*3),
			NULL,
			(12),
			NULL
			) != pdPASS )
            {
            	LOG_INFO("Failed to create CAN_Report_Freq_task");
            	while(1);
            }

	if( xTaskCreate(
			CAN_CMD_ACT_task,
			"CAN ACT Task",
			(configMINIMAL_STACK_SIZE*3),
			NULL,
			(11),
			NULL
			) != pdPASS ){
		LOG_INFO("Failed to create CAN_CMD_ACT_task");
		while(1);
	}


    LOG_INFO("\r\n==FlexCAN loopback example -- Start.==\r\n\r\n");


	vTaskStartScheduler();
}


//Keep Alive
void CAN_KA_task(void* Args){

	TickType_t xLastWakeTime;
	const TickType_t xPeriod = pdMS_TO_TICKS( 5000 );
	xLastWakeTime = xTaskGetTickCount();

	for(;;){
		flexcan_Tx(bufferKA, (uint32_t)0, (uint32_t) 0);
		vTaskDelayUntil(&xLastWakeTime, xPeriod);
	}
}

//Battery Level
void CAN_Battery_task(void* Args){
	TickType_t xLastWakeTime;
	TickType_t xPeriod = pdMS_TO_TICKS( 500 );
	xLastWakeTime = xTaskGetTickCount();

	/*while(!rx_freq){
		vTaskDelayUntil(&xLastWakeTime, xPeriod);
	}*/

	for(;;){
		flexcan_Tx(bufferBat, (uint32_t)0x10, (uint32_t) 0);
		vTaskDelayUntil(&xLastWakeTime, report_Period);
	}
}

//Report Frequency
void CAN_Report_Freq_task(void* Args){
	TickType_t xLastWakeTime;
	const TickType_t xPeriod = pdMS_TO_TICKS( 1000 );
	xLastWakeTime = xTaskGetTickCount();

	uint32_t word0;
	uint32_t word1;
	bool rx_succed;
	uint8_t freq;


	for(;;){
		rx_succed = flexcan_Rx_Non_Blocking(bufferFreq, &word0, &word1);

		if(rx_succed){
			freq = (word0 & 0xFFU);
			report_Period = pdMS_TO_TICKS( (uint32_t)(1000/freq));
			rx_freq = true;
		}

		vTaskDelayUntil(&xLastWakeTime, xPeriod);
	}
}

//Report Frequency
void CAN_CMD_ACT_task(void* Args){
	TickType_t xLastWakeTime;
	const TickType_t xPeriod = pdMS_TO_TICKS( 1000 );
	xLastWakeTime = xTaskGetTickCount();

	uint32_t word0;
	uint32_t word1;
	bool rx_succed;
	uint8_t cmd;


	for(;;){
		rx_succed = flexcan_Rx_Non_Blocking(bufferFreq, &word0, &word1);

		if(rx_succed){
			cmd = (word0 & 0x01U);
			if(cmd == 0){
				GPIO_PinWrite(BOARD_LED_RED_GPIO, BOARD_LED_RED_GPIO_PIN, LOGIC_LED_OFF);
			}
			else{
				GPIO_PinWrite(BOARD_LED_RED_GPIO, BOARD_LED_RED_GPIO_PIN, LOGIC_LED_ON);
			}
		}

		vTaskDelayUntil(&xLastWakeTime, xPeriod);
	}
}
