/* 
* Copyright (c) 2025 P.Cook (alias 'plainFlight')
*
* This file is part of the PlainFlightController distribution (https://github.com/plainFlight/plainFlightController).
* 
* This program is free software: you can redistribute it and/or modify  
* it under the terms of the GNU General Public License as published by  
* the Free Software Foundation, version 3.
*
* This program is distributed in the hope that it will be useful, but 
* WITHOUT ANY WARRANTY; without even the implied warranty of 
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License 
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

/**
* @file   WifiConfig.cpp
* @brief  This class contains methods that controls the wifi and HTML web page.
*/

#include "WifiConfig.hpp"
#include "Config.hpp"
#include "InternalConfig.hpp"


/**
* @brief    Constructore.
* @param    Parameters to be displayed/updated by the WiFi configurator
* @note     TODO - this can be done better in the hpp.
*/
WifiConfig::WifiConfig(FileSystem::NonVolatileData * const theData, float* const batteryVoltage, float* const pitch, float* const roll, float* const yaw)
{
  m_webData = theData;
  m_batteryVoltage = batteryVoltage;
  m_pitch = pitch;
  m_roll = roll;
  m_yaw = yaw;
}


/**
* @brief  Wifi state machine enabled by high throttle stick position when disarmed.
*/
void 
WifiConfig::doWiFiStateMachine()
{
  switch (m_state)
  {
    case WifiState::OFF:
      if constexpr(InternalConfig::DEBUG_CONFIGURATOR){Serial.println("wifi_off");}
      break;

    case WifiState::START:
    {
      if constexpr(InternalConfig::DEBUG_CONFIGURATOR){Serial.println("start_wifi");}
      const bool ok = startWifiConfigurator();
      m_state = (ok) ? WifiState::SERV_CLIENT : WifiState::OFF;
      break;
    }

    default:
    case WifiState::STOP:
      if constexpr(InternalConfig::DEBUG_CONFIGURATOR){Serial.println("stop_wifi");}
      stopWifiConfigurator();
      m_state = WifiState::OFF;
      break;
    
    case WifiState::SERV_CLIENT:
    {
      //serviceWifiConfigurator();
      dnsServer.processNextRequest();
      server.handleClient();
      break;  
    } 
  }
}


/**
* @brief  Initiates a wifi access point and server.
* @return True if wifi started.
*/
bool 
WifiConfig::startWifiConfigurator() 
{
  if constexpr(InternalConfig::DEBUG_CONFIGURATOR){Serial.println("Configuring access point...");}

  IPAddress m_localIP(192,168,4,1);
  IPAddress m_gateway(192,168,4,1);
  IPAddress m_subnet(255,255,255,0);

  WiFi.softAPConfig(m_localIP, m_gateway, m_subnet);
  WiFi.softAP(SSID, PASSWORD);
  m_localIP = WiFi.softAPIP();
  dnsServer.start(53, "*", m_localIP);

  //Pages
  server.on("/", HTTP_GET, [this](){handleRoot();});
  server.on("/main", HTTP_GET, [this](){sendMain();});
  
  //Forms
  server.on("/PITCH", HTTP_POST, [this](){handlePitchGains();});
  server.on("/ROLL", HTTP_POST, [this](){handleRollGains();});
  server.on("/YAW", HTTP_POST, [this](){handleYawGains();});
  server.on("/RATES", HTTP_POST, [this](){handleDegreeRates();});
  server.on("/ANGLE", HTTP_POST, [this](){handleMaxLevelAngles();});
  server.on("/LEVEL_TRIMS", HTTP_POST, [this](){handleLevelTrims();});
  server.on("/SERVO_TRIMS", HTTP_POST, [this](){handleServoTrims();});
  server.on("/VOLT_TRIM", HTTP_POST, [this](){handleBatteryTrim();});

  //Captive portal detection
  server.on("/generate_204", HTTP_GET, [this](){handleRoot();});
  server.on("/hotspot-detect.html", HTTP_GET, [this](){handleRoot();});
  server.on("/connecttest.txt", HTTP_GET, [this](){handleRoot();});
  server.on("/ncsi.txt", HTTP_GET, [this](){handleRoot();});
  server.on("/fwlink", HTTP_GET, [this](){handleRoot();});
  server.onNotFound([this](){handleNotFound();});

  //Handle live updates
  server.on("/MODEL", HTTP_GET, [this](){handleModelAngleUpdates();});

  server.begin();
  return true;//TODO - look at old stuff and see how it worked
}


