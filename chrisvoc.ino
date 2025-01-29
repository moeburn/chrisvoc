#include <TFT_eSPI.h> 
#include <SPI.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Math.h>
#include <Arduino.h>
#include <Wire.h>

#include <NOxGasIndexAlgorithm.h>
#include <SensirionI2CSgp41.h>
#include <SensirionI2cSht4x.h>
#include <VOCGasIndexAlgorithm.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ElegantOTA.h>

const char* ssid = "mikesnet";
const char* password = "springchicken";

WebServer server(80);

#define bkl_pin 2
#define ldr_pin 0

#define button1 3
#define button2 21
#define button3 20
#define button4 7
#define button5 5
#define maxArray 501

float tempOffset = -0.7;

int sampleSecs = 30 * 1000;
int brightness = 10;

 float array1[maxArray];
 float array2[maxArray];
 float array3[maxArray];
 float array4[maxArray];

SensirionI2cSht4x sht4x;
SensirionI2CSgp41 sgp41;
Adafruit_MPU6050 mpu;
VOCGasIndexAlgorithm voc_algorithm;
NOxGasIndexAlgorithm nox_algorithm;
 float minVal = 3.9;
 float maxVal = 4.2;
int readingCount = 0; // Counter for the number of readings
int ldr_read;
int button = 5;
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
TFT_eSPI tft = TFT_eSPI();       //for TFT display
TFT_eSprite img = TFT_eSprite(&tft);

