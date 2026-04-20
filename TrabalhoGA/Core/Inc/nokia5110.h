#ifndef NOKIA5110_H
#define NOKIA5110_H

#include "stm32f3xx_hal.h"
#include <stdbool.h>

#define LCD_WIDTH   84 
#define LCD_HEIGHT  48 

// The Hardware Abstraction Struct
typedef struct {
    SPI_HandleTypeDef *hspi;       // Pointer to the SPI handle (e.g., &hspi1)
    GPIO_TypeDef *dc_port;         // Data/Command GPIO Port
    uint16_t dc_pin;               // Data/Command GPIO Pin
    GPIO_TypeDef *ce_port;         // Chip Enable GPIO Port
    uint16_t ce_pin;               // Chip Enable GPIO Pin
    GPIO_TypeDef *rst_port;        // Reset GPIO Port
    uint16_t rst_pin;              // Reset GPIO Pin
    
    uint8_t displayMap[504];       // The RAM Framebuffer (84 * 48 / 8)
} Nokia_t;

// --- Core Functions ---
void Nokia_Init(Nokia_t *dev, uint8_t contrast);
void Nokia_Update_DMA(Nokia_t *dev);
void Nokia_Clear(Nokia_t *dev, bool isBlack);

// --- Graphics Functions ---
void Nokia_SetPixel(Nokia_t *dev, int x, int y, bool isBlack);
void Nokia_SetLine(Nokia_t *dev, int x0, int y0, int x1, int y1, bool isBlack);
void Nokia_SetRect(Nokia_t *dev, int x0, int y0, int x1, int y1, bool fill, bool isBlack);
void Nokia_SetStr(Nokia_t *dev, char *str, int x, int y, bool isBlack);

#endif