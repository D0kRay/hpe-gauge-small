
/*******************************************************************************
  Configuration：
    Environment:V2.0.17
    Development Boards:ESP32S3 DEV Module
    USB CDC On Boot:Enabled
    Flash Mode:QIO 80MHZ
    Flash Size:16MB(128Mb)
    Partition Scheme:16M Flash(3MB APP/9.9MB FATFA)
    PSRAM:OPI PSRAM
  Code process:
    1. Print development board flash and PSRAM information
    2. Open the AP mode of WiFi and output the account password through the serial port
    3. Initialize the state of WS2812
    4. Initialize IIC
    5. Initialize and check if the TCA6408 expansion chip exists
    6. Initialization detection of QMI8658 IMU chip
    7. Initialize detection PCF85063 RTC clock chip
    8. When printing battery voltage, there is a significant change when the battery is not connected, and the battery capacity is small. When charging, the battery voltage will be pulled up, causing inaccuracy. Attention should be paid
    9. Touch chip detection and initialization
    10. TF card detection shows that video playback is open, but LVGL initialization does not exist
    11. Create an RTOS task, the task is as follows
        -Main task: TF card needs to open video loop playback, and there must be a specified video file in the TF card
        -TCA6408 interrupts task execution, corresponding to touch, IMU, Button, RTC, USB power supply detection status, serial port output
        -Serial port output RTC time, executed once every 10 seconds
        -Serial port output IMU acceleration data, executed once every 1 second
        -WS1282 task, alternating between red and green, executed once every 1 second
  Library & Version:
        Arduino_GFX_Library   1.4.7  -  Note that the old and new versions are not compatible
        SPI   2.0.0
        Wire   2.0.0  -  Note that the old and new versions are not compatible
        lvgl   8.3.6
        WiFi   2.0.0
        Ticker   2.0.0
        Adafruit_NeoPixel   1.12.5
        SD   2.0.0
        FS   2.0.0
        JPEGDEC   1.4.2

  author:JXL
  Data:25-03-25
  version:V1.0.0
 ******************************************************************************/

#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include "gui_guider.h"
#include "events_init.h"
#include "custom.h"
#include <Wire.h>
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include<WiFi.h>
#include <Ticker.h>
#include <Adafruit_NeoPixel.h>
#include "QMI8658.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <SD.h>
#include <SPI.h>
#include "MjpegClass.h"

//WIFI Information
const char *ssid = "JXL-ESP32S3-SPI";
const char *password = "JXL-ESP32S3";  //More than 8 digits

//WS2812
#define WS2812Pin 46
#define WS2812_Count 2
Adafruit_NeoPixel strip(WS2812_Count, WS2812Pin, NEO_GRB + NEO_KHZ800);

//IIC
#define SCL 9
#define SDA 8

//PCF85063
#define RtcInt 45
#define PCF85063I2CAddr 0x51
#define PCF85063RamReg 0x03

//Battery ADC
#define BatPin 1
#define ADC_MaxValue 4095
#define ADC_RefVoltage 3.3
#define ADC_Magnification 2

//IMU QMI8658
#define IMUInt1 12
#define IMUInt2 13

//SW
#define SW_UP 14
#define SW_PW 15
#define SW_Down 16

//USB Power Detecting pins
#define P_ST 17

//CST816
#define TouchRST 0
#define TouchInt 11
#define TouchI2CAddr 0x15

#define ChipIdRegister 0xA7
#define CST716ChipId 0X20
#define CST816SChipId 0XB4
#define CST816TChipId 0XB5
#define CST816DChipId 0XB6
#define CST826ChipId 0X11
#define CST830ChipId 0X12
#define CST836UChipId 0X13

unsigned char ChipID = 0x00;

volatile bool TouchEventFlag = false;

/*mjpeg & SD Card*/
#define MJPEG_FILENAME "/240_30fps.mjpeg"
//If PSRAM is not enough, you don't need to be so large
#define MJPEG_BUFFER_SIZE (240 * 240 * 60)
uint8_t jpegBuffer[MJPEG_BUFFER_SIZE];
int jpegSize;
static MjpegClass mjpeg;

File vFile;
bool PlayStatus = true;

#define MISO     48
#define SCK    41
#define MOSI    47
#define SD_CS   40