#define every(interval) \
    static uint32_t __every__##interval = millis(); \
    if (millis() - __every__##interval >= interval && (__every__##interval = millis()))

  float scalingFactor = 1.2;


void doMainDisplay(){

  img.fillSprite(TFT_BLACK);
  img.drawLine(119, 0, 119, 240, TFT_BLACK);  // Vertical center line
  img.drawLine(120, 0, 120, 240, TFT_BLACK); // Vertical center line (thicker)
  img.drawLine(0, 119, 240, 119, TFT_BLACK);  // Horizontal center line
  img.drawLine(0, 120, 240, 120, TFT_BLACK); // Horizontal center line (thicker)

  img.setTextSize(2); // Font size 2 (16px)
  img.setCursor(24 * scalingFactor, 2 * scalingFactor); // Adjusted to fit top-left quadrant
  img.println("Temp:");
  img.setTextSize(1);
  img.println(ldr_read);
  img.print(button);
  img.setCursor(6 * scalingFactor, 40 * scalingFactor); // Centered vertically in quadrant
  img.setTextSize(3); // Font size 3 (24px)
  img.print(tempSHT, 1);
  img.print("c");

  // Quadrant 2: Top-right
  img.setTextSize(2); // Font size 2 (16px)
  img.setCursor(124 * scalingFactor, 2 * scalingFactor); // Adjusted to fit top-right quadrant
  img.print("Hum:");
  img.setCursor(122 * scalingFactor, 40 * scalingFactor); // Centered vertically in quadrant
  img.setTextSize(3); // Font size 3 (24px)
  img.print(humidity, 1);
  img.print("%");

  // Quadrant 3: Bottom-left
  img.setTextSize(2); // Font size 2 (16px)
  img.setCursor(24 * scalingFactor, 104 * scalingFactor); // Adjusted for bottom-left quadrant
  img.print("VOC:");
  img.setCursor(20 * scalingFactor, 140 * scalingFactor); // Centered vertically in quadrant
  img.setTextSize(3); // Font size 3 (24px)
  img.print(voc_index, 1);


  // Quadrant 4: Bottom-right
  img.setTextSize(2); // Font size 2 (16px)
  img.setCursor(124 * scalingFactor, 104 * scalingFactor); // Adjusted for bottom-right quadrant
  img.print("NOX:");
  img.setCursor(130 * scalingFactor, 140 * scalingFactor); // Centered vertically in quadrant
  img.setTextSize(3); // Font size 3 (24px)
  img.print(nox_index, 0);

  img.setTextSize(1);
  img.setCursor(20 * scalingFactor, (140 * scalingFactor) + 28); // Centered vertically in quadrant
  img.print(srawVoc, 0);
  img.setCursor(130 * scalingFactor, (140 * scalingFactor) + 28); // Centered vertically in quadrant
  img.print(srawNox, 0);
  img.pushSprite(0, 0);


}

void setupChart() {
    img.setCursor(0, 0);
    img.print("<");
    img.print(maxVal, 3);
    
    img.setCursor(0, 229); // Adjusted for 240x240 display
    img.print("<");
    img.print(minVal, 3);
    
    img.setCursor(120, 229); // Adjusted for new width
    img.print("<--");
    img.print(readingCount - 1, 0);
    img.print("*");
    img.print(sampleSecs * 60, 0);
    img.print("s-->");

    

    
    img.setCursor(145, 0);
}

void doTempChart() {
    setupChart();
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




        img.fillRect(0, 0, img.width(), img.height(), TFT_WHITE);
        
        for (int i = maxArray - readingCount; i < (maxArray - 1); i++) {
            int x0 = (i - (maxArray - readingCount)) * xStep;
            int y0 = 239 - ((array1[i] - minVal) * yScale);
            int x1 = (i + 1 - (maxArray - readingCount)) * xStep;
            int y1 = 239 - ((array1[i + 1] - minVal) * yScale);

            if (array1[i] != 0) {
                img.drawLine(x0, y0, x1, y1, TFT_BLACK);
            }
        }
        setupChart();
        img.print("[Temp: ");
        img.print(array1[(maxArray - 1)], 3);
        img.print("c]");
        img.pushSprite(0, 0);
}

void doHumChart() {
    setupChart();
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


        img.fillRect(0, 0, img.width(), img.height(), TFT_WHITE);
        
        for (int i = maxArray - readingCount; i < (maxArray - 1); i++) {
            int x0 = (i - (maxArray - readingCount)) * xStep;
            int y0 = 239 - ((array2[i] - minVal) * yScale);
            int x1 = (i + 1 - (maxArray - readingCount)) * xStep;
            int y1 = 239 - ((array2[i + 1] - minVal) * yScale);
            
            if (array2[i] > 0) {
                img.drawLine(x0, y0, x1, y1, TFT_BLACK);
            }
        }
        setupChart();
        img.print("[Hum: ");
        img.print(array2[(maxArray - 1)], 0);
        img.print("%]");
        img.pushSprite(0, 0);
}

void doVocChart() {
    setupChart();
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


        img.fillRect(0, 0, img.width(), img.height(), TFT_WHITE);
        
        for (int i = maxArray - readingCount; i < (maxArray - 1); i++) {
            int x0 = (i - (maxArray - readingCount)) * xStep;
            int y0 = 239 - ((array3[i] - minVal) * yScale);
            int x1 = (i + 1 - (maxArray - readingCount)) * xStep;
            int y1 = 239 - ((array3[i + 1] - minVal) * yScale);

            if (array3[i] > 0) {
                img.drawLine(x0, y0, x1, y1, TFT_BLACK);
            }
        }
        setupChart();
        img.print("[VOC Index: ");
        img.print(array3[(maxArray - 1)], 2);
        img.print("]");
        img.pushSprite(0, 0);
}

void doNoxChart() {
    setupChart();
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


        img.fillRect(0, 0, img.width(), img.height(), TFT_WHITE);
        
        for (int i = maxArray - readingCount; i < (maxArray - 1); i++) {
            int x0 = (i - (maxArray - readingCount)) * xStep;
            int y0 = 239 - ((array4[i] - minVal) * yScale);
            int x1 = (i + 1 - (maxArray - readingCount)) * xStep;
            int y1 = 239 - ((array4[i + 1] - minVal) * yScale);

            if (array4[i] > 0) {
                img.drawLine(x0, y0, x1, y1, TFT_BLACK);
            }
        }
        setupChart();
        img.print("[Raw NOX: ");
        img.print(array4[(maxArray - 1)], 2);
        img.print("]");
        img.pushSprite(0, 0);
}

void takeSamples(){
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
        
        for (int i = 0; i < (maxArray - 1); i++) {
            array3[i] = array3[i + 1];
        }
        array3[(maxArray - 1)] = voc_index;

        for (int i = 0; i < (maxArray - 1); i++) {
            array4[i] = array4[i + 1];
        }
        array4[(maxArray - 1)] = srawNox;
}

void setup() {

  Serial.begin(115200);
  pinMode(bkl_pin, OUTPUT);
  digitalWrite(bkl_pin, HIGH);
  pinMode(ldr_pin, INPUT);
  pinMode(button1, INPUT_PULLUP);
  pinMode(button2, INPUT_PULLUP);
  pinMode(button3, INPUT_PULLUP);
  pinMode(button4, INPUT_PULLUP);
  pinMode(button5, INPUT_PULLUP);

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.drawRect(0,0, tft.width(), tft.height(), TFT_WHITE);

  tft.setCursor(5,5);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TR_DATUM);
  tft.setTextWrap(true); // Wrap on width
  tft.setTextSize(1);
  tft.print("Loading...");  //display wifi connection progress
  /*WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    tft.print(".");
        if (millis() > 20000) {
            break;
          }
  }
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(5,5);
  tft.println("");
  tft.print("Connected to ");
  tft.println(ssid);
  tft.print("IP address: ");
  tft.println(WiFi.localIP());
  tft.print("Signal strength: ");
  tft.println(WiFi.RSSI());*/
    Wire.begin();
      img.createSprite(240, 240);
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
        tempSHT = temperature + tempOffset;
        humSHT = humidity;
        // convert temperature and humidity to ticks as defined by SGP41
        // interface
        // NOTE: in case you read RH and T raw signals check out the
        // ticks specification in the datasheet, as they can be different for
        // different sensors
        compensationT = static_cast<uint16_t>((tempSHT + 45) * 65535 / 175);
        compensationRh = static_cast<uint16_t>(humSHT * 65535 / 100);
    }
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);
  Serial.println("MPU6050 ready!");
  /*server.on("/", []() {
    server.send(200, "text/plain", "Hi! This is ElegantOTA Demo.");
  });

  ElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();*/
}

