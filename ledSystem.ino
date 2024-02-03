 /************************** Libraries ***********************************/
#include "config.h"
#include <FastLED.h>
#include <WiFi.h> 
#include <TFT_eSPI.h>
#include <Servo.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BMP280.h>
#include "time.h"
/************************ Defines *******************************/

// Led:
#define LED_STRIP_PIN 2
#define NUM_LEDS    3
#define LED_STRIP_BTN_PIN 13
// Temp:
#define BMP_SCK  (13)
#define BMP_MISO (12)
#define BMP_MOSI (11)
#define BMP_CS   (10)

// Time:
const char* ntpServer = "0.be.pool.ntp.org";
const long  gmtOffset_sec = 2*3600;    // Timezone * 3600
const int   daylightOffset_sec = 0;    //Wintertime =3600

/************************ Variables *******************************/

bool onSetup = true; 
// Temp:
int minTemp = 15;
int maxTemp = 15;
float temp;
bool minMaxTempOrJustTempChanged = false;
float gradenHoek = 0.0f;

// LedStrip
int stripBrightness = 50;
bool stripOn = false;
bool buttonChanged = false;
int stripR = 255;
int stripG = 255;
int stripB = 255;
const char* hexColor = "ffffff";

//Debounce button
volatile unsigned long last_switch_time = 0;

// Objects
TFT_eSPI lcd = TFT_eSPI(); 
CRGB leds[NUM_LEDS];
Servo raam;
Adafruit_BMP280 bmp;
struct tm timeinfo;

// Adafruit feeds:
AdafruitIO_Feed *stripBrightnessFeed = io.feed("brightnessLedStrip");
AdafruitIO_Feed *stripBtnFeed = io.feed("btnLedStrip");
AdafruitIO_Feed *graphTempFeed = io.feed("tempgraph");
AdafruitIO_Feed *colorPicker = io.feed("test");
AdafruitIO_Feed *maxTempFeed = io.feed("maxTemp");
AdafruitIO_Feed *minTempFeed = io.feed("minTemp");
AdafruitIO_Feed *hoekRaamFeed = io.feed("hoekRaam");



/*******************************************************************************************************/

void setup() {
  Serial.begin(115200);
  pinMode(LED_STRIP_BTN_PIN, INPUT_PULLUP);
  attachInterrupt(LED_STRIP_BTN_PIN, ledStripBtnHandler, FALLING);
  lcd.init();
  lcd.setRotation(1);
  lcd.fillScreen(0xFFFF);
  lcd.setTextColor(TFT_BLACK);
  lcd.setTextSize(2);
  lcd.println("Getting connected");
  raam.attach(12);
  if (!bmp.begin(0x76)) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                      "try a different address!"));
    while (1) delay(10);
  }

  /* Default settings from datasheet. */
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
  //Set leds
  FastLED.addLeds<WS2812, LED_STRIP_PIN, GRB>(leds, NUM_LEDS);
  // wait for serial monitor to open
  while (! Serial);

  // connect to io.adafruit.com
  Serial.print("Connecting to Adafruit IO");
  io.connect();
  // Set handlers
  stripBtnFeed->onMessage(handleBtnStrip);
  stripBrightnessFeed->onMessage(handleSliderLedStripBrightness);
  colorPicker->onMessage(handleColorPicker);
  minTempFeed->onMessage(minTempHandler);
  maxTempFeed->onMessage(maxTempHandler);

  // wait for a connection
  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  lcd.println("Connected!");

  // Set time:
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  Serial.println();
  Serial.println(io.statusText());

  // Get feeds
  stripBtnFeed->get();
  stripBrightnessFeed->get();
  graphTempFeed->get();
  colorPicker->get();
  maxTempFeed->get();
  minTempFeed->get();
  hoekRaamFeed->get();

  lcd.println("Setting up...");
  maxTempFeed->save(maxTemp);
  delay(1500);
  minTempFeed->save(minTemp);
  delay(1500);
  stripBtnFeed->save(stripOn);
  delay(1500);
  stripBrightnessFeed->save(stripBrightness);
  delay(1500);
  temp = bmp.readTemperature();
  graphTempFeed->save(temp);
  delay(1500);
  colorPicker->save("#ffffff");
  delay(1500);
  hoekRaamFeed->save(gradenHoek);
  delay(1500);
  raam.write(0);
  onSetup = false;
  lcd.println("Ready!");
  delay(3000);
}

int lastSecondOnTimer;

