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
* @file   WifiConfig.hpp
* @brief  This class contains methods that controls the wifi and HTML web page.
*/

#pragma once

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "Html.hpp"
#include "FileSystem.hpp"
#include "Config.hpp"




class WifiConfig : public Html
{
  public:
    WifiConfig(FileSystem::NonVolatileData* const theData, float* const batteryVoltage, float* const pitch, float* const roll, float* const yaw);
    void doWiFiStateMachine();
    bool startWifiConfigurator();
    void stopWifiConfigurator();
    void serviceWifiConfigurator();
    bool hasUpdatedData();

  private:

    enum class WifiState : uint32_t
    {
      OFF = 0U,
      START,
      STOP,
      SERV_CLIENT
    };

    // Set these to your desired credentials.
    static constexpr char SSID[] = "PlainFlight";
    static constexpr char PASSWORD[] = "12345678";
    static constexpr uint32_t HTML_VARIABLES_SIZE = 200U;
    static constexpr uint32_t HTML_DOC_BUFF_SIZE = sizeof(INDEX_HTML) + HTML_VARIABLES_SIZE;

    //Methods
    void updateGains(PIDF::Gains* const theGains);
    void sendMain();
    void handleRates(); 
    void handleTrims();
    void handleRoot();
    void handleNotFound();

    void handlePitchGains();
    void handleRollGains();
    void handleYawGains();
    void handleDegreeRates();
    void handleMaxLevelAngles();
    void handleLevelTrims();
    void handleServoTrims();
    void handleBatteryTrim();

    //Variables
    FileSystem::NonVolatileData* m_webData;
    WifiState m_state = WifiState::START;
    String m_currentLine = "";
    char m_html[HTML_DOC_BUFF_SIZE] = {0};
    bool m_dataUpdated = false;
    float* m_batteryVoltage;
    float* m_pitch;
    float* m_roll;
    float* m_yaw;

    //Objects
    DNSServer dnsServer;
    WebServer server = WebServer(80);
};