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
#include <math.h>

#include "iq.h"

#include "datetime/nmea.h"
#include "mixers/ifMix.h"
#include "detectors/am.h"
#include "detectors/arctg2.h"
#include "detectors/pll.h"
#include "detectors/phaseMod.h"

#include "filters/dcOffset.h"
#include "filters/agc.h"

#include "decoders/pcsk225.h"
#include "decoders/eczas.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

// ADC cycles per sample
#define RF_TICKS_PER_SAMPLE 32

// rf sample rate
// cpu freq / ADC cycles per sample -> ~2.25 MSPS
#define RF_SAMPLE_RATE 72000000 / RF_TICKS_PER_SAMPLE

#define INPUT_SAMPLES 8192
#define INPUT_HALF_SAMPLES INPUT_SAMPLES/2

#define RF_IF_DECIMATION_POW2	8	//=256

#define AF_SAMPLE_RATE (RF_SAMPLE_RATE>>RF_IF_DECIMATION_POW2)

#define AF_SAMPLES (INPUT_SAMPLES>>RF_IF_DECIMATION_POW2)
#define AF_HALF_SAMPLES AF_SAMPLES/2

// AF ticks per sample (RF ticks per sample*decimation ratio)
#define AF_TICKS_PER_SAMPLE (RF_TICKS_PER_SAMPLE<<RF_IF_DECIMATION_POW2)

#define OUTPUT_SAMPLES 128
#define OUTPUT_HALF_SAMPLES OUTPUT_SAMPLES/2

#define UART_BUFFER (AF_HALF_SAMPLES*4+2)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

DAC_HandleTypeDef hdac1;
DMA_HandleTypeDef hdma_dac1_ch1;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart2_tx;
DMA_HandleTypeDef hdma_usart3_tx;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_DAC1_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM4_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */

uint16_t dacOut[OUTPUT_SAMPLES] = {0};
uint16_t rf[INPUT_SAMPLES];

uint16_t *rfFrameBegin = rf;
uint16_t *rfFrameHalf = &rf[INPUT_HALF_SAMPLES];

// these variables are updated from ADC full/half complete interrupts
volatile size_t rfFrameUpdated = INPUT_SAMPLES;
volatile size_t afFrameToUpdate = 0;

size_t rfFrameLastUpdated = 0;

//AF I and Q
struct IQ af_IQ[AF_SAMPLES];
struct IQ *afFrameBegin = af_IQ;
struct IQ *afFrameHalf = &af_IQ[AF_HALF_SAMPLES];
struct IQ *afFrameEnd = &af_IQ[AF_SAMPLES];

struct IQ *afCurrentFrame = af_IQ;

struct IQ filteredIQ;

size_t dacOutPushed = 0;

uint8_t txData[UART_BUFFER] = {0};
size_t txDataPushed = 0;

#define DC_OFFSET_FILTER_WINDOW_ORDER 7		//128
uint16_t dcOffset;

#define AGC_FILTER_WINDOW_ORDER 8		//256
uint32_t agc;

// num of samples from sync bit peak occured to detected
// shared to phaseMod, but needed to calculate sync diff
const uint32_t phaseModSyncBitWidth = 100;

// num of samples per PCSK225 bit (calculated by phaseMod_init)
uint32_t samplesPerPcskBit = 0;

enum {
	IDLE,
	DECODE_PCSK225,
	DECODE_ECZAS
} decoderState = IDLE;

pcsk225Frame pcsk;
eCzasFrame eCzas;

// master timer (seconds from 2000), update from timer irq
volatile uint32_t mstTimer = 0;

// flag that tick happened (set from timer irq, reset by loop)
volatile uint32_t wasTick = 0;

// flag that was sync interrupt (set from sync timer irq, reset by loop)
volatile uint32_t wasSyncIRQ = 0;

// flag that locks resetting sync timer if started to receive sync frame
volatile uint32_t lockSyncTimerReset = 0;

