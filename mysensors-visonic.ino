/*
  Alarm.ino - Sketch for communication with Powermax over MySensors
  Specially made to use a PowerMax Pro with Domoticz
  Created by Peter Bootsma, January 30, 2016.
  License: GPLv3
  
  Used: Arduino Pro Mini 3.3v 8Mhz, NRF24L01+, max3232 and PowerMax Pro Dual RS232 expansion board
  
  Protocolo info: http://www.domoticaforum.eu/viewtopic.php?f=68&t=6581
*/

#include <SoftwareSerial.h>
#include <SPI.h>
#include <MySensor.h>
#include <avr/wdt.h>
#include "Visonic.h"

#define VISONIC_RX_PIN 4
#define VISONIC_TX_PIN 5

Visonic visonic(VISONIC_RX_PIN, VISONIC_TX_PIN);

// MySensors Child ids 1-30 are for zone status
// MySensors Child ids 31-60 reserved for zone battery low
// MySensors Child ids 61-90 reserved for zone tamper
#define MYSENSORS_ARMED_ID 100
#define MYSENSORS_ARMED_HOME_ID 101
#define MYSENSORS_ARMED_AWAY_ID 102
#define MYSENSORS_ALARM_ID 103

#define MYSENSORS_USERPIN_STATE_ADDRESS 0

MySensor gw;
MyMessage msgArmed(MYSENSORS_ARMED_ID, V_STATUS);
MyMessage msgArmedHome(MYSENSORS_ARMED_HOME_ID, V_STATUS);
MyMessage msgArmedAway(MYSENSORS_ARMED_AWAY_ID, V_STATUS);
MyMessage msgAlarm(MYSENSORS_ALARM_ID, V_STATUS);
MyMessage msgZoneStatus(0, V_STATUS);

unsigned long time = 0;

void setup()
{
	Serial.begin(115200);
	Serial.println(F("Starting Alarm"));
	
	// Save userpin in eeprom once, after that change pincode and comment saveState lines 
	// In this way the user pin does not have to be in code
	// 0x12 and 0x34 gives user pin 1234
	//gw.saveState(MYSENSORS_USERPIN_STATE_ADDRESS, 0x12);
	//gw.saveState(MYSENSORS_USERPIN_STATE_ADDRESS + 1, 0x34);
	
	// Load userPin from eeprom
	unsigned int userPin = 
		gw.loadState(MYSENSORS_USERPIN_STATE_ADDRESS) << 8
		| gw.loadState(MYSENSORS_USERPIN_STATE_ADDRESS + 1);
	visonic.logDebug = true;
	visonic.begin(userPin, visonicEvent);
	
	gw.begin(handleMySensorsMessage);

	// Send the Sketch Version Information to the Gateway
	gw.sendSketchInfo("Alarm", "1.0");

	// Register primary sensors to gw
	gw.present(MYSENSORS_ARMED_ID, S_BINARY, "Armed");
	gw.present(MYSENSORS_ARMED_HOME_ID, S_BINARY, "Armed Home");
	gw.present(MYSENSORS_ARMED_AWAY_ID, S_BINARY, "Armed Away");
	gw.present(MYSENSORS_ALARM_ID, S_BINARY, "Alarm");
	
	// Enable watchdog timer of 8 seconds
	wdt_enable(WDTO_8S);
}

void loop()
{
	// Reset watchdog timer
	wdt_reset(); 
	// Process MySensors
	gw.process();
	// Process Visonic
	visonic.process(); 	
}

/*
 * Visonic message handling
 */
