/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ina226.h"

#include <stdbool.h>
#include <string.h>

#define BUFF_SIZE 256

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c3;

RTC_HandleTypeDef hrtc;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

unsigned char rxBuff1[BUFF_SIZE];
unsigned char rxBuff2[BUFF_SIZE];
unsigned char lastMessage[BUFF_SIZE];
bool useBuff1 = true;
unsigned char rxBuffReceived[BUFF_SIZE];
bool waitingForReceiving = false;
volatile unsigned int receivingSize = sizeof(rxBuffReceived);
const char *newline = "\r\n";

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

static void MX_GPIO_Init(void);

static void MX_RTC_Init(void);

static void MX_I2C3_Init(void);

static void MX_USART2_UART_Init(void);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


void RampBlink(bool reverse) {
    if (reverse) {
        for (int i = 0; i < 75; i++) {
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
            float v = (float) i / 75.0f;
            float delay = v * v * v * 75.0f;
            HAL_Delay((int) delay);
        }
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
        return;
    }
    for (int i = 75; i > 0; i--) {
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        float v = (float) i / 75.0f;
        float delay = v * v * v * 75.0f;
        HAL_Delay((int) delay);
    }
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
}

//_Noreturn void Shutdown() {
//    // Shutdown blink
//    RampBlink(true);
//
//    /* Select Standby mode */
//    SET_BIT(PWR->CR, PWR_CR_PDDS);
//
//    /* Set SLEEPDEEP bit of Cortex System Control Register */
//    SET_BIT(SCB->SCR, SCB_SCR_SLEEPDEEP_Msk);
//
//    /* This option is used to ensure that store operations are completed */
//#if defined ( __CC_ARM)
//    __force_stores();
//#endif
//
//    // Enter low-power mode
//    for (;;) {
//        __DSB();
//        __WFI();
//    }
//}

