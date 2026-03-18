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
//-- 1.8   svesl  2025/07/08    change MODBUS Lib to 4.1.1 cgjgh/modbus-esp8266@^4.1.1 change espressif@6.11
//-- 1.9   svesl  2026/01/06    change WIFI Power to 50% change espressif@6.12
//-- 2.0   svesl  2026/01/20    Thread-sichere Übergabe MQTT to Modbus fix evtl. SM Error msg in WR, Optimierungen, Debug macros, clean up
//-- --------------------------------------------------------------------------
//-- TODO
//-- Speicherung Zählerstande für Import Export im Flash oder nvram für reboot Sinnvoll?
//-- MQTT path wrong emul (u) ator

#define Version 2.0
#include <Arduino.h>
#include <ModbusIP_ESP8266.h> //für MODBUS
#include <WiFi.h>             //für WiFi Verbinding
#include <ESPmDNS.h>
#include <mqtt_client.h> //für MQTT
#include <iostream>
#include <cstring>
#include <Preferences.h> // manages non volatile memory

#include "FroniuSM_Registerdef.h"
#include "myMQTTDefs.h"
#include "Debug_Macros.h"

// MODBUS server object
ModbusIP mb;
// void initModbuscommon(void);
void fillModbusregs(void);
bool blockIMfirstread = false; // Blockiert MODBUS bis erster gültiger Zählerwert für Import vorliegt
bool blockEXfirstread = false; // Blockiert MODBUS bis erster gültiger Zählerwert für Export vorliegt

// MQTT
void init_mqtt(void);                                        // MQTT Initialisieren
esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event); // Eventhandler startet bei MQTT-Events
esp_mqtt_client_config_t mqtt_cfg = {};
esp_mqtt_client_handle_t mqttclient = {};

// Thread sicherung
QueueHandle_t mqttQueue;
typedef struct
{
  char topic[MAX_TOPIC_LEN];
  char payload[MAX_PAYLOAD_LEN];
  uint16_t payload_len;
} MqttMessage_t;

// WiFi
void init_WiFi(void);
void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
uint32_t wifi_connect_retry = 0;
const char *hostname = "esp32-FronSM2MODBUS";
uint32_t reconnect_cnt_mqtt = 0;
uint32_t reconnect_cnt_wifi = 0;
bool isConnected = false;

