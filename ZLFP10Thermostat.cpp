


#include "ZLFP10Thermostat.h"
#include <ZLFP10Controller.h>
#include "LEDStatusStrip.h"
#include "DewPoint.h"
#define STATUSPINCOUNT 4
#define STATUSBASEPIN 8
#define LOOP_DELAY_MS 200
#define FCU_SETTINGS_POLL_MS 5000




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

void ZLFP10Thermostat::setClientSerial(HardwareSerial &pswSerial, uint8_t pRS485DEPin,uint8_t pRS485REPin,  uint8_t ModbusID)
{
  FCUController.setClientHardwareSerial(pswSerial,  pRS485DEPin,  pRS485REPin, ModbusID);
}

ZLFP10ModbusServer TheServer;

void ZLFP10Thermostat::setServerSerial(SoftwareSerial &pswSerial, uint8_t pRS485DEPin,uint8_t pRS485REPin, uint8_t ModbusID)
{
  FCUController.setServerSoftwareSerial(pswSerial,  pRS485DEPin,  pRS485REPin,  ModbusID, &TheServer);
  TheServer.SetupClient(FCUController.GetClient());
  TheServer.SetParentThermostat(this);
}

void ZLFP10Thermostat::setup() 
{
    DEBUG_INFO(DEBUG_MODULE_THERMOSTAT, "Starting thermostat setup");
    DEBUG_INFO_STR(DEBUG_MODULE_THERMOSTAT, "File", __FILE__);
    DEBUG_INFO_STR(DEBUG_MODULE_THERMOSTAT, "Build date", __DATE__);

    LEDStatusStrip theLEDStatusStrip;
  
    setThermostatInterval(0.2);
    setDefaultStage(2);
    MultiStageThermostat::setup();
    DEBUG_INFO(DEBUG_MODULE_THERMOSTAT, "Thermostat setup complete");
    EnableDehumidify();
}


