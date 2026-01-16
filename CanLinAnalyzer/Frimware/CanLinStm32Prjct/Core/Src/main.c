/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "string.h"
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
CAN_HandleTypeDef hcan;

UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart2_tx;

/* USER CODE BEGIN PV */
CAN_FilterTypeDef       sFilterConfig;
CAN_TxHeaderTypeDef     CanTxHeader;
CAN_RxHeaderTypeDef     CanRxHeader;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_CAN_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define PACKET_SIZE 	16
#define RX_BUFFER_SIZE 	32 // 2x Packet Size
#define TX_BUFFER_SIZE 	16
uint8_t 			RxData_Processing[PACKET_SIZE];
uint8_t 			RxData[RX_BUFFER_SIZE];
uint8_t				TxData[TX_BUFFER_SIZE];
uint8_t				TxCpltFull;
uint16_t 			CanErrorCountTx;
uint16_t 			CanErrorCountRx;
uint8_t 			CanTxData[8];
uint8_t 			CanRxData[8];
uint32_t 			CanTxMailbox;
volatile uint8_t 	data_received_flag = 0;


void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART2)
    {
        // İlk yarıyı güvenli bir yere kopyala
        memcpy(RxData_Processing, &RxData[0], PACKET_SIZE);
        data_received_flag = 1;
    }
}

// Tam Doldu (32. byte geldiğinde tetiklenir ve başa döner)
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART2)
    {
        // İkinci yarıyı güvenli bir yere kopyala
        memcpy(RxData_Processing, &RxData[PACKET_SIZE], PACKET_SIZE);
        data_received_flag = 1;
    }
}


void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART2)
    {
        __HAL_UART_CLEAR_OREFLAG(huart);
        __HAL_UART_CLEAR_NEFLAG(huart);
        __HAL_UART_CLEAR_FEFLAG(huart);


        HAL_UART_Receive_DMA(huart, RxData, 16);
    }
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    //CAN Mesajını Al
    HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &CanRxHeader, CanRxData);


    //UART Meşgul mü
    if (huart2.gState != HAL_UART_STATE_READY)
    {
        return;
    }

    // BYTE 0: Header, seri porttan gelen veri doğru mu diye uygulama kontrol edecek.
    TxData[0] = 0xAA;

    // BYTE 1: ID Type
    uint32_t tempID;
    if (CanRxHeader.IDE == CAN_ID_EXT)
    {
        TxData[1] = 0x01;        // EXT
        tempID = CanRxHeader.ExtId;
    }
    else
    {
        TxData[1] = 0x00;        // STD
        tempID = CanRxHeader.StdId;
    }

    // BYTE 2-5: ID
    TxData[2] = (tempID >> 24) & 0xFF;
    TxData[3] = (tempID >> 16) & 0xFF;
    TxData[4] = (tempID >> 8)  & 0xFF;
    TxData[5] = (tempID & 0xFF);

    // BYTE 6: DLC
    TxData[6] = CanRxHeader.DLC;

    // BYTE 7-14: Data Payload
    // döngü sabit dlc dışındakiler 0 olacak
    for (uint8_t i = 0; i < 8; i++)
    {
        if (i < CanRxHeader.DLC)
        {
            TxData[7 + i] = CanRxData[i];
        }
        else
        {
            TxData[7 + i] = 0x00; // Kullanılmayan byte'ları temizle
        }
    }

    // BYTE 15: Footer, seri porttan gelen veri doğru mu diye uygulama kontrol edecek.
    TxData[15] = 0x55;
    HAL_UART_Transmit_DMA(&huart2, TxData, 16);
}