void visonicEvent(int eventType, int zone) {
	
	switch (eventType) {
		case VISONIC_EVENT_ENROLL:
			// Choose install powerlink in the powermax menu once
			// Automatically handled by library
			Serial.println(F("Enroll request"));
			break;
		case VISONIC_EVENT_ACCESS_DENIED:
			// If you get this message, is is not enrolled 
			// or it does not have the right user pincode
			Serial.println(F("Access denied"));
			break;
		case VISONIC_EVENT_ZONE_BATTERY_LOW:
			// Uncommented to get battery low
			// Serial.print(F("Zone battery low: "));
			// Serial.println(zone);
			// gw.send(msgZoneStatus.setSensor(30 + zone).set(true));
			break;
		case VISONIC_EVENT_ZONE_BATTERY_OK:
			// Uncommented to get battery low
			// Serial.println(F("Zone battery ok: "));
			// Serial.println(zone);
			// gw.send(msgZoneStatus.setSensor(30 + zone).set(false));
			break;
		case VISONIC_EVENT_ZONE_ENROLLED:
			Serial.print(F("Zone Enrolled: "));
			Serial.println(zone);
			gw.present(zone, S_BINARY, "Zone");
			// Uncommented to get battery low
			// gw.present(30 + zone, S_BINARY, "Zone Battery");
			// gw.present(30 + zone, S_BINARY, "Zone Tamper");
			break;
		case VISONIC_EVENT_ZONE_BYPASSED:
			Serial.print(F("Zone Bypassed: "));
			Serial.println(zone);
			break;
		case VISONIC_EVENT_ZONE_OPEN:
			Serial.print(F("Zone Open: "));
			Serial.println(zone);
			gw.send(msgZoneStatus.setSensor(zone).set(true));
			break;
		case VISONIC_EVENT_ZONE_MOTION:
			Serial.print(F("Zone Motion: "));
			Serial.println(zone);
			gw.send(msgZoneStatus.setSensor(zone).set(true));
			break;
		case VISONIC_EVENT_ZONE_CLOSED:
			Serial.print(F("Zone Closed: "));
			Serial.println(zone);
			gw.send(msgZoneStatus.setSensor(zone).set(false));
			break;
		case VISONIC_EVENT_DISARMED:
			Serial.println(F("Disarmed"));
			gw.send(msgArmed.set(false));
			gw.send(msgArmedHome.set(false));
			gw.send(msgArmedAway.set(false));
			break;
		case VISONIC_EVENT_ARMED_HOME_EXIT_DELAY:
			Serial.println(F("Armed home exit delay"));
			gw.send(msgArmed.set(true));
			gw.send(msgArmedHome.set(true));
			gw.send(msgArmedAway.set(false));
			break;
		case VISONIC_EVENT_ARMED_AWAY_EXIT_DELAY:
			Serial.println(F("Armed away exit delay"));
			gw.send(msgArmed.set(true));
			gw.send(msgArmedHome.set(false));
			gw.send(msgArmedAway.set(true));
			break;
		case VISONIC_EVENT_ARMED_HOME:
			Serial.println(F("Armed home"));
			break;
		case VISONIC_EVENT_ARMED_AWAY:
			Serial.println(F("Armed away"));
			break;
		case VISONIC_EVENT_ALARM:
			Serial.println(F("Alarm"));
			gw.send(msgAlarm.set(true));
			break;
		case VISONIC_EVENT_ALARM_END:
			Serial.println(F("Alarm End"));
			gw.send(msgAlarm.set(false));
			break;
	}
}

/*
 * Message handling from MySensors
 */
void handleMySensorsMessage(const MyMessage &message) {
	switch (message.sensor) {
		case MYSENSORS_ARMED_ID:
			if (message.type == V_STATUS) {
				if (atoi(message.data)) {
					visonic.armHome();
				} else {
					visonic.disarm();
				}
			}
			break;
		case MYSENSORS_ARMED_HOME_ID:
			if (message.type == V_STATUS) {
				if (atoi(message.data)) {
					visonic.armHome();
				} else {
					visonic.disarm();
				}
			}
			break;
		case MYSENSORS_ARMED_AWAY_ID:
			if (message.type == V_STATUS) {
				if (atoi(message.data)) {
					visonic.armAway();
				} else {
					visonic.disarm();
				}
			}
			break;
	}
}
