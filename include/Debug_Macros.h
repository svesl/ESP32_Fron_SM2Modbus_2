#ifndef DEBUG_MACROS_H
#define DEBUG_MACROS_H

// Schalter: einkommentieren für Debug-Build
//#define DEBUG_ENABLED

#ifdef DEBUG
  #define DEBUG_BEGIN(baud)          \
    do {                           \
      Serial.begin(baud);          \
      while (!Serial) { }          \
    } while (0)
  #define DEBUG_PRINT(x)           Serial.print(x)
  #define DEBUG_PRINTLN(x)         Serial.println(x)
  #define DEBUG_PRINTF(fmt, ...)   Serial.printf((fmt), __VA_ARGS__)

#else

  #define DEBUG_BEGIN(baud)
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(fmt, ...)

#endif

#endif // DEBUG_MACROS_H