void Send_CAN_Message_From_Buffer()
{

    // ID Tipi (Byte 1)
    // 0x01: Extended, 0x00: Standard
    if (RxData_Processing[1] == 0x01)
    {
        CanTxHeader.IDE = CAN_ID_EXT;
        // Big Endian ID Birleştirme (Byte 2,3,4,5)
        CanTxHeader.ExtId = (RxData_Processing[2] << 24) | (RxData_Processing[3] << 16) | (RxData_Processing[4] << 8) | RxData_Processing[5];
        CanTxHeader.StdId = 0;
    }
    else
    {
        CanTxHeader.IDE = CAN_ID_STD;
        // Standart ID 11-bit olduğu için 32-bitlik verinin son 11 biti
        CanTxHeader.StdId = (RxData_Processing[2] << 24) | (RxData_Processing[3] << 16) | (RxData_Processing[4] << 8) | RxData_Processing[5];
        CanTxHeader.ExtId = 0;
    }

    // DLC (Byte 6)
    CanTxHeader.DLC = RxData_Processing[6];
    if(CanTxHeader.DLC > 8) CanTxHeader.DLC = 8; // Güvenlik limiti

    // RTR ve TimeStamp Ayarları
    CanTxHeader.RTR = CAN_RTR_DATA;
    CanTxHeader.TransmitGlobalTime = DISABLE;

    // Veriyi Kopyala (Byte 7 den itibaren)
    for (int i = 0; i < CanTxHeader.DLC; i++)
    {
        CanTxData[i] = RxData_Processing[7 + i];
    }

    // CAN Hattı Müsait mi? (Mailbox Kontrolü)
   HAL_CAN_AddTxMessage(&hcan, &CanTxHeader, CanTxData, &CanTxMailbox);

}

void Set_CAN_BaudRate(uint8_t index)
{
    // Mevcut hızı tut
    static uint8_t current_baud_index = 0;

    // Eğer aynı hız geldiyse işlem yapma
    if (current_baud_index == index) return;

    // 1. CAN Modülünü Durdur
    HAL_CAN_Stop(&hcan);

    // Prescaler Değerini Belirle
    uint32_t prescaler = 0;

    switch (index)
    {
        case 0: // 125 kbps
            prescaler = 24;
            break;
        case 1: // 250 kbps
            prescaler = 12;
            break;
        case 2: // 500 kbps
            prescaler = 6;
            break;
        case 3: // 1 Mbps
            prescaler = 3;
            break;
        default:
            // Hatalı index gelirse varsayılan 500k
            prescaler = 6;
            break;
    }

    hcan.Init.Prescaler = prescaler;

    // 4. Yeniden Init Et
    if (HAL_CAN_Init(&hcan) != HAL_OK)
    {
        // Hata Yönetimi (Örn: LED Yak)
        Error_Handler();
    }

    // 5. CAN'ı Tekrar Başlat
    if (HAL_CAN_Start(&hcan) != HAL_OK)
    {
        // Hata Yönetimi
        Error_Handler();
    }

    // Yeni hızı kaydet
    current_baud_index = index;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

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
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_CAN_Init();
  /* USER CODE BEGIN 2 */

  HAL_UART_Receive_DMA(&huart2, RxData, RX_BUFFER_SIZE);
  HAL_UART_Transmit_DMA(&huart2, TxData, 16);
  HAL_CAN_Start(&hcan);
  HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	 if(data_received_flag == 1)
	 {
		 if(RxData_Processing[15] == 0x55)
		 {
			 Send_CAN_Message_From_Buffer();
		 }

	      data_received_flag = 0; // Flag'i sıfırla
	 }

	 Set_CAN_BaudRate(RxData_Processing[0]);


    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI48;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CAN Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN_Init(void)
{

  /* USER CODE BEGIN CAN_Init 0 */

  /* USER CODE END CAN_Init 0 */

  /* USER CODE BEGIN CAN_Init 1 */

  /* USER CODE END CAN_Init 1 */
  hcan.Instance = CAN;
  hcan.Init.Prescaler = 24;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_11TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_4TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = ENABLE;
  hcan.Init.AutoWakeUp = ENABLE;
  hcan.Init.AutoRetransmission = ENABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN_Init 2 */

  CanTxHeader.RTR 	= 	CAN_RTR_DATA;
  CanTxHeader.DLC 	= 	8;

  sFilterConfig.FilterBank 		= 	0;
  sFilterConfig.FilterMode 		= 	CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale 	= 	CAN_FILTERSCALE_32BIT;

  sFilterConfig.FilterIdHigh 		= 	0x0000;
  sFilterConfig.FilterIdLow 		= 	0x0000;
  sFilterConfig.FilterMaskIdHigh 	= 	0x0000;
  sFilterConfig.FilterMaskIdLow 	= 	0x0000;

  sFilterConfig.FilterFIFOAssignment 	= CAN_RX_FIFO0;
  sFilterConfig.FilterActivation 		= ENABLE;
  sFilterConfig.SlaveStartFilterBank 	= 14;

    if (HAL_CAN_ConfigFilter(&hcan, &sFilterConfig) != HAL_OK)
      {
        Error_Handler();
      }

  /* USER CODE END CAN_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 1500000;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_8;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel4_5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel4_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_5_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
