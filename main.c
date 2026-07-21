/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "app_touchgfx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "Components/ili9341/ili9341.h"
#include "radar_app.h"
#include "hcsr04.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define REFRESH_COUNT           ((uint32_t)1386)   /* SDRAM refresh counter */
#define HCSR04_PERIOD_MS        100U
#define HCSR04_TIMEOUT_MS       30U
#define SDRAM_TIMEOUT           ((uint32_t)0xFFFF)

/**
  * @brief  FMC SDRAM Mode definition register defines
  */
#define SDRAM_MODEREG_BURST_LENGTH_1             ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_LENGTH_2             ((uint16_t)0x0001)
#define SDRAM_MODEREG_BURST_LENGTH_4             ((uint16_t)0x0002)
#define SDRAM_MODEREG_BURST_LENGTH_8             ((uint16_t)0x0004)
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL      ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_TYPE_INTERLEAVED     ((uint16_t)0x0008)
#define SDRAM_MODEREG_CAS_LATENCY_2              ((uint16_t)0x0020)
#define SDRAM_MODEREG_CAS_LATENCY_3              ((uint16_t)0x0030)
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD    ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_PROGRAMMED ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE     ((uint16_t)0x0200)

#define I2C3_TIMEOUT_MAX                    0x3000 /*<! The value of the maximal timeout for I2C waiting loops */
#define SPI5_TIMEOUT_MAX                    0x1000

/* --- Servo MG90S PWM control (calibrated for full 180 sweep) --- */
#define SERVO_MIN_PULSE_US       550U
#define SERVO_CENTER_PULSE_US    1500U
#define SERVO_MAX_PULSE_US       2450U
#define SERVO_MIN_ANGLE_DEG      0U
#define SERVO_MAX_ANGLE_DEG      180U

#if SERVO_MIN_PULSE_US >= SERVO_CENTER_PULSE_US
#error "SERVO_MIN_PULSE_US must be less than SERVO_CENTER_PULSE_US"
#endif

#if SERVO_CENTER_PULSE_US >= SERVO_MAX_PULSE_US
#error "SERVO_CENTER_PULSE_US must be less than SERVO_MAX_PULSE_US"
#endif

#if SERVO_MAX_PULSE_US > 20000U
#error "SERVO_MAX_PULSE_US must be within TIM4 PWM period (20ms)"
#endif

#define SERVO_ENABLE_BRINGUP_TEST    1U
#define SERVO_TEST_HOLD_MS           2000U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CRC_HandleTypeDef hcrc;

DMA2D_HandleTypeDef hdma2d;

I2C_HandleTypeDef hi2c3;

LTDC_HandleTypeDef hltdc;

SPI_HandleTypeDef hspi5;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim4;