// Array of TopicHandler to match topics to register
TopicHandler handlers[] = {
    {SUB_TOPIC_P_TO, MODBUS_REG_P_TO},
    {SUB_TOPIC_I_TO, MODBUS_REG_I_TO},
    {SUB_TOPIC_I_L1, MODBUS_REG_I_L1},
    {SUB_TOPIC_I_L2, MODBUS_REG_I_L2},
    {SUB_TOPIC_I_L3, MODBUS_REG_I_L3},
    {SUB_TOPIC_U_L1, MODBUS_REG_U_L1},
    {SUB_TOPIC_U_L2, MODBUS_REG_U_L2},
    {SUB_TOPIC_U_L3, MODBUS_REG_U_L3},
    {SUB_TOPIC_FREQ, MODBUS_REG_FREQ},
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

void BootCntService(void);                               // count, store non-volatile and print number of boots
void checkEnergydatablock(const char *topic, int value); // Check auf Werte in Energy Register

void setup()
{
  // Init Serial monitor
  DEBUG_BEGIN(115200);

  DEBUG_PRINTLN("__ OK __");
  DEBUG_PRINTLN("Fronius SmartMeter Emulator");
  DEBUG_PRINT("Version: ");
  DEBUG_PRINTLN(Version);

  BootCntService();

  init_WiFi();

  mqttQueue = xQueueCreate(QUEUE_SIZE, sizeof(MqttMessage_t));
  init_mqtt();

  mb.server();
  // initModbuscommon();
  delay(100);
  fillModbusregs();
  delay(1000);
  DEBUG_PRINTLN("Start MODBUS Server");
}

void loop()
{

  MqttMessage_t msg;
  // Warte max. 10 ms auf neue Nachricht
  if (xQueueReceive(mqttQueue, &msg, 10 / portTICK_PERIOD_MS) == pdTRUE)
  {
    // Hier im Arduino-"loop"-Thread → alles sicher
    // DEBUG_PRINTF("Topic: %s  Wert= %s\r\n", msg.topic, msg.payload);

    for (const auto &handler : handlers) // Suchen nach passendem Register
    {
      // DEBUG_PRINT("*");
      if (strcmp(msg.topic, handler.topic) == 0)
      {
        float value = atof(msg.payload);
        // Energiewerte prüfen ob Import oder export <= 0
        if ((strcmp(msg.topic, SUB_TOPIC_E_IM) == 0 && value <= 0) || strcmp(msg.topic, SUB_TOPIC_E_EX) == 0 && value <= 0)
        {
          DEBUG_PRINTF("Achtung Energy value 0 on Topic: %s  Register= 0x%x Wert= %f\r\n", handler.topic, handler.regadress, value);
        }
        else
        {
          // DEBUG_PRINTF("Find Topic: %s Register= 0x%x Wert= %f\r\n", handler.topic, handler.regadress, value);
          union
          {
            float f;
            uint32_t i;
            uint16_t s[2];
          } u;
          u.f = value;

          // Big-Endian: Register N = MSB, Register N+1 = LSB
          // *reg_high = u.s[0]; // Hochwertiges 16-Bit (Bits 31-16)
          // *reg_low = u.s[1];  // Niedrigwertiges 16-Bit (Bits 15-0) }

          mb.Hreg(handler.regadress, u.s[1]);
          mb.Hreg((handler.regadress + 1), u.s[0]);

        }
        // Chek first Val in import and export register
        checkEnergydatablock(msg.topic, value);
        break;
      }
    }
  }

  // Prüfe ob Energiezählerstände vorhanden
  if (blockIMfirstread && blockEXfirstread)
  {
    mb.task(); // MODBUS Task ausführen
    // DEBUG_PRINTLN("MODBUS Task");
  }
}

// Init Funktions
// void initModbuscommon(void)
// {
//   strlcpy(modbus_common.mfrname, MODBUS_REG_SM_MFRNAME, sizeof(modbus_common.mfrname));
//   strlcpy(modbus_common.model, MODBUS_REG_SM_MODEL, sizeof(modbus_common.model));
//   strlcpy(modbus_common.options, MODBUS_REG_SM_NAME, sizeof(modbus_common.options));
//   strlcpy(modbus_common.version, MODBUS_REG_SM_VERSION, sizeof(modbus_common.version));
//   strlcpy(modbus_common.serial, MODBUS_REG_SM_SN, sizeof(modbus_common.serial));
// }

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
    DEBUG_PRINTF("Vendor: write %d to register %d\r\n", value, (0x9c44 + (i / 2)));
  }
  // 40020 - 40035 Device Model Name
  for (uint8_t i = 0; i < 32; i += 2)
  {
    uint16_t value = 0;
    if (strlen(modbus_common.model) > i)
      value = (modbus_common.model[i] << 8) | (i + 1 < strlen(modbus_common.model) ? modbus_common.model[i + 1] : 0);
    mb.addHreg(0x9c54 + (i / 2), value); // 40020 - 40035 Device Model Name
    DEBUG_PRINTF("Modelname: write %d to register %d\r\n", value, (0x9c54 + (i / 2)));
  }
  // 40036 - 40043 Options
  for (uint8_t i = 0; i < 16; i += 2)
  {
    uint16_t value = 0;
    if (strlen(modbus_common.options) > i)
      value = (modbus_common.options[i] << 8) | (i + 1 < strlen(modbus_common.options) ? modbus_common.options[i + 1] : 0);
    mb.addHreg(0x9c64 + (i / 2), value); // 40036 - 40043 Options
    DEBUG_PRINTF("Options: write %d to register %d\r\n", value, (0x9c64 + (i / 2)));
  }
  // 40044 - 40051 Version
  for (uint8_t i = 0; i < 16; i += 2)
  {
    uint16_t value = 0;
    if (strlen(modbus_common.version) > i)
      value = (modbus_common.version[i] << 8) | (i + 1 < strlen(modbus_common.version) ? modbus_common.version[i + 1] : 0);
    mb.addHreg(0x9c6c + (i / 2), value); // 40044 - 40051 Version
    DEBUG_PRINTF("Version: write %d to register %d\r\n", value, (0x9c6c + (i / 2)));
  }
  // 40052 - 40067 Serial Number
  for (uint8_t i = 0; i < 32; i += 2)
  {
    uint16_t value = 0;
    if (strlen(modbus_common.serial) > i)
      value = (modbus_common.serial[i] << 8) | (i + 1 < strlen(modbus_common.serial) ? modbus_common.serial[i + 1] : 0);
    mb.addHreg(0x9c74 + (i / 2), value); // 40052 - 40067 Serial Number
    DEBUG_PRINTF("Modbus: write %d to register %d\r\n", value, (0x9c74 + (i / 2)));
  }

  mb.addHreg(0x9c84, MODBUS_REG_SM_ADDR); // 40068 DeviceAddress Modbus TCP Address: 202  240 (Adress 1) 241 (Adress 2)
  mb.addHreg(0x9c85, 213);                // 40069 SunSpec_DID
  mb.addHreg(0x9c86, 124);                // 40070 SunSpec_Length
  mb.addHreg(0x9c87, 0, 123);             // 40071 - 40194 smartmeter data
  mb.addHreg(0x9d03, 65535);              // 40195 end block identifier
  mb.addHreg(0x9d04, 0);                  // 40196
}

