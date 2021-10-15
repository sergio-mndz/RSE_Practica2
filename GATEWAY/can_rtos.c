/*
 * can_rtos.c
 *
 *  Created on: 14 oct 2021
 *      Author: Isaac
 */

#include "can_rtos.h"

/***************************
 * Variables
 **************************/

volatile bool rxComplete = false;
volatile bool txComplete = false;

flexcan_handle_t flexcanHandle;
flexcan_mb_transfer_t canXfer[CAN_BUFFERS];
flexcan_frame_t canFrame[CAN_BUFFERS];
flexcan_rx_mb_config_t mbConfig[CAN_BUFFERS];

static uint8_t new_can_buff = 0;
static uint8_t rx_buffs[CAN_BUFFERS];
static uint8_t tx_buffs[CAN_BUFFERS];
static uint8_t rx_buffs_num = 0;
static uint8_t tx_buffs_num = 0;

static uint8_t last_rx;

/***************************
 * Code
 **************************/

/*!
 * @brief FlexCAN Call Back function
 */
static FLEXCAN_CALLBACK(flexcan_callback)
{
	uint8_t i;
    switch (status)
    {
        /* Process FlexCAN Rx event. */
        case kStatus_FLEXCAN_RxIdle:
            i = rx_buffs_num;
        	while(i > 0 && rxComplete != true){
        		i -= 1;
        		if(rx_buffs[i] == result){
        			rxComplete = true;
        			last_rx = rx_buffs[i];
        		}
        	}
            break;

        /* Process FlexCAN Tx event. */
        case kStatus_FLEXCAN_TxIdle:
            i = tx_buffs_num;
        	while(i > 0 && txComplete != true){
        		i -= 1;
        		if(tx_buffs[i] == result){
        			txComplete = true;
        		}
        	}
            break;

        default:
            break;
    }
}


/*!
 * @brief FlexCan modude in RTOS initialize function.
 */
void init_flexcan_rtos(void){
	flexcan_config_t flexcanConfig;

	/* Init FlexCAN module. */
    /*
     * flexcanConfig.clkSrc                 = kFLEXCAN_ClkSrc0;
     * flexcanConfig.baudRate               = 1000000U;
     * flexcanConfig.baudRateFD             = 2000000U;
     * flexcanConfig.maxMbNum               = 16;
     * flexcanConfig.enableLoopBack         = false;
     * flexcanConfig.enableSelfWakeup       = false;
     * flexcanConfig.enableIndividMask      = false;
     * flexcanConfig.disableSelfReception   = false;
     * flexcanConfig.enableListenOnlyMode   = false;
     * flexcanConfig.enableDoze             = false;
     */
	FLEXCAN_GetDefaultConfig(&flexcanConfig);

	#if defined(EXAMPLE_CAN_CLK_SOURCE)
    	flexcanConfig.clkSrc = EXAMPLE_CAN_CLK_SOURCE;
	#endif

    /* LOOP BACK ENABLE*/
	//flexcanConfig.enableLoopBack = true;
	flexcanConfig.baudRate       = 125000U;

	FLEXCAN_Init(EXAMPLE_CAN, &flexcanConfig, EXAMPLE_CAN_CLK_FREQ);

	FLEXCAN_TransferCreateHandle(EXAMPLE_CAN, &flexcanHandle,flexcan_callback, NULL);
}



uint8_t set_Rx_Buffer(uint32_t id){
	mbConfig[new_can_buff].format = kFLEXCAN_FrameFormatStandard;
	mbConfig[new_can_buff].type = kFLEXCAN_FrameTypeData;
	mbConfig[new_can_buff].id = FLEXCAN_ID_STD(id);

	FLEXCAN_SetRxMbConfig(EXAMPLE_CAN, new_can_buff, &mbConfig[new_can_buff], true);

	canXfer[new_can_buff].mbIdx = (uint8_t)new_can_buff;
	canXfer[new_can_buff].frame = &canFrame[new_can_buff];

	rx_buffs[rx_buffs_num] = new_can_buff;
	rx_buffs_num += 1;
	new_can_buff += 1;
	return (new_can_buff - 1);
}

