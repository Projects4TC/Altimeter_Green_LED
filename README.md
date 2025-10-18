//I will add more items about the wiring attachment here.
The screen size I was working with is 
#define TFT_WIDTH  240 
#define TFT_HEIGHT 280
ST7789 Library LCD screen from WaveShare fits perfectly on those 1-2.5 meter size planes.
I will try to post the TFT_eSPI User setup file here as well in case you want to copy my wiring, or further pins to make sense when starting your own version with updates.

// ###### EDIT THE PIN NUMBERS IN THE LINES FOLLOWING TO SUIT YOUR ESP32 SETUP   ######

// For ESP32 Dev board (only tested with ILI9341 display)
// The hardware SPI can be mapped to any pins

//#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS  4 // Chip select control pin
#define TFT_DC  5 // Data Command control pin
#define TFT_RST  19 // Reset pin (could connect to RST pin)
//#define TFT_RST  -1  // Set TFT_RST to -1 if display RESET is connected to ESP32 board RST
