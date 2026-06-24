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
#include "crc.h"
#include "dma.h"
#include "iwdg.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stmflash.h"
#include "usbd_cdc_if.h"
#include <stdarg.h>
#include "iap.h"
#include "usbd_cdc_if.h"
#include "ring_buffer.h"
#include "ymodem.h"
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

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#define UPDATE_MAGIC_APP  0xA55AA55A
#define UPDATE_MAGIC_BOOT 0x5AA5A5A5
// 放入.noinit段，上电不自动清零
__attribute__((section(".noinit"))) volatile uint32_t g_UpdateFlag;
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
  MX_USB_Device_Init();
  MX_LPUART1_UART_Init();
  MX_CRC_Init();
  // MX_IWDG_Init();
  /* USER CODE BEGIN 2 */
  uint32_t i = 0;
  // 按下按键上电等待3s，强制进入APP升级
  while(HAL_GPIO_ReadPin(key_GPIO_Port, key_Pin) == GPIO_PIN_RESET)
  {
    i++;
    if(i >= 30)
    {
      i = 0;
      HAL_IWDG_Refresh(&hiwdg);
      HAL_GPIO_WritePin(led_GPIO_Port, led_Pin, GPIO_PIN_SET);
      HAL_Delay(800);
      // 执行 RESET 命令
      g_UpdateFlag = UPDATE_MAGIC_BOOT;
      // __DSB();
      // __ISB();
      // NVIC_SystemReset();
      break;
    }
    HAL_IWDG_Refresh(&hiwdg);
    HAL_Delay(30);

    HAL_GPIO_TogglePin(led_GPIO_Port, led_Pin);
  }
  
  HAL_IWDG_Refresh(&hiwdg);
  // 如果是APP触发软复位和更新标志
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != RESET
    && g_UpdateFlag == UPDATE_MAGIC_APP)
  {
    // 继续执行固件更新操作
    g_UpdateFlag = 0;
  }
  else if (g_UpdateFlag == UPDATE_MAGIC_BOOT)
  {
    // 继续执行固件更新操作
    g_UpdateFlag = 0;
  }
  else
  {
    g_UpdateFlag = 0;
    __HAL_RCC_CLEAR_RESET_FLAGS();
    // 没有事件发生，直接跳转APP
    if (iap_load_app(0x08008000) < 0)
    {
      // 固件损坏或非固件文件，执行bootloader等待接收固件
    }
  }

  YMODEM_Init(&ymodem_fsm);
  RingBuff_Init(&usb_ringBuffer); // 初始化全局环形缓冲区

  uint32_t sys_cnt = 0;
  uint32_t breath_cnt = 0;
  uint8_t duty = 0;
  uint8_t dir = 1;

  HAL_IWDG_Refresh(&hiwdg);
  for(;;)
  {
    uint32_t tick = HAL_GetTick();
    static uint32_t tick_200ms = 0;
    if (tick - tick_200ms >= 200)
    {
      tick_200ms = tick;

      HAL_IWDG_Refresh(&hiwdg);

      if(RingBuff_GetSize(&usb_ringBuffer) > 0)
      {
        break;
      }
      if (usb_vcp_opened) // 等待上位机打开串口
      {
        YMODEM_SendByte(YMODEM_C);
      }
    }

    sys_cnt++;
    // 高频PWM：每次循环直接判断，无延时
    if (sys_cnt < duty)
    {
      HAL_GPIO_WritePin(led_GPIO_Port, led_Pin, GPIO_PIN_RESET); // 亮
    }
    else
    {
      HAL_GPIO_WritePin(led_GPIO_Port, led_Pin, GPIO_PIN_SET);  // 灭
    }
    // PWM周期限制，防止计数溢出，255刚好对应duty范围0~255
    if (sys_cnt >= 255)
    {
      sys_cnt = 0;
    }
    // 单独慢速计数器，控制呼吸明暗变化速度
    breath_cnt++;
    if (breath_cnt >= 15000) // 数值越大，呼吸越慢
    {
      breath_cnt = 0;
      if (dir)
      {
        duty++;
        if (duty >= 255)
          dir = 0;
      }
      else
      {
        duty--;
        if (duty == 0)
          dir = 1;
      }
    }
  }
  __HAL_CRC_DR_RESET(&hcrc);
  HAL_IWDG_Refresh(&hiwdg);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t tick = HAL_GetTick();
    static uint32_t tick_1ms = 0;
    static uint32_t tick_500ms = 0;
    
    if (tick - tick_500ms >= 500)
    {
      tick_500ms = tick;

      HAL_IWDG_Refresh(&hiwdg);
    }

    static uint32_t last_tick = 0;
    static uint8_t  state = 0;          // 0: 等待周期开始, 1: 亮灯, 2: 灭灯
    static uint8_t  flash_count = 0;    // 当前周期内已完成的闪烁次数

    switch (state)
    {
      case 0: // 等待500ms周期开始
        if (tick - last_tick >= 1000)
        {
          flash_count = 0;                // 重置闪烁计数
          HAL_GPIO_WritePin(led_GPIO_Port, led_Pin, GPIO_PIN_RESET);   // 亮灯
          state = 1;
          last_tick = tick;
        }
        break;

      case 1: // 亮灯状态，持续40ms后熄灭
        if (tick - last_tick >= 50)
        {
          HAL_GPIO_WritePin(led_GPIO_Port, led_Pin, GPIO_PIN_SET); // 灭灯
          state = 2;
          last_tick = tick;
        }
        break;

      case 2: // 灭灯状态，持续40ms后判断
        if (tick - last_tick >= 50)
        {
          flash_count++;
          if (flash_count < 2)            // 还没闪够两次，再亮一次
          {
            HAL_GPIO_WritePin(led_GPIO_Port, led_Pin, GPIO_PIN_RESET);
            state = 1;
            last_tick = tick;
          }
          else                            // 两次已闪完，进入空闲等待
          {
            state = 0;
            last_tick = tick;           // 重置计时起点，准备下一个500ms周期
          }
        }
        break;

      default:
        state = 0;
        break;
    }
    
    if (tick - tick_1ms >= 1)
    {
      tick_1ms = tick;
      // YMODEM解析
      uint16_t size = RingBuff_GetSize(&usb_ringBuffer);
      if (size == 133 || (usb_ringBuffer.buffer[usb_ringBuffer.tail] == 0x04)) // 标准包大小
      {
        uint8_t data[1029];

        HAL_GPIO_TogglePin(led_GPIO_Port, led_Pin);

        volatile uint16_t len = RingBuff_ReadBytes(&usb_ringBuffer, data, size);

        YMODEM_Parse(&ymodem_fsm, data, &len);
      }
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

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_LSI
                              |RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV3;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
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
     ex: UART_Send("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
