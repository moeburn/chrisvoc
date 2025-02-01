//#include <FS.h>
//using fs::FS;

#include <TFT_eSPI.h>
#include <SPI.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#include <Arduino.h>
#include <Wire.h>
#include <FS.h>
#include <NOxGasIndexAlgorithm.h>
#include <SensirionI2CSgp41.h>
#include <SensirionI2cSht4x.h>
#include <VOCGasIndexAlgorithm.h>
#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>

#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <ArduinoOTA.h>
#include <Preferences.h>


Preferences preferences;
char auth[] = "eT_7FL7IUpqonthsAr-58uTK_-su_GYy";  //BLYNK
const char *ssid = "mikesnet";
const char *password = "springchicken";
#include <WiFiManager.h>
WiFiManager wm;
WebServer server(80);

#define bkl_pin 2
#define ldr_pin 0

#define button1 3
#define button2 21
#define button3 20
#define button4 7
#define button5 5
#define maxArray 501
bool editinterval = false;
bool editcalib = false;
bool editbright = false;
bool editauto = false;
bool menumode = false;
bool mainmenu = true;
float HCOffset = -0.7;
float tempOffset = 0.0;
int newldr;
int sampleSecs = 30;
int sampleMilliSecs = sampleSecs * 1000;
int brightness = 20;
bool autobright = true;
bool wifisaved = false;
int MENU_MAX = 7;
float array1[maxArray];
float array2[maxArray];
float array3[maxArray];
float array4[maxArray];
uint8_t rng, gng, bng;
uint8_t rvg, gvg, bvg;

SensirionI2cSht4x sht4x;
SensirionI2CSgp41 sgp41;
Adafruit_MPU6050 mpu;
VOCGasIndexAlgorithm voc_algorithm;
NOxGasIndexAlgorithm nox_algorithm;
float minVal = 3.9;
float maxVal = 4.2;
int readingCount = 0;  // Counter for the number of readings
int ldr_read;
int button = 5;
int menusel = 1;
// time in seconds needed for NOx conditioning
uint16_t conditioning_s = 10;

char errorMessage[256];
uint16_t error;
float humidity = 0;     // %RH
float temperature = 0;  // degreeC
uint16_t srawVoc = 0;
uint16_t srawNox = 0;
uint16_t defaultCompenstaionRh = 0x8000;  // in ticks as defined by SGP41
uint16_t defaultCompenstaionT = 0x6666;   // in ticks as defined by SGP41
uint16_t compensationRh = 0;              // in ticks as defined by SGP41
uint16_t compensationT = 0;               // in ticks as defined by SGP41
int32_t voc_index = voc_algorithm.process(srawVoc);
int32_t nox_index = nox_algorithm.process(srawNox);
float tempSHT, humSHT;
unsigned long buttonTimer;
TFT_eSPI tft = TFT_eSPI();  //for TFT display
TFT_eSprite img = TFT_eSprite(&tft);
uint8_t redVoc, greenVoc, blueVoc;
uint8_t redNox, greenNox, blueNox;

#define every(interval) \
  static uint32_t __every__##interval = millis(); \
  if (millis() - __every__##interval >= interval && (__every__##interval = millis()))


uint16_t RGBto565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r / 8) << 11) | ((g / 4) << 5) | (b / 8);
}

float scalingFactor = 1.2;

void getVocColor(int voc_index, uint8_t &r, uint8_t &g, uint8_t &b) {
  if (voc_index < 150) {
    // Fully green
    r = 0;
    g = 255;
    b = 0;
  } else if (voc_index < 250) {
    // Transition from green to yellow (blend in red)
    r = map(voc_index, 150, 250, 0, 255);
    g = 255;
    b = 0;
  } else if (voc_index < 400) {
    // Transition from yellow to orange (reduce green slightly)
    r = 255;
    g = map(voc_index, 250, 400, 255, 128);
    b = 0;
  } else {
    // Transition from orange to full red
    r = 255;
    g = map(voc_index, 400, 500, 128, 0);
    b = 0;
  }
}

void getNoxColor(int nox_index, uint8_t &r, uint8_t &g, uint8_t &b) {
  if (nox_index <= 1) {
    // Fully green
    r = 0;
    g = 255;
    b = 0;
  } else if (nox_index <= 20) {
    // Transition from green to yellow (blend in red)
    r = map(nox_index, 2, 20, 0, 255);
    g = 255;
    b = 0;
  } else if (nox_index <= 100) {
    // Transition from yellow to orange (reduce green slightly)
    r = 255;
    g = map(nox_index, 20, 100, 255, 128);
    b = 0;
  } else {
    // Above 100: Fully red
    r = 255;
    g = 0;
    b = 0;
  }
}

