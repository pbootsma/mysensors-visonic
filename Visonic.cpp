/*
  Visonic.cpp - Library for Visonic Powermax.
  Created by Peter Bootsma, January 30, 2016.
  License: GPLv3
  
  Protocolo info: http://www.domoticaforum.eu/viewtopic.php?f=68&t=6581
  
  Remarks for myself:
  D AB 5 0 2 0 0 0 0 0 0 0 43 A A  For alarm phone -> 2 ?
*/

#include "Visonic.h"

// Optimize to use flash instead of ram, not needed now
byte VISONIC_PAYLOAD_ENROLL_REQUEST[] = 	{ 0xAB, 0x0A, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x43 };
byte VISONIC_PAYLOAD_ENROLL_RESPONSE[] = 	{ 0xAB, 0x0A, 0x00, 0x00, VISONIC_ENROLL_PINCODE_1, VISONIC_ENROLL_PINCODE_2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x43 };
byte VISONIC_PAYLOAD_ACK[] = 				{ 0x02, 0x43 };
byte VISONIC_PAYLOAD_ACCESS_DENIED[] = 		{ 0x08, 0x43 };
byte VISONIC_PAYLOAD_CONNECT[] = 			{ 0xAB, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x43 };
byte VISONIC_PAYLOAD_STATUS_REQUEST[] = 	{ 0xA2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x43 };
byte VISONIC_PAYLOAD_DISARM[] = 			{ 0xA1, 0x00, 0x00, 0x00, 0x00 /* user pin 1 */, 0x00 /* user pin 2 */, 0x00, 0x00, 0x00, 0x00, 0x00, 0x43 };
byte VISONIC_PAYLOAD_ARM_HOME[] = 			{ 0xA1, 0x00, 0x00, 0x04, 0x00 /* user pin 1 */, 0x00 /* user pin 2 */, 0x00, 0x00, 0x00, 0x00, 0x00, 0x43 };
byte VISONIC_PAYLOAD_ARM_AWAY[] = 			{ 0xA1, 0x00, 0x00, 0x05, 0x00 /* user pin 1 */, 0x00 /* user pin 2 */, 0x00, 0x00, 0x00, 0x00, 0x00, 0x43 };

Visonic::Visonic(int rxPin, int txPin) 
	: softwareSerial(SoftwareSerial(rxPin, txPin))  {
	motionTimeoutInMinutes = VISONIC_DEFAULT_MOTION_TIMEOUT;
}

void Visonic::begin(unsigned int _userPin, void (*_eventCallback)(int eventType, int zone)) {
	userPin = _userPin;
	eventCallback = _eventCallback;
	
	byte userPin1 = userPin >> 8;
	byte userPin2 = userPin & 0xFF;
//	if (logDebug) {
//		Serial.print("Using user pin: ");
//		Serial.print(userPin1, HEX);
//		Serial.println(userPin2, HEX);
//	}
	VISONIC_PAYLOAD_DISARM[4] = userPin1;
	VISONIC_PAYLOAD_DISARM[5] = userPin2;
	VISONIC_PAYLOAD_ARM_HOME[4] = userPin1;
	VISONIC_PAYLOAD_ARM_HOME[5] = userPin2;
	VISONIC_PAYLOAD_ARM_AWAY[4] = userPin1;
	VISONIC_PAYLOAD_ARM_AWAY[5] = userPin2;
	
	softwareSerial.begin(9600); // 9600 8 N 1
	send(VISONIC_PAYLOAD_CONNECT, 12); // Connect
	if (logDebug) Serial.println(F("Connect request send"));
}

void Visonic::process() {
	char ch = softwareSerial.read();
	if (ch != -1) {
		byte b = (byte)ch;
		if (logDebug) Serial.print(b, HEX);
		if (logDebug) Serial.print(F(" "));
		
		// Buffer handling
		if (bufferIndex > 0 || b == VISONIC_PREAMBLE) { // Start buffer with preamble of message
			buffer[bufferIndex] = b;
			bufferIndex++;
		}
		if (bufferIndex >= VISONIC_BUFFER_LENGTH) {
			bufferIndex = 0; // Clear before overflow
			if (logDebug) Serial.println(F("Buffer overflow, clearing buffer"));
		}
		
		if (isMessage()) {
			if (logDebug) Serial.println();
			if (isPayloadEqualTo(VISONIC_PAYLOAD_ENROLL_REQUEST, 12)) {
				send(VISONIC_PAYLOAD_ENROLL_RESPONSE, 12);
				eventCallback(VISONIC_EVENT_ENROLL, 0);
			} else if (isPayloadEqualTo(VISONIC_PAYLOAD_ACK, 2)) {
				if (logDebug) Serial.println(F("Panel Ack"));
			} else {
				send(VISONIC_PAYLOAD_ACK, 2); // Ack
				
				// Process Message
				if (isPayloadEqualTo(VISONIC_PAYLOAD_ACCESS_DENIED, 2)) {
					if (logDebug) Serial.println(F("Acces Denied"));
				} else if (bufferIndex == 15) { // Payload of length 12
					switch (buffer[1]) {
						case 0xA5: // General Event
							handleVisonicA5();
							break;
					}
				}
			}
			bufferIndex = 0; // Clear buffer
		}
	} else {
		// No data available
		unsigned int now = millis() >> 16;
		for (byte i=0; i<30; i++) {
			// Now cannot be zero, otherwise 0-1 will be a large number, only a problem in first minute
			if (zoneLastMotionTime[i] > 0 && now > 0 && (now-zoneLastMotionTime[i]) >= motionTimeoutInMinutes) {
				Serial.println("Closing Zone with timer");
				eventCallback(VISONIC_EVENT_ZONE_CLOSED, i);
				zoneLastMotionTime[i] = 0;
			}
		}
		
	}
}

