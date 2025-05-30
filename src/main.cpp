//-- ---------------------------------------------------------------------------
//--
//-- Autor(en):  svesl
//--
//-- MQTT to MODBUS Fronius Smart Meterb Emulator
//-- recive data via MQTT and place it on MODBUS Register
//
//-- ----------------------------------------------------------------------------
//--
//-- Name: Fron SM 2 Modbus
//--
//--
//-- Beschränkungen:
//--
//-- --------------------------------------------------------------------------
//-- Änderunghistorie :
//-- --------------------------------------------------------------------------
//-- Version  Autor    Datum      Beschreibung    Name
//-- --------------------------------------------------------------------------
//-- 1.0   svesl  2024/11/22    first v ersion
//-- 1.2   svesl  2024/11/28    test and add registers
//-- 1.3   svesl  2024/12/16    add WiFi Events and reconnect support
//-- 1.4   svesl  2024/12/19    add Block MODBUS befor first value on import and export available
//-- 1.5   svesl  2025/1/10     add Block value = 0 on import and export 
//-- 1.6   svesl  2025/3/12     add reboot count support
//-- 1.7   svesl  2025/5/30     add reconnect counter on wifi and mqtt for debugging 
//--                            add adress define
//-- --------------------------------------------------------------------------
//-- TODO
//-- Speicherung Zählerstande für Import Export im Flash oder nvram für reboot

#define Version 1.7
#include <Arduino.h>
#include <ModbusIP_ESP8266.h> //für MODBUS
#include <WiFi.h>             //für WiFi Verbinding
#include <ESPmDNS.h>
#include <mqtt_client.h> //
#include <iostream>
#include <cstring>
#include <Preferences.h> // manages non volatile memory

#include "FroniuSM_Registerdef.h"
#include "myMQTTDefs.h"

// Modbus server object
ModbusIP mb;
void initModbuscommon(void);
void fillModbusregs(void);

// MQTT
void init_mqtt(void);                                        // MQTT Initialisieren
esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event); // Eventhandler startet bei MQTT-Events
esp_mqtt_client_config_t mqtt_cfg = {};
esp_mqtt_client_handle_t mqttclient = {};

bool handleTopic(const char *c_topic, const char *st_dat);

bool isConnected = false;
bool blockmbread = false;
bool blockIMfirstread = false; // Blockiert MODBUS bis erster gültiger Zählerwert für Import vorliegt
bool blockEXfirstread = false; // Blockiert MODBUS bis erster gültiger Zählerwert für Export vorliegt

void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
uint32_t wifi_connect_retry = 0;
const char *hostname = "esp32-FronSM2MODBUS";
uint32_t reconnect_cnt_mqtt=0;
uint32_t reconnect_cnt_wifi=0;


// Array of TopicHandler to match topics to register
TopicHandler handlers[] = {
    {SUB_TOPIC_I_TO, MODBUS_REG_I_TO},
    {SUB_TOPIC_I_L1, MODBUS_REG_I_L1},
    {SUB_TOPIC_I_L2, MODBUS_REG_I_L2},
    {SUB_TOPIC_I_L3, MODBUS_REG_I_L3},
    {SUB_TOPIC_U_L1, MODBUS_REG_U_L1},
    {SUB_TOPIC_U_L2, MODBUS_REG_U_L2},
    {SUB_TOPIC_U_L3, MODBUS_REG_U_L3},
    {SUB_TOPIC_FREQ, MODBUS_REG_FREQ},
    {SUB_TOPIC_P_TO, MODBUS_REG_P_TO},
    {SUB_TOPIC_P_L1, MODBUS_REG_P_L1},
    {SUB_TOPIC_P_L2, MODBUS_REG_P_L2},
    {SUB_TOPIC_P_L3, MODBUS_REG_P_L3},
    {SUB_TOPIC_E_EX, MODBUS_REG_E_EX},
    {SUB_TOPIC_E_IM, MODBUS_REG_E_IM},
};
// Array of base register entry
// ca_stdregisters modbus_common;
ca_stdregisters modbus_common = {
    MODBUS_REG_SM_MFRNAME,
    MODBUS_REG_SM_MODEL,
    MODBUS_REG_SM_NAME,
    MODBUS_REG_SM_VERSION,
    MODBUS_REG_SM_SN};


void BootCntService(void); //count, store non-volatile and print number of boots