/**
* @brief  Disconnects and turns off wifi.
* @note   Wifi is always off when the model is armed.
*/
void 
WifiConfig::stopWifiConfigurator() 
{
  if constexpr(InternalConfig::DEBUG_CONFIGURATOR){Serial.println("Client Disconnected.");}
  WiFi.softAPdisconnect();
  WiFi.mode(WIFI_OFF);
  if constexpr(InternalConfig::DEBUG_CONFIGURATOR){Serial.println("Wifi stopped");}
}


/**
* @brief  Informs caller when new data has been captured.
* @return true when a value has been updated.
*/
bool 
WifiConfig::hasUpdatedData()
{
  const bool updated = m_dataUpdated;
  m_dataUpdated = false;
  return updated;
}


/**
* @brief    Send the main HTML page.
*/
void
WifiConfig::sendMain()
{
  int32_t degreesPerSec = 0;

  if constexpr(Config::GYRO_RATE == GyroRate::IS_250_DEGS_SECOND)
  {
    degreesPerSec = 250;
  }
  else
  {
    if constexpr(Config::GYRO_RATE == GyroRate::IS_500_DEGS_SECOND)
    {
      degreesPerSec = 500;
    }
  }

  const int32_t n = snprintf (m_html, HTML_DOC_BUFF_SIZE, INDEX_HTML, 
                          InternalConfig::SOFTWARE_VERSION,
                          m_webData->gains.pitch.p, m_webData->gains.pitch.i, m_webData->gains.pitch.d, m_webData->gains.pitch.ff,
                          m_webData->gains.roll.p, m_webData->gains.roll.i, m_webData->gains.roll.d, m_webData->gains.roll.ff,
                          m_webData->gains.yaw.p, m_webData->gains.yaw.i, m_webData->gains.yaw.d, m_webData->gains.yaw.ff,
                          (m_webData->rates.pitch/100), degreesPerSec, (m_webData->rates.roll/100), degreesPerSec, (m_webData->rates.yaw/100), degreesPerSec, 
                          (m_webData->maxAngle.pitch/100), (m_webData->maxAngle.roll/100), 
                          *m_pitch, *m_roll, m_webData->levelTrim.pitch, m_webData->levelTrim.roll, 
                          m_webData->servoTrim.servo1, m_webData->servoTrim.servo2, m_webData->servoTrim.servo3, m_webData->servoTrim.servo4,
                          *m_batteryVoltage, m_webData->batteryScaler);

  if (HTML_DOC_BUFF_SIZE < (n+1))
  {
    //TODO error !!
    Serial.println("snprintf buff size error !");
  }
  else
  {
    server.send(200, "text/html", m_html);
  }
}
    

/**
* @brief    Handle root.
*/
void
WifiConfig::handleRoot()
{
  Serial.println("handle root");

  server.sendHeader("Location", "http://192.168.4.1/main", true);
  server.send(302, "text/plain", "");
}


/**
* @brief    Handle web requests that are not known.
*/
void
WifiConfig::handleNotFound()
{
  Serial.println("handle not found");

  server.sendHeader("Location", "http://192.168.4.1/main", true);
  server.send(302, "text/plain", "");
}


/**
* @brief    Common decoding method for all PIDF gains.
* @param    Pointer to gains structure to populate.
*/
void
WifiConfig::updateGains(PIDF::Gains* const theGains)
{
  const uint32_t P = server.arg("P").toInt();
  const uint32_t I = server.arg("I").toInt();
  const uint32_t D = server.arg("D").toInt();
  const uint32_t F = server.arg("F").toInt();

  if constexpr(InternalConfig::DEBUG_CONFIGURATOR)
  {
    Serial.println("Gains updated:");
    Serial.println("P:" + String(P));
    Serial.println("I:" + String(I));
    Serial.println("D:" + String(D));
    Serial.println("F:" + String(F));
  }

  theGains->p = P;
  theGains->i = I;
  theGains->d = D;
  theGains->ff = F;
}


/**
* @brief    Decode the pitch gains.
*/
void
WifiConfig::handlePitchGains()
{
  updateGains(&m_webData->gains.pitch);
  server.sendHeader("Location", "/main");
  server.send(303);
  m_dataUpdated = true;
}


/**
* @brief    Decode the roll gains.
*/
void
WifiConfig::handleRollGains()
{
  updateGains(&m_webData->gains.roll);
  server.sendHeader("Location", "/main");
  server.send(303);
  m_dataUpdated = true;
}