uint8_t set_Tx_Buffer(uint32_t id, uint8_t dlc){

	FLEXCAN_SetTxMbConfig(EXAMPLE_CAN, new_can_buff, true);

	canFrame[new_can_buff].format 	= (uint8_t)kFLEXCAN_FrameFormatStandard;
	canFrame[new_can_buff].type 	= (uint8_t)kFLEXCAN_FrameTypeData;
	canFrame[new_can_buff].id 		= FLEXCAN_ID_STD(id);
	canFrame[new_can_buff].length	= (uint8_t)dlc;

	canXfer[new_can_buff].mbIdx 	= (uint8_t)new_can_buff;
	canXfer[new_can_buff].frame 	= &canFrame[new_can_buff];

	tx_buffs[rx_buffs_num] = new_can_buff;
	tx_buffs_num += 1;
	new_can_buff += 1;

	return (new_can_buff - 1);
}

bool flexcan_Rx_Non_Blocking(uint8_t buffer, uint32_t *word0, uint32_t *word1){
	(void)FLEXCAN_TransferReceiveNonBlocking(EXAMPLE_CAN, &flexcanHandle, &canXfer[buffer]);
	if(rxComplete && last_rx == buffer){
		rxComplete = false;

		*word0 = canFrame[buffer].dataWord0;
		*word1 = canFrame[buffer].dataWord1;

		LOG_INFO("\r\nReceived message from ID %X\r\n", canFrame[buffer].id );
		LOG_INFO("rx word0 = 0x%x\r\n", canFrame[buffer].dataWord0);
		LOG_INFO("rx word1 = 0x%x\r\n", canFrame[buffer].dataWord1);

		return true;
	}
	else{
		return false;
	}
}

void flexcan_Rx_Blocking(uint8_t buffer, uint32_t *word0, uint32_t *word1){
	do{
		(void)FLEXCAN_TransferReceiveNonBlocking(EXAMPLE_CAN, &flexcanHandle, &canXfer[buffer]);
	}while(rxComplete == false || last_rx != buffer);
	rxComplete = false;

	*word0 = canFrame[buffer].dataWord0;
	*word1 = canFrame[buffer].dataWord1;

	LOG_INFO("\r\nReceived message from ID %X\r\n", canFrame[buffer].id );
	LOG_INFO("rx word0 = 0x%x\r\n", canFrame[buffer].dataWord0);
	LOG_INFO("rx word1 = 0x%x\r\n", canFrame[buffer].dataWord1);
}

bool flexcan_Tx(uint8_t buffer, uint32_t word0, uint32_t word1){
	if(!txComplete){
		canFrame[buffer].dataWord0 =	CAN_WORD0_DATA_BYTE_0(word0 & 0xFFU) |
										CAN_WORD0_DATA_BYTE_1((word0 >> 8U) & 0xFFU) |
										CAN_WORD0_DATA_BYTE_2((word0 >> 16U) & 0xFFU) |
										CAN_WORD0_DATA_BYTE_3((word0 >> 24U) & 0xFFU);

		canFrame[buffer].dataWord1 =	CAN_WORD0_DATA_BYTE_0(word1 & 0xFFU) |
										CAN_WORD0_DATA_BYTE_1((word1 >> 8U) & 0xFFU) |
										CAN_WORD0_DATA_BYTE_2((word1 >> 16U) & 0xFFU) |
										CAN_WORD0_DATA_BYTE_3((word1 >> 24U) & 0xFFU);

		(void)FLEXCAN_TransferSendNonBlocking(EXAMPLE_CAN, &flexcanHandle, &canXfer[buffer]);
		LOG_INFO("Send message to ID %X\r\n", canFrame[buffer].id);
		LOG_INFO("tx word0 = 0x%x\r\n", canFrame[buffer].dataWord0);
		LOG_INFO("tx word1 = 0x%x\r\n", canFrame[buffer].dataWord1);

		txComplete = false;
		return true;
	}
	else {
		return false;
	}
}

