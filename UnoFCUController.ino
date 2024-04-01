#include <MultiStageThermostat.h>



#include "ZLFP10Thermostat.h"
//#define DEBUGOUTPUTDEVICE_LCD
#define DEBUGOUTPUTDEVICE_SERIAL

#ifdef DEBUGOUTPUTDEVICE_LCD
  #include <LiquidCrystal_I2C.h>
  LiquidCrystal_I2C lcd(0x27, 20, 4);
  struct LCDStream : public Stream{
  NullStream( void ) { return; };
  int available( void ) { return 0; };
  void flush( void ) { return; };
  int peek( void ) { return -1; };
  int read( void ){ return -1; };;
  size_t write( uint8_t u_Data ){ return lcd.write(u_Data); };
};
LCDStream lcdDebug;


#endif  



#define RS485_RX_PIN 12  // this goes to RD pin on 485
#define RS485_TX_PIN 10  // this goes to DI pin on 485 
#define DHT_SENSOR_PIN 8
#define RS485_RE_PIN 11
#define RS485_DE_PIN 11
#define TEMP_READER_PIN 6

ZLFP10Thermostat theThermostat(DHT_SENSOR_PIN);
SoftwareSerial SoftSerial(RS485_RX_PIN, RS485_TX_PIN);

struct NullStream : public Stream{
  NullStream( void ) { return; };
  int available( void ) { return 0; };
  void flush( void ) { return; };
  int peek( void ) { return -1; };
  int read( void ){ return -1; };;
  size_t write( uint8_t u_Data ){ return u_Data, 0x01; };
};

NullStream DebugNull;
void setup() {
  theThermostat.setTempReader(TEMP_READER_PIN);
  DebugSetup();
  theThermostat.setSerial(SoftSerial,RS485_DE_PIN, RS485_RE_PIN );
  
  
  theThermostat.setup();
  
}

void loop()
{
  theThermostat.loop( );
}
  
  

  void DebugSetup()
{
  theThermostat.SetDebugOutput(&DebugNull);
    #ifdef DEBUGOUTPUTDEVICE_SERIAL
      Serial.begin(9600);

      while(!Serial && millis() < 5000)
        ;
        Serial.println("starting");
      if(Serial)
      {
        theThermostat.SetDebugOutput(&Serial);
      }
    #endif
 
   #ifdef DEBUGOUTPUTDEVICE_LCD
       lcd.init();  // initialize the lcd
       lcd.clear();
       // Print a message to the LCD.
        lcd.backlight();
        lcd.setCursor(0, 0);
        lcd.print("--------------------");
        lcd.setCursor(6, 1);
        lcd.print("GEEEKPI");
        lcd.setCursor(1, 2);
        lcd.print("Arduino IIC Screen");
        lcd.setCursor(0, 3);
        lcd.print("--------------------");
        lcd.clear();
        lcd.setCursor(0, 0);
        theThermostat.SetDebugOutput(&lcdDebug);
   #endif
   
}

    void  SetStatus(int newStatus)
    {
        theThermostat.SetStatus(newStatus);
    };
    void  Warning(int WarningLevel)
    {
        theThermostat.Warning(WarningLevel);
    };
    void FatalError(int ErrorLevel)
    {
      theThermostat.FatalError(ErrorLevel);
    };