/*GC9A01*/
#define CONFIG_IDF_TARGET_ESP32S3
#define GFX_BL 42
#define BL_Freq 5000
unsigned int BL_Brightness = 255;
Arduino_DataBus *bus = new Arduino_ESP32SPI(18 /* DC */, 2 /* CS */, 3 /* SCK */, 10 /* MOSI */, -1 /* MISO */, HSPI /* spi_num */);
Arduino_GFX *gfx = new Arduino_GC9A01(bus, 21 /* RST */, 0 /* rotation */, true /* IPS */);

//Define LVGL resolution and other parameters
static uint32_t screenWidth = 240;
static uint32_t screenHeight = 240;
//Defining Buffers
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf = (lv_color_t *)malloc(sizeof(lv_color_t) * screenWidth * 10);
//Used when initializing the display
static lv_disp_drv_t disp_drv;
//Used when initializing touch
static lv_indev_drv_t indev_drv;
//The structure contains all screens and components, which is essential and cannot be placed in setup. Use this pointer to find any object in the program
lv_ui guider_ui;

static int displayBack(JPEGDRAW *pDraw)
{
  gfx->draw16bitBeRGBBitmap(pDraw->x, pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
  return 1;
}

//Display fill Associated with LCD driver
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
   uint32_t w = (area->x2 - area->x1 + 1);
   uint32_t h = (area->y2 - area->y1 + 1);

   gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);

   lv_disp_flush_ready(disp);
}

void my_touch_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
  //Store the pressed coordinates and status
  lv_coord_t last_x = 0;
  lv_coord_t last_y = 0;
  unsigned int X_H4 = 0;
  unsigned int X_L8 = 0;
  unsigned int Y_H4 = 0;
  unsigned int Y_L8 = 0;

  //True if there is touch, false otherwise
  if(TouchEventFlag) {
      Wire.beginTransmission(TouchI2CAddr);
      Wire.write(0x03);
      Wire.endTransmission(false);
      //Wire.beginTransmission(TouchI2CAddr);
      Wire.requestFrom(TouchI2CAddr, 1 ,true);
      X_H4 = Wire.read();

      Wire.beginTransmission(TouchI2CAddr);
      Wire.write(0x04);
      Wire.endTransmission(false);
      //Wire.beginTransmission(TouchI2CAddr);
      Wire.requestFrom(TouchI2CAddr, 1 ,true);
      X_L8 = Wire.read();

      Wire.beginTransmission(TouchI2CAddr);
      Wire.write(0x05);
      Wire.endTransmission(false);
      //Wire.beginTransmission(TouchI2CAddr);
      Wire.requestFrom(TouchI2CAddr, 1 ,true);
      Y_H4 = Wire.read();

      Wire.beginTransmission(TouchI2CAddr);
      Wire.write(0x06);
      Wire.endTransmission(false);
      //Wire.beginTransmission(TouchI2CAddr);
      Wire.requestFrom(TouchI2CAddr, 1 ,true);
      Y_L8 = Wire.read();

      //last_x = 0xFF - (X_H4 << 8 | X_L8)&0X0FFF;
      last_x = (X_H4 << 8 | X_L8)&0X0FFF;
      last_y = (Y_H4 << 8 | Y_L8)&0X0FFF;
      data->point.x = last_x;
      data->point.y = last_y;

      TouchEventFlag = false;
      //Serial.printf("Touch Point:%02X , %02X \r\n",last_x,last_y);
      data->state = LV_INDEV_STATE_PR;
  }
  else {
      data->state = LV_INDEV_STATE_REL;
  }

}

void SystemInfo()
{
  uint32_t flash_size = 0;
  esp_flash_get_size(NULL, &flash_size);
  Serial.printf("\r\nFlash Size: %d bytes (%.2f MB)", flash_size, flash_size / (1024.0 * 1024.0));
  size_t psram_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  if (psram_size > 0) {
    Serial.printf("\r\nPSRAM Size: %d bytes (%.2f MB)", psram_size, psram_size / (1024.0 * 1024.0));
  } else {
    Serial.println("No PSRAM detected");
  }
  delay(10);
  }