void setup()
{
  // Init Serial monitor
  Serial.begin(115200);
  while (!Serial)
  {
  }
  Serial.println("__ OK __");
  Serial.println("Fronius SmartMeter Emulator");
  Serial.print("Version: ");
  Serial.println(Version);

  BootCntService();

  WiFi.setTxPower(WIFI_POWER_MINUS_1dBm);
  // Connect to WiFi
  WiFi.onEvent(WiFiEvent);
  // WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(hostname);
  WiFi.begin(ssid, pass);
  delay(200);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(". ");
    delay(500);
  }
  IPAddress wIP = WiFi.localIP();
  Serial.printf("WIFi IP address: %u.%u.%u.%u\n", wIP[0], wIP[1], wIP[2], wIP[3]);

  init_mqtt();

  mb.server();
  // initModbuscommon();
  fillModbusregs();

  Serial.println("Start MODBUS Server");
}

void loop()
{
  if (!blockmbread && blockIMfirstread && blockEXfirstread)
  {
     mb.task(); //Serial.print(".");
  }
}

bool handleTopic(const char *c_topic, const char *st_dat)
{
  for (const auto &handler : handlers)
  {
    // Serial.printf("count %d",cnt);
    if (strcmp(c_topic, handler.topic) == 0)
    {

      String str_dat = st_dat;
      float value = str_dat.toFloat();
      if ((strcmp(c_topic, SUB_TOPIC_E_IM) == 0 && value <= 0) || strcmp(c_topic, SUB_TOPIC_E_EX) == 0 && value <= 0)
      {
        Serial.printf("Achtung Energy value 0 on Topic: %s  Register= 0x%x Wert= %f\r\n", handler.topic, handler.regadress, value);
      }
      else
      {
        uint16_t *hexbytes = reinterpret_cast<uint16_t *>(&value);
        blockmbread = true;
        mb.Hreg(handler.regadress, hexbytes[1]);
        mb.Hreg((handler.regadress + 1), hexbytes[0]);
        blockmbread = false;
       // Serial.printf("Topic: %s  Register= 0x%x Wert= %f\r\n", handler.topic, handler.regadress, value);
      }
      // Chek first Val in import and export register
      if (!blockIMfirstread || !blockEXfirstread)
      {
        if (!blockIMfirstread && strcmp(c_topic, SUB_TOPIC_E_IM) == 0 && value > 0)
        {
          blockIMfirstread = true;
        }
        if (!blockEXfirstread && strcmp(c_topic, SUB_TOPIC_E_EX) == 0 && value > 0)
        {
          blockEXfirstread = true;
        }
      }

      return true;
    }
  }
  return false;
}

void initModbuscommon(void)
{
  strlcpy(modbus_common.mfrname, MODBUS_REG_SM_MFRNAME, sizeof(modbus_common.mfrname));
  strlcpy(modbus_common.model, MODBUS_REG_SM_MODEL, sizeof(modbus_common.model));
  strlcpy(modbus_common.options, MODBUS_REG_SM_NAME, sizeof(modbus_common.options));
  strlcpy(modbus_common.version, MODBUS_REG_SM_VERSION, sizeof(modbus_common.version));
  strlcpy(modbus_common.serial, MODBUS_REG_SM_SN, sizeof(modbus_common.serial));
}

