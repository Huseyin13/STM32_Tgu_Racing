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
#include "math.h" //fabs fonksiyonu için

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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
ADC_HandleTypeDef hadc1; //ADC1 yapılandırması
ADC_HandleTypeDef hadc2; //ADC2 yapılandırması

CAN_HandleTypeDef hcan1; //CAN1 yapılandırması

UART_HandleTypeDef huart2; //USART2 yapılandırması

/* USER CODE BEGIN PV */
//CAN mesajı için değişkenler
CAN_TxHeaderTypeDef Txheader;
CAN_RxHeaderTypeDef Rxheader;

uint8_t TxData[8];
uint8_t RxData[8];
uint32_t TxMailbox;

// ADC okuma için değişkenler
uint16_t adc_value, adc_value_2 = 0;
float adc_voltage, adc_voltage_2 = 0;
float v_real1 = 0.0f, v_real2 = 0.0f;
float percent1 = 0.0f, percent2 = 0.0f;





void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    if(hcan == &hcan1)
    {
        HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &Rxheader, RxData);



    }
}

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_CAN1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Voltaj bölücü katsayısı yaklaşık 1.59
// ADC_Volt * SCALE = Gerçek_Volt
#define SCALE   1.59

// Pedal aralıkları (volt cinsinden)
#define APPS1_MIN   0.5
#define APPS1_MAX   4.5
#define APPS2_MIN   0.5
#define APPS2_MAX   4.5

// Implausibility / fault parametreleri
#define APPS_IMPLAUS_THRESHOLD  10.0f // Yüzde cinsinden
#define APPS_IMPLAUS_PERSIST_MS 100 // 100 ms şartı (kural)
#define SHORTCIRCUIT_VOLT_TOLERANCE 0.02f // Voltaj toleransı (volt cinsinden)
#define SHORTCIRCUIT_PERSIST_MS 100 // 100 ms şartı (kural)

// Status bits (TxData[6])
// bit0 = 1 => implausibility latched (100ms geçti)
// bit1 = 1 => sensor1 fault (out of range)
// bit2 = 1 => sensor2 fault (out of range)
// bit3 = 1 => short-circuit suspected
// diğer bitler rezerv

// Status bits ve fault latch için değişkenler
uint8_t status_bits = 0;// CAN mesajında gönderilecek status byte

//fault latch
uint8_t implaus_latched = 0; // APPS implausibility fault latch
uint32_t implaus_start_time = 0;
uint32_t shortcircuit_start_time = 0;

uint8_t shortc_suspected = 0;

// Yüzde hesaplama fonksiyonu
float apps_to_percent(float v_real, float v_min, float v_max){
	float percent = ((v_real - v_min) / (v_max - v_min)) * 100.0f;
	    if (percent < 0) percent = 0;
	    if (percent > 100) percent = 100;
	    return percent;
}