/**
* @brief    Decode the yaw gains.
*/
void
WifiConfig::handleYawGains()
{
  updateGains(&m_webData->gains.yaw);
  server.sendHeader("Location", "/main");
  server.send(303);
  m_dataUpdated = true;
}


/**
* @brief    Decode the degrees pers second rates
*/
void
WifiConfig::handleDegreeRates()
{
  const uint32_t pitch = static_cast<uint32_t>(server.arg("pitch").toInt() * 100);
  const uint32_t roll = static_cast<uint32_t>(server.arg("roll").toInt() * 100);
  const uint32_t yaw = static_cast<uint32_t>(server.arg("yaw").toInt() * 100);

  if constexpr(InternalConfig::DEBUG_CONFIGURATOR)
  {
    Serial.println("Degree rates updated:");
    Serial.println("Pitch:" + String(pitch));
    Serial.println("Roll:" + String(roll));
    Serial.println("Yaw:" + String(yaw));
  }

  m_webData->rates.pitch = pitch;
  m_webData->rates.roll = roll;
  m_webData->rates.yaw = yaw;

  server.sendHeader("Location", "/main");
  server.send(303);
  m_dataUpdated = true;
}


/**
* @brief    Decode the max angle trims
*/
void
WifiConfig::handleMaxLevelAngles()
{
  const uint32_t pitch = static_cast<uint32_t>(server.arg("pitch").toInt() * 100);
  const uint32_t roll = static_cast<uint32_t>(server.arg("roll").toInt() * 100);

  if constexpr(InternalConfig::DEBUG_CONFIGURATOR)
  {
    Serial.println("Level rates updated:");
    Serial.println("Pitch:" + String(pitch));
    Serial.println("Roll:" + String(roll));
  }

  m_webData->maxAngle.pitch = pitch;
  m_webData->maxAngle.roll = roll;

  server.sendHeader("Location", "/main");
  server.send(303);
  m_dataUpdated = true;
}


/**
* @brief    Decode the new level trims
*/
void
WifiConfig::handleLevelTrims()
{
  const float pitch = server.arg("pitch").toFloat();
  const float roll = server.arg("roll").toFloat();

  if constexpr(InternalConfig::DEBUG_CONFIGURATOR)
  {
    Serial.println("Level trims updated:");
    Serial.println("Pitch:" + String(pitch));
    Serial.println("Roll:" + String(roll));
  }

  m_webData->levelTrim.pitch = pitch;
  m_webData->levelTrim.roll = roll;

  server.sendHeader("Location", "/main");
  server.send(303);
  m_dataUpdated = true;
}


/**
* @brief    Decode the new servo trims
*/
void
WifiConfig::handleServoTrims()
{
  const int32_t servo1 = server.arg("Servo1").toInt();
  const int32_t servo2 = server.arg("Servo2").toInt();
  const int32_t servo3 = server.arg("Servo3").toInt();
  const int32_t servo4 = server.arg("Servo4").toInt();

  if constexpr(InternalConfig::DEBUG_CONFIGURATOR)
  {
    Serial.println("Servo trims updated:");
    Serial.println("servo1:" + String(servo1));
    Serial.println("servo2:" + String(servo2));
    Serial.println("servo3:" + String(servo3));
    Serial.println("servo4:" + String(servo4));
  }

  m_webData->servoTrim.servo1 = servo1;
  m_webData->servoTrim.servo2 = servo2;
  m_webData->servoTrim.servo3 = servo3;
  m_webData->servoTrim.servo4 = servo4;

  server.sendHeader("Location", "/main");
  server.send(303);
  m_dataUpdated = true;
}


/**
* @brief    Decode the new battery scaler
*/
void
WifiConfig::handleBatteryTrim()
{
  const float voltCalibration = server.arg("volts").toFloat();

  if constexpr(InternalConfig::DEBUG_CONFIGURATOR)
  {
    Serial.println("Voltage calibration updated:");
    Serial.print("Cal:");
    Serial.println(voltCalibration, 5);
  }

  m_webData->batteryScaler = voltCalibration;

  server.sendHeader("Location", "/main");
  server.send(303);
  m_dataUpdated = true;
}


/**
* @brief    Decode the new battery scaler
*/
void
WifiConfig::handleModelAngleUpdates()
{
  char json[64];

  snprintf(json, sizeof(json),
            "{\"pitch\":%.1f,\"roll\":%.1f}",
            *m_pitch, *m_roll);

  if constexpr(InternalConfig::DEBUG_CONFIGURATOR)
  {
    Serial.println("Get Angle Updates:");
    Serial.println(json);
  }

  server.send(200, "application/json", json);
}