void WifiTest()
{
  //1 = STA  0=AP
  #if 0
    Serial.print("\r\nStarting STA");
    bool setHostname("JXL_ESP32-S3");
    WiFi.begin(ssid,password);
    int WifiCount = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
      WifiCount++;
      delay(500);
      Serial.print(".");
      if(WifiCount>30) break;
    }
    if(WiFi.status() != WL_CONNECTED)
    {
      Serial.print("\r\nwifi not connected!");
      }
    else
    {
      Serial.print("\r\nWiFi connected!");
      Serial.print("\r\nIP address: ");
      Serial.print(WiFi.localIP());
      }
  #else
    Serial.print("\r\nStarting AP");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid,password);
    delay(500);
    Serial.printf("\r\nWiFi SSID:%s  WiFi Password:%s",ssid,password);
  #endif
  }

void SWInit()
{
  pinMode(SW_UP,INPUT_PULLUP);
  pinMode(SW_PW,INPUT_PULLUP);
  pinMode(SW_Down,INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SW_UP), SW_UPHandleInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW_PW), SW_PWHandleInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW_Down), SW_DownHandleInterrupt, FALLING);
  }

void USBDetectionConfig()
{
  pinMode(P_ST,INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(P_ST), USBDetectionHandleInterrupt, FALLING);
  }

void WS2812Init()
{
  strip.begin();   //Initializing the WS2812
  strip.clear();   //Clear All LEDs
  strip.show();   //Turn off all lights
  //Set the first light to red (R, G, B)
  strip.setPixelColor(0, strip.Color(20, 0, 0));
  //Set the second light to green
  strip.setPixelColor(1, strip.Color(0, 20, 0));
  strip.show();   //Send data to update the lamp beads
  }

void IICInit()
{
  Wire.begin();
  Wire.setClock(400000);
  delay(10);
  }

void QMI8658Init()
{
  pinMode(IMUInt1,INPUT_PULLUP);
  pinMode(IMUInt2,INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(IMUInt1), IMUHandleInterrupt1, FALLING);
  attachInterrupt(digitalPinToInterrupt(IMUInt2), IMUHandleInterrupt2, FALLING);

  QMI8658_init();
  delay(10);
}

void PCF85063Init()
{
  pinMode(RtcInt,INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RtcInt), RtcIntHandleInterrupt, FALLING);
  
  int PCF85063TempData = 0;
  Wire.beginTransmission(PCF85063I2CAddr);
  Wire.write(PCF85063RamReg);
  Wire.write(0x55);
  Wire.endTransmission(true);

  delay(10);

  Wire.beginTransmission(PCF85063I2CAddr);
  Wire.write(PCF85063RamReg);
  Wire.endTransmission(false);
  Wire.requestFrom(PCF85063I2CAddr, 1 ,true);
  PCF85063TempData = Wire.read();

  if(PCF85063TempData == 0x55) Serial.print("\r\nPCF85063 pass!");
  else Serial.print("\r\nPCF85063 fail!");

  delay(10);

  //0x00 register default parameters
  //0x01 register setting enables one minute interrupt
  Wire.beginTransmission(PCF85063I2CAddr);
  Wire.write(0x01);
  Wire.write(0x20);
  Wire.endTransmission(true);


//  //Setting the time
//  int Year = 25;
//  int Months = 3;
//  int Weekdays = 1;   //unday is 0
//  int Days = 24;
//  int Hours = 20;
//  int Minutes = 49;
//  int Seconds = 0;
//  
//  Wire.beginTransmission(PCF85063I2CAddr);
//  Wire.write(0x0A);
//  Wire.write((((Year/10)<<4)&0xf0) | ((Year%10)&0x0f));
//  Wire.endTransmission(true);
//  delay(10);
//  Wire.beginTransmission(PCF85063I2CAddr);
//  Wire.write(0x09);
//  Wire.write((((Months/10)<<4)&0x10) | ((Months%10)&0x0f));
//  Wire.endTransmission(true);
//  delay(10);
//  Wire.beginTransmission(PCF85063I2CAddr);
//  Wire.write(0x08);
//  Wire.write(Weekdays);
//  Wire.endTransmission(true);
//  delay(10);
//  Wire.beginTransmission(PCF85063I2CAddr);
//  Wire.write(0x07);
//  Wire.write((((Days/10)<<4)&0x30) | ((Days%10)&0x0f));
//  Wire.endTransmission(true);
//  delay(10);
//  Wire.beginTransmission(PCF85063I2CAddr);
//  Wire.write(0x06);
//  Wire.write((((Hours/10)<<4)&0x30) | ((Hours%10)&0x0f));
//  Wire.endTransmission(true);
//  delay(10);
//  Wire.beginTransmission(PCF85063I2CAddr);
//  Wire.write(0x05);
//  Wire.write((((Minutes/10)<<4)&0x70) | ((Minutes%10)&0x0f));
//  Wire.endTransmission(true);

  delay(10);
  }

