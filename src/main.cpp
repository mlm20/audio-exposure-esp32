// #############################################################################
// setup

//// libraries
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <Wire.h>

//// dB meter setup
// SDA and SCL pins
#define I2C_SDA_DBM 5
#define I2C_SCL_DBM 18

// I2C address
#define DBM_ADDR 0x48

// device registers
#define DBM_REG_VERSION 0x00
#define DBM_REG_ID3 0x01
#define DBM_REG_ID2 0x02
#define DBM_REG_ID1 0x03
#define DBM_REG_ID0 0x04
#define DBM_REG_SCRATCH 0x05
#define DBM_REG_CONTROL 0x06
#define DBM_REG_TAVG_HIGH 0x07
#define DBM_REG_TAVG_LOW 0x08
#define DBM_REG_RESET 0x09
#define DBM_REG_DECIBEL 0x0A
#define DBM_REG_MIN 0x0B
#define DBM_REG_MAX 0x0C
#define DBM_REG_THR_MIN 0x0D
#define DBM_REG_THR_MAX 0x0E
#define DBM_REG_HISTORY_0 0x14
#define DBM_REG_HISTORY_99 0x77

TwoWire dbmeterWire = TwoWire(1);  // Use a separate I2C bus for dB meter

// Function to read a register from the meter
uint8_t dbmeter_readreg(TwoWire *dev, uint8_t regaddr) {
    dev->beginTransmission(DBM_ADDR);
    dev->write(regaddr);
    dev->endTransmission();
    dev->requestFrom(DBM_ADDR, 1);
    delay(10);
    return dev->read();
}

//// OLED screen initialisation
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1  // Reset pin
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//// passwords and API keys
// ThingSpeak API key
String apiKey = "5UYDKHVX8HOP2JCV";

// wifi details
const char *ssid = "BT-ZPA2CK";
const char *pass = "AeqfM6MyMdiUMc";
WiFiClient client;

// #############################################################################
// init code, to run once
void setup() {
    Serial.begin(115200);

    //// OLED screen setup
    // initialize the OLED object
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
            ;  // Don't proceed, loop forever
    }

    // Clear the buffer.
    display.clearDisplay();

    // Display trail Text
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 28);
    display.println("Hello world!");
    display.display();

    // make wifi connection
    delay(6000);
    WiFi.begin(ssid, pass);

    // write in serial once wifi is connected
    while (WiFi.status() != WL_CONNECTED) {
        delay(5000);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");

    // Initialize I2C for dB meter
    dbmeterWire.begin(I2C_SDA_DBM, I2C_SCL_DBM, 10000);
}

// #############################################################################
// main code, to run repeatedly
void loop() {
    // Read version register
    uint8_t version = dbmeter_readreg(&dbmeterWire, DBM_REG_VERSION);
    Serial.printf("Version = 0x%02X\r\n", version);

    // Read ID registers
    uint8_t id[4];
    id[0] = dbmeter_readreg(&dbmeterWire, DBM_REG_ID3);
    id[1] = dbmeter_readreg(&dbmeterWire, DBM_REG_ID2);
    id[2] = dbmeter_readreg(&dbmeterWire, DBM_REG_ID1);
    id[3] = dbmeter_readreg(&dbmeterWire, DBM_REG_ID0);
    Serial.printf("Unique ID = %02X %02X %02X %02X\r\n", id[3], id[2], id[1],
                  id[0]);

    uint8_t db, dbmin, dbmax;
    String DBMjson;

    // Read decibel, min, and max
    db = dbmeter_readreg(&dbmeterWire, DBM_REG_DECIBEL);
    if (db != 255) {
        dbmin = dbmeter_readreg(&dbmeterWire, DBM_REG_MIN);
        dbmax = dbmeter_readreg(&dbmeterWire, DBM_REG_MAX);
        Serial.printf("dB reading = %03d \t [MIN: %03d \tMAX: %03d] \r\n", db,
                      dbmin, dbmax);

        // Assemble JSON with dB SPL reading
        DBMjson = String(db);
        Serial.println("JSON: \n" + DBMjson);

        // Make a POST request with the data to ThingSpeak
        if (WiFi.status() == WL_CONNECTED) {
            HTTPClient http;

            String thingspeakEndpoint =
                "http://api.thingspeak.com/update?api_key=" + apiKey +
                "&field1=" + String(DBMjson);

            http.begin(thingspeakEndpoint);

            int httpResponseCode = http.GET();

            if (httpResponseCode == 200) {
                // Successful response from ThingSpeak
                String response = http.getString();
                Serial.println(httpResponseCode);  // Print return code
                Serial.println(response);          // Print request answer
            } else {
                Serial.print("Error on sending GET to ThingSpeak: ");
                Serial.println(httpResponseCode);
            }

            http.end();  // Free resources
        } else {
            Serial.println("Something wrong with WiFi?");
        }
    }

    // Wait for 5 seconds before posting another reading
    delay(5000);
}