void loop() 
{
  io.run();
  
  if(buttonChanged)
  {
    buttonChanged = false;
    stripOn ? stripBtnFeed->save("ON") : stripBtnFeed->save("OFF");
  }
  printTime();

  if(minMaxTempOrJustTempChanged){
    openWindow();
  }
}
void openWindow(){
  minMaxTempOrJustTempChanged = false;
    if( minTemp < maxTemp)
    {
      if( temp <= minTemp)
      {
        raam.write(0); 
        hoekRaamFeed->save(0);
      }
      else if (temp >= maxTemp)
      {
        raam.write(45*4);
        hoekRaamFeed->save(45);
      }
      else{
        gradenHoek = mapFloat(temp,minTemp,maxTemp,0,45);
        hoekRaamFeed->save(gradenHoek);
        raam.write(gradenHoek*4);
      }
    }
    else // Min gelijk aan max
    {
      if( temp < minTemp)
      {
        raam.write(0); 
        hoekRaamFeed->save(0);
        Serial.print("Maak hoek 0");
      }
      else if (temp >= maxTemp)
      {
        raam.write(45*4);
        hoekRaamFeed->save(45);
        Serial.println("Maak hoek 45");
      }
    }
}

void printTime(){
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to get time");
    return;
  }
  if(lastSecondOnTimer != timeinfo.tm_sec){
    lcd.setCursor(67, 10, 2);
    lcd.fillScreen(0xFFFF);
    lcd.println(&timeinfo, "%H:%M:%S");
    lcd.setCursor(70, 70, 2);
    lcd.print(temp);lcd.println(" °C");
    lastSecondOnTimer = timeinfo.tm_sec;

    if(timeinfo.tm_sec == 0)
    {
      float lastTemp = temp;
      temp = bmp.readTemperature();
      graphTempFeed->save(temp);
      if(lastTemp != temp)
      {
        minMaxTempOrJustTempChanged = true;
      }
    }
  }
}

// Strip brightness
void stripOnOff()
{
  if(stripOn)
  {
    fill_solid(leds, NUM_LEDS, CRGB(stripR, stripG, stripB));
    FastLED.setBrightness(stripBrightness);
  }
  else{
    FastLED.clear();
  }
  FastLED.show();
}

void getRGB(String hexvalue) {
  hexvalue.toUpperCase();
  char c[7];
  hexvalue.toCharArray(c, 8);
  stripR = hexcolorToInt(c[1], c[2]);
  stripG = hexcolorToInt(c[3], c[4]);
  stripB = hexcolorToInt(c[5], c[6]);
}

int hexcolorToInt(char upper, char lower)
{
  int uVal = (int)upper;
  int lVal = (int)lower;
  uVal = uVal > 64 ? uVal - 55 : uVal - 48;
  uVal = uVal << 4;
  lVal = lVal > 64 ? lVal - 55 : lVal - 48;
  return uVal + lVal;
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// HANDLERS
/////////////////////////////////////
void minTempHandler(AdafruitIO_Data *data)
{
  if(!onSetup)
  {
    minTemp = data->toInt();
    Serial.println("Got minTemp");
    if(maxTemp < minTemp)
    {
      minTemp = maxTemp;
      minTempFeed->save(minTemp);
    } 
    minMaxTempOrJustTempChanged = true;
  }
}

void maxTempHandler(AdafruitIO_Data *data)
{
  if(!onSetup)
  {
    maxTemp = data->toInt();
    Serial.println("Got maxTemp");
    if(maxTemp < minTemp)
    {
      maxTemp = minTemp;
      maxTempFeed->save(minTemp);
    } 
    minMaxTempOrJustTempChanged = true;
  }
}

void handleBtnStrip(AdafruitIO_Data *data) 
{
  if(!onSetup)
  {
    if(data->toString() == "ON")
    {
      Serial.println("Got button");
      stripOn = true;
    }
    else
    {
      Serial.println("Got button");
      stripOn = false;
    }
    stripOnOff();
  } 
}


void handleSliderLedStripBrightness(AdafruitIO_Data *data)
{
  if(!onSetup)
  {
    stripBrightness = data->toInt() * 2.55;
    Serial.println("Got brightness");
    stripOnOff();
  }
}

void handleColorPicker(AdafruitIO_Data *data)
{
  if(!onSetup)
  {
    getRGB(data->toString());
    Serial.println("Got color");
    stripOnOff();
  }
}

void ledStripBtnHandler()
{
  unsigned long interrupt_time = millis();
  if (interrupt_time - last_switch_time > 250)
  {
    stripOn = !stripOn;
    buttonChanged = true;
  }
  last_switch_time = interrupt_time;
}