// **********************************************************************************
// Scan-I2C-WiFi
// **********************************************************************************
// Written by Charles-Henri Hallard (http://hallard.me)
//
// History : V1.00 2014-04-21 - First release
//         : V1.11 2015-09-23 - rewrite for ESP8266 target
//         : V1.20 2016-07-13 - Added new OLED Library and NeoPixelBus
//
// **********************************************************************************

#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <Wire.h>
#include "icons.h"
#include "fonts.h"

// ===========================================
// Setup your board configuration here
// ===========================================

// I2C Pins Settings
#define SDA_PIN D3
#define SDC_PIN D4

// Display Settings
// OLED will be checked with this address and this address+1
// so here 0x03c and 0x03d
#define I2C_DISPLAY_ADDRESS 0x3c
// Choose OLED Type (one only)
#define OLED_SSD1306
//#define OLED_SH1106

// ===========================================
// End of configuration
// ===========================================

#if defined (OLED_SSD1306)
#include <SSD1306Wire.h>
#include <OLEDDisplayUi.h>
#elif defined (OLED_SH1106)
#include <SH1106Wire.h>
#include <OLEDDisplayUi.h>
#endif


// Number of line to display for devices and Wifi
#define I2C_DISPLAY_DEVICE  4
#define WIFI_DISPLAY_NET    4
#ifdef OLED_SSD1306
SSD1306Wire  display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
#else
SH1106Wire  display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
#endif
OLEDDisplayUi ui( &display );

Ticker ticker;
Ticker tickerWifi;
bool readyForUpdate = false;  // flag to launch update (I2CScan)
bool readyForWifiUpdate = false;  // flag to launch update (I2CScan)


bool has_display          = false;  // if I2C display detected
uint8_t NumberOfI2CDevice = 0;      // number of I2C device detected
int8_t NumberOfNetwork    = 0;      // number of wifi networks detected

int8_t currentNumberOfNetwork    = 0;      // number of wifi networks detected
bool startNetwork = true;
int8_t delayCounter    = 0;      // number of wifi networks detected
int8_t delayCounterMax    = 126;      // number of wifi networks detected


char i2c_dev[I2C_DISPLAY_DEVICE][32]; // Array on string displayed


/* ======================================================================
  Function: i2cScan
  Purpose : scan I2C bus
  Input   : specifc address if looking for just 1 specific device
  Output  : number of I2C devices seens
  Comments: -
  ====================================================================== */
uint8_t i2c_scan(uint8_t address = 0xff)
{
  uint8_t error;
  int nDevices;
  uint8_t start = 1 ;
  uint8_t end   = 0x7F ;
  uint8_t index = 0;
  char device[16];
  char buffer[32];

  if (address >= start && address <= end) {
    start = address;
    end   = address + 1;
    Serial.print(F("Searching for device at address 0x"));
    Serial.printf("%02X ", address);
  } else {
    Serial.println(F("Scanning I2C bus ..."));
  }

  nDevices = 0;
  for (address = start; address < end; address++ ) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.printf("Device ");

      if (address == 0x40)
        strcpy(device, "TH02" );
      else if (address == 0x29 || address == 0x39 || address == 0x49)
        strcpy(device, "TSL2561" );
      else if (address == I2C_DISPLAY_ADDRESS || address == I2C_DISPLAY_ADDRESS + 1)
        strcpy(device, "OLED SSD1306" );
      else if (address == 0x64)
        strcpy(device, "ATSHA204" );
      else
        strcpy(device, "Unknown" );

      sprintf(buffer, "0x%02X : %s", address, device );
      if (index < I2C_DISPLAY_DEVICE) {
        strcpy(i2c_dev[index++], buffer );
      }

      Serial.println(buffer);
      nDevices++;
    }
    else if (error == 4)
    {
      Serial.printf("Unknow error at address 0x%02X", address);
    }

    yield();
  }
  if (nDevices == 0)
    Serial.println(F("No I2C devices found"));
  else
    Serial.printf("Scan done, %d device found\r\n", nDevices);

  return nDevices;
}


/* ======================================================================
  Function: drawProgress
  Purpose : prograss indication
  Input   : OLED display pointer
          percent of progress (0..100)
          String above progress bar
          String below progress bar
  Output  : -
  Comments: -
  ====================================================================== */
void drawProgress(OLEDDisplay *display, int percentage, String labeltop, String labelbot) {
  if (has_display) {
    display->clear();
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(Roboto_Condensed_Bold_Bold_16);
    display->drawString(64, 8, labeltop);
    display->drawProgressBar(10, 28, 108, 12, percentage);
    display->drawString(64, 48, labelbot);
    display->display();
  }
}

/* ======================================================================
  Function: drawProgress
  Purpose : prograss indication
  Input   : OLED display pointer
          percent of progress (0..100)
          String above progress bar
  Output  : -
  Comments: -
  ====================================================================== */
void drawProgress(OLEDDisplay *display, int percentage, String labeltop ) {
  drawProgress(display, percentage, labeltop, String(""));
}


/* ======================================================================
  Function: drawFrameNet
  Purpose : WiFi network info screen (called by OLED ui)
  Input   : OLED display pointer
  Output  : -
  Comments: -
  ====================================================================== */