SDRAM_HandleTypeDef hsdram1;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for GUI_Task */
osThreadId_t GUI_TaskHandle;
const osThreadAttr_t GUI_Task_attributes = {
  .name = "GUI_Task",
  .stack_size = 8192 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE BEGIN PV */
uint8_t isRevD = 0; /* Applicable only for STM32F429I DISCOVERY REVD and above */

osMessageQueueId_t radarQueueHandle = NULL;

volatile uint16_t g_radar_angle_deg = 0;
volatile uint16_t g_radar_distance_mm = 0;
volatile uint8_t g_radar_valid = 0;
volatile uint32_t g_radar_samples_sent = 0;

#if RADAR_CALIBRATION_MODE == 1U
volatile RadarCalibrationStats g_radarCalibrationStats;
#endif

typedef enum {
    RADAR_SCAN_MOVE_SERVO = 0,
    RADAR_SCAN_WAIT_SERVO,
    RADAR_SCAN_START_MEASUREMENT,
    RADAR_SCAN_WAIT_MEASUREMENT,
    RADAR_SCAN_PUBLISH_SAMPLE,
    RADAR_SCAN_NEXT_ANGLE
} RadarScanState;

volatile RadarControlState g_radarControlState = { .state = { .appMode = RADAR_APP_RUNNING, .speedMode = RADAR_SPEED_FAST } };

typedef enum {
    B1_RELEASED = 0,
    B1_DEBOUNCE_PRESS,
    B1_PRESSED,
    B1_LONG_REPORTED,
    B1_DEBOUNCE_RELEASE
} B1_State;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CRC_Init(void);
static void MX_I2C3_Init(void);
static void MX_SPI5_Init(void);
static void MX_FMC_Init(void);
static void MX_LTDC_Init(void);
static void MX_DMA2D_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM4_Init(void);
void StartDefaultTask(void *argument);
extern void TouchGFX_Task(void *argument);

/* USER CODE BEGIN PFP */
static void BSP_SDRAM_Initialization_Sequence(SDRAM_HandleTypeDef *hsdram, FMC_SDRAM_CommandTypeDef *Command);



static uint8_t            I2C3_ReadData(uint8_t Addr, uint8_t Reg);
static void               I2C3_WriteData(uint8_t Addr, uint8_t Reg, uint8_t Value);
static uint8_t            I2C3_ReadBuffer(uint8_t Addr, uint8_t Reg, uint8_t *pBuffer, uint16_t Length);

/* SPIx bus function */
static void               SPI5_Write(uint16_t Value);
static uint32_t           SPI5_Read(uint8_t ReadSize);
static void               SPI5_Error(void);

/* Link function for LCD peripheral */
void                      LCD_IO_Init(void);
void                      LCD_IO_WriteData(uint16_t RegValue);
void                      LCD_IO_WriteReg(uint8_t Reg);
uint32_t                  LCD_IO_ReadData(uint16_t RegValue, uint8_t ReadSize);
void                      LCD_Delay(uint32_t delay);

/* IOExpander IO functions */
void                      IOE_Init(void);
void                      IOE_ITConfig(void);
void                      IOE_Delay(uint32_t Delay);
void                      IOE_Write(uint8_t Addr, uint8_t Reg, uint8_t Value);
uint8_t                   IOE_Read(uint8_t Addr, uint8_t Reg);
uint16_t                  IOE_ReadMultiple(uint8_t Addr, uint8_t Reg, uint8_t *pBuffer, uint16_t Length);

/* Servo MG90S control functions */
static void Servo_SetPulseUs(uint16_t pulse_us);
static void Servo_SetAngleDeg(uint16_t angle_deg);
#if 0
static void Servo_RunBringupTest(void);
#endif

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#include <stdbool.h>

static LCD_DrvTypeDef* LcdDrv;


uint32_t I2c3Timeout = I2C3_TIMEOUT_MAX; /*<! Value of Timeout when I2C communication fails */
uint32_t Spi5Timeout = SPI5_TIMEOUT_MAX; /*<! Value of Timeout when SPI communication fails */
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
  MX_CRC_Init();
  MX_I2C3_Init();
  MX_SPI5_Init();
  MX_FMC_Init();
  MX_LTDC_Init();
  MX_DMA2D_Init();
  MX_TIM2_Init();
  MX_TIM4_Init();
  MX_TouchGFX_Init();
  /* Call PreOsInit function */
  MX_TouchGFX_PreOSInit();
  /* USER CODE BEGIN 2 */
  /* Load compare register with 1500 µs (90 deg) BEFORE enabling output,
     because sConfigOC.Pulse was generated as 0 by CubeMX.               */
  Servo_SetAngleDeg(90U);

  if (HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  radarQueueHandle = osMessageQueueNew(RADAR_QUEUE_LENGTH, sizeof(RadarSample), NULL);
  if (radarQueueHandle == NULL)
  {
      Error_Handler();
  }
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of GUI_Task */
  GUI_TaskHandle = osThreadNew(TouchGFX_Task, NULL, &GUI_Task_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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
  RCC_OscInitStruct.PLL.PLLN = 360;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
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
  * @brief CRC Initialization Function
  * @param None
  * @retval None
  */
static void MX_CRC_Init(void)
{

  /* USER CODE BEGIN CRC_Init 0 */

  /* USER CODE END CRC_Init 0 */

  /* USER CODE BEGIN CRC_Init 1 */

  /* USER CODE END CRC_Init 1 */
  hcrc.Instance = CRC;
  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CRC_Init 2 */

  /* USER CODE END CRC_Init 2 */

}

/**
  * @brief DMA2D Initialization Function
  * @param None
  * @retval None
  */
static void MX_DMA2D_Init(void)
{

  /* USER CODE BEGIN DMA2D_Init 0 */

  /* USER CODE END DMA2D_Init 0 */

  /* USER CODE BEGIN DMA2D_Init 1 */

  /* USER CODE END DMA2D_Init 1 */
  hdma2d.Instance = DMA2D;
  hdma2d.Init.Mode = DMA2D_M2M;
  hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
  hdma2d.Init.OutputOffset = 0;
  hdma2d.LayerCfg[1].InputOffset = 0;
  hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_RGB565;
  hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
  hdma2d.LayerCfg[1].InputAlpha = 0;
  if (HAL_DMA2D_Init(&hdma2d) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_DMA2D_ConfigLayer(&hdma2d, 1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DMA2D_Init 2 */

  /* USER CODE END DMA2D_Init 2 */

}

/**
  * @brief I2C3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C3_Init(void)
{

  /* USER CODE BEGIN I2C3_Init 0 */

  /* USER CODE END I2C3_Init 0 */

  /* USER CODE BEGIN I2C3_Init 1 */

  /* USER CODE END I2C3_Init 1 */
  hi2c3.Instance = I2C3;
  hi2c3.Init.ClockSpeed = 100000;
  hi2c3.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_DISABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C3_Init 2 */

  /* USER CODE END I2C3_Init 2 */

}

/**
  * @brief LTDC Initialization Function
  * @param None
  * @retval None
  */
static void MX_LTDC_Init(void)
{

  /* USER CODE BEGIN LTDC_Init 0 */

  /* USER CODE END LTDC_Init 0 */

  LTDC_LayerCfgTypeDef pLayerCfg = {0};

  /* USER CODE BEGIN LTDC_Init 1 */

  /* USER CODE END LTDC_Init 1 */
  hltdc.Instance = LTDC;
  hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
  hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
  hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
  hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
  hltdc.Init.HorizontalSync = 9;
  hltdc.Init.VerticalSync = 1;
  hltdc.Init.AccumulatedHBP = 29;
  hltdc.Init.AccumulatedVBP = 3;
  hltdc.Init.AccumulatedActiveW = 269;
  hltdc.Init.AccumulatedActiveH = 323;
  hltdc.Init.TotalWidth = 279;
  hltdc.Init.TotalHeigh = 327;
  hltdc.Init.Backcolor.Blue = 0;
  hltdc.Init.Backcolor.Green = 0;
  hltdc.Init.Backcolor.Red = 0;
  if (HAL_LTDC_Init(&hltdc) != HAL_OK)
  {
    Error_Handler();
  }
  pLayerCfg.WindowX0 = 0;
  pLayerCfg.WindowX1 = 240;
  pLayerCfg.WindowY0 = 0;
  pLayerCfg.WindowY1 = 320;
  pLayerCfg.PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
  pLayerCfg.Alpha = 255;
  pLayerCfg.Alpha0 = 0;
  pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
  pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
  pLayerCfg.FBStartAdress = 0;
  pLayerCfg.ImageWidth = 240;
  pLayerCfg.ImageHeight = 320;
  pLayerCfg.Backcolor.Blue = 0;
  pLayerCfg.Backcolor.Green = 0;
  pLayerCfg.Backcolor.Red = 0;
  if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LTDC_Init 2 */
    /*Select the device */
  LcdDrv = &ili9341_drv;
  /* LCD Init */
  LcdDrv->Init();

  LcdDrv->DisplayOff();
  /* USER CODE END LTDC_Init 2 */

}

/**
  * @brief SPI5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI5_Init(void)
{

  /* USER CODE BEGIN SPI5_Init 0 */

  /* USER CODE END SPI5_Init 0 */

  /* USER CODE BEGIN SPI5_Init 1 */

  /* USER CODE END SPI5_Init 1 */
  /* SPI5 parameter configuration*/
  hspi5.Instance = SPI5;
  hspi5.Init.Mode = SPI_MODE_MASTER;
  hspi5.Init.Direction = SPI_DIRECTION_2LINES;
  hspi5.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi5.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi5.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi5.Init.NSS = SPI_NSS_SOFT;
  hspi5.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi5.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi5.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi5.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi5.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi5) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI5_Init 2 */
  // Check if the board has the old or new revision of the gyroscope
  // This tells if the board is revision D or newer
  // It is used to handle the touch input correctly
  // Note: SPI5 NCS (gyroscope) is PC1 on STM32F429I-DISC1 schematic.
  //       CubeMX no longer generates SPI5_NCS_* macros because PC1 was
  //       reassigned to HCSR04_TRIG.  Using direct port/pin values here.
  const uint8_t READ_ID_CMD = 0x8F; // 0b10001111 = set read bit and register address of WHO_AM_I
  uint8_t pdata = 0;
  HAL_GPIO_WritePin(SPI5_NCS_GPIO_Port, SPI5_NCS_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi5, &READ_ID_CMD, 1, 1000);
  HAL_SPI_Receive(&hspi5, &pdata, 1, 1000);
  HAL_GPIO_WritePin(SPI5_NCS_GPIO_Port, SPI5_NCS_Pin, GPIO_PIN_SET);
  if (pdata == 0xD3) // 0b11010011
  {
    isRevD = 1;
  }
  /* USER CODE END SPI5_Init 2 */

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
  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 89;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
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
  if (HAL_TIM_IC_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

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
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 89;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 19999;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 1500;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */
  HAL_TIM_MspPostInit(&htim4);

}

/* FMC initialization function */
static void MX_FMC_Init(void)
{

  /* USER CODE BEGIN FMC_Init 0 */

  /* USER CODE END FMC_Init 0 */

  FMC_SDRAM_TimingTypeDef SdramTiming = {0};

  /* USER CODE BEGIN FMC_Init 1 */

  /* USER CODE END FMC_Init 1 */

  /** Perform the SDRAM1 memory initialization sequence
  */
  hsdram1.Instance = FMC_SDRAM_DEVICE;
  /* hsdram1.Init */
  hsdram1.Init.SDBank = FMC_SDRAM_BANK2;
  hsdram1.Init.ColumnBitsNumber = FMC_SDRAM_COLUMN_BITS_NUM_8;
  hsdram1.Init.RowBitsNumber = FMC_SDRAM_ROW_BITS_NUM_12;
  hsdram1.Init.MemoryDataWidth = FMC_SDRAM_MEM_BUS_WIDTH_16;
  hsdram1.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
  hsdram1.Init.CASLatency = FMC_SDRAM_CAS_LATENCY_3;
  hsdram1.Init.WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
  hsdram1.Init.SDClockPeriod = FMC_SDRAM_CLOCK_PERIOD_2;
  hsdram1.Init.ReadBurst = FMC_SDRAM_RBURST_DISABLE;
  hsdram1.Init.ReadPipeDelay = FMC_SDRAM_RPIPE_DELAY_1;
  /* SdramTiming */
  SdramTiming.LoadToActiveDelay = 2;
  SdramTiming.ExitSelfRefreshDelay = 7;
  SdramTiming.SelfRefreshTime = 4;
  SdramTiming.RowCycleDelay = 7;
  SdramTiming.WriteRecoveryTime = 3;
  SdramTiming.RPDelay = 2;
  SdramTiming.RCDDelay = 2;

  if (HAL_SDRAM_Init(&hsdram1, &SdramTiming) != HAL_OK)
  {
    Error_Handler( );
  }

  /* USER CODE BEGIN FMC_Init 2 */

  FMC_SDRAM_CommandTypeDef command;

  /* Program the SDRAM external device */
  BSP_SDRAM_Initialization_Sequence(&hsdram1, &command);
  /* USER CODE END FMC_Init 2 */
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
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, VSYNC_FREQ_Pin|RENDER_TIME_Pin|FRAME_RATE_Pin|MCU_ACTIVE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, SPI5_NCS_Pin|LCD_NCS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12|GPIO_PIN_13|HCSR04_TRIG_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : VSYNC_FREQ_Pin RENDER_TIME_Pin FRAME_RATE_Pin MCU_ACTIVE_Pin */
  GPIO_InitStruct.Pin = VSYNC_FREQ_Pin|RENDER_TIME_Pin|FRAME_RATE_Pin|MCU_ACTIVE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : SPI5_NCS_Pin LCD_NCS_Pin */
  GPIO_InitStruct.Pin = SPI5_NCS_Pin|LCD_NCS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : USER_B1_Pin MEMS_INT1_Pin */
  GPIO_InitStruct.Pin = USER_B1_Pin|MEMS_INT1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PD12 PD13 */
  GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : HCSR04_TRIG_Pin */
  GPIO_InitStruct.Pin = HCSR04_TRIG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(HCSR04_TRIG_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/**
  * @brief  Perform the SDRAM external memory initialization sequence
  * @param  hsdram: SDRAM handle
  * @param  Command: Pointer to SDRAM command structure
  * @retval None
  */
static void BSP_SDRAM_Initialization_Sequence(SDRAM_HandleTypeDef *hsdram, FMC_SDRAM_CommandTypeDef *Command)
{
 __IO uint32_t tmpmrd =0;

  /* Step 1:  Configure a clock configuration enable command */
  Command->CommandMode             = FMC_SDRAM_CMD_CLK_ENABLE;
  Command->CommandTarget           = FMC_SDRAM_CMD_TARGET_BANK2;
  Command->AutoRefreshNumber       = 1;
  Command->ModeRegisterDefinition  = 0;

  /* Send the command */
  HAL_SDRAM_SendCommand(hsdram, Command, SDRAM_TIMEOUT);

  /* Step 2: Insert 100 us minimum delay */
  /* Inserted delay is equal to 1 ms due to systick time base unit (ms) */
  HAL_Delay(1);

  /* Step 3: Configure a PALL (precharge all) command */
  Command->CommandMode             = FMC_SDRAM_CMD_PALL;
  Command->CommandTarget           = FMC_SDRAM_CMD_TARGET_BANK2;
  Command->AutoRefreshNumber       = 1;
  Command->ModeRegisterDefinition  = 0;

  /* Send the command */
  HAL_SDRAM_SendCommand(hsdram, Command, SDRAM_TIMEOUT);

  /* Step 4: Configure an Auto Refresh command */
  Command->CommandMode             = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
  Command->CommandTarget           = FMC_SDRAM_CMD_TARGET_BANK2;
  Command->AutoRefreshNumber       = 4;
  Command->ModeRegisterDefinition  = 0;

  /* Send the command */
  HAL_SDRAM_SendCommand(hsdram, Command, SDRAM_TIMEOUT);

  /* Step 5: Program the external memory mode register */
  tmpmrd = (uint32_t)SDRAM_MODEREG_BURST_LENGTH_1          |
                     SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL   |
                     SDRAM_MODEREG_CAS_LATENCY_3           |
                     SDRAM_MODEREG_OPERATING_MODE_STANDARD |
                     SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;

  Command->CommandMode             = FMC_SDRAM_CMD_LOAD_MODE;
  Command->CommandTarget           = FMC_SDRAM_CMD_TARGET_BANK2;
  Command->AutoRefreshNumber       = 1;
  Command->ModeRegisterDefinition  = tmpmrd;

  /* Send the command */
  HAL_SDRAM_SendCommand(hsdram, Command, SDRAM_TIMEOUT);

  /* Step 6: Set the refresh rate counter */
  /* Set the device refresh rate */
  HAL_SDRAM_ProgramRefreshRate(hsdram, REFRESH_COUNT);
}

/**
  * @brief  IOE Low Level Initialization.
  */
void IOE_Init(void)
{
  //Dummy function called when initializing to stmpe811 to setup the i2c.
  //This is done with cubmx and is therfore not done here.
}

/**
  * @brief  IOE Low Level Interrupt configuration.
  */
void IOE_ITConfig(void)
{
  //Dummy function called when initializing to stmpe811 to setup interupt for the i2c.
  //The interupt is not used in our case, therefore nothing is done here.
}

/**
  * @brief  IOE Writes single data operation.
  * @param  Addr: I2C Address
  * @param  Reg: Reg Address
  * @param  Value: Data to be written
  */
void IOE_Write(uint8_t Addr, uint8_t Reg, uint8_t Value)
{
  I2C3_WriteData(Addr, Reg, Value);
}

/**
  * @brief  IOE Reads single data.
  * @param  Addr: I2C Address
  * @param  Reg: Reg Address
  * @retval The read data
  */
uint8_t IOE_Read(uint8_t Addr, uint8_t Reg)
{
  return I2C3_ReadData(Addr, Reg);
}

/**
  * @brief  IOE Reads multiple data.
  * @param  Addr: I2C Address
  * @param  Reg: Reg Address
  * @param  pBuffer: pointer to data buffer
  * @param  Length: length of the data
  * @retval 0 if no problems to read multiple data
  */
uint16_t IOE_ReadMultiple(uint8_t Addr, uint8_t Reg, uint8_t *pBuffer, uint16_t Length)
{
 return I2C3_ReadBuffer(Addr, Reg, pBuffer, Length);
}

/**
  * @brief  IOE Delay.
  * @param  Delay in ms
  */
void IOE_Delay(uint32_t Delay)
{
  HAL_Delay(Delay);
}

/**
  * @brief  Writes a value in a register of the device through BUS.
  * @param  Addr: Device address on BUS Bus.
  * @param  Reg: The target register address to write
  * @param  Value: The target register value to be written
  */
static void I2C3_WriteData(uint8_t Addr, uint8_t Reg, uint8_t Value)
{
  HAL_StatusTypeDef status = HAL_OK;

  status = HAL_I2C_Mem_Write(&hi2c3, Addr, (uint16_t)Reg, I2C_MEMADD_SIZE_8BIT, &Value, 1, I2c3Timeout);

  /* Check the communication status */
  if(status != HAL_OK)
  {
    /* Re-Initialize the BUS */
    //I2Cx_Error();
  }
}

/**
  * @brief  Reads a register of the device through BUS.
  * @param  Addr: Device address on BUS Bus.
  * @param  Reg: The target register address to write
  * @retval Data read at register address
  */
static uint8_t I2C3_ReadData(uint8_t Addr, uint8_t Reg)
{
  HAL_StatusTypeDef status = HAL_OK;
  uint8_t value = 0;

  status = HAL_I2C_Mem_Read(&hi2c3, Addr, Reg, I2C_MEMADD_SIZE_8BIT, &value, 1, I2c3Timeout);

  /* Check the communication status */
  if(status != HAL_OK)
  {
    /* Re-Initialize the BUS */
    //I2Cx_Error();

  }
  return value;
}

/**
  * @brief  Reads multiple data on the BUS.
  * @param  Addr: I2C Address
  * @param  Reg: Reg Address
  * @param  pBuffer: pointer to read data buffer
  * @param  Length: length of the data
  * @retval 0 if no problems to read multiple data
  */
static uint8_t I2C3_ReadBuffer(uint8_t Addr, uint8_t Reg, uint8_t *pBuffer, uint16_t Length)
{
  HAL_StatusTypeDef status = HAL_OK;

  status = HAL_I2C_Mem_Read(&hi2c3, Addr, (uint16_t)Reg, I2C_MEMADD_SIZE_8BIT, pBuffer, Length, I2c3Timeout);

  /* Check the communication status */
  if(status == HAL_OK)
  {
    return 0;
  }
  else
  {
    /* Re-Initialize the BUS */
    //I2Cx_Error();

    return 1;
  }
}

/**
  * @brief  Reads 4 bytes from device.
  * @param  ReadSize: Number of bytes to read (max 4 bytes)
  * @retval Value read on the SPI
  */
static uint32_t SPI5_Read(uint8_t ReadSize)
{
  HAL_StatusTypeDef status = HAL_OK;
  uint32_t readvalue;

  status = HAL_SPI_Receive(&hspi5, (uint8_t*) &readvalue, ReadSize, Spi5Timeout);

  /* Check the communication status */
  if(status != HAL_OK)
  {
    /* Re-Initialize the BUS */
    SPI5_Error();
  }

  return readvalue;
}

/**
  * @brief  Writes a byte to device.
  * @param  Value: value to be written
  */
static void SPI5_Write(uint16_t Value)
{
  HAL_StatusTypeDef status = HAL_OK;

  status = HAL_SPI_Transmit(&hspi5, (uint8_t*) &Value, 1, Spi5Timeout);

  /* Check the communication status */
  if(status != HAL_OK)
  {
    /* Re-Initialize the BUS */
    SPI5_Error();
  }
}

/**
  * @brief  SPI5 error treatment function.
  */
static void SPI5_Error(void)
{
  /* De-initialize the SPI communication BUS */
  //HAL_SPI_DeInit(&SpiHandle);

  /* Re- Initialize the SPI communication BUS */
  //SPIx_Init();
}

void LCD_IO_Init(void)
{
  /* Set or Reset the control line */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_SET);
}

/**
  * @brief  Writes register value.
  */
void LCD_IO_WriteData(uint16_t RegValue)
{
  /* Set WRX to send data */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_SET);

  /* Reset LCD control line(/CS) and Send data */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_RESET);
  SPI5_Write(RegValue);

  /* Deselect: Chip Select high */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_SET);
}

/**
  * @brief  Writes register address.
  */
void LCD_IO_WriteReg(uint8_t Reg)
{
  /* Reset WRX to send command */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_RESET);

  /* Reset LCD control line(/CS) and Send command */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_RESET);
  SPI5_Write(Reg);

  /* Deselect: Chip Select high */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_SET);
}