// Function to calculate the contrasting text color (black or white)
uint16_t getContrastTextColor(uint8_t r, uint8_t g, uint8_t b) {
    float luminance = 0.299 * r + 0.587 * g + 0.114 * b;
    return (luminance > 128) ? TFT_BLACK : TFT_WHITE;
}

void doMainDisplay() {

  img.fillSprite(TFT_BLACK);


  img.setTextSize(2);                                    // Font size 2 (16px)
  img.setCursor(24 * scalingFactor, 2 * scalingFactor);  // Adjusted to fit top-left quadrant
  img.setTextColor(TFT_PINK);
  img.println("Temp:");
  img.setTextColor(TFT_WHITE);
  // img.setTextSize(1);
  // img.println(ldr_read);
  // img.print(newldr);
  img.setCursor(6 * scalingFactor, 40 * scalingFactor);  // Centered vertically in quadrant
  img.setTextSize(3);                                    // Font size 3 (24px)
  img.print(tempSHT, 1);
  img.print((char)247);
  img.print("c");

  // Quadrant 2: Top-right
  img.setTextSize(2);                                     // Font size 2 (16px)
  img.setCursor(128 * scalingFactor, 2 * scalingFactor);  // Adjusted to fit top-right quadrant
  img.setTextColor(TFT_CYAN);
  img.print("Hum:");
  img.setTextColor(TFT_WHITE);
  img.setCursor(122 * scalingFactor, 40 * scalingFactor);  // Centered vertically in quadrant
  img.setTextSize(3);                                      // Font size 3 (24px)
  img.print(humidity, 1);
  img.print("%");
  

  // Quadrant 3: Bottom-left
  getVocColor(voc_index, redVoc, greenVoc, blueVoc);
  uint16_t textColor1 = getContrastTextColor(redVoc, greenVoc, blueVoc);
  img.setTextColor(textColor1);
  img.fillRect(0, 120, 120, 240, RGBto565(redVoc, greenVoc, blueVoc));
  img.setTextSize(2);                                      // Font size 2 (16px)
  img.setCursor(28 * scalingFactor, 108 * scalingFactor);  // Adjusted for bottom-left quadrant
  img.print("VOC:");
  img.setCursor(20 * scalingFactor, 140 * scalingFactor);  // Centered vertically in quadrant
  img.setTextSize(4);                                      // Font size 3 (24px)
  img.print(voc_index, 1);


  // Quadrant 4: Bottom-right
  img.setTextSize(2);   
  getNoxColor(nox_index, redNox, greenNox, blueNox);      
  uint16_t textColor2 = getContrastTextColor(redNox, greenNox, blueNox);
  img.setTextColor(textColor2);
  img.fillRect(120, 120, 240, 240, RGBto565(redNox, greenNox, blueNox));// Font size 2 (16px)
  img.setCursor(128 * scalingFactor, 108 * scalingFactor);  // Adjusted for bottom-right quadrant
  img.print("NOX:");
  img.setCursor(130 * scalingFactor, 140 * scalingFactor);  // Centered vertically in quadrant
  img.setTextSize(4);                                       // Font size 3 (24px)
  img.print(nox_index, 0);

  img.drawLine(119, 0, 119, 240, TFT_WHITE);  // Vertical center line
  img.drawLine(120, 0, 120, 240, TFT_WHITE);  // Vertical center line (thicker)
  img.drawLine(0, 119, 240, 119, TFT_WHITE);  // Horizontal center line
  img.drawLine(0, 120, 240, 120, TFT_WHITE);  // Horizontal center line (thicker)
  //img.setTextSize(1);
  // img.setCursor(20 * scalingFactor, (140 * scalingFactor) + 28); // Centered vertically in quadrant
  // img.print(srawVoc, 0);
  // img.setCursor(130 * scalingFactor, (140 * scalingFactor) + 28); // Centered vertically in quadrant
  // img.print(srawNox, 0);
  img.pushSprite(0, 0);
}


