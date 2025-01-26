#include <TFT_eSPI.h> 
#include <SPI.h>


#include <Arduino.h>
#include <Wire.h>

#include <NOxGasIndexAlgorithm.h>
#include <SensirionI2CSgp41.h>
#include <SensirionI2cSht4x.h>
#include <VOCGasIndexAlgorithm.h>

#define bkl_pin 2
#define ldr_pin 0

SensirionI2cSht4x sht4x;
SensirionI2CSgp41 sgp41;

VOCGasIndexAlgorithm voc_algorithm;
NOxGasIndexAlgorithm nox_algorithm;
int ldr_read;
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

TFT_eSPI tft = TFT_eSPI();       //for TFT display
TFT_eSprite img = TFT_eSprite(&tft);

#define every(interval) \
    static uint32_t __every__##interval = millis(); \
    if (millis() - __every__##interval >= interval && (__every__##interval = millis()))


void doDisplay(){
  float scalingFactor = 1.2;
  img.fillSprite(TFT_BLACK);
  img.drawLine(119, 0, 119, 240, TFT_BLACK);  // Vertical center line
  img.drawLine(120, 0, 120, 240, TFT_BLACK); // Vertical center line (thicker)
  img.drawLine(0, 119, 240, 119, TFT_BLACK);  // Horizontal center line
  img.drawLine(0, 120, 240, 120, TFT_BLACK); // Horizontal center line (thicker)

  img.setTextSize(1); // Font size 2 (16px)
  img.setCursor(24 * scalingFactor, 2 * scalingFactor); // Adjusted to fit top-left quadrant
  img.println("Temp:");
  img.print(ldr_read);
  img.setCursor(6 * scalingFactor, 40 * scalingFactor); // Centered vertically in quadrant
  img.setTextSize(2); // Font size 3 (24px)
  img.print(temperature, 1);
  img.print("c");

  // Quadrant 2: Top-right
  img.setTextSize(1); // Font size 2 (16px)
  img.setCursor(124 * scalingFactor, 2 * scalingFactor); // Adjusted to fit top-right quadrant
  img.print("Hum:");
  img.setCursor(130 * scalingFactor, 40 * scalingFactor); // Centered vertically in quadrant
  img.setTextSize(2); // Font size 3 (24px)
  img.print(humidity, 0);
  img.print("%");

  // Quadrant 3: Bottom-left
  img.setTextSize(1); // Font size 2 (16px)
  img.setCursor(24 * scalingFactor, 104 * scalingFactor); // Adjusted for bottom-left quadrant
  img.print("VOC Index:");
  img.setCursor(20 * scalingFactor, 140 * scalingFactor); // Centered vertically in quadrant
  img.setTextSize(2); // Font size 3 (24px)
  img.print(voc_index, 1);


  // Quadrant 4: Bottom-right
  img.setTextSize(1); // Font size 2 (16px)
  img.setCursor(124 * scalingFactor, 104 * scalingFactor); // Adjusted for bottom-right quadrant
  img.print("NOX Index:");
  img.setCursor(130 * scalingFactor, 140 * scalingFactor); // Centered vertically in quadrant
  img.setTextSize(2); // Font size 3 (24px)
  img.print(nox_index, 0);
  img.pushSprite(0, 0);

}


void setup() {

  Serial.begin(115200);
  pinMode(bkl_pin, OUTPUT);
  pinMode(ldr_pin, INPUT);
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);


  tft.setCursor(0,0);
  tft.setTextColor(TFT_BLUE, TFT_BLACK);
  tft.setTextDatum(TR_DATUM);
  tft.setTextWrap(true); // Wrap on width
  tft.setTextSize(1);
  tft.print("Loading...");  //display wifi connection progress
  tft.drawRect(0,0, tft.width(), tft.height(), TFT_WHITE);
    Wire.begin();
      img.createSprite(240, 240);
      img.fillSprite(TFT_BLACK);
    sht4x.begin(Wire, SHT40_I2C_ADDR_44);
    sgp41.begin(Wire);
  delay(500);
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
        tempSHT = temperature;
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
  every(250){
    ldr_read = analogRead(ldr_pin);
    analogWrite(bkl_pin, map(ldr_read, 0, 1024, 0, 255));
  }
  every(1000){

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
    doDisplay();
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
        tempSHT = temperature;
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

}