void enableHighPower() {
    HAL_GPIO_WritePin(HIGH_POWER_SINK_GPIO_Port, HIGH_POWER_SINK_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(HIGH_POWER_GPIO_Port, HIGH_POWER_Pin, GPIO_PIN_RESET);
}

void disableHighPower() {
    HAL_GPIO_WritePin(HIGH_POWER_SINK_GPIO_Port, HIGH_POWER_SINK_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(HIGH_POWER_GPIO_Port, HIGH_POWER_Pin, GPIO_PIN_SET);
}

void HAL_UARTEx_RxEventCallback(__attribute__((unused)) UART_HandleTypeDef *huart, uint16_t size) {
    if (!waitingForReceiving) {
        return;
    }
    unsigned char *buffer = useBuff1 ? rxBuff1 : rxBuff2;
    memcpy(&buffer, rxBuffReceived, size);
    buffer[size] = '\0';
    waitingForReceiving = false;
    useBuff1 = !useBuff1;
}


bool send(const char *toSend) {
    HAL_UARTEx_ReceiveToIdle_IT(&huart2, rxBuffReceived, receivingSize);
    waitingForReceiving = true;

    char sendBuff[BUFF_SIZE];
    strcat(sendBuff, toSend);
    strcat(sendBuff, newline);
    HAL_StatusTypeDef status = HAL_UART_Transmit(&huart2,
                                                 (uint8_t *) sendBuff,
                                                 strlen(sendBuff) - 1, // -1 to remove null terminator
                                                 150);
    if (status != HAL_OK) {
        Error_Handler();
    }

    int i = 0;
    while (waitingForReceiving && i < 100) {
        HAL_Delay(100);
        i++;
    }
    if (waitingForReceiving) {
        // Timeout
        return false;
    }
    unsigned char *backBuffer = useBuff1 ? rxBuff2 : rxBuff1;
    memcpy(lastMessage, backBuffer, strlen((char *) backBuffer));

    if (strstr((char *) backBuffer, "OK\r\n")) {
        return true;
    }
    return false;
}


void sendData(double voltage) {

    HAL_GPIO_WritePin(SIM_POWERKEY_GPIO_Port, SIM_POWERKEY_Pin, GPIO_PIN_SET);
    HAL_Delay(500);
    HAL_GPIO_WritePin(SIM_POWERKEY_GPIO_Port, SIM_POWERKEY_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SIM_DTR_GPIO_Port, SIM_DTR_Pin, GPIO_PIN_RESET);
    HAL_Delay(600);

    // Send OK until we are getting OK back
    const char at[] = "\r\n";

    int retries = 10;
    bool ok = false;
    while (!ok && retries > 0) {
        ok = send("AT");
        retries--;
    }

    if (retries == 0) {
        Error_Handler();
    }

    // Disable echo
    if (!send("ATE0")) {
        Error_Handler();
    }

    // Set error handling on
    if (!send("AT+CMEE=2")) {
        Error_Handler();
    }

    retries = 10;
    while (!strstr((char *) lastMessage, "+CPIN: READY") && retries > 0) {
        if (!send("AT+CPIN?")) {
            Error_Handler();
        }
        retries--;
    }

    if (retries == 0) {
        Error_Handler();
    }

    if (!send("AT+CNMP=13")) {
        Error_Handler();
    }

    if (!send("AT+CMNB=2")) {
        Error_Handler();
    }

    retries = 10;
    while (!strstr((char *) lastMessage, "+CGREG: 0,1") && retries > 0) {
        if (!send("AT+CGREG?")) {
            Error_Handler();
        }
        retries--;
    }

    if (!send("AT+CGDCONT=1,\"IP\",\"internet.tele2.se\"")) {
        Error_Handler();
    }

    if (!send("AT+CNCFG=0,1,\"internet.tele2.se\"")) {
        Error_Handler();
    }

    if (!send("AT+CNACT=0,1")) {
        Error_Handler();
    }



//    enum RegStatus {
//        REG_NO_RESULT    = -1,
//        REG_UNREGISTERED = 0,
//        REG_SEARCHING    = 2,
//        REG_DENIED       = 3,
//        REG_OK_HOME      = 1,
//        REG_OK_ROAMING   = 5,
//        REG_UNKNOWN      = 4,
//    };

//    RegStatus getRegistrationStatus() {
//        RegStatus epsStatus =
//                (RegStatus)thisModem().getRegistrationStatusXREG("CEREG");
//        // If we're connected on EPS, great!
//        if (epsStatus == REG_OK_HOME || epsStatus == REG_OK_ROAMING) {
//            return epsStatus;
//        } else {
//            // Otherwise, check GPRS network status
//            // We could be using GPRS fall-back or the board could be being moody
//            return (RegStatus)thisModem().getRegistrationStatusXREG("CGREG");
//        }
//    }

//    int8_t getRegistrationStatusXREG(const char* regCommand) {
//        thisModem().sendAT('+', regCommand, '?');
//        // check for any of the three for simplicity
//        int8_t resp = thisModem().waitResponse(GF("+CREG:"), GF("+CGREG:"),
//                                               GF("+CEREG:"));
//        if (resp != 1 && resp != 2 && resp != 3) { return -1; }
//        thisModem().streamSkipUntil(','); /* Skip format (0) */
//        int status = thisModem().stream.parseInt();
//        thisModem().waitResponse();
//        return status;
//    }


}

/* USER CODE END 0 */
/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void) {

    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_RTC_Init();
    MX_I2C3_Init();
    MX_USART2_UART_Init();
    /* USER CODE BEGIN 2 */
    /* Check and handle if the system was resumed from StandBy mode */
    if (__HAL_PWR_GET_FLAG(PWR_FLAG_SB) != RESET) {
        /* Clear Standby flag */
        __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);
    }
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);


    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1) {
        RampBlink(false);

        INA226_wakeUpAndSmellTheVoltage(&hi2c3);
        HAL_Delay(1000);

        // fetch the voltage
        double voltage;
        if (INA226_getBusV(&hi2c3, &voltage) != HAL_OK) {
            Error_Handler();
        }
        INA226_standBy(&hi2c3);
        enableHighPower();

        sendData(voltage);

        disableHighPower();

        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
        HAL_Delay(6000);
        //Shutdown();
    }
    /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    /** Configure the main internal regulator output voltage
    */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /** Initializes the RCC Oscillators according to the specified parameters
    * in the RCC_OscInitTypeDef structure.
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
    RCC_OscInitStruct.MSIState = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = 0;
    RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_5;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
        Error_Handler();
    }
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2 | RCC_PERIPHCLK_I2C3
                                         | RCC_PERIPHCLK_RTC;
    PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
    PeriphClkInit.I2c3ClockSelection = RCC_I2C3CLKSOURCE_PCLK1;
    PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
        Error_Handler();
    }
}

/**
  * @brief I2C3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C3_Init(void) {

    /* USER CODE BEGIN I2C3_Init 0 */

    /* USER CODE END I2C3_Init 0 */

    /* USER CODE BEGIN I2C3_Init 1 */

    /* USER CODE END I2C3_Init 1 */
    hi2c3.Instance = I2C3;
    hi2c3.Init.Timing = 0x00000708;
    hi2c3.Init.OwnAddress1 = 0;
    hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c3.Init.OwnAddress2 = 0;
    hi2c3.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c3) != HAL_OK) {
        Error_Handler();
    }

    /** Configure Analogue filter
    */
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
        Error_Handler();
    }

    /** Configure Digital filter
    */
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK) {
        Error_Handler();
    }
    /* USER CODE BEGIN I2C3_Init 2 */

    /* USER CODE END I2C3_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void) {

    /* USER CODE BEGIN RTC_Init 0 */

    /* USER CODE END RTC_Init 0 */

    /* USER CODE BEGIN RTC_Init 1 */

    /* USER CODE END RTC_Init 1 */

    /** Initialize RTC Only
    */
    hrtc.Instance = RTC;
    hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv = 127;
    hrtc.Init.SynchPrediv = 255;
    hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
    if (HAL_RTC_Init(&hrtc) != HAL_OK) {
        Error_Handler();
    }

    /** Enable the WakeUp
    */
    if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 60, RTC_WAKEUPCLOCK_CK_SPRE_16BITS) != HAL_OK) {
        Error_Handler();
    }
    /* USER CODE BEGIN RTC_Init 2 */

    /* USER CODE END RTC_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void) {

    /* USER CODE BEGIN USART2_Init 0 */

    /* USER CODE END USART2_Init 0 */

    /* USER CODE BEGIN USART2_Init 1 */

    /* USER CODE END USART2_Init 1 */
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_SWAP_INIT;
    huart2.AdvancedInit.Swap = UART_ADVFEATURE_SWAP_ENABLE;
    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }
    /* USER CODE BEGIN USART2_Init 2 */

    /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOC, HIGH_POWER_SINK_Pin | HIGH_POWER_Pin | LED_Pin, GPIO_PIN_SET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOC, SIM_POWERKEY_Pin | SIM_DTR_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pins : PC13 PC14 PC15 PC4
                             PC5 PC6 PC10 PC11
                             PC12 */
    GPIO_InitStruct.Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15 | GPIO_PIN_4
                          | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_10 | GPIO_PIN_11
                          | GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /*Configure GPIO pins : PH0 PH1 */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

    /*Configure GPIO pins : HIGH_POWER_SINK_Pin HIGH_POWER_Pin */
    GPIO_InitStruct.Pin = HIGH_POWER_SINK_Pin | HIGH_POWER_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /*Configure GPIO pins : PA0 PA1 PA4 PA5
                             PA6 PA7 PA8 PA9
                             PA10 PA11 PA12 PA15 */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5
                          | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9
                          | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*Configure GPIO pins : PB0 PB1 PB2 PB10
                             PB11 PB12 PB13 PB14
                             PB15 PB3 PB4 PB5
                             PB6 PB7 PB8 PB9 */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_10
                          | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14
                          | GPIO_PIN_15 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5
                          | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /*Configure GPIO pins : SIM_POWERKEY_Pin SIM_DTR_Pin */
    GPIO_InitStruct.Pin = SIM_POWERKEY_Pin | SIM_DTR_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /*Configure GPIO pin : LED_Pin */
    GPIO_InitStruct.Pin = LED_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pin : PD2 */
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
_Noreturn void Error_Handler(void) {
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1) {
        HAL_Delay(250);
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    }
    /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