void fillModbusregs(void)
{
  mb.addHreg(0x9c40, 21365); // 40000 Sunspec Id start "SunS"
  mb.addHreg(0x9c41, 28243); // 40001 Sunspec Id end
  mb.addHreg(0x9c42, 1);     // 40002 SunSpec_DID       "1"
  mb.addHreg(0x9c43, 65);    // 40003 SunSpec_Length    "65"

  // 40004 Smart Meter Manufacturer "Fronius"
  for (uint8_t i = 0; i < 32; i += 2)
  {
    uint16_t value = 0;
    if (strlen(modbus_common.mfrname) > i)
      value = (modbus_common.mfrname[i] << 8) | (i + 1 < strlen(modbus_common.mfrname) ? modbus_common.mfrname[i + 1] : 0);
    mb.addHreg(0x9c44 + (i / 2), value); // 40004 - 40019 Manufacturer name
    // Serial.printf("Vendor: write %d to register %d\r\n", value, (0x9c44 + (i / 2)));
  }
  // 40020 - 40035 Device Model Name
  for (uint8_t i = 0; i < 32; i += 2)
  {
    uint16_t value = 0;
    if (strlen(modbus_common.model) > i)
      value = (modbus_common.model[i] << 8) | (i + 1 < strlen(modbus_common.model) ? modbus_common.model[i + 1] : 0);
    mb.addHreg(0x9c54 + (i / 2), value); // 40020 - 40035 Device Model Name
    // Serial.printf("Modelname: write %d to register %d\r\n", value, (0x9c54 + (i / 2)));
  }
  // 40036 - 40043 Options
  for (uint8_t i = 0; i < 16; i += 2)
  {
    uint16_t value = 0;
    if (strlen(modbus_common.options) > i)
      value = (modbus_common.options[i] << 8) | (i + 1 < strlen(modbus_common.options) ? modbus_common.options[i + 1] : 0);
    mb.addHreg(0x9c64 + (i / 2), value); // 40036 - 40043 Options
    // Serial.printf("Options: write %d to register %d\r\n", value, (0x9c64 + (i / 2)));
  }
  // 40044 - 40051 Version
  for (uint8_t i = 0; i < 16; i += 2)
  {
    uint16_t value = 0;
    if (strlen(modbus_common.version) > i)
      value = (modbus_common.version[i] << 8) | (i + 1 < strlen(modbus_common.version) ? modbus_common.version[i + 1] : 0);
    mb.addHreg(0x9c6c + (i / 2), value); // 40044 - 40051 Version
    // Serial.printf("Version: write %d to register %d\r\n", value, (0x9c6c + (i / 2)));
  }
  // 40052 - 40067 Serial Number
  for (uint8_t i = 0; i < 32; i += 2)
  {
    uint16_t value = 0;
    if (strlen(modbus_common.serial) > i)
      value = (modbus_common.serial[i] << 8) | (i + 1 < strlen(modbus_common.serial) ? modbus_common.serial[i + 1] : 0);
    mb.addHreg(0x9c74 + (i / 2), value); // 40052 - 40067 Serial Number
    // Serial.printf("Modbus: write %d to register %d\r\n", value, (0x9c74 + (i / 2)));
  }

  mb.addHreg(0x9c84, MODBUS_REG_SM_ADDR);    // 40068 DeviceAddress Modbus TCP Address: 202  240 (Adress 1) 241 (Adress 2)
  mb.addHreg(0x9c85, 213);    // 40069 SunSpec_DID
  mb.addHreg(0x9c86, 124);    // 40070 SunSpec_Length
  mb.addHreg(0x9c87, 0, 123); // 40071 - 40194 smartmeter data
  mb.addHreg(0x9d03, 65535);  // 40195 end block identifier
  mb.addHreg(0x9d04, 0);      // 40196
}

void init_mqtt(void)
{
  mqtt_cfg.uri = CONFIG_BROKER_URL;
  mqtt_cfg.port = CONFIG_BROKER_PRT;
  mqtt_cfg.event_handle = mqtt_event_handler;
  mqtt_cfg.username = CONFIG_BROKER_USR;
  mqtt_cfg.password = CONFIG_BROKER_PSW;
  mqttclient = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_start(mqttclient);
  delay(200);
  // esp_mqtt_client_subscribe(mqttclient, SUB_TOPIC_I_AC, 1);
  // delay(200);
  // esp_mqtt_client_publish(mqttclient, "SEND_TOPIC_1", "testesp online", 15, 1, false);
}

void BootCntService(void)
{
  Preferences prefs;              // start instance for nvs usage
  unsigned int bootCount;         // Zähler der Geräteneustarts
  if (!prefs.begin("nvs", false)) // Initialize the non volatile memory
  {
    Serial.println("NVS error:");
    //return ER_NVS;
  }
  else
  {
    Serial.println("NVS load bootcount");
    bootCount = prefs.getUInt("bootcnt_por");
    Serial.printf("NVS bootcount read %d\n", bootCount);
    bootCount++;
    prefs.putUInt("bootcnt_por", bootCount); // store new value
  }
}

esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
  char c_topic[event->topic_len + 1];
  char c_payload[event->data_len + 1];
  String st_dat;

  switch (event->event_id)
  {
  case MQTT_EVENT_CONNECTED:
    Serial.println("MQTT verbunden  ");
    Serial.print("MQTT subscribed id: ");
    Serial.println(SUB_TOPIC_BASE);
    esp_mqtt_client_subscribe(mqttclient, SUB_TOPIC_BASE, 0);
    // esp_mqtt_client_subscribe(mqttclient, SUB_TOPIC_I_AC, 1);
    isConnected = true;
    reconnect_cnt_mqtt++;
    Serial.printf("Num of MQTT connectins: %u\n", reconnect_cnt_mqtt);
    break;
  case MQTT_EVENT_DISCONNECTED:
    isConnected = false;
    Serial.println("MQTT nicht verbunden  ");
    break;
  case MQTT_EVENT_SUBSCRIBED:
    Serial.println("MQTT subscribe ");
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    Serial.println("MQTT unsubscribe ");
    break;
  case MQTT_EVENT_PUBLISHED:
    Serial.println("MQTT published ");
    break;
  case MQTT_EVENT_DATA:
    // Serial.println("MQTT received ");
    // Topic prüfen über event->topic
    // Daten string über event->data
    // Extrahieren der Zeichenketten für Topic und Daten

    strncpy(c_topic, event->topic, event->topic_len);
    strncpy(c_payload, event->data, event->data_len);

    c_topic[event->topic_len] = 0;
    c_payload[event->data_len] = 0;

    // Serial.printf("NEW Topic= %s\r\n", c_topic);
    // Serial.printf("NEW Data= %s\r\n", c_payload);

    handleTopic(c_topic, c_payload);

    break;
  case MQTT_EVENT_ERROR:
    Serial.println("MQTT error ");
    break;
  default:
    break;
  }
  return ESP_OK;
}

