/*
 * This example shows how to:
 * 1. define a temperature sensor accessory and its characteristics (in my_accessory.c).
 * 2. report the sensor value to HomeKit (just random value here, you need to change it to your real sensor value).
 *
 * This HomeKit library is mostly written in C,
 * you can define your accessory/service/characteristic in a .c file,
 * since the library provides convenient Macro (C only, CPP can not compile) to do this.
 * But it is possible to do this in .cpp or .ino (just not so conveniently), do it yourself if you like.
 * Check out homekit/characteristics.h and use the Macro provided to define your accessory.
 *
 * Generally, the Arduino libraries (e.g. sensors, ws2812) are written in cpp,
 * you can include and use them in a .ino or a .cpp file (but can NOT in .c).
 * A .ino is a .cpp indeed.
 *
 * You can define some variables in a .c file, e.g. int my_value = 1;,
 * and you can access this variable in a .ino or a .cpp by writing extern "C" int my_value;.
 *
 * So, if you want use this HomeKit library and other Arduino Libraries together,
 * 1. define your HomeKit accessory/service/characteristic in a .c file
 * 2. in your .ino, include some Arduino Libraries and you can use them normally
 *                  write extern "C" homekit_characteristic_t xxxx; to access the characteristic defined in your .c file
 *                  write your logic code (eg. read sensors) and
 *                  report your data by writing your_characteristic.value.xxxx_value = some_data; homekit_characteristic_notify(..., ...)
 */

#include <Arduino.h>
#include <arduino_homekit_server.h>
#include "stdint.h"
#include "wifi_info.h"
#include "DHT.h"
#include "ccs811.h"
#include <Wire.h> // I2C library


#define DHTPIN 2
//#define DHTTYPE DHT22 //Se selecciona el DHT22(hay otros DHT)
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
CCS811 ccs811(D3);

#define LOG_D(fmt, ...)   printf_P(PSTR(fmt "\n") , ##__VA_ARGS__);

void setup() {
	Serial.begin(115200);
  dht.begin();
  

  
  Serial.print("setup: ccs811 lib  version: "); Serial.println(CCS811_VERSION);
  
    // Enable I2C
  Wire.begin(); 
  

  // Enable CCS811
  ccs811.set_i2cdelay(50); // Needed for ESP8266 because it doesn't handle I2C clock stretch correctly
  
  bool ok= ccs811.begin();
  if( !ok ) Serial.println("setup: CCS811 begin FAILED");

  // Print CCS811 versions
  Serial.print("setup: hardware    version: "); Serial.println(ccs811.hardware_version(),HEX);
  Serial.print("setup: bootloader  version: "); Serial.println(ccs811.bootloader_version(),HEX);
  Serial.print("setup: application version: "); Serial.println(ccs811.application_version(),HEX);

  // Start measuring
  ok= ccs811.start(CCS811_MODE_1SEC);
  if( !ok ) Serial.println("setup: CCS811 start FAILED");

	wifi_connect(); // in wifi_info.h
	my_homekit_setup();
}

void loop() {
	my_homekit_loop();
	delay(10);
}

//==============================
// Homekit setup and loop
//==============================

// access your homekit characteristics defined in my_accessory.c
extern "C" homekit_server_config_t config;
extern "C" homekit_characteristic_t cha_current_temperature;
extern "C" homekit_characteristic_t cha_humidity;
extern "C" homekit_characteristic_t cha_co2;

static uint32_t next_heap_millis = 0;
static uint32_t next_report_millis = 0;

void my_homekit_setup() {
	arduino_homekit_setup(&config);
}

void my_homekit_loop() {
	arduino_homekit_loop();
	const uint32_t t = millis();
	if (t > next_report_millis) {
		// report sensor values every 10 seconds
		next_report_millis = t + 10 * 1000;
		my_homekit_report();
	}
	if (t > next_heap_millis) {
		// show heap info every 5 seconds
		next_heap_millis = t + 5 * 1000;
		LOG_D("Free heap: %d, HomeKit clients: %d",
				ESP.getFreeHeap(), arduino_homekit_connected_clients_count());

	}
}

void my_homekit_report() {
  float temperature_value = dht.readTemperature();
  float humidity_value = dht.readHumidity();
  uint16_t feco2, etvoc, errstat, raw;
  ccs811.read(&feco2,&etvoc,&errstat,&raw); 
  float eco2 = feco2 * 1.0;
  
  // Print measurement results based on status
  if( errstat==CCS811_ERRSTAT_OK ) { 
    Serial.print("CCS811: ");
    Serial.print("eco2=");  Serial.print(eco2);     Serial.print(" ppm  ");
    Serial.print("etvoc="); Serial.print(etvoc);    Serial.print(" ppb  ");
    //Serial.print("raw6=");  Serial.print(raw/1024); Serial.print(" uA  "); 
    //Serial.print("raw10="); Serial.print(raw%1024); Serial.print(" ADC  ");
    //Serial.print("R="); Serial.print((1650*1000L/1023)*(raw%1024)/(raw/1024)); Serial.print(" ohm");
    Serial.println();
  } else if( errstat==CCS811_ERRSTAT_OK_NODATA ) {
    Serial.println("CCS811: waiting for (new) data");
  } else if( errstat & CCS811_ERRSTAT_I2CFAIL ) { 
    Serial.println("CCS811: I2C error");
  } else {
    Serial.print("CCS811: errstat="); Serial.print(errstat,HEX); 
    Serial.print("="); Serial.println( ccs811.errstat_str(errstat) ); 
  }


	cha_current_temperature.value.float_value = temperature_value;
	cha_humidity.value.float_value = humidity_value;
  cha_co2.value.float_value = eco2;
	LOG_D("Current temperature: %.1f humidity: %.1f CO2:  %.1f", temperature_value, humidity_value, eco2);
	homekit_characteristic_notify(&cha_current_temperature, cha_current_temperature.value);
  homekit_characteristic_notify(&cha_humidity, cha_humidity.value);
  homekit_characteristic_notify(&cha_co2, cha_co2.value);

}
