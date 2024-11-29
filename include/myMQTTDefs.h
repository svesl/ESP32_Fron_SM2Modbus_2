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

#define SUB_TOPIC_BASE "zuSMemuluator/+"          // Empfangspfad

#define SUB_TOPIC_I_TO "zuSMemuluator/I_TO"       // Empfangspfad 40071
#define SUB_TOPIC_I_L1 "zuSMemuluator/I_L1"       // Empfangspfad 40073
#define SUB_TOPIC_I_L2 "zuSMemuluator/I_L2"       // Empfangspfad 40075
#define SUB_TOPIC_I_L3 "zuSMemuluator/I_L3"       // Empfangspfad 40077

#define SUB_TOPIC_U_L1 "zuSMemuluator/U_L1"       // Empfangspfad 40081
#define SUB_TOPIC_U_L2 "zuSMemuluator/U_L2"       // Empfangspfad 40083
#define SUB_TOPIC_U_L3 "zuSMemuluator/U_L3"       // Empfangspfad 40085

#define SUB_TOPIC_FREQ "zuSMemuluator/F"          // Empfangspfad 40095

#define SUB_TOPIC_P_TO "zuSMemuluator/P"          // Empfangspfad 40097
#define SUB_TOPIC_P_L1 "zuSMemuluator/P_L1"       // Empfangspfad 40099
#define SUB_TOPIC_P_L2 "zuSMemuluator/P_L2"       // Empfangspfad 40101
#define SUB_TOPIC_P_L3 "zuSMemuluator/P_L3"       // Empfangspfad 40103

#define SUB_TOPIC_E_EX "zuSMemuluator/E_EINSP"    // Empfangspfad 40129
#define SUB_TOPIC_E_IM "zuSMemuluator/E_TOTAL"    // Empfangspfad 40137

// #########################################################################################################


// Struct to pair topics with their update functions
struct TopicHandler {
    const char* topic;
    uint16_t regadress;
};

#endif