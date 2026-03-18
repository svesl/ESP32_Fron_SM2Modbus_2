#ifndef _MY_MQTT_DEF_h_
#define _MY_MQTT_DEF_h_

#include <Arduino.h>
#include "secure.h"
// #############   WiFi Einstellungen  #####################################################################
// frome secure.h or here
// const char *ssid = "...";        // your network SSID (name)
// const char *pass = "...";        // your network password
// #########################################################################################################

// #############   MQTT Einstellungen  #####################################################################
// frome secure.h or here
// #define CONFIG_BROKER_URL "mqtt://..."       // your MQTT Broker IP
// #define CONFIG_BROKER_PRT 1883               // your MQTT Broker Port
// #define CONFIG_BROKER_USR "..."              // your MQTT Broker User 
// #define CONFIG_BROKER_PSW "..."              // your MQTT Broker User Passwort  

#define SUB_TOPIC_BASE "zuSMemulator/+"          // Empfangspfad

#define SUB_TOPIC_I_TO "zuSMemulator/I_TO"       // Empfangspfad 40071
#define SUB_TOPIC_I_L1 "zuSMemulator/I_L1"       // Empfangspfad 40073
#define SUB_TOPIC_I_L2 "zuSMemulator/I_L2"       // Empfangspfad 40075
#define SUB_TOPIC_I_L3 "zuSMemulator/I_L3"       // Empfangspfad 40077

#define SUB_TOPIC_U_L1 "zuSMemulator/U_L1"       // Empfangspfad 40081
#define SUB_TOPIC_U_L2 "zuSMemulator/U_L2"       // Empfangspfad 40083
#define SUB_TOPIC_U_L3 "zuSMemulator/U_L3"       // Empfangspfad 40085

#define SUB_TOPIC_FREQ "zuSMemulator/F"          // Empfangspfad 40095

#define SUB_TOPIC_P_TO "zuSMemulator/P"          // Empfangspfad 40097
#define SUB_TOPIC_P_L1 "zuSMemulator/P_L1"       // Empfangspfad 40099
#define SUB_TOPIC_P_L2 "zuSMemulator/P_L2"       // Empfangspfad 40101
#define SUB_TOPIC_P_L3 "zuSMemulator/P_L3"       // Empfangspfad 40103

#define SUB_TOPIC_E_EX "zuSMemulator/E_EINSP"    // Empfangspfad 40129
#define SUB_TOPIC_E_IM "zuSMemulator/E_TOTAL"    // Empfangspfad 40137

// #########################################################################################################

#define MAX_TOPIC_LEN 32
#define MAX_PAYLOAD_LEN 16 // 100000kWh -> 100 000 000
#define QUEUE_SIZE 20      // Puffer für 10 Nachrichten

// Struct to pair topics with their update functions
struct TopicHandler {
    const char* topic;
    uint16_t regadress;
};

#endif