/**
  * @brief  Reads register value.
  * @param  RegValue Address of the register to read
  * @param  ReadSize Number of bytes to read
  * @retval Content of the register value
  */
uint32_t LCD_IO_ReadData(uint16_t RegValue, uint8_t ReadSize)
{
  uint32_t readvalue = 0;

  /* Select: Chip Select low */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_RESET);

  /* Reset WRX to send command */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_RESET);

  SPI5_Write(RegValue);

  readvalue = SPI5_Read(ReadSize);

  /* Set WRX to send data */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_SET);

  /* Deselect: Chip Select high */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_SET);

  return readvalue;
}

/**
  * @brief  Wait for loop in ms.
  * @param  Delay in ms.
  */
void LCD_Delay(uint32_t Delay)
{
  HAL_Delay(Delay);
}

/**
 * @brief  Provides a 1ms delay safely using FreeRTOS or HAL.
 *         Use osDelay when FreeRTOS is running.
 */
void delay_ms(uint32_t delay)
{
    osDelay(delay);
}


/* ---------------------------------------------------------------------------
 * Servo MG90S control functions
 * --------------------------------------------------------------------------- */

/**
  * @brief  Set the servo PWM pulse width in microseconds.
  * @param  pulse_us  Desired pulse width (clamped to SERVO_MIN/MAX_PULSE_US).
  * @retval None
  */