void Visonic::handleVisonicA5() {
	switch (buffer[3]) {
		case 0x02: // Zone Open and Low Battery
			if (logDebug) Serial.println(F("Zone status for Open and Battery"));

			// Only report all zones once and all changes
			for (int i=1; i<=30; i++) {
				if (isZoneEnrolled(i)) {
					bool zoneIsOpen = isBitActiveForZone(4, i);
					if (zoneIsOpen != wasZoneOpen(i)){
						eventCallback(zoneIsOpen ? VISONIC_EVENT_ZONE_OPEN : VISONIC_EVENT_ZONE_CLOSED, i);
					}
					bool batteryIsLow = isBitActiveForZone(8, i);
					if (batteryIsLow != wasZoneBatteryLow(i)){
						eventCallback(batteryIsLow ? VISONIC_EVENT_ZONE_BATTERY_LOW : VISONIC_EVENT_ZONE_BATTERY_OK, i);
					}
				}
			}
			for (int i=0; i<4; i++) zoneOpen[i] = buffer[4 + i];
			for (int i=0; i<4; i++) zoneBatteryLow[i] = buffer[8 + i];
			break;
		case 0x06: // Zone Enrolled and Bypassed
			if (logDebug) Serial.println(F("Zone status for Enrolled and Bypassed"));
			
			for (int i=1; i<=30; i++) {
				if (isZoneEnrolled(i) == false && isBitActiveForZone(4, i)) {
					eventCallback(VISONIC_EVENT_ZONE_ENROLLED, i);
					// Report initial status. Status is send before enrollment
					eventCallback(wasZoneOpen(i) ? VISONIC_EVENT_ZONE_OPEN : VISONIC_EVENT_ZONE_CLOSED, i);
					eventCallback(wasZoneBatteryLow(i) ? VISONIC_EVENT_ZONE_BATTERY_LOW : VISONIC_EVENT_ZONE_BATTERY_OK, i);
				}
			}
			for (int i=1; i<=30; i++) {
				if (isBitActiveForZone(8, i)) {
					eventCallback(VISONIC_EVENT_ZONE_BYPASSED, i);
				}
			}
			for (int i=0; i<4; i++) zoneEnrolled[i] = buffer[4 + i];
			break;
		case 0x03: // Tamper
			break;
		case 0x04: // Event
			handleVisonicA5Event();
			break;
	}
}