void BatInfo()
{
  int adcRaw = analogRead(BatPin);   //Read ADC raw value
  float voltage = (adcRaw / (float)ADC_MaxValue) * ADC_RefVoltage;   //Calculate the voltage after voltage division
  float realVoltage = voltage * 2;  //Restore the original voltage (before voltage division)
  Serial.print("\r\nBattery Voltage:");
  Serial.print(realVoltage, 2);
  Serial.print("V");
  }

void TouchInit()
{
  //Reset Touch
  pinMode(TouchRST, OUTPUT);
  pinMode(TouchInt,INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TouchInt), TouchHandleInterrupt, FALLING);
  digitalWrite(TouchRST, LOW);
  delay(10);
  digitalWrite(TouchRST, HIGH);
  delay(50);

  Wire.beginTransmission(TouchI2CAddr);
  Wire.write(ChipIdRegister);
  Wire.endTransmission(false);
  //Wire.beginTransmission(TouchI2CAddr);
  Wire.requestFrom(TouchI2CAddr, 1 ,true);
  ChipID = Wire.read();

  Serial.printf("\r\nTouchChipID: 0x%02X",ChipID);
  if(ChipID == CST716ChipId) Serial.println(",Touch chip model :CST716");
  else if(ChipID == CST816SChipId) Serial.println(",Touch chip model :CST816S");
  else if(ChipID == CST816TChipId) Serial.println(",Touch chip model :CST816T");
  else if(ChipID == CST816DChipId) Serial.println(",Touch chip model :CST816D");
  else if(ChipID == CST826ChipId) Serial.println(",Touch chip model :CST826");
  else if(ChipID == CST830ChipId) Serial.println(",Touch chip model :CST830");
  else if(ChipID == CST836UChipId) Serial.println(",Touch chip model :CST836U");
  else Serial.println(",error!");
}

void Display()
{
  //Display Initialization
  gfx->begin();   //Initialize LCD
  gfx->fillScreen(BLACK);   //Background color Black
  //Backlight settings
#ifdef GFX_BL
  ledcSetup(1, BL_Freq, 8);
  ledcAttachPin(GFX_BL, 1);
  ledcWrite(1, BL_Brightness);
#endif
  pinMode(SD_CS,INPUT_PULLUP);
  delay(10);
  SPI.begin(SCK, MISO, MOSI, SD_CS);
  delay(10);
  if (!SD.begin(SD_CS, SPI, 80000000))   //1-bit SD bus mode
  {
    Serial.println(F("ERROR: SD card mount failed!"));
    lv_init();   //lvgl system initialization
    //Buffer
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, screenWidth * 10);
    //Initialize the display
    lv_disp_drv_init(&disp_drv);
    //Change the following line to your display resolution
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
  
    //Initialize the (fake) input device driver
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;   //Select the input device type as touch
    indev_drv.read_cb = my_touch_read;   //Add callback function/interrupt callback
    lv_indev_drv_register(&indev_drv);
  
    setup_ui(&guider_ui);   //Initialize the UI interface
    events_init(&guider_ui);   //Event Initialization
    custom_init(&guider_ui);   //Run custom code, such as associating actual output with the value displayed in the GUI
  
    Serial.println("Setup done");
    }
  else
  {
    PlayStatus = false;
    }
  }