static void Servo_SetPulseUs(uint16_t pulse_us)
{
  /* Clamp to safe operating range */
  if (pulse_us < SERVO_MIN_PULSE_US)
  {
    pulse_us = SERVO_MIN_PULSE_US;
  }
  else if (pulse_us > SERVO_MAX_PULSE_US)
  {
    pulse_us = SERVO_MAX_PULSE_US;
  }

  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, pulse_us);
}

/**
  * @brief  Set the servo position by angle in degrees.
  * @param  angle_deg  Desired angle 0-180 (clamped).
  *         0 deg  -> SERVO_MIN_PULSE_US
  *         90 deg -> SERVO_CENTER_PULSE_US
  *         180 deg-> SERVO_MAX_PULSE_US
  * @retval None
  */
static void Servo_SetAngleDeg(uint16_t angle_deg)
{
  /* Clamp angle */
  if (angle_deg > SERVO_MAX_ANGLE_DEG)
  {
    angle_deg = SERVO_MAX_ANGLE_DEG;
  }

  /* Linear mapping */
  /* Cast to uint32_t to prevent overflow in intermediate multiplication */
  uint16_t pulse_us = (uint16_t)(SERVO_MIN_PULSE_US
      + (((uint32_t)angle_deg * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) + (SERVO_MAX_ANGLE_DEG / 2))
        / SERVO_MAX_ANGLE_DEG));

  Servo_SetPulseUs(pulse_us);
}

