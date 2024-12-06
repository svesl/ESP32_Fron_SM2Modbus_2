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

#define Version 1.2
#include <Arduino.h>
#include <ModbusIP_ESP8266.h> //für MODBUS
#include <WiFi.h>             //für WiFi Verbinding
#include <mqtt_client.h>      //
#include <iostream>
#include <cstring>

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
//ca_stdregisters modbus_common;
ca_stdregisters modbus_common = {
    MODBUS_REG_SM_MFRNAME,
    MODBUS_REG_SM_MODEL,
    MODBUS_REG_SM_NAME,
    MODBUS_REG_SM_VERSION,
    MODBUS_REG_SM_SN};

void setup()
{
  // Init Serial monitor
  Serial.begin(115200);
  while (!Serial)
  {
  }
  Serial.println("__ OK __");
  Serial.println("Dipl.-Ing. (FH) Sven Slawinski");
  Serial.print("Version ");
  Serial.println(Version);

  // Connect to WiFi
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
  //initModbuscommon();
  fillModbusregs();

  Serial.println("Start MODBUS Server");
}

void loop()
{
  if (!blockmbread)
  {
    mb.task();
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
      uint16_t *hexbytes = reinterpret_cast<uint16_t *>(&value);
      blockmbread = true;
      mb.Hreg(handler.regadress, hexbytes[1]);
      mb.Hreg((handler.regadress + 1), hexbytes[0]);
      blockmbread = false;
      Serial.printf("Topic= %s  Register= 0x%x Wert= %f\r\n", handler.topic, handler.regadress, value);

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

  mb.addHreg(0x9c84, 100);    // 40068 DeviceAddress Modbus TCP Address: 202
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