void RTOSInit()
{
      //(Task function, task name, stack size in bytes, task parameters, priority, task handle, core)
      xTaskCreatePinnedToCore(mainTask, "mainTask", 15000, NULL, 1, NULL, 1);
      xTaskCreatePinnedToCore(PCF85063RTC_Task, "PCF85063_RTC", 6000, NULL, 2, NULL, 0);
      xTaskCreatePinnedToCore(QMI8658IMU_Task, "QMI8658IMU_Task", 6000, NULL, 3, NULL, 0);
      xTaskCreatePinnedToCore(WS2812Task, "WS2812Task", 6000, NULL, 4, NULL, 0);
  }

void TouchHandleInterrupt(void)
{ 
  TouchEventFlag = true;
  }

void IMUHandleInterrupt1(void)
{
  Serial.print("\r\nIMU Int1");
  }

void IMUHandleInterrupt2(void)
{
  Serial.print("\r\nIMU Int2");
  }

void SW_UPHandleInterrupt(void)
{
  Serial.print("\r\nSW UP Int");
  }

void SW_PWHandleInterrupt(void)
{
  Serial.print("\r\nSW PW Int");
  }
  
void SW_DownHandleInterrupt(void)
{
  Serial.print("\r\nSW Down Int");
  }

void USBDetectionHandleInterrupt(void)
{
  Serial.print("\r\nCharging...");
  }

void RtcIntHandleInterrupt(void)
{
  Serial.print("\r\nRTC Int");
  }

void setup(void)
{
  Serial.begin(115200);
  /*Development board information printing*/
  SystemInfo();
  /*WiFi test*/
  WifiTest();
  /*Initialization button*/
  SWInit();
  /*USB insertion detection initialization*/
  USBDetectionConfig();
  /*Initialize WS2812 status*/
  WS2812Init();
  /*Initialize the bus for IIC devices*/
  IICInit();
  /*IMU chip QMI8658 detection and initialization*/
  QMI8658Init();
  /*PCF85063 RTC clock chip detection and initialization*/
  PCF85063Init();
  /*Battery voltage information printing*/
  BatInfo();
  /*Touch detection and initialization*/
  TouchInit();
  /*TF card detection/LVGL display touch...*/
  Display();
  /*RTOS task creation*/
  RTOSInit();
  delay(100);
}

void loop()
{
  
}

void mainTask(void *parameter)
{
  TickType_t lastWakeTime = xTaskGetTickCount();   //Get the current Tick
  while(1)
  {
    if(PlayStatus)
    {
      lv_timer_handler(); /* let the GUI do its work */
      //Serial.printf("\r\nmainTask: %d", uxTaskGetStackHighWaterMark(NULL));
      vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(5));
      }
    else
    {
      uint8_t *mjpeg_buf = (uint8_t *)ps_malloc(MJPEG_BUFFER_SIZE);
      Start:
      vFile = SD.open(MJPEG_FILENAME);
      if (!vFile || vFile.isDirectory())
      {
        Serial.println(F("ERROR: Failed to open " MJPEG_FILENAME " file for reading"));
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(1000));
        esp_restart();
        }
      else
      {
        mjpeg.setup(&vFile, mjpeg_buf, displayBack , true , 0 , 0 , gfx->width() /* widthLimit */, gfx->height());
        Serial.println(F("MJPEG video start"));
        while (vFile.available() && mjpeg.readMjpegBuf())
        {
          // Play video
          mjpeg.drawJpg();
          //Serial.printf("\r\nmainTask: %d", uxTaskGetStackHighWaterMark(NULL));
          vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(33));
          }
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(10));
        Serial.println(F("MJPEG video end"));
        vFile.close();
        goto Start;
        }
     }
    }
  vTaskDelete( NULL );
  }