#if 0
static void Servo_RunBringupTest(void)
{
  Servo_SetAngleDeg(60U);
  osDelay(SERVO_TEST_HOLD_MS);

  Servo_SetAngleDeg(90U);
  osDelay(SERVO_TEST_HOLD_MS);

  Servo_SetAngleDeg(120U);
  osDelay(SERVO_TEST_HOLD_MS);

  Servo_SetAngleDeg(90U);
}
#endif

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */
  // Enable TIM2 Input Capture ONCE
  if (HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1) != HAL_OK)
  {
      Error_Handler();
  }
  __HAL_TIM_SET_CAPTUREPOLARITY(&htim2, TIM_CHANNEL_1, TIM_INPUTCHANNELPOLARITY_RISING);

  static uint16_t radar_current_angle = RADAR_MIN_ANGLE_DEG;
#if RADAR_CALIBRATION_MODE == 1U
  radar_current_angle = RADAR_CALIBRATION_ANGLE_DEG;
  g_radarCalibrationStats.angle_deg = RADAR_CALIBRATION_ANGLE_DEG;
  g_radarCalibrationStats.min_distance_mm = UINT16_MAX;
  g_radarCalibrationStats.max_distance_mm = 0;
  g_radarCalibrationStats.average_distance_mm = 0;
  g_radarCalibrationStats.sum_distance_mm = 0;
  g_radarCalibrationStats.warmup_samples_seen = 0;
  g_radarCalibrationStats.total_samples = 0;
  g_radarCalibrationStats.valid_samples = 0;
  g_radarCalibrationStats.invalid_samples = 0;
  g_radarCalibrationStats.complete = 0;