void displayMenu() {

  int fontsize = 8;
  img.setTextSize(1);
  img.fillScreen(TFT_BLACK);
  img.setCursor(0, 0);
  img.setTextColor(TFT_WHITE, TFT_BLACK);
  if (WiFi.status() == WL_CONNECTED) {
    img.print("Connected! to: ");
    img.println(WiFi.localIP());
    img.print("RSSI: ");
    img.println(WiFi.RSSI());
    img.println("");
  } else {
    img.setCursor(0, fontsize * 3);
  }
  if (menusel == 1) {
    img.setTextColor(TFT_BLACK, TFT_WHITE);
  } else {
    img.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  img.println("Start Wifi");
  if (menusel == 2) {
    img.setTextColor(TFT_BLACK, TFT_WHITE);
  } else {
    img.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  img.println("Change Sample Rate");
  if (menusel == 3) {
    img.setTextColor(TFT_BLACK, TFT_WHITE);
  } else {
    img.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  img.println("Set Temp Offset");
  if (menusel == 4) {
    img.setTextColor(TFT_BLACK, TFT_WHITE);
  } else {
    img.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  img.println("Set Brightness");
  if (menusel == 5) {
    img.setTextColor(TFT_BLACK, TFT_WHITE);
  } else {
    img.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  img.println("Auto Brightness");
  if (menusel == 6) {
    img.setTextColor(TFT_BLACK, TFT_WHITE);
  } else {
    img.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  img.println("Forget Wifi.");
  if (menusel == 7) {
    img.setTextColor(TFT_BLACK, TFT_WHITE);
  } else {
    img.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  img.println("Exit");

  img.setCursor(150, fontsize * 4);
  if (editinterval) {
    img.setTextColor(TFT_BLACK, TFT_WHITE);
  } else {
    img.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  float sampleMins = sampleSecs / 60.0;
  img.print(sampleMins, 1);
  img.println(" mins");
  img.setCursor(150, fontsize * 5);
  if (editcalib) {
    img.setTextColor(TFT_BLACK, TFT_WHITE);
  } else {
    img.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  img.print(tempOffset, 1);
  img.println(" c");
  img.setCursor(150, fontsize * 6);
  if (editbright) {
    img.setTextColor(TFT_BLACK, TFT_WHITE);
  } else {
    img.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  img.print(brightness);
  img.println(" (1-255)");
  img.setCursor(150, fontsize * 7);
  if (editauto) {
    img.setTextColor(TFT_BLACK, TFT_WHITE);
  } else {
    img.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  img.print(autobright);
  //img.setCursor(200, fontsize*8);
  //if (wifireset) {img.println("Reset!");}
  img.setCursor(0, 106);
  img.print("rawVOC: ");
  img.print(srawVoc);
  img.print(" | Raw Temp: ");
  img.print(temperature);
  img.print((char)247);
  img.print("c");
  img.setCursor(0, 114);
  img.print("rawNOX: ");
  img.print(srawNox);
  img.print(" | Hum: ");
  img.print(humidity);
  img.print("%");
  img.pushSprite(0, 0);
}

void setupChart(int decimals) {
  img.setTextSize(1);
  img.setTextColor(TFT_WHITE);
  img.setCursor(0, 0);
  img.print("<");
  img.print(maxVal, decimals);

  img.setCursor(0, 229);  // Adjusted for 240x240 display
  img.print("<");
  img.print(minVal, decimals);

  img.setCursor(120, 229);  // Adjusted for new width
  img.print("<--");
  img.print(readingCount - 1, 0);
  img.print("*");
  img.print(sampleSecs, 0);
  img.print("s-->");
  img.setCursor(145, 0);
}

void doTempChart() {
  
  minVal = array1[maxArray - readingCount];
  maxVal = array1[maxArray - readingCount];


  for (int i = maxArray - readingCount + 1; i < maxArray; i++) {
    if (array1[i] != 0) {
      if (array1[i] < minVal) minVal = array1[i];
      if (array1[i] > maxVal) maxVal = array1[i];
    }
  }

  // Adjust scaling factors
  float yScale = 239.0 / (maxVal - minVal);
  float xStep = 240.0 / (readingCount - 1);




  img.fillRect(0, 0, img.width(), img.height(), TFT_BLACK);

  for (int i = maxArray - readingCount; i < (maxArray - 1); i++) {
    int x0 = (i - (maxArray - readingCount)) * xStep;
    int y0 = 239 - ((array1[i] - minVal) * yScale);
    int x1 = (i + 1 - (maxArray - readingCount)) * xStep;
    int y1 = 239 - ((array1[i + 1] - minVal) * yScale);

    if (array1[i] != 0) {
      img.drawLine(x0, y0, x1, y1, TFT_PINK);
    }
  }
  setupChart(2);
  img.print("[Temp: ");
  img.print(array1[(maxArray - 1)], 2);
  img.print((char)247);
  img.print("c]");
  img.pushSprite(0, 0);
}

void doHumChart() {
  
  minVal = array2[maxArray - readingCount];
  maxVal = array2[maxArray - readingCount];


  for (int i = maxArray - readingCount + 1; i < maxArray; i++) {
    if ((array2[i] < minVal) && (array2[i] > 0)) {
      minVal = array2[i];
    }
    if (array2[i] > maxVal) {
      maxVal = array2[i];
    }
  }

  float yScale = 239.0 / (maxVal - minVal);
  float xStep = 240.0 / (readingCount - 1);


  img.fillRect(0, 0, img.width(), img.height(), TFT_BLACK);

  for (int i = maxArray - readingCount; i < (maxArray - 1); i++) {
    int x0 = (i - (maxArray - readingCount)) * xStep;
    int y0 = 239 - ((array2[i] - minVal) * yScale);
    int x1 = (i + 1 - (maxArray - readingCount)) * xStep;
    int y1 = 239 - ((array2[i + 1] - minVal) * yScale);

    if (array2[i] > 0) {
      img.drawLine(x0, y0, x1, y1, TFT_CYAN);
    }
  }
  setupChart(2);
  img.print("[Hum: ");
  img.print(array2[(maxArray - 1)], 1);
  img.print("%]");
  img.pushSprite(0, 0);
}

void doVocChart() {
  
  minVal = array3[maxArray - readingCount];
  maxVal = array3[maxArray - readingCount];


  for (int i = maxArray - readingCount + 1; i < maxArray; i++) {
    if ((array3[i] < minVal) && (array3[i] > 0)) {
      minVal = array3[i];
    }
    if (array3[i] > maxVal) {
      maxVal = array3[i];
    }
  }

  float yScale = 239.0 / (maxVal - minVal);
  float xStep = 240.0 / (readingCount - 1);


  img.fillRect(0, 0, img.width(), img.height(), TFT_BLACK);


  for (int i = maxArray - readingCount; i < (maxArray - 1); i++) {
    int x0 = (i - (maxArray - readingCount)) * xStep;
    int y0 = 239 - ((array3[i] - minVal) * yScale);
    int x1 = (i + 1 - (maxArray - readingCount)) * xStep;
    int y1 = 239 - ((array3[i + 1] - minVal) * yScale);
    getVocColor(array3[i], rvg, gvg, bvg);
    if (array3[i] > 0) {
      img.drawLine(x0, y0, x1, y1, RGBto565(rvg,gvg,bvg));
    }
  }
  setupChart(0);
  img.print("[VOC Index: ");
  img.print(array3[(maxArray - 1)], 0);
  img.print("]");
  img.pushSprite(0, 0);
}

void doNoxChart() {

  minVal = array4[maxArray - readingCount];
  maxVal = array4[maxArray - readingCount];

  for (int i = maxArray - readingCount + 1; i < maxArray; i++) {
    if ((array4[i] < minVal) && (array4[i] > 0)) {
      minVal = array4[i];
    }
    if (array4[i] > maxVal) {
      maxVal = array4[i];
    }
  }

  float yScale = 239.0 / (maxVal - minVal);
  float xStep = 240.0 / (readingCount - 1);


  img.fillRect(0, 0, img.width(), img.height(), TFT_BLACK);
  
  for (int i = maxArray - readingCount; i < (maxArray - 1); i++) {
    int x0 = (i - (maxArray - readingCount)) * xStep;
    int y0 = 239 - ((array4[i] - minVal) * yScale);
    int x1 = (i + 1 - (maxArray - readingCount)) * xStep;
    int y1 = 239 - ((array4[i + 1] - minVal) * yScale);
    getNoxColor(array4[i], rng, gng, bng);
    if (array4[i] > 0) {
      img.drawLine(x0, y0, x1, y1, RGBto565(rng,gng,bng));
    }
  }
  setupChart(0);
  img.print("[Raw NOX: ");
  img.print(array4[(maxArray - 1)], 0);
  img.print("]");
  img.pushSprite(0, 0);
}

void takeSamples() {
  if (readingCount < maxArray) {
    readingCount++;
  }

  for (int i = 0; i < (maxArray - 1); i++) {
    array1[i] = array1[i + 1];
  }
  array1[(maxArray - 1)] = tempSHT;

  for (int i = 0; i < (maxArray - 1); i++) {
    array2[i] = array2[i + 1];
  }
  array2[(maxArray - 1)] = humidity;

  if (voc_index > 0) {
    for (int i = 0; i < (maxArray - 1); i++) {
      array3[i] = array3[i + 1];
    }
    array3[(maxArray - 1)] = voc_index;
  }

  if (nox_index > 0) {
    for (int i = 0; i < (maxArray - 1); i++) {
      array4[i] = array4[i + 1];
    }
    array4[(maxArray - 1)] = nox_index;
  }
}

void buttonUP(){
        if (editinterval) {
        sampleSecs -= 30;
        sampleMilliSecs = sampleSecs * 1000;
        }
        if (editcalib) { tempOffset -= 0.1; }
        if (editbright) { brightness -= 1; }
        if (editauto) { autobright = !autobright; }
        if (mainmenu) { menusel++; }
        if (menusel > MENU_MAX) { menusel = 1; }
        if (menusel < 1) { menusel = MENU_MAX; }
        if (sampleSecs < 30) { sampleSecs = 30; }
        if (sampleSecs > 86400) { sampleSecs = 86400; }
        if (brightness < 1) { brightness = 1; }
        if (brightness > 255) { brightness = 255; }
}

void buttonDOWN(){

        if (editinterval) {
          sampleSecs += 30;
          sampleMilliSecs = sampleSecs * 1000;
        }
        if (editcalib) { tempOffset += 0.1; }
        if (editbright) { brightness += 1; }
        if (editauto) { autobright = !autobright; }
        if (mainmenu) { menusel--; }
        if (menusel > MENU_MAX) { menusel = 1; }
        if (menusel < 1) { menusel = MENU_MAX; }
        if (sampleSecs < 30) { sampleSecs = 30; }
        if (sampleSecs > 86400) { sampleSecs = 86400; }
        if (brightness < 1) { brightness = 1; }
        if (brightness > 255) { brightness = 255; }
}

void setup() {
  setCpuFrequencyMhz(80);
  Serial.begin(115200);
  pinMode(bkl_pin, OUTPUT);
  digitalWrite(bkl_pin, HIGH);
  pinMode(ldr_pin, INPUT);
  pinMode(button1, INPUT_PULLUP);
  pinMode(button2, INPUT_PULLUP);
  pinMode(button3, INPUT_PULLUP);
  pinMode(button4, INPUT_PULLUP);
  pinMode(button5, INPUT_PULLUP);
  preferences.begin("my-app", false);
  sampleSecs = preferences.getUInt("sampleSecs", 30);
  tempOffset = preferences.getFloat("tempOffset", 0);
  brightness = preferences.getUInt("brightness", 20);
  autobright = preferences.getBool("autobright", true);
  wifisaved = preferences.getBool("wifisaved", false);
  preferences.end();
  sampleMilliSecs = sampleSecs * 1000;
  Wire.begin();
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);
  Serial.println("MPU6050 ready!");
  tft.init();
  tft.setRotation(0);
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Determine dominant axis
  float x = a.acceleration.x;
  float y = a.acceleration.y;
  float z = a.acceleration.z;
  const float FLAT_THRESHOLD = 8.0; // Adjust based on testing

    if (abs(z) > FLAT_THRESHOLD) {
      tft.setRotation(0);
      
    } else{
        if (abs(y) > abs(x)) {
          if (y > 0) {
            tft.setRotation(3);  // Up
            //Serial.println("Orientation: UP");
          } else {
            tft.setRotation(1);  // Down
            //Serial.println("Orientation: DOWN");
          }
        } else {
          if (x > 0) {
            tft.setRotation(0);  // Right
            //Serial.println("Orientation: RIGHT");
          } else {
            tft.setRotation(2);  // Left
            //Serial.println("Orientation: LEFT");
          }
        }
      }
  tft.fillScreen(TFT_BLACK);
  tft.drawRect(0, 0, tft.width(), tft.height(), TFT_WHITE);

  tft.setCursor(5, 5);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TR_DATUM);
  tft.setTextWrap(true);  // Wrap on width
  tft.setTextSize(2);
  tft.println("Loading VOC Meter...");  //display wifi connection progress
  
  if (wifisaved) {

    //tft.fillScreen(TFT_BLACK);
    //tft.setCursor(5, 5);
    tft.print("Found wifi credentials, connecting to ");  //display wifi connection progress
    tft.print(wm.getWiFiSSID());

    WiFi.mode(WIFI_STA);
    WiFi.begin(wm.getWiFiSSID().c_str(), wm.getWiFiPass().c_str());
    bool lowpower = false;
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      tft.print(".");
      if ((millis() > 10000) && (!lowpower)) {  //display.print("!");
        lowpower = true;
        tft.println("");
        tft.print("Trying low-power mode...");
        WiFi.setTxPower(WIFI_POWER_8_5dBm);
      }
      if (millis() > 20000) {
        tft.fillScreen(TFT_RED);
        tft.setCursor(5, 5);
        tft.print("Failed to connect to saved wifi, aborting.");  //display wifi connection progress
        delay(3000);
        break;
      }
    }
    if (WiFi.status() == WL_CONNECTED) {
      tempOffset = -0.7;
      tft.fillScreen(TFT_GREEN);
      tft.setCursor(5, 5);
      tft.println("");
      tft.print("Connected to ");
      tft.println(wm.getWiFiSSID());
      tft.print("IP address: ");
      tft.println(WiFi.localIP());
      tft.print("Signal strength: ");
      tft.println(WiFi.RSSI());
      Blynk.config(auth, IPAddress(216, 110, 224, 105), 8080);
      Blynk.connect();
      while ((!Blynk.connected()) && (millis() < 20000)) {
        delay(500);
      }
      server.on("/", []() {
        server.send(200, "text/plain", "Hi! This is ElegantOTA Demo.");
      });

      ElegantOTA.begin(&server);  // Start ElegantOTA
      server.begin();
      ArduinoOTA.setHostname("chrisvoc");
      ArduinoOTA.begin();
      delay(3000);
    }
  }

  img.createSprite(240, 240);
  img.setTextWrap(true);
  img.fillSprite(TFT_BLACK);
  sht4x.begin(Wire, SHT40_I2C_ADDR_44);
  sgp41.begin(Wire);
  //delay(500);
  int32_t index_offset;
  int32_t learning_time_offset_hours;
  int32_t learning_time_gain_hours;
  int32_t gating_max_duration_minutes;
  int32_t std_initial;
  int32_t gain_factor;
  voc_algorithm.get_tuning_parameters(
    index_offset, learning_time_offset_hours, learning_time_gain_hours,
    gating_max_duration_minutes, std_initial, gain_factor);

  Serial.println("\nVOC Gas Index Algorithm parameters");
  Serial.print("Index offset:\t");
  Serial.println(index_offset);
  Serial.print("Learning time offset hours:\t");
  Serial.println(learning_time_offset_hours);
  Serial.print("Learning time gain hours:\t");
  Serial.println(learning_time_gain_hours);
  Serial.print("Gating max duration minutes:\t");
  Serial.println(gating_max_duration_minutes);
  Serial.print("Std inital:\t");
  Serial.println(std_initial);
  Serial.print("Gain factor:\t");
  Serial.println(gain_factor);

  nox_algorithm.get_tuning_parameters(
    index_offset, learning_time_offset_hours, learning_time_gain_hours,
    gating_max_duration_minutes, std_initial, gain_factor);

  Serial.println("\nNOx Gas Index Algorithm parameters");
  Serial.print("Index offset:\t");
  Serial.println(index_offset);
  Serial.print("Learning time offset hours:\t");
  Serial.println(learning_time_offset_hours);
  Serial.print("Gating max duration minutes:\t");
  Serial.println(gating_max_duration_minutes);
  Serial.print("Gain factor:\t");
  Serial.println(gain_factor);
  Serial.println("");
  error = sht4x.measureHighPrecision(temperature, humidity);
  if (error) {
    Serial.print(
      "SHT4x - Error trying to execute measureHighPrecision(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
    Serial.println("Fallback to use default values for humidity and "
                   "temperature compensation for SGP41");
    compensationRh = defaultCompenstaionRh;
    compensationT = defaultCompenstaionT;
    Serial.flush();
  } else {
    tempSHT = temperature + tempOffset + HCOffset;
    humSHT = humidity;
    // convert temperature and humidity to ticks as defined by SGP41
    // interface
    // NOTE: in case you read RH and T raw signals check out the
    // ticks specification in the datasheet, as they can be different for
    // different sensors
    compensationT = static_cast<uint16_t>((tempSHT + 45) * 65535 / 175);
    compensationRh = static_cast<uint16_t>(humSHT * 65535 / 100);
  }
}

void loop() {

  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
    server.handleClient();
    ElegantOTA.loop();
    Blynk.run();
  }

 delay(10);

  every(100) {
    if (!menumode) {
      if (!digitalRead(button1)) { button = 1; }  //towards
      if (!digitalRead(button2)) { button = 2; }  //down
      if (!digitalRead(button3)) { button = 3; }  //away
      if (!digitalRead(button4)) { button = 4; }  //up
      if (!digitalRead(button5)) {                //pressed
        buttonTimer = millis();
        button = 5;
        while (!digitalRead(5)) {
          delay(10);
          if ((millis() - buttonTimer) > 2000) {
            tft.setTextSize(2);
            tft.setCursor(0, 0);
            tft.fillScreen(TFT_BLACK);
            analogWrite(bkl_pin, 255);
            tft.println("Loading menu...");
            while (!digitalRead(5)) { delay(10); }
            menumode = true;
            delay(100);
            return;
          }
        }
      }
      switch (button) {
        case 1:
          doTempChart();
          break;
        case 2:
          doHumChart();
          break;
        case 3:
          doVocChart();
          break;
        case 4:
          doNoxChart();
          break;
        case 5:
          doMainDisplay();
      }
    } else {  // editinterval, editcalib, editbright, editauto, menumode;  tempOffset brightness autobright
      
      if (!digitalRead(button2)) {
        if (tft.getRotation() == 0) {buttonUP();}
        else {buttonDOWN();} 

      }
      if (!digitalRead(button4)) {
        if (tft.getRotation() == 2) {buttonUP();}
        else {buttonDOWN();} 
      }
      if (!digitalRead(button5)) {
        switch (menusel) {
          case 1:
            {
              img.fillScreen(TFT_BLACK);
              img.setCursor(0, 0);
              img.setTextSize(2);
              img.println("Use your mobile phone to connect to ");
              img.println("[VOC Meter Setup] then browse to");
              img.println("http://192.168.4.1 to connect to WiFi");
              img.pushSprite(0, 0);
              WiFi.mode(WIFI_STA);
              wm.setConfigPortalTimeout(300);
              if (wm.getWiFiIsSaved()) {
                wm.startConfigPortal("VOC Meter Setup");
              } else {
                bool res;
                tft.setTextSize(2);
                tft.setCursor(0, 0);
                res = wm.autoConnect("VOC Meter Setup");
                if (!res) {  //if the wifi manager failed to connect to wifi
                  tft.fillScreen(TFT_RED);
                  tft.println("Failed to connect.");
                  delay(3000);
                } else {
                  //if you get here you have connected to the WiFi
                  tft.fillScreen(TFT_GREEN);
                  tft.println("Connected!");
                  wifisaved = true;
                  preferences.begin("my-app", false);
                  preferences.putBool("wifisaved", wifisaved);
                  preferences.end();
                  Blynk.config(auth, IPAddress(216, 110, 224, 105), 8080);
                  Blynk.connect();
                  while ((!Blynk.connected()) && (millis() < 20000)) {
                    delay(500);
                  }
                  server.on("/", []() {
                    server.send(200, "text/plain", "Hi! This is ElegantOTA Demo.");
                  });

                  ElegantOTA.begin(&server);  // Start ElegantOTA
                  server.begin();
                  ArduinoOTA.setHostname("chrisvoc");
                  ArduinoOTA.begin();

                  tft.println("Update server started:");
                  tft.print(WiFi.localIP());
                  tft.println("/update");
                  tft.println("Press button to continue.");
                  while (digitalRead(button5)) { delay(10); }
                  tft.setTextSize(2);
                  tft.setCursor(0, 0);
                  tft.fillScreen(TFT_BLACK);
                  tft.println("Loading menu...");
                }
              }
              break;
            }
          case 2:  // editinterval, editcalib, editbright, editauto, menumode;  tempOffset brightness autobright
            editinterval = !editinterval;
            mainmenu = !mainmenu;
            preferences.begin("my-app", false);
            preferences.putUInt("sampleSecs", sampleSecs);
            preferences.end();
            break;
          case 3:
            editcalib = !editcalib;
            mainmenu = !mainmenu;
            preferences.begin("my-app", false);
            preferences.putFloat("tempOffset", tempOffset);
            preferences.end();
            break;
          case 4:
            editbright = !editbright;
            mainmenu = !mainmenu;
            preferences.begin("my-app", false);
            preferences.putUInt("brightness", brightness);
            preferences.end();
            break;
          case 5:
            editauto = !editauto;
            mainmenu = !mainmenu;
            preferences.begin("my-app", false);
            preferences.putBool("autobright", autobright);
            preferences.end();
            break;
          case 6:
            wm.resetSettings();
            tft.fillScreen(TFT_GREEN);
            tft.setCursor(0, 0);
            tft.println("Wifi config deleted.");
            tempOffset = 0;
            wifisaved = false;
            preferences.begin("my-app", false);
            preferences.putFloat("tempOffset", tempOffset);
            preferences.putBool("wifisaved", wifisaved);
            preferences.end();
            delay(3000);
            ESP.restart();
            break;
          case 7:
            menumode = false;
            break;
        }
        while (!digitalRead(5)) { delay(10); }
        delay(100);
      }
      displayMenu();
    }


  }

  every(1000) {


    // 3. Measure SGP4x signals
    if (conditioning_s > 0) {  //
      // During NOx conditioning (10s) SRAW NOx will remain 0
      error =
        sgp41.executeConditioning(compensationRh, compensationT, srawVoc);
      conditioning_s--;
    } else {
      error = sgp41.measureRawSignals(compensationRh, compensationT, srawVoc,
                                      srawNox);
    }

    // 4. Process raw signals by Gas Index Algorithm to get the VOC and NOx
    // index
    //    values
    if (error) {
      Serial.print("SGP41 - Error trying to execute measureRawSignals(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
      Serial.flush();
    } else {
      voc_index = voc_algorithm.process(srawVoc);
      nox_index = nox_algorithm.process(srawNox);
    }
  }

  every(2000) {
    const float FLAT_THRESHOLD = 8.0; // Adjust based on testing
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    // Determine dominant axis
    float x = a.acceleration.x;
    float y = a.acceleration.y;
    float z = a.acceleration.z;
    if (!menumode) {
      if (autobright) {

        ldr_read = analogRead(ldr_pin);
        newldr = map(ldr_read, 0, 4096, 0, 255);
        newldr = newldr + brightness;
        if (newldr < 1) { newldr = 1; }
        if (newldr > 255) { newldr = 255; }
        analogWrite(bkl_pin, newldr);
      }
      else {analogWrite(bkl_pin, brightness);}
    }
    else {analogWrite(bkl_pin, 255);}

    if (abs(z) > FLAT_THRESHOLD) {
      tft.setRotation(0);
      
    } else{
        if (abs(y) > abs(x)) {
          if (y > 0) {
            tft.setRotation(3);  // Up
            //Serial.println("Orientation: UP");
          } else {
            tft.setRotation(1);  // Down
            //Serial.println("Orientation: DOWN");
          }
        } else {
          if (x > 0) {
            tft.setRotation(0);  // Right
            //Serial.println("Orientation: RIGHT");
          } else {
            tft.setRotation(2);  // Left
            //Serial.println("Orientation: LEFT");
          }
        }
      }
  }

  every(15000) {
    error = sht4x.measureHighPrecision(temperature, humidity);
    if (error) {
      Serial.print(
        "SHT4x - Error trying to execute measureHighPrecision(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
      Serial.println("Fallback to use default values for humidity and "
                     "temperature compensation for SGP41");
      compensationRh = defaultCompenstaionRh;
      compensationT = defaultCompenstaionT;
      Serial.flush();
    } else {
      tempSHT = temperature + tempOffset + HCOffset;
      humSHT = humidity;
      compensationT = static_cast<uint16_t>((tempSHT + 45) * 65535 / 175);
      compensationRh = static_cast<uint16_t>(humSHT * 65535 / 100);
    }
  }



  every(sampleMilliSecs) {

    takeSamples();
    Blynk.virtualWrite(V71, srawVoc);
    Blynk.virtualWrite(V72, srawNox);
    Blynk.virtualWrite(V73, voc_index);
    Blynk.virtualWrite(V74, nox_index);
    Blynk.virtualWrite(V75, tempSHT);
    Blynk.virtualWrite(V76, humidity);
  }
}