void drawFrameNet(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  char buff[64];
  sprintf(buff, "%d Wifi Network", NumberOfNetwork);
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(Roboto_Condensed_Bold_Bold_16);
  display->drawString(x + 64, y + 0 , buff);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(Roboto_Condensed_12);


  for (int i = 0; i < WIFI_DISPLAY_NET; i++) {
    // Print SSID and RSSI for each network found
    if (i + currentNumberOfNetwork < NumberOfNetwork) {
      sprintf(buff, "%s %c", WiFi.SSID(i + currentNumberOfNetwork).c_str(), WiFi.encryptionType(i + currentNumberOfNetwork) == ENC_TYPE_NONE ? ' ' : '*' );
      display->drawString(x + 0, y + 16 + 12 * i, buff);
    }
  }

  ui.disableIndicator();
}

/* ======================================================================
  Function: drawFrameWifi
  Purpose : WiFi logo and IP address
  Input   : OLED display pointer
  Output  : -
  Comments: -
  ====================================================================== */
void drawFrameWifi(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(Roboto_Condensed_Bold_Bold_16);
  // see http://blog.squix.org/2015/05/esp8266-nodemcu-how-to-create-xbm.html
  // on how to create xbm files
  display->drawXbm( x + (128 - WiFi_width) / 2, 0, WiFi_width, WiFi_height, WiFi_bits);
  //display->drawString(x+ 64, WiFi_height+4, WiFi.localIP().toString());
  ui.disableIndicator();
}

/* ======================================================================
  Function: drawFrameLogo
  Purpose : Company logo info screen (called by OLED ui)
  Input   : OLED display pointer
  Output  : -
  Comments: -
  ====================================================================== */
void drawFrameLogo(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->clear();
  display->drawXbm(x + (128 - ch2i_width) / 2, y, ch2i_width, ch2i_height, ch2i_bits);
  ui.disableIndicator();
}

// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
FrameCallback frames[] = {drawFrameNet};
int numberOfFrames = 1;


/* ======================================================================
  Function: setReadyForUpdate
  Purpose : Called by ticker to tell main loop we need to update data
  Input   : -
  Output  : -
  Comments: -
  ====================================================================== */
void setReadyForUpdate() {
  Serial.println("Setting readyForUpdate to true");
  readyForUpdate = true;
}

void setReadyForWifiUpdate() {
  Serial.println("Setting readyForWifiUpdate to true");
  readyForWifiUpdate = true;
}


void wifiscan() {
  uint16_t progress = 0;
  Serial.println(F("WiFi scan start"));
  drawProgress(&display, progress, F("Scanning WiFi"));

  // WiFi.scanNetworks will return the number of networks found
  NumberOfNetwork = 0;
  WiFi.scanNetworks(true);
  // Async, wait start
  while (WiFi.scanNetworks() != WIFI_SCAN_RUNNING );

  do {
    progress++;
    if (progress > 100) {
      progress = 100;
    }
    drawProgress(&display, progress, F("Scanning WiFi"));

    NumberOfNetwork = WiFi.scanComplete();
    //Serial.printf("NumberOfNetwork=%d\n", NumberOfNetwork);
  } while (NumberOfNetwork == WIFI_SCAN_RUNNING || NumberOfNetwork == WIFI_SCAN_FAILED);

  Serial.println(F("scan done"));

}

void displayWifiLogo() {
  if (has_display) {
    Serial.println(F("Display found"));
    // initialize dispaly
    display.init();
    display.flipScreenVertically();
    display.clear();
    display.setFont(Roboto_Condensed_Bold_Bold_16);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawXbm( (128 - WiFi_width) / 2, 0, WiFi_width, WiFi_height, WiFi_bits);
    display.drawString( 64, WiFi_height + 4, "Wifi scanner");
    display.drawString( 64, WiFi_height + 4, "Wifi scanner");
    display.display();
    display.setContrast(255);
    delay(1000);
  }

}


/* ======================================================================
  Function: setup
  Purpose : you should know ;-)
  ====================================================================== */
void setup()
{
  uint8_t pbar = 0;

  Serial.begin(115200);
  Serial.print(F("\r\nBooting on "));
  Serial.println(ARDUINO_BOARD);

  //Wire.pins(SDA, SCL);
  Wire.begin(SDA_PIN, SDC_PIN);
  Wire.setClock(100000);

  if (i2c_scan(I2C_DISPLAY_ADDRESS)) {
    has_display = true;
  } else {
    if (i2c_scan(I2C_DISPLAY_ADDRESS + 1)) {
      has_display = true;
    }
  }

  displayWifiLogo();

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  delay(100);

  wifiscan();
  Serial.println(F("Setup done"));
  drawProgress(&display, 100, F("Setup Done"));


  if (has_display) {
    ui.setTargetFPS(30);
    //ui.setFrameAnimation(SLIDE_LEFT);
    ui.disableAutoTransition();
    ui.setFrames(frames, numberOfFrames);
    ui.init();
    display.flipScreenVertically();
  }

  ticker.attach(5, setReadyForUpdate);
  tickerWifi.attach(60, setReadyForWifiUpdate);
}

/* ======================================================================
  Function: loop
  Purpose : you should know ;-)
  ====================================================================== */
void loop()
{
  if (has_display) {
    int remainingTimeBudget = ui.update();

    if (remainingTimeBudget > 0) {
      // You can do some work here
      // Don't do stuff if you are below your
      // time budget.
      delay(remainingTimeBudget);
    }
  }

  if (readyForUpdate) {
    readyForUpdate = false;
    currentNumberOfNetwork += WIFI_DISPLAY_NET;
    if (currentNumberOfNetwork >= NumberOfNetwork) {
      currentNumberOfNetwork = 0;
    }
  }
  if (readyForWifiUpdate) {
    readyForWifiUpdate = false;
    wifiscan();
    displayWifiLogo();
    currentNumberOfNetwork = 0;
  }
}