void PCF85063RTC_Task(void *parameter)
{
  TickType_t lastWakeTime = xTaskGetTickCount();

  while(1)
  {
    Wire.beginTransmission(PCF85063I2CAddr);
    Wire.write(0x0A);
    Wire.endTransmission(false);
    Wire.requestFrom(PCF85063I2CAddr, 1 ,true);
    int PCF85063Years = Wire.read();
    PCF85063Years = ((PCF85063Years&0xf0)>>4)*10+(PCF85063Years&0x0f);
  
    Wire.beginTransmission(PCF85063I2CAddr);
    Wire.write(0x09);
    Wire.endTransmission(false);
    Wire.requestFrom(PCF85063I2CAddr, 1 ,true);
    int PCF85063Months = Wire.read();
    PCF85063Months = ((PCF85063Months&0x10)>>4)*10+(PCF85063Months&0x0f);
  
    Wire.beginTransmission(PCF85063I2CAddr);
    Wire.write(0x08);
    Wire.endTransmission(false);
    Wire.requestFrom(PCF85063I2CAddr, 1 ,true);
    int PCF85063Weekdays = Wire.read();
    PCF85063Weekdays = (PCF85063Weekdays>0)? PCF85063Weekdays:7;
  
    Wire.beginTransmission(PCF85063I2CAddr);
    Wire.write(0x07);
    Wire.endTransmission(false);
    Wire.requestFrom(PCF85063I2CAddr, 1 ,true);
    int PCF85063Days = Wire.read();
    PCF85063Days = ((PCF85063Days&0x30)>>4)*10+(PCF85063Days&0x0f);
  
    Wire.beginTransmission(PCF85063I2CAddr);
    Wire.write(0x06);
    Wire.endTransmission(false);
    Wire.requestFrom(PCF85063I2CAddr, 1 ,true);
    int PCF85063Hours = Wire.read();
    PCF85063Hours = ((PCF85063Hours&0x30)>>4)*10+(PCF85063Hours&0x0f);
  
    Wire.beginTransmission(PCF85063I2CAddr);
    Wire.write(0x05);
    Wire.endTransmission(false);
    Wire.requestFrom(PCF85063I2CAddr, 1 ,true);
    int PCF85063Minutes = Wire.read();
    PCF85063Minutes = ((PCF85063Minutes&0x70)>>4)*10+(PCF85063Minutes&0x0f);
  
    Wire.beginTransmission(PCF85063I2CAddr);
    Wire.write(0x04);
    Wire.endTransmission(false);
    Wire.requestFrom(PCF85063I2CAddr, 1 ,true);
    int PCF85063Seconds = Wire.read();
    PCF85063Seconds = ((PCF85063Seconds&0x70)>>4)*10+(PCF85063Seconds&0x0f);
  
    Serial.printf("\r\n20%02d-%02d-%02d %02d:%02d:%02d",PCF85063Years,PCF85063Months,PCF85063Days,PCF85063Hours,PCF85063Minutes,PCF85063Seconds);

    //Serial.printf("\r\nPCF85063RTC_Task: %d", uxTaskGetStackHighWaterMark(NULL));
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(10000));
    }
  vTaskDelete( NULL );
  }

void QMI8658IMU_Task(void *parameter)
{
  TickType_t lastWakeTime = xTaskGetTickCount();
  float acc[3], gyro[3];
  unsigned int tim_count = 0;
  uint16_t result;
  while(1)
  {
    QMI8658_read_xyz(acc, gyro, &tim_count);
    //Serial.printf("acc_x:%.2f  acc_y:%.2f  acc_z:%.2f  \r\ngyro_x:%.2f  gyro_y:%.2f  gyro_z:%.2f  \r\n",acc[0],acc[1],acc[2],gyro[0],gyro[1],gyro[2]);
    Serial.printf("\r\nacc_x:%.2f  acc_y:%.2f  acc_z:%.2f",acc[0],acc[1],acc[2]);
    //Serial.printf("\r\nQMI8658IMU_Task: %d", uxTaskGetStackHighWaterMark(NULL));
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(1000));
    }
  vTaskDelete( NULL );
  }

void WS2812Task(void *parameter)
{
  TickType_t lastWakeTime = xTaskGetTickCount();
  while(1)
  {
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(1000));
    //Set the first light to green (R, G, B)
    strip.setPixelColor(0, strip.Color(0, 20, 0));
    //Set the second light to red
    strip.setPixelColor(1, strip.Color(20, 0, 0));
    strip.show();
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(1000));
    //Set the first light to red (R, G, B)
    strip.setPixelColor(0, strip.Color(20, 0, 0));
    //Set the second light to green
    strip.setPixelColor(1, strip.Color(0, 20, 0));
    strip.show();
    //Serial.printf("\r\nWS2812Task: %d", uxTaskGetStackHighWaterMark(NULL));
    }
  vTaskDelete( NULL );
  }
  
