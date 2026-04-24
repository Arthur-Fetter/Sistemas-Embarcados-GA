/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "rtc.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32f3xx.h"
#include <stdio.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define HDC1080_ADDR 0x40 << 1
#define REG_TEMP     0x00
#define REG_CONFIG   0x02
#define TEMP_MAX     50
#define TEMP_MIN     0
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile uint8_t buttonState = 0;
volatile uint8_t buttonWakeUp = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int _write(int file, char *ptr, int len)
{
  HAL_UART_Transmit(&huart2, (uint8_t *)ptr, len, HAL_MAX_DELAY);
  return len;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_13) {

    buttonWakeUp = 1;
  }
}

typedef enum {
  ESTADO_SETUP,
  ATIVAR_SLEEP,
  LEITURA_TEMP,
  LEITURA_POT,
  ALERTA_LED,
  VERIFICA_BOTAO,
  ALTERA_UNIDADE_MEDIDA,
  RENDENIZA_TEXTO,
  ESTADO_SAIR,
} EstadoId;

typedef struct {
  float temperatura;
  float ref_temperatura;
  char unidade_medida;
} Contexto;

typedef EstadoId EstadoFunc(Contexto *contexto);

// Setup realiza a inicializacao do contexto e configura o sensor HDC
EstadoId estado_setup(Contexto *contexto) {
  contexto->temperatura = 0.0f;
  contexto->ref_temperatura = 0.0f;
  contexto->unidade_medida = 'C';

  // Config I2C
  uint8_t config_data[2] = {0x10, 0x00};
  HAL_I2C_Mem_Write(&hi2c1, HDC1080_ADDR, REG_CONFIG, I2C_MEMADD_SIZE_8BIT, config_data, 2, 100);
  
  return LEITURA_TEMP;
}

// Ativa modo STOP do microcontrolador. Um timer é configurado para emitir um EXTI a cada 500ms
EstadoId ativar_sleep(Contexto *contexto) {
  HAL_SuspendTick();
  HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 0x400, RTC_WAKEUPCLOCK_RTCCLK_DIV16);
  HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
  HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
  SystemClock_Config();
  HAL_ResumeTick();

  return LEITURA_TEMP;
}

// Utiliza a interface I2C para ler a temperatura medida pelo sensor HDC
EstadoId leitura_temp(Contexto *contexto) {
  uint8_t reg = REG_TEMP;
  HAL_I2C_Master_Transmit(&hi2c1, HDC1080_ADDR, &reg, 1, 100);

  HAL_Delay(20);

  uint8_t data[2];
  if (HAL_I2C_Master_Receive(&hi2c1, HDC1080_ADDR, data, 2, 100) == HAL_OK) {
    uint16_t raw_temp = (data[0] << 8) | data[1];
    contexto->temperatura = ((float)raw_temp / 65536.0f) * 165.0f - 40.0f;
  }
  
  return LEITURA_POT;
}

// Calibra o ADC (descalibrado a cada parada do microcontrolador), le e converte o valor para uma temperatura de referencia
EstadoId leitura_pot(Contexto *contexto) {
  HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
  HAL_ADC_Start(&hadc1);
  if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
    uint32_t adc_value = HAL_ADC_GetValue(&hadc1);
    contexto->ref_temperatura = (adc_value * 50.0f) / 4095.0f;    
  }
  HAL_ADC_Stop(&hadc1);

  return ALERTA_LED;
}

// Liga o LED de aviso caso a temperatura medida seja maior que a de referencia
EstadoId alerta_led(Contexto *contexto) {
  if ((int)(contexto->temperatura) >= (int)(contexto->ref_temperatura)) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
  } else {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
  }

  return VERIFICA_BOTAO;
}

// Caso a ocorra a interrupcao do botao, realiza um debounce e modifica o estado do botao
EstadoId verifica_botao(Contexto *contexto) {
  if (buttonWakeUp) {
    buttonWakeUp = 0;

    if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {
      buttonState ^= 1;
    }
  }
  return ALTERA_UNIDADE_MEDIDA;
}

// Altera a unidade de medida caso o estado do botao seja diferente
EstadoId altera_unidade_medida(Contexto *contexto) {
  switch (buttonState) {
  case 0:
    contexto->unidade_medida = 'C';
    break;
  case 1:
    contexto->unidade_medida = 'F';
    break;
  }
  return RENDENIZA_TEXTO;
}

// Imprime na interface serial a temperatura de referencia e a temperatura medida
EstadoId rendeniza_texto(Contexto *contexto) {
  printf("Temperatura referencia: %d%c\r\n", (int)(contexto->ref_temperatura), contexto->unidade_medida);
  printf("Temperatura medida = %d%c\r\n", (int)(contexto->temperatura), contexto->unidade_medida);

  return ATIVAR_SLEEP;
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
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  MX_SPI1_Init();
  MX_RTC_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */
  Contexto contexto;
  EstadoId estadoAtual = ESTADO_SETUP;

  EstadoFunc *tabela_estados[] = {
    [ESTADO_SETUP]          = estado_setup,
    [ATIVAR_SLEEP]          = ativar_sleep,
    [LEITURA_TEMP]          = leitura_temp,
    [LEITURA_POT]           = leitura_pot,
    [ALERTA_LED]            = alerta_led,
    [VERIFICA_BOTAO]        = verifica_botao,
    [ALTERA_UNIDADE_MEDIDA] = altera_unidade_medida,
    [RENDENIZA_TEXTO]       = rendeniza_texto
  };
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (estadoAtual != ESTADO_SAIR)
  {
    /* USER CODE BEGIN 3 */

    estadoAtual = tabela_estados[estadoAtual](&contexto);
    /* USER CODE END WHILE */
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

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1|RCC_PERIPHCLK_RTC
                              |RCC_PERIPHCLK_ADC12;
  PeriphClkInit.Adc12ClockSelection = RCC_ADC12PLLCLK_DIV1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
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
