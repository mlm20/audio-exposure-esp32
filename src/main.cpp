// #############################################################################
// setup

//// libraries
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>

//// OLED screen setup
// variables
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET -1     // Reset pin # (or -1 if sharing Arduino reset pin)

// init display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//// dB meter setup
// SDA and SCL pins
#define I2C_SDA 5
#define I2C_SCL 18

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

TwoWire dbmeterWire(0);

// Function to read a register from the meter
uint8_t dbmeter_readreg(TwoWire *dev, uint8_t regaddr) {
    dev->beginTransmission(DBM_ADDR);
    dev->write(regaddr);
    dev->endTransmission();
    dev->requestFrom(DBM_ADDR, 1);
    delay(10);
    return dev->read();
}

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

    // OLED display setup
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // OLED display start
    display.display();                          // show buffer
    display.clearDisplay();                     // clear buffer
    display.setTextSize(1);                     // Set text size to 1 (1-6)
    display.setTextColor(WHITE);  // Set text color to WHITE (no choice lol)
    display.setCursor(0, 0);      // cursor to upper left corner
    display.println("dB Meter");  // write title
    display.display();            // show title
    delay(2000);

    // make wifi connection
    WiFi.begin(ssid, pass);

    // write in serial once wifi is connected
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");

    // print on OLED display once wifi is connected
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.print("WiFi connected");
    display.display();
    delay(4000);
    display.clearDisplay();
}

// #############################################################################
// main code, to run repeatedly
void loop() {
    // Initialize I2C at 10kHz
    dbmeterWire.begin(I2C_SDA, I2C_SCL, 10000);

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
    while (1) {
        // Read decibel, min and max
        db = dbmeter_readreg(&dbmeterWire, DBM_REG_DECIBEL);
        if (db == 255) continue;
        dbmin = dbmeter_readreg(&dbmeterWire, DBM_REG_MIN);
        dbmax = dbmeter_readreg(&dbmeterWire, DBM_REG_MAX);
        Serial.printf("dB reading = %03d \t [MIN: %03d \tMAX: %03d] \r\n", db,
                      dbmin, dbmax);

        // Assemble JSON with dB SPL reading
        DBMjson = String(db);
        Serial.println("JSON: \n" + DBMjson);

        // Update OLED display with dB reading
        display.clearDisplay();
        display.setCursor(0, 0);
        display.setTextSize(2);  // Increase text size for better visibility
        display.setTextColor(WHITE);
        display.print("dB: ");
        display.print(db);
        display.display();

        // Make a POST request with the data to ThingSpeak
        if (WiFi.status() == WL_CONNECTED) {
            HTTPClient http;

            String thingspeakEndpoint =
                "http://api.thingspeak.com/update?api_key=" + apiKey +
                "&field1=" + String(DBMjson);

            http.begin(thingspeakEndpoint);

            int httpResponseCode =
                http.GET();  // Send the actual GET request for ThingSpeak

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

        // Wait for 5 seconds before posting another reading
        delay(5000);
    }
}
