#ifndef _FRON_SM_REG_h_
#define _FRON_SM_REG_h_

#define MODBUS_REG_SM_MFRNAME "Fronius"
#define MODBUS_REG_SM_MODEL "Smart Meter 63A"
#define MODBUS_REG_SM_NAME "<primary>"
#define MODBUS_REG_SM_SN "00000001"
#define MODBUS_REG_SM_VERSION "2.9"

#define MODBUS_REG_I_TO (uint16_t)0x9c87  //40071
#define MODBUS_REG_I_L1 (uint16_t)0x9c89  //40073
#define MODBUS_REG_I_L2 (uint16_t)0x9c8b  //40075
#define MODBUS_REG_I_L3 (uint16_t)0x9c8d  //40077

#define MODBUS_REG_U_L1 (uint16_t)0x9c91  //40081
#define MODBUS_REG_U_L2 (uint16_t)0x9c93  //40083
#define MODBUS_REG_U_L3 (uint16_t)0x9c95  //40085

#define MODBUS_REG_FREQ (uint16_t)0x9c9f  //40095

#define MODBUS_REG_P_TO (uint16_t)0x9ca1  //40097
#define MODBUS_REG_P_L1 (uint16_t)0x9ca3  //40099
#define MODBUS_REG_P_L2 (uint16_t)0x9ca5  //40101
#define MODBUS_REG_P_L3 (uint16_t)0x9ca7  //40103

#define MODBUS_REG_E_EX (uint16_t)0x9cc1  //40129
#define MODBUS_REG_E_IM (uint16_t)0x9cc9  //40137



struct ca_stdregisters
{
  char mfrname[32];
  char model[32];
  char options[16];
  char version[16];
  char serial[16];
} ;



#endif