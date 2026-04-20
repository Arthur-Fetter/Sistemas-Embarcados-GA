#include "nokia5110.h"
#include "fonts.h"

// --- Private Hardware Helper Functions ---
static void LCD_Write_Cmd(Nokia_t *dev, uint8_t cmd) {
    HAL_GPIO_WritePin(dev->dc_port, dev->dc_pin, GPIO_PIN_RESET); // Command Mode
    HAL_GPIO_WritePin(dev->ce_port, dev->ce_pin, GPIO_PIN_RESET); // Enable Chip
    HAL_SPI_Transmit(dev->hspi, &cmd, 1, 10);                     // Blocking TX (Fine for init)
    HAL_GPIO_WritePin(dev->ce_port, dev->ce_pin, GPIO_PIN_SET);   // Disable Chip
}

// --- Public API ---

void Nokia_Init(Nokia_t *dev, uint8_t contrast) {
    // Hardware Reset
    HAL_GPIO_WritePin(dev->rst_port, dev->rst_pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(dev->rst_port, dev->rst_pin, GPIO_PIN_SET);
    
    LCD_Write_Cmd(dev, 0x21); // Extended commands
    LCD_Write_Cmd(dev, 0x80 | contrast); // Contrast
    LCD_Write_Cmd(dev, 0x04); // Temp coefficient
    LCD_Write_Cmd(dev, 0x14); // LCD bias mode
    LCD_Write_Cmd(dev, 0x20); // Normal commands
    LCD_Write_Cmd(dev, 0x0C); // Normal display mode
    
    Nokia_Clear(dev, false);
}

void Nokia_Clear(Nokia_t *dev, bool isBlack) {
    uint8_t fill = isBlack ? 0xFF : 0x00;
    for (int i = 0; i < 504; i++) {
        dev->displayMap[i] = fill;
    }
}

// THE DMA MAGIC
void Nokia_Update_DMA(Nokia_t *dev) {
    // 1. Reset MCU pointer to 0,0
    LCD_Write_Cmd(dev, 0x80); 
    LCD_Write_Cmd(dev, 0x40); 
    
    // 2. Set to Data Mode
    HAL_GPIO_WritePin(dev->dc_port, dev->dc_pin, GPIO_PIN_SET); 
    HAL_GPIO_WritePin(dev->ce_port, dev->ce_pin, GPIO_PIN_RESET);
    
    // 3. Fire the DMA! The CPU immediately moves to the next line of code.
    HAL_SPI_Transmit_DMA(dev->hspi, dev->displayMap, 504);
}

// --- Graphics Math ---

void Nokia_SetPixel(Nokia_t *dev, int x, int y, bool isBlack) {
    if ((x >= 0) && (x < LCD_WIDTH) && (y >= 0) && (y < LCD_HEIGHT)) {
        int shift = y % 8;
        if (isBlack) dev->displayMap[x + (y/8)*LCD_WIDTH] |= 1<<shift;
        else         dev->displayMap[x + (y/8)*LCD_WIDTH] &= ~(1<<shift);
    }
}

void Nokia_SetStr(Nokia_t *dev, char *str, int x, int y, bool isBlack) {
    while (*str != 0x00) {
        int column; 
        for (int i=0; i<5; i++) {
            column = ASCII[(*str) - 0x20][i];
            for (int j=0; j<8; j++) {
                if (column & (0x01 << j)) Nokia_SetPixel(dev, x+i, y+j, isBlack);
                else                      Nokia_SetPixel(dev, x+i, y+j, !isBlack);
            }
        }
        x += 6; // Move right for next character
        str++;
    }
}

// ... Add your setLine and setRect functions here, updating 
// `setPixel1` to `Nokia_SetPixel(dev, x, y, isBlack)`