#endif
#if RADAR_CALIBRATION_MODE == 0U
  static int8_t radar_direction = 1;
#endif
  static RadarScanState radar_scan_state = RADAR_SCAN_MOVE_SERVO;
  static uint32_t state_tick = 0;
  
  static B1_State b1_state = B1_RELEASED;
  static uint32_t b1_tick_start = 0;
  
  HCSR04_Result result = {0, 0, 0};

  /* Infinite loop */
  for(;;)
  {
    uint32_t now = osKernelGetTickCount();
    HCSR04_ProcessTimeout(now); // Let the HCSR04 driver process its own internal timeouts

    GPIO_PinState b1_pin = HAL_GPIO_ReadPin(USER_B1_GPIO_Port, USER_B1_Pin);
    uint8_t b1_short_press = 0;
    uint8_t b1_long_press = 0;

    switch (b1_state)
    {
        case B1_RELEASED:
            if (b1_pin == GPIO_PIN_SET)
            {
                b1_tick_start = now;
                b1_state = B1_DEBOUNCE_PRESS;
            }
            break;
        case B1_DEBOUNCE_PRESS:
            if (b1_pin == GPIO_PIN_SET)
            {
                if ((now - b1_tick_start) >= 30U)
                {
                    b1_state = B1_PRESSED;
                }
            }
            else
            {
                b1_state = B1_RELEASED;
            }
            break;
        case B1_PRESSED:
            if (b1_pin == GPIO_PIN_SET)
            {
                if ((now - b1_tick_start) >= 1000U)
                {
                    b1_long_press = 1;
                    b1_state = B1_LONG_REPORTED;
                }
            }
            else
            {
                b1_short_press = 1;
                b1_tick_start = now;
                b1_state = B1_DEBOUNCE_RELEASE;
            }
            break;
        case B1_LONG_REPORTED:
            if (b1_pin == GPIO_PIN_RESET)
            {
                b1_tick_start = now;
                b1_state = B1_DEBOUNCE_RELEASE;
            }
            break;
        case B1_DEBOUNCE_RELEASE:
            if (b1_pin == GPIO_PIN_RESET)
            {
                if ((now - b1_tick_start) >= 30U)
                {
                    b1_state = B1_RELEASED;
                }
            }
            else
            {
                b1_tick_start = now; // Wait until stable low
            }
            break;
    }

#if RADAR_CALIBRATION_MODE == 0U
    if (g_radarControlState.state.appMode == RADAR_APP_RUNNING)
    {
        if (b1_long_press)
        {
            RadarControlState nextState = g_radarControlState;
            nextState.state.appMode = RADAR_APP_SPEED_SETTING;
            g_radarControlState.packed = nextState.packed;
        }
    }
    else if (g_radarControlState.state.appMode == RADAR_APP_SPEED_SETTING)
    {
        if (b1_short_press)
        {
            RadarControlState nextState = g_radarControlState;
            if (nextState.state.speedMode == RADAR_SPEED_FAST)
            {
                nextState.state.speedMode = RADAR_SPEED_SLOW;
            }
            else
            {
                nextState.state.speedMode = RADAR_SPEED_FAST;
            }
            nextState.state.appMode = RADAR_APP_RUNNING;
            g_radarControlState.packed = nextState.packed;
            
            // Clean up state machine for resume
            radar_scan_state = RADAR_SCAN_MOVE_SERVO;
        }
    }

    if (g_radarControlState.state.appMode == RADAR_APP_SPEED_SETTING)
    {
        if (radar_scan_state == RADAR_SCAN_WAIT_MEASUREMENT)
        {
            if (HCSR04_GetResult(&result))
            {
                // Discard and clean state
                radar_scan_state = RADAR_SCAN_MOVE_SERVO;
            }
            else if ((int32_t)(now - state_tick) >= RADAR_MEASURE_TIMEOUT_MS)
            {
                // Timeout, clear it
                HCSR04_GetResult(&result); 
                radar_scan_state = RADAR_SCAN_MOVE_SERVO;
            }
        }
        else
        {
            // Just jump to MOVE_SERVO immediately so we are ready when resumed
            radar_scan_state = RADAR_SCAN_MOVE_SERVO;
        }
        
        osDelay(RADAR_IDLE_DELAY_MS);
        continue;
    }
#else
    // Silence unused variable warnings in Calibration mode
    (void)b1_short_press;
    (void)b1_long_press;
#endif

    switch (radar_scan_state)
    {
      case RADAR_SCAN_MOVE_SERVO:
        Servo_SetAngleDeg(radar_current_angle);
        state_tick = now;
        radar_scan_state = RADAR_SCAN_WAIT_SERVO;
        break;

      case RADAR_SCAN_WAIT_SERVO:
      {
        uint32_t settle_ms = RADAR_SPEED_FAST_SETTLE_MS;
        if (g_radarControlState.state.speedMode == RADAR_SPEED_SLOW)
        {
            settle_ms = RADAR_SPEED_SLOW_SETTLE_MS;
        }
#if RADAR_CALIBRATION_MODE == 1U
        settle_ms = RADAR_SPEED_FAST_SETTLE_MS;
#endif

        if ((int32_t)(now - state_tick) >= settle_ms)
        {
          radar_scan_state = RADAR_SCAN_START_MEASUREMENT;
        }
        break;
      }

      case RADAR_SCAN_START_MEASUREMENT:
        if (HCSR04_StartMeasurement())
        {
          state_tick = now;
          radar_scan_state = RADAR_SCAN_WAIT_MEASUREMENT;
        }
        // If driver busy, stay in this state and try again next loop
        break;

      case RADAR_SCAN_WAIT_MEASUREMENT:
        if (HCSR04_GetResult(&result))
        {
          radar_scan_state = RADAR_SCAN_PUBLISH_SAMPLE;
        }
        else if ((int32_t)(now - state_tick) >= RADAR_MEASURE_TIMEOUT_MS)
        {
          // Controller-level guard timeout (fallback if driver fails to time out itself)
          result.distance_mm = 0;
          result.valid = 0;
          result.pulse_width_us = 0;
          radar_scan_state = RADAR_SCAN_PUBLISH_SAMPLE;
        }
        break;

      case RADAR_SCAN_PUBLISH_SAMPLE:
      {
        RadarSample sample;
        sample.angle_deg = radar_current_angle;
        sample.distance_mm = result.distance_mm;
        sample.valid = result.valid;
        sample.timestamp_ms = now;

        // Update debug variables
        g_radar_angle_deg = sample.angle_deg;
        g_radar_distance_mm = sample.distance_mm;
        g_radar_valid = sample.valid;
        
#if RADAR_CALIBRATION_MODE == 1U
        if (g_radarCalibrationStats.warmup_samples_seen < RADAR_CALIBRATION_WARMUP_SAMPLES)
        {
            ++g_radarCalibrationStats.warmup_samples_seen;
        }
        else if (g_radarCalibrationStats.complete == 0U)
        {
            ++g_radarCalibrationStats.total_samples;
        
            if (result.valid != 0U &&
                result.distance_mm >= 20U &&
                result.distance_mm <= 4000U)
            {
                ++g_radarCalibrationStats.valid_samples;
                g_radarCalibrationStats.sum_distance_mm += result.distance_mm;
        
                if (result.distance_mm < g_radarCalibrationStats.min_distance_mm)
                {
                    g_radarCalibrationStats.min_distance_mm = result.distance_mm;
                }
        
                if (result.distance_mm > g_radarCalibrationStats.max_distance_mm)
                {
                    g_radarCalibrationStats.max_distance_mm = result.distance_mm;
                }
        
                g_radarCalibrationStats.average_distance_mm =
                    (uint16_t)(g_radarCalibrationStats.sum_distance_mm /
                               g_radarCalibrationStats.valid_samples);
            }
            else
            {
                ++g_radarCalibrationStats.invalid_samples;
            }
        
            if (g_radarCalibrationStats.total_samples >= RADAR_CALIBRATION_SAMPLES)
            {
                g_radarCalibrationStats.complete = 1U;
            }
        }
#endif
        
        // Push to queue, overwrite oldest if full
        if (osMessageQueuePut(radarQueueHandle, &sample, 0U, 0U) != osOK)
        {
            RadarSample discarded;
            (void)osMessageQueueGet(radarQueueHandle, &discarded, NULL, 0U);
            (void)osMessageQueuePut(radarQueueHandle, &sample, 0U, 0U);
        }
        
        g_radar_samples_sent++;
        radar_scan_state = RADAR_SCAN_NEXT_ANGLE;
        break;
      }

      case RADAR_SCAN_NEXT_ANGLE:
#if RADAR_CALIBRATION_MODE == 0U
        if (radar_direction > 0)
        {
            if ((radar_current_angle + RADAR_STEP_ANGLE_DEG) >= RADAR_MAX_ANGLE_DEG)
            {
                radar_current_angle = RADAR_MAX_ANGLE_DEG;
                radar_direction = -1;
            }
            else
            {
                radar_current_angle += RADAR_STEP_ANGLE_DEG;
            }
        }
        else
        {
            if (radar_current_angle <= (RADAR_MIN_ANGLE_DEG + RADAR_STEP_ANGLE_DEG))
            {
                radar_current_angle = RADAR_MIN_ANGLE_DEG;
                radar_direction = 1;
            }
            else
            {
                radar_current_angle -= RADAR_STEP_ANGLE_DEG;
            }
        }
#endif
        radar_scan_state = RADAR_SCAN_MOVE_SERVO;
        break;
        
      default:
        radar_scan_state = RADAR_SCAN_MOVE_SERVO;
        break;
    }

    osDelay(RADAR_IDLE_DELAY_MS);
  }
  /* USER CODE END 5 */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

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
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
