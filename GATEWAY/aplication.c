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
#include "fsl_phy.h"

#include "fsl_phyksz8081.h"
#include "fsl_enet_mdio.h"
#include "fsl_device_registers.h"

#include "mqtt.h"
#include "can_rtos.h"
#include "FIFO.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/* Message IDs*/
#define KEEP_ALIVE		0x100U
#define BATERY_LEVEL	0x25U
#define REPORT_FREQ		0x55U
#define CMD_ACTUATOR	0x80U

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

void CAN_KA_task(void* Args);
void CAN_Battery_task(void* Args);
/*void CAN_Report_Freq_task(void* Args);
void CAN_CMD_ACT_task(void* Args);*/
void SEND_BATERY_DATA(void* Args);

void red_led_callback(char *msg, int len);
void period_callback(char *msg, int len);

/*******************************************************************************
 * Variables
 ******************************************************************************/

uint8_t bufferKA, bufferBat, bufferFreq, bufferAct;

bool keepAlive = false;
bool freq_received = false;

TickType_t battery_period = pdMS_TO_TICKS( 10000 );

char mqtt_battery_topic[] = "IsaacGallegos/feeds/batterylevel";
char *mqtt_sub_topic[] = {"IsaacGallegos/feeds/red-led", "IsaacGallegos/feeds/periodo"};
int mqtt_sub_qos[] = {0, 0};
void (*topics_callbacks[])(char *msg, int len) = {red_led_callback, period_callback};

/*******************************************************************************
 * Code
 ******************************************************************************/


/*!
 * @brief Main function
 */
int main(void)
{
    flexcan_config_t flexcanConfig;
    SYSMPU_Type *base = SYSMPU;

    /* Initialize board hardware. */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitDebugConsole();
    SystemCoreClockUpdate();

    /* Disable SYSMPU. */
        base->CESR &= ~SYSMPU_CESR_VLD_MASK;

        set_subscribed_topics(mqtt_sub_topic, mqtt_sub_qos, topics_callbacks, 3);

    /* Initialize lwIP from thread */
	if (sys_thread_new("main", lwip_mqtt_stack_init, NULL, INIT_THREAD_STACKSIZE, configMAX_PRIORITIES) == NULL)
	   {
		   LWIP_ASSERT("main(): Task creation failed.", 0);
	   }

	if(!Init_FIFO(16)){
		PRINTF("Failed to create FIFO");
		while(1);
	}

	init_flexcan_rtos();

    bufferFreq = set_Tx_Buffer((uint32_t)REPORT_FREQ, 1);
    bufferAct = set_Tx_Buffer((uint32_t)CMD_ACTUATOR, 1);

    bufferKA = set_Rx_Buffer((uint32_t)KEEP_ALIVE);
    bufferBat = set_Rx_Buffer((uint32_t)BATERY_LEVEL);

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
	   SEND_BATERY_DATA,
		"MQTT DATA Task",
		(configMINIMAL_STACK_SIZE*3),
		NULL,
		(12),
		NULL
		) != pdPASS )
	   {
		LOG_INFO("Failed to create SEND_BATERY_DATA");
		while(1);
	   }


	vTaskStartScheduler();
}

void red_led_callback(char *msg, int len)
{
	if(keepAlive){
		if(strncmp(msg, "ON", len) == 0)
		{
			flexcan_Tx(bufferAct, (uint32_t)1, (uint32_t)0);
			PRINTF("Red Led is on\r\n");
		}
		else if (strncmp(msg, "OFF", len) == 0)
		{
			flexcan_Tx(bufferAct, (uint32_t)0, (uint32_t)0);
			PRINTF("Red Led is off\r\n");
		}
	}
}


void period_callback(char *msg, int len)
{
	char tmp[64];
	uint32_t periodo;

	freq_received = true;
	strncpy( tmp, msg, len );
	periodo = atoi(tmp);
	flexcan_Tx(bufferAct, periodo, (uint32_t)0);
	PRINTF("Period is %s\r\n", tmp);

}

//Keep Alive
void CAN_KA_task(void* Args){

	TickType_t xLastWakeTime;
	const TickType_t xPeriod = pdMS_TO_TICKS( 5000 );
	xLastWakeTime = xTaskGetTickCount();

	uint32_t word0;
	uint32_t word1;
	bool rx_succed;

	for(;;){
		rx_succed = flexcan_Rx_Non_Blocking(bufferKA, &word0, &word1);

		if(rx_succed){
			keepAlive = true;
		}

		vTaskDelayUntil(&xLastWakeTime, xPeriod);
	}
}

//Battery Level
void CAN_Battery_task(void* Args){
	TickType_t xLastWakeTime;
	TickType_t xPeriod = pdMS_TO_TICKS( 1000 );
	xLastWakeTime = xTaskGetTickCount();
	uint32_t word0;
	uint32_t word1;
	bool rx_succed;
	uint16_t battery_level;


	while(!keepAlive){
		vTaskDelayUntil(&xLastWakeTime, xPeriod);
	}

	for(;;){
		rx_succed = flexcan_Rx_Non_Blocking(bufferBat, &word0, &word1);

		if(rx_succed){
			battery_level = (word0 & 0xFFFF);
			Fifo_Push((void*)&battery_level, 1);
		}

		vTaskDelayUntil(&xLastWakeTime, battery_period);
	}
}

//Report Frequency
/*void CAN_Report_Freq_task(void* Args){
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
			battery_period = pdMS_TO_TICKS( (uint32_t)(1000/freq));
			rx_freq = true;
		}

		vTaskDelayUntil(&xLastWakeTime, xPeriod);
	}
}*/

//Report Frequency
/*void CAN_CMD_ACT_task(void* Args){
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
			//Prender LED
		}

		vTaskDelayUntil(&xLastWakeTime, xPeriod);
	}
}*/

//Report Frequency
void SEND_BATERY_DATA(void* Args){
	TickType_t xLastWakeTime;
	const TickType_t xPeriod = pdMS_TO_TICKS( 30000 );
	xLastWakeTime = xTaskGetTickCount();
	char battery_data[60];
	char tmp_battery_str[10];
	uint16_t str_len;

	uint32_t len_data;
	uint16_t tmp_battery;


	while(!keepAlive){
		vTaskDelayUntil(&xLastWakeTime, xPeriod);
	};

	for(;;){
		len_data = Get_Elements_Count();
		if(len_data > 0){
			Fifo_Pop((void*)&tmp_battery, 1);
			str_len = sprintf(battery_data, "%d", tmp_battery);
			len_data -= 1;
		}
		while(len_data > 0){
			Fifo_Pop((void*)&tmp_battery, 1);
			str_len += sprintf(tmp_battery_str, ", %d", tmp_battery);
			strcat(battery_data, tmp_battery_str);
			len_data -= 1;
		}

		mqtt_publish_mesagge(mqtt_battery_topic,  battery_data);
		vTaskDelayUntil(&xLastWakeTime, xPeriod);
	}
}
