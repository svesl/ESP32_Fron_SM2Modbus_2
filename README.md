
# ESP32 Fronius Smartmeter MQTT zu MODBUS Emulator

Der ESP32 emuliert einen Fronius Smart Meter als TCPI MODBUS-Gerät.
Er kann Verbrauchsdaten über MQTT empfangen und setzt diese auf die entsprechenden MODBUS-Register um.

Verfügbar als Visual Studio Code mit PlatformIO Projekt.

## Voraussetzungen

- Energie bzw. Verbrauchsdaten oder weitere Messwerte stehen zur Verfügung und können über MQTT verteilt werden.
- Fronius Gen24 Wechselrichter
- Hardware ESP32 (devKit)
WiFi und MQTT Zugangsinformationen sind in include/myMQTTDef.h einzutragen.

Empfängt die Energiedaten über MQTT --> Topics siehe include/myMQTTDef.h\
und stellt diese im entsprechenden MODBUS Register zur Verfügung --> siehe include/FroniusSM_Register\
MODBUS-Register Format ist float.



Bsp:\
Zählerstand in Wh 7420836.0 = 7420.836 kWh\
MQTT:\
Topic: "zuSMemuluator/E_EINSP"    \
Payload: 7420836.0 \
geht in \
MODBUS Register (uint16_t)0x9cc1  //40129 in MODBUS-Tabelle von Fronius Register 40130

Achtung! In der Registertabelle von Fronius sind die Adressen immer mit +1 angegeben. 

## Verwendete Bibliotheken
Modbus Bibliothek für Arduino

Copyright (c) \
2015, Andr� Sarmento Barbosa\
2017, Alexander Emelianov (a.m.emelianov@gmail.com)\
https://github.com/emelianov/modbus-esp8266

## Hinweise

Dies ist eine Quick and Dirty Bastellösung. Es sind bisher nicht alle Register verfügbar.

Getestet mit  mit Primo Gen24 ohne Batterie. Somit nur die Anzeige und Auswertung im Fronius bzw. Solarweb verwendet.\
Auslesen des Zählers des Netzbetreibers mit ESPHome.\
Automatisierung in Homeassistant, welche diese Daten zyklisch an die entsprechenden MQTT Topics verteilt.

## Bugs

Führt evtl. zu einer Meldung im Fronius SolarWeb Info Display, dass die Firmware veraltet ist.

Fronius SolarWeb:\
Die Leistungen werden im Energieflussdiagramm aus der übertragenen Leistung dargestellt.
Der Energieflussgraph berechnet die Leistung aus der verbrauchten Energie. Dies kann dazu führen, dass z.B. bei gerundeten Energiewerten beim Auslesen keine Änderung auftritt und der Leistungswert im Diagramm für den Zeitraum 0W ist.

## Spende

Wenn es hilft und gefällt, würde ich mich über eine kleine Spende freuen :)

[PayPal](https://paypal.me/ESPFSMMQTT2MB?country.x=DE&locale.x=de_DE) 

Danke!

## Haftungsausschluss

DIESE SOFTWARE WIRD VON DEN URHEBERRECHTSINHABERN UND MITWIRKENDEN „WIE GESEHEN“ ZUR VERFÜGUNG GESTELLT. ALLE AUSDRÜCKLICHEN ODER STILLSCHWEIGENDEN GEWÄHRLEISTUNGEN, EINSCHLIESSLICH, ABER NICHT BESCHRÄNKT AUF STILLSCHWEIGENDE GEWÄHRLEISTUNGEN DER MARKTGÄNGIGKEIT UND EIGNUNG FÜR EINEN BESTIMMTEN ZWECK, SIND AUSGESCHLOSSEN. UNTER KEINEN UMSTÄNDEN SIND DER URHEBERRECHTSINHABER ODER DIE MITWIRKENDEN HAFTBAR FÜR DIREKTE,
DIREKTE, INDIREKTE, ZUFÄLLIGE, BESONDERE, BEISPIELHAFTE ODER FOLGESCHÄDEN (EINSCHLIESSLICH, ABER NICHT BESCHRÄNKT AUF DIE BESCHAFFUNG VON ERSATZGÜTERN ODER -DIENSTLEISTUNGEN, NUTZUNGSAUSFALL, DATENVERLUST, ENTGANGENEN GEWINN ODER GESCHÄFTSUNTERBRECHUNG, ENTGANGENEN GEWINN ODER GESCHÄFTSUNTERBRECHUNG), UNABHÄNGIG DAVON, OB SIE AUF VERTRAG, FAHRLÄSSIGKEIT ODER UNERLAUBTE HANDLUNG (EINSCHLIESSLICH FAHRLÄSSIGKEIT ODER ANDERWEITIG) GRÜNDEN, DIE IN IRGENDEINER WEISE AUS DER NUTZUNG DIESER SOFTWARE RESULTIEREN, SELBST WENN AUF DIE MÖGLICHKEIT SOLCHER SCHÄDEN HINGEWIESEN WURDE.
SELBST WENN AUF DIE MÖGLICHKEIT SOLCHER SCHÄDEN HINGEWIESEN WURDE.