void ZLFP10Thermostat::DisplayStatus() {
    // Use the debug framework for status display
    DEBUG_INFO(DEBUG_MODULE_THERMOSTAT, "Status update");
    
    // Thermostat integer properties
    const char* thermIntNames[] = {"On/Off", "Mode", "Fan Mode Setting", "Setpoint", "Stage", "Next check"};
    DEBUG_INFO_NAMED(DEBUG_MODULE_THERMOSTAT, "FCU Settings (1)", 6, thermIntNames,
        Onoff,
        Mode,
        FCUController.FCUSettings.FanModeSetting,
        FCUSetTemp,
        getLastStage(),
        (nextAdjustmentTime > millis()) ? (nextAdjustmentTime-millis())/1000 : 0);
    
    // Thermostat float properties
    const char* thermFloatNames[] = {"Real room Temp", "Humidity", "Dew point", "Upper threshold", "Lower threshold"};
    DEBUG_INFO_NAMED_FLOAT(DEBUG_MODULE_THERMOSTAT, "FCU Settings (2)", 5, thermFloatNames,
        getTemp(), // Actual temperature value
        getHumidity(), // Actual humidity value
        DewPoint(getTemp(), getHumidity()), // Actual dew point value
        upperthreshold, // Actual upper threshold value
        lowerthreshold); // Actual lower threshold value
    
    // FCU properties - integer values
    const char* fcuIntNames[] = {"Temp pin", "Fan RPM", "Fan setting", "Valve open", "Fan fault"};
    DEBUG_INFO_NAMED(DEBUG_MODULE_FCU, "FCU Settings (3)", 5, fcuIntNames,
        FCUController.lastTempPin,
        FCUController.FCUSettings.FanRPM,
        FCUController.FCUSettings.FanSetting,
        FCUController.FCUSettings.valveOpen,
        FCUController.FCUSettings.FanFault);
    
    // FCU properties - float values
    const char* fcuFloatNames[] = {"FCU room temp", "Coil temp"};
    // Cast to float to ensure proper va_arg handling in infoNamedFloat
    DEBUG_INFO_NAMED_FLOAT(DEBUG_MODULE_FCU, "FCU Settings (4)", 2, fcuFloatNames,
        (float)FCUController.FCUSettings.RoomTemp, // Actual room temperature value
        (float)FCUController.FCUSettings.coilTemp); // Actual coil temperature value
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
    DEBUG_INFO(DEBUG_MODULE_THERMOSTAT, "Restarting session - applying new settings");
    
    nextcheck = 0;
    settings.CoolingSetpoint=FCUSetTemp;
    settings.HeatingSetpoint=FCUSetTemp;
    settings.mode=Mode;
     
    setSettings(settings);
    setStageDelays(MAXFANSPEED+1, Delays);
    theLEDStatusStrip.BlinkEm(2, 100);
    theLEDStatusStrip.SetStatus(2);
    
    // Check if calibration is needed for the current mode
    if(FCUController.NeedsCalibration(Mode)) {
        FCUController.Calibrate();
    } else {
        FCUController.LoadCalibrationData();
    }

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
    

void ZLFP10Thermostat::notifySetpointWrite(bool isCooling, short setpointValue)
{
    FCUSetTemp = setpointValue;
    Mode = isCooling ? MODE_COOL : MODE_HEAT;

    DEBUG_INFO(DEBUG_MODULE_THERMOSTAT, "Setpoint write from Modbus - applying immediately");
    DEBUG_INFO_INT(DEBUG_MODULE_THERMOSTAT, "FCU Set Temp", FCUSetTemp);

    Settings newSettings = {Mode, FCUSetTemp, FCUSetTemp, 0};
    setSettings(newSettings);

    int newStage = getStage();
    FCUController.SetFanSpeed(newStage);
    theLEDStatusStrip.SetStatus(newStage);
    oldStage = newStage;
    nextcheck = 0;
}

void ZLFP10Thermostat::loop() 
{
    // Service HA Modbus requests first so writes are not blocked by FCU polling
    FCUController.ServiceAnyRequests();

    float oldtemp = getTemp();
    delay(LOOP_DELAY_MS);
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
            // Only restart session (and recalibrate) when mode changes
            // On/Off and setpoint changes no longer trigger recalibration
            if(oldMode!=Mode)
            {
              DEBUG_INFO(DEBUG_MODULE_THERMOSTAT, "Mode changed - restarting session");
              DEBUG_INFO_INT(DEBUG_MODULE_THERMOSTAT, "Old Mode", oldMode);
              DEBUG_INFO_INT(DEBUG_MODULE_THERMOSTAT, "New Mode", Mode);
              RestartSession();
            }
            else if(oldOnOff!=Onoff || oldSetTemp!= FCUSetTemp)
            {
              // For on/off and setpoint changes, just update settings without recalibration
              DEBUG_INFO(DEBUG_MODULE_THERMOSTAT, "Settings changed - updating without recalibration");
              DEBUG_INFO_INT(DEBUG_MODULE_THERMOSTAT, "On/Off", Onoff);
              DEBUG_INFO_INT(DEBUG_MODULE_THERMOSTAT, "FCU Set Temp", FCUSetTemp);
              
              // Update thermostat settings to reset thresholds for new setpoint
              Settings newSettings = {Mode, FCUSetTemp, FCUSetTemp, 0}; // mode, heat, cool, hum
              setSettings(newSettings);
              
              // Force SetFanSpeed call to update PWM for new setpoint
              nextcheck = 0; // This will force SetFanSpeed to be called below
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
            nextcheck = now + FCU_SETTINGS_POLL_MS;
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
      FCUSetTemp = FCUController.FCUSettings.CoolSetpointAuto;

    }
    else
    {
      short FCUMode;
      FCUMode = FCUController.FCUSettings.Mode;
      FCUSetTemp=0;
      if(FCUMode==FCU_MODE_HEAT)
      {
        FCUSetTemp = FCUController.FCUSettings.HeatSetpointAuto;
        Mode=MODE_HEAT;
      }
      if(FCUMode==FCU_MODE_COOL)
      {
        FCUSetTemp = FCUController.FCUSettings.CoolSetpointAuto;
        Mode=MODE_COOL;
      }
  }
  


}

void ZLFP10Thermostat::SetDebugOutput(Stream * pDebug)
{
  DebugStream=pDebug;
  FCUController.SetDebugOutput(pDebug);
};

// Getter methods for FCU settings
word ZLFP10Thermostat::getFCUOnOffStatus() {
    return FCUController.FCUSettings.Onoff;
}

word ZLFP10Thermostat::getFCUModeStatus() {
    return FCUController.FCUSettings.Mode;
}

word ZLFP10Thermostat::getFCUFanSpeedStatus() {
    return FCUController.FCUSettings.FanModeSetting;
}

// Additional getter methods for new registers
int ZLFP10Thermostat::getTempFault() {
    return FCUController.FCUSettings.TempFault;
}

int ZLFP10Thermostat::getCoilTempFault() {
    return FCUController.FCUSettings.CoilTempFault;
}

float ZLFP10Thermostat::getHumidity() {
    return lastHum; // Use inherited humidity from Arduino sensor
}

float ZLFP10Thermostat::getActualHumidity() {
    return lastHum; // Return humidity * 10 for register 39322 (similar to getActualRoomTemp)
}

float ZLFP10Thermostat::getDewPoint() {
  return (DewPoint(getTemp(), getHumidity()));
}


// Missing getter methods for FCU holding registers
word ZLFP10Thermostat::getCoolSetpoint() {
    return FCUController.FCUSettings.CoolSetpoint;
}

word ZLFP10Thermostat::getHeatSetpoint() {
    return FCUController.FCUSettings.HeatSetpoint;
}

word ZLFP10Thermostat::getCoolSetpointAuto() {
  return FCUController.FCUSettings.CoolSetpointAuto;
}

word ZLFP10Thermostat::getHeatSetpointAuto() {
  return FCUController.FCUSettings.HeatSetpointAuto;
}

// Missing getter methods for FCU input registers
word ZLFP10Thermostat::getFCURoomTemp() {
    // RoomTemp is stored as short (signed), convert to word for Modbus
    // Values are already divided by 10 in ReadSettings, so we return as-is
    return (word)((short)FCUController.FCUSettings.RoomTemp);
}

word ZLFP10Thermostat::getCoilTemp() {
    // coilTemp is stored as short (signed), convert to word for Modbus
    // Values are already divided by 10 in ReadSettings, so we return as-is
    return (word)((short)FCUController.FCUSettings.coilTemp);
}

word ZLFP10Thermostat::getFanSetting() {
    return FCUController.FCUSettings.FanSetting;
}

word ZLFP10Thermostat::getFanRPM() {
    return FCUController.FCUSettings.FanRPM;
}

word ZLFP10Thermostat::getValveOpen() {
    return FCUController.FCUSettings.valveOpen;
}

word ZLFP10Thermostat::getFanFault() {
    return FCUController.FCUSettings.FanFault;
}

// Missing getter methods for thermostat state
word ZLFP10Thermostat::getOnoff() {
    return Onoff;
}

word ZLFP10Thermostat::getMode() {
    return Mode;
}

word ZLFP10Thermostat::getFCUSetTemp() {
    return FCUSetTemp;
}