// flag that occurs that we have recived legit eCzas frame and will be used to sync
volatile uint32_t eczasFrameRecvd = 0;

// offset between first bit and sync timer started in samples
volatile uint32_t syncTimerOffset = 0;

// sync status
typedef enum {
	INIT_NO_SYNC,
	SYNCED,
	SYNC_LOST
} syncState_t;

syncState_t syncState = INIT_NO_SYNC;
syncState_t prevSyncState = INIT_NO_SYNC;

// timestamp of last sync	(0 means no sync)
uint32_t lastSyncAt = 0;


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_ADC1_Init();
  MX_DAC1_Init();
  MX_TIM3_Init();
  MX_USART3_UART_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_TIM4_Init();
  MX_TIM2_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */

  HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)dacOut, OUTPUT_SAMPLES, DAC_ALIGN_12B_R);
  HAL_TIM_Base_Start(&htim4);

  HAL_ADC_Start_DMA(&hadc1,(uint32_t*)rf,INPUT_SAMPLES);
  HAL_TIM_Base_Start(&htim3);

  //init filters
  dcOffset_init(DC_OFFSET_FILTER_WINDOW_ORDER);
  agc_init(AGC_FILTER_WINDOW_ORDER);
  filteredIQ.i = 0;
  filteredIQ.q = 0;

  //init mixers
  ifMix_init(RF_SAMPLE_RATE,RF_IF_DECIMATION_POW2,&dcOffset);
  ifMix_setFreq(225000);

  //init detectors
  amDetector_init();
  pll_init(RF_SAMPLE_RATE,0);
  samplesPerPcskBit = phaseMod_init(AF_SAMPLE_RATE,50,phaseModSyncBitWidth);

  // init PPS timer
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 0);	// no PWM cause we are not synced yet
  __HAL_TIM_ENABLE_IT(&htim2, TIM_IT_UPDATE);

  HAL_TIM_Base_Start_IT(&htim1);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	  // check first if rfFrameUpdated does not overflow RF array
	  if (rfFrameUpdated < INPUT_SAMPLES && rfFrameUpdated != rfFrameLastUpdated) {

		  struct IQ *afBufferBegin = &af_IQ[afFrameToUpdate];
		  struct IQ *afBufferEnd = &af_IQ[afFrameToUpdate+AF_HALF_SAMPLES];

		  ifMix_Mix(&rf[rfFrameUpdated], &rf[rfFrameUpdated+INPUT_HALF_SAMPLES],afBufferBegin);

		  // update dcOffset
		  dcOffset = dcOffset_filter(*rfFrameBegin);

		  //update AGC
		  agc = agc_update(*rfFrameBegin-dcOffset);

		  txData[0] = 0xFF;
		  txData[1] = 0xFF;
		  uint8_t *txBuffer = txData+2;

		  struct IQ *afBuffer = afBufferBegin;
		  while(afBuffer < afBufferEnd) {

			  // Automatic gain control
			  int32_t I = afBuffer->i * agc;
			  int32_t Q = afBuffer->q * agc;

			  //filter IQ
			  filteredIQ.i += (I - filteredIQ.i) >> 4;
			  filteredIQ.q += (Q - filteredIQ.q) >> 4;

			  int32_t amSample;
			  amDetector_detectSample(&filteredIQ,&amSample);
			  dacOut[dacOutPushed] = (amSample >> 8) & 0xFFFF;
			  dacOutPushed++;
			  if(dacOutPushed >= OUTPUT_SAMPLES) dacOutPushed = 0;

			  uint32_t angle = IQarctg2(&filteredIQ);
			  uint32_t pll = 0;
			  uint32_t phaseLocked = pll_detect(angle,&pll);

			  if (phaseLocked) {
				  int32_t phaseMod = angle - pll;
				  phaseModReturn_t phaseModRet = (uint16_t)phaseMod_push(phaseMod);

				  if (phaseModRet == FOUND_SYNC_ZERO) {
					  // Calculate time diff
					  // samples from current sample to end + samples from middle of sync sample to sample which confirms it
					  syncTimerOffset = afBufferEnd-afBuffer + phaseModSyncBitWidth;

					  // lock timer resetting by ADC callbacks
					  lockSyncTimerReset = 1;
				  } else if (phaseModRet == FULL_FRAME_RECVD) {
					  //schedule decooding PCSK225 frame
					  decoderState = DECODE_PCSK225;
				  } else if (phaseModRet == FAIL_INVALID_PREAMBLE) {
					  // corrupted frame, reset timer
					  lockSyncTimerReset = 0;
				  }
			  } else {
				  // reset phaseMod internal state machine and Carrier LED
				  phaseMod_cancel();
			  }

			  // GNU radio output
			  uint16_t i_val = (uint16_t)((afBuffer->i >> 4) & 0xFFFF);
			  uint16_t q_val = (uint16_t)((afBuffer->q >> 4) & 0xFFFF);
			  if(i_val == 0xFFFF) i_val = 0xFFFE;
			  if(q_val == 0xFFFF) q_val = 0xFFFE;

			  txBuffer[0] = (i_val >> 8) & 0xFF;
			  txBuffer[1] = i_val & 0xFF;
			  txBuffer[2] = (q_val >> 8) & 0xFF;
			  txBuffer[3] = q_val & 0xFF;
			  txBuffer+=4;


			  afBuffer++;
		  }

		  //push compressed audio to UART
		  while (huart2.gState != HAL_UART_STATE_READY);
		  HAL_UART_Transmit_DMA(&huart2,txData,sizeof(txData));

		  //AF buffer hop and rollover
		  afCurrentFrame += AF_HALF_SAMPLES;
		  if(afCurrentFrame >= afFrameEnd) afCurrentFrame = afFrameBegin;

		  //IF buffer swap to wait from next update from ISR
		  rfFrameLastUpdated = rfFrameUpdated;

		  // do not do nothing more
		  continue;
	  }

	  if (decoderState == DECODE_PCSK225) {
		  size_t len = 0;
		  uint8_t *data = phaseMod_getBitstream(&len);

		  pcsk225_decode(data,len,&pcsk);
		  pcsk225_print(&huart1,&pcsk);

		  if (pcsk.header == 0x60) {
			  // calculate offset between first met header bit and real first header bit
			  size_t missingHeaderBits = 16-pcsk.preambleLen;
			  syncTimerOffset += missingHeaderBits*samplesPerPcskBit;

			  decoderState = DECODE_ECZAS;
		  } else {
			  //cancel timer lock
			  lockSyncTimerReset = 0;
			  decoderState = IDLE;
		  }
	  } else if (decoderState == DECODE_ECZAS) {
			  eCzas_decode(&pcsk,&eCzas);
			  // uncomment for debug info (but it may stuck waiting for UART free so
			  // eCzas_print(&huart1,&eCzas);

			  if (eCzas_verify(&eCzas)) {
				  // we have received legit frame
				  eczasFrameRecvd = 1;
			  } else {
				  //cancel timer lock
				  lockSyncTimerReset = 0;
			  }

			  decoderState = IDLE;
	  }

	  if (wasTick) {
		  wasTick = 0;

		  // send timestamp over NMEA interface (if synced only, severy 10 secs)
		  if (syncState == SYNCED && mstTimer % 10 == 0) {
			  // get secs/100 (PPS timer value/ticks per 1/100 sec)
			  uint32_t secCents = __HAL_TIM_GET_COUNTER(&htim2)/720000;
			  nmea_sendGPRMCFromTimestamp(&huart3,mstTimer,secCents);
		  }
	  } else if (wasSyncIRQ) {
		  wasSyncIRQ = 0;

		  // sync timers
		  if (eczasFrameRecvd) {
			  eczasFrameRecvd = 0;

			  // received frame describes the time which was
			  // 2 secs (timer period) + time diff ( <1s, loaded to PPS timer)
			  mstTimer = eCzas.counter*3 + 2;

			  // set sync flags
			  lastSyncAt = mstTimer;
			  syncState = SYNCED;
		  }

		  //unlock sync timer resets
		  lockSyncTimerReset = 0;
	  }

	  // Sync status state machine
	  if (syncState == INIT_NO_SYNC && mstTimer > 180) {
		  // not synced from 3 minutes
		  syncState = SYNC_LOST;
	  } else if (syncState == SYNCED) {
		  int32_t timeFromLastSync = mstTimer - lastSyncAt;
		  if (timeFromLastSync > 3600) {
			  //was synced but no sync for 1 hour
			  syncState = SYNC_LOST;
		  }
	  }

	  if (prevSyncState != syncState) {

		  // toggle PPS output
		  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2,
				  syncState == SYNCED ? 3600000 : 0
		  );

		  // deal with ERROR output
		  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, (syncState == SYNC_LOST));

		  prevSyncState = syncState;
	  }

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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1|RCC_PERIPHCLK_USART2
                              |RCC_PERIPHCLK_USART3|RCC_PERIPHCLK_I2C1
                              |RCC_PERIPHCLK_TIM1|RCC_PERIPHCLK_TIM2
                              |RCC_PERIPHCLK_TIM34;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  PeriphClkInit.Usart3ClockSelection = RCC_USART3CLKSOURCE_PCLK1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
  PeriphClkInit.Tim1ClockSelection = RCC_TIM1CLK_HCLK;
  PeriphClkInit.Tim2ClockSelection = RCC_TIM2CLK_HCLK;
  PeriphClkInit.Tim34ClockSelection = RCC_TIM34CLK_HCLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
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

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.SamplingTime = ADC_SAMPLETIME_19CYCLES_5;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief DAC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC1_Init(void)
{

  /* USER CODE BEGIN DAC1_Init 0 */

  /* USER CODE END DAC1_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC1_Init 1 */

  /* USER CODE END DAC1_Init 1 */

  /** DAC Initialization
  */
  hdac1.Instance = DAC1;
  if (HAL_DAC_Init(&hdac1) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT1 config
  */
  sConfig.DAC_Trigger = DAC_TRIGGER_T4_TRGO;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC1_Init 2 */

  /* USER CODE END DAC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00201D2B;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 7999;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 18000;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 71999999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 31;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 0;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 8191;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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
  huart2.Init.BaudRate = 921600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
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
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 38400;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
  /* DMA1_Channel3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
  /* DMA1_Channel4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
  /* DMA1_Channel7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LD2_Pin|GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD2_Pin PA6 PA7 */
  GPIO_InitStruct.Pin = LD2_Pin|GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PC7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PB6 */
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

// when half or full buffer

// Called when buffer is completely filled
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc) {
	// reset sync timer until not locked while time frame received
	if (!lockSyncTimerReset) __HAL_TIM_SET_COUNTER(&htim1, 0);

	rfFrameUpdated = 0;
	afFrameToUpdate = 0;

}
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
	// reset sync timer until not locked while time frame received
	if (!lockSyncTimerReset) __HAL_TIM_SET_COUNTER(&htim1, 0);

	rfFrameUpdated = INPUT_HALF_SAMPLES;
	afFrameToUpdate = AF_HALF_SAMPLES;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM2) {
		// 1PPS timer
		mstTimer++;
		wasTick = 1;
	} else if (htim->Instance == TIM1 ) {
		// sync timer
		// now we are 2 secs + 8000 ticks (interrupt period) + syncTimerOffset samples from sync time.
		// so we add 2 seconds to timestamp received from eCzas frame (done in main loop)
		// also we need to push PPS timer by some extra time forward, which happens here
		__HAL_TIM_SET_COUNTER(&htim2, 8000+syncTimerOffset*AF_TICKS_PER_SAMPLE);

		wasSyncIRQ = 1;
	}

}
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