void init_WiFi(void)
{
  DEBUG_PRINTLN("INIT WIFI");
  WiFi.setTxPower(WIFI_POWER_13dBm); // min // 50% WIFI_POWER_13dBm
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
  DEBUG_PRINTF("WIFi IP address: %u.%u.%u.%u\n", wIP[0], wIP[1], wIP[2], wIP[3]);
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
  DEBUG_PRINTLN("INIT MQTT");
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
    DEBUG_PRINTLN("NVS error:");
    // return ER_NVS;
  }
  else
  {
    DEBUG_PRINTLN("NVS load bootcount");
    bootCount = prefs.getUInt("bootcnt_por");
    DEBUG_PRINTF("NVS bootcount read %d\n", bootCount);
    bootCount++;
    prefs.putUInt("bootcnt_por", bootCount); // store new value
  }
}

void checkEnergydatablock(const char *topic, int value)
{
  // 1. Früh raus, wenn nichts mehr zu tun ist
  if (blockIMfirstread && blockEXfirstread)
  {
    return;
  }

  // 2. Wenn value nicht > 0, ist auch nichts zu tun
  if (value <= 0)
  {
    return;
  }

  // 3. Nur noch die benötigten Vergleiche ausführen
  if (!blockIMfirstread && strcmp(topic, SUB_TOPIC_E_IM) == 0)
  {
    blockIMfirstread = true;
    DEBUG_PRINTLN("unBlock import");
    return; // optional, wenn sich IM/EX nie im gleichen topic melden
  }

  if (!blockEXfirstread && strcmp(topic, SUB_TOPIC_E_EX) == 0)
  {
    blockEXfirstread = true;
    DEBUG_PRINTLN("unBlock export");
  }
}

// Event Händler
esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
  // char c_topic[event->topic_len + 1];
  // char c_payload[event->data_len + 1];
  // String st_dat;

  MqttMessage_t msg;
  int tlen = 0;
  int plen = 0;

  switch (event->event_id)
  {
  case MQTT_EVENT_CONNECTED:
    DEBUG_PRINTLN("MQTT verbunden  ");
    DEBUG_PRINT("MQTT subscribed id: ");
    DEBUG_PRINTLN(SUB_TOPIC_BASE);
    esp_mqtt_client_subscribe(mqttclient, SUB_TOPIC_BASE, 0);
    // esp_mqtt_client_subscribe(mqttclient, SUB_TOPIC_I_AC, 1);
    isConnected = true;
    reconnect_cnt_mqtt++;
    DEBUG_PRINTF("Num of MQTT connectins: %u\n", reconnect_cnt_mqtt);
    break;
  case MQTT_EVENT_DISCONNECTED:
    isConnected = false;
    DEBUG_PRINTLN("MQTT nicht verbunden  ");
    break;
  case MQTT_EVENT_SUBSCRIBED:
    DEBUG_PRINTLN("MQTT subscribe ");
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    DEBUG_PRINTLN("MQTT unsubscribe ");
    break;
  case MQTT_EVENT_PUBLISHED:
    DEBUG_PRINTLN("MQTT published ");
    break;
  case MQTT_EVENT_DATA:
    // DEBUG_PRINTLN("MQTT received ");
    // Topic prüfen über event->topic
    // Daten string über event->data
    // Extrahieren der Zeichenketten für Topic und Daten

    // Topic: memcpy ist perfekt, da Länge bekannt (event->topic_len)
    tlen = min(event->topic_len, (int)(MAX_TOPIC_LEN - 1));
    memcpy(msg.topic, event->topic, tlen);
    msg.topic[tlen] = '\0'; // Manuell terminieren → schneller als strncpy!

    // Payload: gleiches Muster
    plen = min(event->data_len, (int)(MAX_PAYLOAD_LEN - 1));
    memcpy(msg.payload, event->data, plen);
    msg.payload[plen] = '\0';

    xQueueSend(mqttQueue, &msg, 0);

    // strncpy(c_topic, event->topic, event->topic_len);
    // strncpy(c_payload, event->data, event->data_len);

    // c_topic[event->topic_len] = 0;
    // c_payload[event->data_len] = 0;

    // // Serial.printf("NEW Topic= %s\r\n", c_topic);
    // // Serial.printf("NEW Data= %s\r\n", c_payload);

    // handleTopic(c_topic, c_payload);

    break;
  case MQTT_EVENT_ERROR:
    DEBUG_PRINTLN("MQTT error ");
    break;
  default:
    break;
  }
  return ESP_OK;
}