void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info)
{
  Serial.printf("[WiFi-event] event: %d\n", event);
  // Serial.printf("[WiFi-event] info: %d\n", info);

  switch (event)
  {
  case ARDUINO_EVENT_WIFI_READY:
    Serial.println("WiFi interface ready");
    break;
  case ARDUINO_EVENT_WIFI_SCAN_DONE:
    Serial.println("Completed scan for access points");
    break;
  case ARDUINO_EVENT_WIFI_STA_START:
    Serial.println("WiFi client started");
    break;
  case ARDUINO_EVENT_WIFI_STA_STOP:
    Serial.println("WiFi client stopped");
    break;
  case ARDUINO_EVENT_WIFI_STA_CONNECTED:
    Serial.println("Connected to access point");
    Serial.println(WiFi.getHostname());
    Serial.printf("RSSI: %d\n", WiFi.RSSI());
    wifi_connect_retry = 0;
    reconnect_cnt_wifi++;
    Serial.printf("Num of WiFi connectins: %u\n", reconnect_cnt_wifi);
    
    break;
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    Serial.printf("Disconnected from WiFi access point");
    Serial.printf("Disconnected from WiFi access point. Count = %d\n", wifi_connect_retry);
    wifi_connect_retry++; // keine Überlaufüberwachung
    WiFi.reconnect();

    break;
  case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
    Serial.println("Authentication mode of access point has changed");
    break;
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    Serial.print("Obtained IP address: ");
    Serial.println(WiFi.localIP());
    if (!MDNS.begin(WiFi.getHostname()))
    { // http://esp32.local
      Serial.println("Error setting up MDNS responder!");
    }
    else
    {
      Serial.println("mDNS responder started");
      Serial.printf("Call: http://%s", WiFi.getHostname());
      Serial.println(".local");
    }
    break;
  case ARDUINO_EVENT_WIFI_STA_LOST_IP:
    Serial.println("Lost IP address and IP address is reset to 0");
    break;
  case ARDUINO_EVENT_WPS_ER_SUCCESS:
    Serial.println("WiFi Protected Setup (WPS): succeeded in enrollee mode");
    break;
  case ARDUINO_EVENT_WPS_ER_FAILED:
    Serial.println("WiFi Protected Setup (WPS): failed in enrollee mode");
    break;
  case ARDUINO_EVENT_WPS_ER_TIMEOUT:
    Serial.println("WiFi Protected Setup (WPS): timeout in enrollee mode");
    break;
  case ARDUINO_EVENT_WPS_ER_PIN:
    Serial.println("WiFi Protected Setup (WPS): pin code in enrollee mode");
    break;
  case ARDUINO_EVENT_WIFI_AP_START:
    Serial.println("WiFi access point started");
    break;
  case ARDUINO_EVENT_WIFI_AP_STOP:
    Serial.println("WiFi access point  stopped");
    break;
  case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
    Serial.println("Client connected");
    break;
  case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
    Serial.println("Client disconnected");
    break;
  case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
    Serial.println("Assigned IP address to client");
    break;
  case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:
    Serial.println("Received probe request");
    break;
  case ARDUINO_EVENT_ETH_GOT_IP6:
    Serial.println("IPv6 is preferred");
    break;
  case ARDUINO_EVENT_ETH_START:
    Serial.println("Ethernet started");
    break;
  case ARDUINO_EVENT_ETH_STOP:
    Serial.println("Ethernet stopped");
    break;
  case ARDUINO_EVENT_ETH_CONNECTED:
    Serial.println("Ethernet connected");
    break;
  case ARDUINO_EVENT_ETH_DISCONNECTED:
    Serial.println("Ethernet disconnected");
    break;
  case ARDUINO_EVENT_ETH_GOT_IP:
    Serial.println("Obtained IP address");
    break;
  default:
    Serial.print("unknown event: ");
    Serial.println(event);
    break;
  }
}