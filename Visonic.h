/*
  Visonic.h - Library for Visonic Powermax.
  Created by Peter Bootsma, January 30, 2016.
  License: GPLv3
  
  Protocolo info: http://www.domoticaforum.eu/viewtopic.php?f=68&t=6581
*/

#ifndef Visonic_h
#define Visonic_h

#include <SoftwareSerial.h>
#include "Arduino.h"


#define VISONIC_BUFFER_LENGTH 25

#define VISONIC_PREAMBLE 	0x0D
#define VISONIC_POSTAMBLE 	0x0A

#define VISONIC_ENROLL_PINCODE_1 	0x32
#define VISONIC_ENROLL_PINCODE_2 	0x45

#define VISONIC_DEFAULT_MOTION_TIMEOUT 5 // in 65 seconds, 1 minute can be instant

#define VISONIC_EVENT_ENROLL 				1
#define VISONIC_EVENT_ACCESS_DENIED 		2
#define VISONIC_EVENT_ZONE_ENROLLED 		3
#define VISONIC_EVENT_ZONE_BYPASSED 		4
#define VISONIC_EVENT_ZONE_BATTERY_LOW 		5
#define VISONIC_EVENT_ZONE_BATTERY_OK 		6
#define VISONIC_EVENT_ZONE_MOTION 			7
#define VISONIC_EVENT_ZONE_OPEN 			8
#define VISONIC_EVENT_ZONE_CLOSED 			9
#define VISONIC_EVENT_DISARMED				10
#define VISONIC_EVENT_ARMED_HOME			11
#define VISONIC_EVENT_ARMED_HOME_EXIT_DELAY	12
#define VISONIC_EVENT_ARMED_AWAY			13
#define VISONIC_EVENT_ARMED_AWAY_EXIT_DELAY	14
#define VISONIC_EVENT_ALARM					15
#define VISONIC_EVENT_ALARM_END				16

class Visonic
{
  public:
    Visonic(int rxPin, int txPin);
    void begin(unsigned int userPin, void (*_eventCallback)(int eventType, int eventId));
    void process();
    void saveUserPin();
    
    void disarm();
    void armHome();
    void armAway();
    
    bool logDebug;
    byte motionTimeoutInMinutes; // Minutes of 65.5 seconds
  private:
	// Private variables
  	void (*eventCallback)(int eventType, int zone);
  	byte buffer[VISONIC_BUFFER_LENGTH];
	byte bufferIndex;
	unsigned int userPin;
    SoftwareSerial softwareSerial;
    bool isAlarm;
    byte zoneEnrolled[4];
    byte zoneOpen[4];
    unsigned int zoneLastMotionTime[30]; // millis() >> 16, 65.5 seconds per 1, saves 60 bytes ram
    byte zoneBatteryLow[4];
	
	// Private methods
	void handleVisonicA5();
	void handleVisonicA5Event();
	bool isMessage();
    bool isPayloadEqualTo(byte payload[], int length);
    bool send(byte payload[], byte length);
    bool isZoneEnrolled(byte zone);
    bool wasZoneOpen(byte zone);
    bool wasZoneBatteryLow(byte zone);
    bool isBitActiveForZone(int offset, byte zone);
};

#endif