void loop() {
  //server.handleClient();
  //ElegantOTA.loop();




  every(100){
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    // Determine dominant axis
    float x = a.acceleration.x;
    float y = a.acceleration.y;
    int newldr;
    ldr_read = analogRead(ldr_pin);
    newldr = map(ldr_read, 0, 4096, 0, 255);
    newldr = newldr + brightness;
    if (newldr < 1) {newldr=1;}
    if (newldr > 255) {newldr=255;}
    analogWrite(bkl_pin, newldr);
    if (!digitalRead(button1)) {button = 1;} //towards
    if (!digitalRead(button2)) {button = 2;} //down
    if (!digitalRead(button3)) {button = 3;} //away
    if (!digitalRead(button4)) {button = 4;} //up
    if (!digitalRead(button5)) {                //pressed
        buttonTimer = millis();
        button = 5;
        while (!digitalRead(5))
        {
          delay(10);
          if ((millis() - buttonTimer) > 2000) {
            button = 6;
            return;
          }
        }
        
      } 
    if (abs(y) > abs(x)) {
      if (y > 0) {
        tft.setRotation(3); // Up
        //Serial.println("Orientation: UP");
      } else {
        tft.setRotation(1); // Down
        //Serial.println("Orientation: DOWN");
      }
    } else {
      if (x > 0) {
        tft.setRotation(0); // Right
        //Serial.println("Orientation: RIGHT");
      } else {
        tft.setRotation(2); // Left
        //Serial.println("Orientation: LEFT");
      }
    }
  }

  every(1000){
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

    // 3. Measure SGP4x signals
    if (conditioning_s > 0) {
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

  every (15000){
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
        tempSHT = temperature + tempOffset;
        humSHT = humidity;
        compensationT = static_cast<uint16_t>((tempSHT + 45) * 65535 / 175);
        compensationRh = static_cast<uint16_t>(humSHT * 65535 / 100);
    }

  }

  every (sampleSecs) {
    takeSamples();
  }

}