//sensör voltajının beklenen aralıkta olup olmadığını kontrol et
uint8_t is_voltage_in_range(float v, float v_min, float v_max, float tolerance){
	if(v < (v_min - tolerance) || v > (v_max + tolerance)){
		return 0; // out of range
	}
	else{
		return 1; // in range
	}
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
  MX_ADC1_Init();
  MX_CAN1_Init();
  MX_USART2_UART_Init();
  MX_ADC2_Init();
  /* USER CODE BEGIN 2 */
  HAL_CAN_Start(&hcan1);
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);

  //CAN mesajı header ayarları
  Txheader.DLC = 4;
  Txheader.StdId =0x111;
  Txheader.IDE = CAN_ID_STD;
  Txheader.RTR = CAN_RTR_DATA;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	  //ADC1'den veri okuma
	  HAL_ADC_Start(&hadc1);

	  if(HAL_ADC_PollForConversion(&hadc1, 1000) == HAL_OK){
	  adc_value = HAL_ADC_GetValue(&hadc1);
	  adc_voltage = adc_value*3.3/4095; //ADC->volt dönüşümü
	  v_real1 = adc_voltage * SCALE; //gerçek voltaj
	  percent1 = apps_to_percent(v_real1, APPS1_MIN, APPS1_MAX);
	  }
	  HAL_ADC_Stop(&hadc1);

	  //ADC2'den veri okuma
	  HAL_ADC_Start(&hadc2);

	  if(HAL_ADC_PollForConversion(&hadc2, 1000) == HAL_OK){
		  adc_value_2 = HAL_ADC_GetValue(&hadc2);
		  adc_voltage_2 = adc_value_2*3.3/4095;
		  v_real2 = adc_voltage_2 * SCALE;
		  percent2 = apps_to_percent(v_real2, APPS2_MIN, APPS2_MAX);
	  }
	  HAL_ADC_Stop(&hadc2);

	  status_bits = 0; // her döngüde status bitlerini sıfırla

	  //sensör voltaj aralığında mı kontrol et
	  float tolerance = 0.1f; // 100mV tolerans
	  if(!is_voltage_in_range(v_real1, APPS1_MIN, APPS1_MAX, tolerance)){
		  status_bits |= 0x02; // sensor1 fault
	  }
	  if(!is_voltage_in_range(v_real2, APPS2_MIN, APPS2_MAX, tolerance)){
		  status_bits |= 0x04; // sensor2 fault
	  }

	  // APPS implausibility
	  float diff = fabsf(percent1 - percent2);

	  uint32_t current_time = HAL_GetTick();
	  if(diff > APPS_IMPLAUS_THRESHOLD || (status_bits & 0x06) != 0){
		  // implausibility başladı
		  if(implaus_start_time == 0){
			  implaus_start_time = current_time;
		  }
		  //100 ms geçtiyse latchle
		  if(implaus_latched == 0 && (current_time - implaus_start_time) >= APPS_IMPLAUS_PERSIST_MS){
			  implaus_latched = 1; // latch the fault
			  status_bits |= 0x01; // implausibility latched
			  //motor controller'a CAN ile bildirim gönder (aşağıda Tx gönderiliyor)

		  }
	} else{
		  // implausibility yok
		  implaus_start_time = 0;
	  }
	  //Short-circuit benzeri durum tespiti
	  if(fabsf(v_real1 - v_real2) < SHORTCIRCUIT_VOLT_TOLERANCE){
		  // short-circuit benzeri durum başladı
		  if(shortcircuit_start_time == 0){
			  shortcircuit_start_time = current_time;
		  }
		  //100 ms geçtiyse şüpheli olarak işaretle
		  if(shortc_suspected == 0 && (current_time - shortcircuit_start_time) >= SHORTCIRCUIT_PERSIST_MS){
			  shortc_suspected = 1;
			  status_bits |= 0x08; // short-circuit suspected
			  //motor controller'a CAN ile bildirim gönder (aşağıda Tx gönderiliyor)

		  }
	  } else{
		  // short-circuit benzeri durum yok
		  shortcircuit_start_time = 0;
		  shortc_suspected = 0;
	  }
//	  Afully released check: eğer manuel moddaysa (kodda manual kontrol yok) fren/idle gereksinimi
//      Kural: tamamen bırakılmış pedal (0%) -> EV: wheel torque <= 0Nm.
//      Burada sadece yardımcı status gönderiyoruz; gerçek torque kesmeyi motor controller yapmalı.
	  if(percent1 < 1.0f && percent2 < 1.0f){
		  // tamamen bırakılmış
		  // ek bir status biti ayarlanabilir (örneğin bit4)
		  status_bits |= 0x10; // fully released
	  }

	  /*     CAN mesajı gönderimi
	         TxData[0] = percent1 (0-100)
	         TxData[1] = percent2
	         TxData[2..5] = raw ADCs (little endian)
	         TxData[6] = status byte
	         TxData[7] = reserved/checksum (opsiyonel)
	      */

	  TxData[0] = (uint8_t)(percent1 + 0.5f); // yuvarlama için 0.5 ekle
	  TxData[1] = (uint8_t)(percent2 + 0.5f);
	  TxData[2] = (uint8_t)(adc_value & 0xFF);
	  TxData[3] = (uint8_t)((adc_value >> 8) & 0xFF);
	  TxData[4] = (uint8_t)(adc_value_2 & 0xFF);
	  TxData[5] = (uint8_t)((adc_value_2 >> 8) & 0xFF);
	  TxData[6] = status_bits;
	  TxData[7] = 0; // future use / checksum


	  HAL_CAN_AddTxMessage(&hcan1, &Txheader, TxData, &TxMailbox);

	  HAL_Delay(10);




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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_56CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc2.Init.Resolution = ADC_RESOLUTION_12B;
  hadc2.Init.ScanConvMode = DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.NbrOfConversion = 1;
  hadc2.Init.DMAContinuousRequests = DISABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_56CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

}

/**
  * @brief CAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 14;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_9TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = DISABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = DISABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */
  CAN_FilterTypeDef canfilterconfig;

  canfilterconfig.FilterActivation = ENABLE;
  canfilterconfig.FilterBank = 0;
  canfilterconfig.FilterFIFOAssignment = CAN_FILTER_FIFO0;
  canfilterconfig.FilterIdHigh = 0x000;
  canfilterconfig.FilterIdLow = 0x0000;
  canfilterconfig.FilterMaskIdHigh = 0x000;
  canfilterconfig.FilterMaskIdLow = 0x0000;
  canfilterconfig.FilterMode = CAN_FILTERMODE_IDMASK;
  canfilterconfig.FilterScale = CAN_FILTERSCALE_32BIT;

  HAL_CAN_ConfigFilter(&hcan1, &canfilterconfig);


  /* USER CODE END CAN1_Init 2 */

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
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
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
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
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