void Visonic::handleVisonicA5Event() {
	byte systemStatus = buffer[4];
	byte stateFlags = buffer[5];
	
	String systemStatusMessage[] = { "Disarm", "Exit Delay", "Exit Delay", "Entry Delay", "Armed Home", "Armed Away", "User Test", "Downloading", "Programming", "Installer", "Home Bypass", "Away Bypass", "Ready", "Not Ready"};
	if (logDebug) Serial.print(F("Status: "));
	if (logDebug) Serial.println(systemStatusMessage[systemStatus]);
	
	if (isAlarm && (stateFlags & 0x01)) {
		// Ready bit set, so alarm is cancelled or only the zone is restored?
		eventCallback(VISONIC_EVENT_ALARM_END, 0);
		isAlarm = false;
	}
	
	switch (stateFlags & 0xE0) { // Only bit 6, 7 or 8 
		case 0x20: {
			byte zone = buffer[6];
			switch (buffer[7]) {
				case 0x01: // Tamper
					//eventCallback(VISONIC_EVENT_ZONE_TAMPER, zone);
					break;
				case 0x02: // Tamper Restore
					//eventCallback(VISONIC_EVENT_ZONE_TAMPER_END, zone);
					break;
				case 0x03: // Open
					eventCallback(VISONIC_EVENT_ZONE_OPEN, zone);
					break;
				case 0x04: // Close
					eventCallback(VISONIC_EVENT_ZONE_CLOSED, zone);
					break;
				case 0x05: // Motion
					eventCallback(VISONIC_EVENT_ZONE_MOTION, zone);
					zoneLastMotionTime[zone] = millis() >> 16;
					if (zoneLastMotionTime[zone] == 0) zoneLastMotionTime[zone] = 1; // Fix for motion in first 65.5 seconds
					break;
				default:
					if (logDebug) {
						Serial.print(F("Unknown event type: 0x"));
						Serial.println(buffer[7], HEX);
					}
					break;
			}
		} 	break;
		case 0x40:
			switch (systemStatus) {
				case 0x00: // Disarm
					eventCallback(VISONIC_EVENT_DISARMED, 0);
					if (isAlarm) {
						eventCallback(VISONIC_EVENT_ALARM_END, 0);
						isAlarm = false;
					}
					break;
				case 0x01: // Exit delay home
					eventCallback(VISONIC_EVENT_ARMED_HOME_EXIT_DELAY, 0);
					break;
				case 0x02: // Exit delay away
					eventCallback(VISONIC_EVENT_ARMED_AWAY_EXIT_DELAY, 0);
					break;
				case 0x04: // Armed home
					eventCallback(VISONIC_EVENT_ARMED_HOME, 0);
					break;
				case 0x05: // Armed away
					eventCallback(VISONIC_EVENT_ARMED_AWAY, 0);
					break;
			}
			break;
		case 0xA0:
			if (logDebug) Serial.println(F("Alarm Event"));
			isAlarm = true;
			eventCallback(VISONIC_EVENT_ALARM, 0);
			break;
	}
}

bool Visonic::isMessage() {
	bool isMessage = false;
	if (bufferIndex >= 4 && buffer[bufferIndex-1] == VISONIC_POSTAMBLE) {
		// Checksum calc
		unsigned long checksum = 0;
		for (int i=1; i<(bufferIndex-2); i++) {
			checksum += buffer[i];
		}
		checksum = checksum % 255;
		if (checksum % 0xFF != 0) checksum = checksum ^ 0xFF;
		if (checksum == buffer[bufferIndex-2]) isMessage = true;
	}
	return isMessage;
}

bool Visonic::isPayloadEqualTo(byte payload[], int length) {
	bool isPayloadEqual = false;
	
	if ((length + 3) == bufferIndex) {
		isPayloadEqual = true;
		for (int i=0; i<length; i++) {
			if (payload[i] != buffer[i+1]) {
				isPayloadEqual = false;
				break;
			}
		}
	}
	
	return isPayloadEqual;
}

bool Visonic::send(byte payload[], byte length) {
	//if (logDebug) Serial.print(F("Sending: "));
	softwareSerial.write(VISONIC_PREAMBLE);
	unsigned long checksum = 0;
	for (int i=0; i<length; i++) {
		//if (logDebug) Serial.print(F(" "));
		//if (logDebug) Serial.print(payload[i], HEX);
		checksum += payload[i];
		softwareSerial.write(payload[i]);
	}
	checksum = checksum % 255;
	if (checksum % 0xFF != 0) checksum = checksum ^ 0xFF;
	
	softwareSerial.write(checksum);
	softwareSerial.write(VISONIC_POSTAMBLE);
	//if (logDebug) Serial.println();
}

bool Visonic::isZoneEnrolled(byte zone) {
	byte byteIndex = (zone - 1)/8;
	byte b = zoneEnrolled[byteIndex];
	byte bitIndex = zone - byteIndex*8 - 1;
	return (0x01 << bitIndex) & b;
}

bool Visonic::wasZoneOpen(byte zone) {
	byte byteIndex = (zone - 1)/8;
	byte b = zoneOpen[byteIndex];
	byte bitIndex = zone - byteIndex*8 - 1;
	return (0x01 << bitIndex) & b;
}

bool Visonic::wasZoneBatteryLow(byte zone) {
	byte byteIndex = (zone - 1)/8;
	byte b = zoneBatteryLow[byteIndex];
	byte bitIndex = zone - byteIndex*8 - 1;
	return (0x01 << bitIndex) & b;
}

bool Visonic::isBitActiveForZone(int offset, byte zone) {
	byte byteIndex = (zone - 1)/8;
	byte b = buffer[offset + byteIndex];
	byte bitIndex = zone - byteIndex*8 - 1;
	return (0x01 << bitIndex) & b;
}

void Visonic::disarm() {
	send(VISONIC_PAYLOAD_DISARM, 12);
}
void Visonic::armHome() {
	send(VISONIC_PAYLOAD_ARM_HOME, 12);
}
void Visonic::armAway() {
	send(VISONIC_PAYLOAD_ARM_AWAY, 12);
}