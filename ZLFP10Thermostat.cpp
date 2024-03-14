


#include "ZLFP10Thermostat.h"
#include <ZLFP10Controller.h>

ModbusMaster node;

ZLFP10Thermostat::ZLFP10Thermostat(uint8_t pDHTSensorPin):MultiStageThermostat(pDHTSensorPin) 
{
        Mode=MODE_OFF;
}
void ZLFP10Thermostat::setTempReader(uint8_t pTempReaderPin)
{
  FCUController.setTempReader(pTempReaderPin);
}
void ZLFP10Thermostat::setSerial(SoftwareSerial &pswSerial, uint8_t pRS485DEPin,uint8_t pRS485REPin)
{
  FCUController.setSerial(pswSerial,  pRS485DEPin,  pRS485REPin);
}

void ZLFP10Thermostat::setup() 
{
   
    DebugStream->println("starting");


    
  
    setThermostatInterval(0.1);
    setDefaultStage(2);
    MultiStageThermostat::setup();
    DebugStream->println("done starting");
}


void ZLFP10Thermostat::DisplayStatus() {
    unsigned long hour;
    unsigned long minutes;
    unsigned long seconds;
    unsigned long now = millis();
    seconds = now / 1000;
    minutes = now / 60000;
    seconds -= minutes * 60;

    hour = minutes / 60;
    minutes -= hour * 60;

    DebugStream->print(hour);
    DebugStream->print(":");
    if (minutes < 10)
        DebugStream->print('0');
    DebugStream->print(minutes);
    DebugStream->print(":");

    if (seconds < 10)
        DebugStream->print('0');
    DebugStream->print(seconds);
    // sensor properties
    DebugStream->print(", Temp=");
    DebugStream->print(getTemp(), 1);
    
    DebugStream->print(", Humidity=");
    DebugStream->print(getHumidity(), 0);
    // thermostat properties
    DebugStream->print(", Setpoint:");
    DebugStream->print(FCUSetTemp );
    DebugStream->print(", Stage=");
    DebugStream->print(getLastStage());
    DebugStream->print(", U=");
    DebugStream->print(upperthreshold,1);
    DebugStream->print(" L=");
    DebugStream->print(lowerthreshold,1);
    DebugStream->print(" Next check=");
    if (nextAdjustmentTime > millis())
    {
        Serial.print((nextAdjustmentTime-millis())/1000);
    }
    else
        Serial.print("0");


    //FCU properties
    DebugStream->print(", Temp Pin=");
    DebugStream->print(FCUController.lastTempPin);
    DebugStream->print(", Reported Room Temp=");
    DebugStream->print(FCUController.FCUSettings.RoomTemp );
    
    
    DebugStream->print(",  RPM=");
    DebugStream->print(FCUController.FCUSettings.FanRPM);

    DebugStream->print(", Fan setting=");
    DebugStream->print(FCUController.FCUSettings.FanSetting);
    DebugStream->print(", Coil Temperature=");
    DebugStream->print(FCUController.FCUSettings.coilTemp);
    DebugStream->print(", Valve=");
    DebugStream->print(FCUController.FCUSettings.valveOpen);
    DebugStream->print("   " );
    DebugStream->print('\r');
}
int Delays[]=
{
  ADJUSTMENT_INTERVAL+60, 
  ADJUSTMENT_INTERVAL,
  ADJUSTMENT_INTERVAL,
  ADJUSTMENT_INTERVAL, 
  ADJUSTMENT_INTERVAL
};

void ZLFP10Thermostat::RestartSession()
{
     nextcheck = 0;
    settings.CoolingSetpoint=FCUSetTemp;
    settings.HeatingSetpoint=FCUSetTemp;
    settings.mode=Mode;
     
    setSettings(settings);
    setStageDelays(MAXFANSPEED+1, Delays);

    FCUController.Calibrate(Mode==MODE_HEAT, FCUSetTemp);
 
}


void ZLFP10Thermostat::loop() 

{

    // once a second read temp, 
    // once a minute, and on change in temp, read settings

    float oldtemp = getTemp();

    delay(1000);

    ReadTemp();

        unsigned long now = millis();
    float temp=getTemp();
    if (oldtemp != temp || now > nextcheck  || nextcheck == 0)
    {
            short oldOnOff, oldMode, oldSetTemp;
            oldOnOff=Onoff;
            oldMode=Mode ;
            oldSetTemp= FCUSetTemp;


            ReadFCUSettings();
            // if on/off, mode or setpoint has changed since last iteration, reset everything
            if(oldOnOff!=Onoff || oldMode!=Mode ||  oldSetTemp!= FCUSetTemp)
            {
            
              RestartSession();
            }

            int oldStage=getLastStage();
            int newStage=getStage();
            if(oldStage!=newStage || nextcheck==0)// first time in
            {
              int targetTemp;
              if(Mode==MODE_HEAT)
              {
                  targetTemp=FCUSetTemp-newStage;
              }
              else
              {
                  targetTemp=FCUSetTemp+newStage;
              }
              DebugStream->println();
              
              FCUController.SetFanSpeed(newStage, targetTemp);
            }

            nextcheck = now +10000; // don't check for 60 seconds unless the temperature chnages

        }
        
        DisplayStatus();
        
    
   


}

void ZLFP10Thermostat::ReadFCUSettings()
{
  FCUController.ReadSettings();  

  Onoff=FCUController.FCUSettings.Onoff;
    
    if(FCUController.FCUSettings.Mode== FCU_MODE_AUTO) // auto temp mode, compare setpoints against actual temp
    {
      
      if(getTemp() > FCUController.FCUSettings.AutoCoolingSetpoint) // cooling set point
      {
        Mode=MODE_COOL;
      }
      if(getTemp() < FCUController.FCUSettings.AutoHeatingSetpoint) // heating set point
      {
        Mode=MODE_HEAT;
      }

      // have to break this out because setpoint could change outside of mode change
      if(Mode== MODE_HEAT)
      {
        FCUSetTemp=FCUController.FCUSettings.AutoHeatingSetpoint;
      }
      if(Mode== MODE_COOL)
      {
        FCUSetTemp= FCUController.FCUSettings.AutoHeatingSetpoint;
      }

    }
    else
    {
      short FCUMode;
      FCUMode = FCUController.FCUSettings.Mode;
      FCUSetTemp=0;
      if(FCUMode==FCU_MODE_HEAT)
      {
        FCUSetTemp = FCUController.FCUSettings.HeatSetpoint;
        Mode=MODE_HEAT;
      }
      if(FCUMode==FCU_MODE_COOL)
      {
        FCUSetTemp = FCUController.FCUSettings.CoolSetpoint;
        Mode=MODE_COOL;
      }
  }
  


}


void ZLFP10Thermostat::SetDebugOutput(Stream * pDebug)
{
  DebugStream=pDebug;
  FCUController.SetDebugOutput(pDebug);
};