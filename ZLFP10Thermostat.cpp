


#include "ZLFP10Thermostat.h"
#include <ZLFP10Controller.h>
#include <LEDStatusStrip.h>
#include <DewPoint.h>
#define STATUSPINCOUNT 4
#define STATUSBASEPIN 8


ModbusMaster node;

ZLFP10Thermostat::ZLFP10Thermostat(uint8_t pDHTSensorPin):DehumidifyingMultiStageThermostat(pDHTSensorPin) 
{
        Mode=MODE_OFF;
        theLEDStatusStrip.SetPins(STATUSBASEPIN, STATUSPINCOUNT);
}
void ZLFP10Thermostat::setTempPins(uint8_t pRoomTempPin, uint8_t pCoilTempPin)
{
  FCUController.setRoomTempPin(pRoomTempPin);
  FCUController.setCoilTempPin(pCoilTempPin);
}
void ZLFP10Thermostat::setSerial(HardwareSerial &pswSerial, uint8_t pRS485DEPin,uint8_t pRS485REPin)
{
  FCUController.setHardwareSerial(pswSerial,  pRS485DEPin,  pRS485REPin);
}

void ZLFP10Thermostat::setup() 
{
   
    DebugStream->println("starting");


    
  
    setThermostatInterval(0.2);
    setDefaultStage(2);
    MultiStageThermostat::setup();
    DebugStream->println("done starting");
    EnableDehumidify();
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
    
    DebugStream->print(", Hum=");
    DebugStream->print(getHumidity(), 0);
        DebugStream->print(", DP=");
    DebugStream->print(DewPoint(getTemp(),getHumidity()), 0);

    // thermostat properties
    DebugStream->print(", Setpt:");
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
    DebugStream->print(", Error=");
    DebugStream->print(FCUController.FCUSettings.FanFault);
    
    
    DebugStream->print("   " );
    DebugStream->println('\r');
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
    theLEDStatusStrip.BlinkEm(2, 100);
    theLEDStatusStrip.SetStatus(2);
    FCUController.Calibrate();

    // for unknown reasons, when the FCU is powered off and on it comes up with an E0 error. This detects that error and attempts to clear it
    // by soft-powering off and on
    if(FCUController.FCUSettings.Onoff)
    {
      if(FCUController.FCUSettings.FanFault)
      {
        FCUController.SetOnOff(false);
        delay(1000);
        FCUController.SetOnOff(true);

      }
    }
    DehuSettings dhs;
    dhs.bottomTempLimit=1.0;
	dhs.bottomTempRange=0.2;
	dhs.HumidityLimit=55;
	dhs.HumidityRange=5;

  SetParameters(&dhs);
}

int oldStage=-1;
short oldCoil=-1;
    

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
              DebugStream->println("Restarting session");
              DebugStream->print("Onoff=");
              DebugStream->print(Onoff);
              DebugStream->print(" Mode=");
              DebugStream->print(Mode);
              DebugStream->print(" FCUSetTemp=");
              DebugStream->print(FCUSetTemp);
              DebugStream->println();
              RestartSession();
            }

            int newStage=getStage();

            if(oldStage!=newStage || nextcheck==0)// first time in
            {
              DebugStream->println();
              FCUController.SetFanSpeed(newStage);
              theLEDStatusStrip.SetStatus(newStage);
            }
              oldStage=newStage;
              if(newStage==4)
              {
                if(FCUController.FCUSettings.coilTemp != oldCoil)
                {
                  DebugStream->println();
                }
              }
            oldCoil=FCUController.FCUSettings.coilTemp;
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
      Mode=MODE_COOL;
      FCUSetTemp = FCUController.FCUSettings.CoolSetpoint;
      /*if(getTemp() > FCUController.FCUSettings.AutoCoolingSetpoint) // cooling set point
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
      */
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