void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info)
{
  DEBUG_PRINTF("[WiFi-event] event: %d\n", event);
  // Serial.printf("[WiFi-event] info: %d\n", info);

  switch (event)
  {
  case ARDUINO_EVENT_WIFI_READY:
    DEBUG_PRINTLN("WiFi interface ready");
    break;
  case ARDUINO_EVENT_WIFI_SCAN_DONE:
    DEBUG_PRINTLN("Completed scan for access points");
    break;
  case ARDUINO_EVENT_WIFI_STA_START:
    DEBUG_PRINTLN("WiFi client started");
    break;
  case ARDUINO_EVENT_WIFI_STA_STOP:
    DEBUG_PRINTLN("WiFi client stopped");
    break;
  case ARDUINO_EVENT_WIFI_STA_CONNECTED:
    DEBUG_PRINTLN("Connected to access point");
    DEBUG_PRINTLN(WiFi.getHostname());
    DEBUG_PRINTF("RSSI: %d\n", WiFi.RSSI());
    wifi_connect_retry = 0;
    reconnect_cnt_wifi++;
    DEBUG_PRINTF("Num of WiFi connectins: %u\n", reconnect_cnt_wifi);

    break;
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    // DEBUG_PRINTLN("Disconnected from WiFi access point");
    DEBUG_PRINTF("Disconnected from WiFi access point. Count = %d\n", wifi_connect_retry);
    wifi_connect_retry++; // keine Überlaufüberwachung
    WiFi.reconnect();

    break;
  case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
    DEBUG_PRINTLN("Authentication mode of access point has changed");
    break;
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    Serial.print("Obtained IP address: ");
    DEBUG_PRINTLN(WiFi.localIP());
    if (!MDNS.begin(WiFi.getHostname()))
    { // http://esp32.local
      DEBUG_PRINTLN("Error setting up MDNS responder!");
    }
    else
    {
      DEBUG_PRINTLN("mDNS responder started");
      DEBUG_PRINTF("Call: http://%s", WiFi.getHostname());
      DEBUG_PRINTLN(".local");
    }
    break;
  case ARDUINO_EVENT_WIFI_STA_LOST_IP:
    DEBUG_PRINTLN("Lost IP address and IP address is reset to 0");
    break;
  case ARDUINO_EVENT_WPS_ER_SUCCESS:
    DEBUG_PRINTLN("WiFi Protected Setup (WPS): succeeded in enrollee mode");
    break;
  case ARDUINO_EVENT_WPS_ER_FAILED:
    DEBUG_PRINTLN("WiFi Protected Setup (WPS): failed in enrollee mode");
    break;
  case ARDUINO_EVENT_WPS_ER_TIMEOUT:
    DEBUG_PRINTLN("WiFi Protected Setup (WPS): timeout in enrollee mode");
    break;
  case ARDUINO_EVENT_WPS_ER_PIN:
    DEBUG_PRINTLN("WiFi Protected Setup (WPS): pin code in enrollee mode");
    break;
  case ARDUINO_EVENT_WIFI_AP_START:
    DEBUG_PRINTLN("WiFi access point started");
    break;
  case ARDUINO_EVENT_WIFI_AP_STOP:
    DEBUG_PRINTLN("WiFi access point  stopped");
    break;
  case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
    DEBUG_PRINTLN("Client connected");
    break;
  case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
    DEBUG_PRINTLN("Client disconnected");
    break;
  case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
    DEBUG_PRINTLN("Assigned IP address to client");
    break;
  case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:
    DEBUG_PRINTLN("Received probe request");
    break;
  case ARDUINO_EVENT_ETH_GOT_IP6:
    DEBUG_PRINTLN("IPv6 is preferred");
    break;
  case ARDUINO_EVENT_ETH_START:
    DEBUG_PRINTLN("Ethernet started");
    break;
  case ARDUINO_EVENT_ETH_STOP:
    DEBUG_PRINTLN("Ethernet stopped");
    break;
  case ARDUINO_EVENT_ETH_CONNECTED:
    DEBUG_PRINTLN("Ethernet connected");
    break;
  case ARDUINO_EVENT_ETH_DISCONNECTED:
    DEBUG_PRINTLN("Ethernet disconnected");
    break;
  case ARDUINO_EVENT_ETH_GOT_IP:
    DEBUG_PRINTLN("Obtained IP address");
    break;
  default:
    Serial.print("unknown event: ");
    DEBUG_PRINTLN(event);
    break;
  }
}
