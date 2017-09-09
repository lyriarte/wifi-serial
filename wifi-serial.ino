/*
 * Copyright (c) 2017, Luc Yriarte
 * License: BSD <http://www.opensource.org/licenses/bsd-license.php>
 */


#include <ESP8266WiFi.h>


/* **** **** **** **** **** ****
 * Constants
 * **** **** **** **** **** ****/

#define BPS_HOST 9600
#define COMMS_BUFFER_SIZE 128
#define SERIAL_PEER_DELAY_MS 15000

#define wifiSSID "SSID"
#define wifiPASSWD "password"
#define serverPORT 80
#define AP_SUBNET 64
#define WIFI_CLIENT_DELAY 500
#define WIFI_CONNECT_RETRY 10

enum {
	METHOD,
	URI,
	IGNORE
};


/* **** **** **** **** **** ****
 * Global variables
 * **** **** **** **** **** ****/

/*
 * WiFi
 */

WiFiServer wifiServer(serverPORT);
WiFiClient wifiClient;

int wifiStatus = WL_IDLE_STATUS;
bool wifiAPmode = false;
char apSSID[] = "ESP_XXXXXX";
char wifiMacStr[] = "00:00:00:00:00:00";
byte wifiMacBuf[6];
IPAddress wifiIP; 

/* 
 * serial comms buffer
 */
char commsBuffer[COMMS_BUFFER_SIZE];


/* **** **** **** **** **** ****
 * Functions
 * **** **** **** **** **** ****/

void setup() {
	Serial.begin(BPS_HOST);
	wifiMacInit();
	Serial.print("WiFi.macAddress: ");
	Serial.println(wifiMacStr);
	if (!wifiConnect(WIFI_CONNECT_RETRY)) {
		wifiAPInit();
		Serial.print("WiFi.softAP: ");
		Serial.println(apSSID);
	}
	wifiIP = WiFi.localIP();
	Serial.print("WiFi.localIP: ");
	Serial.println(wifiIP);
	wifiServer.begin();
}

/*
 * WiFi
 */

void wifiMacInit() {
	WiFi.macAddress(wifiMacBuf);
	int i,j,k;
	j=0; k=4;
	for (i=0; i<6; i++) {
		wifiMacStr[j] = '0' + (wifiMacBuf[i] >> 4);
		if (wifiMacStr[j] > '9')
			wifiMacStr[j] += 7;
		if (i>2)
			apSSID[k++] = wifiMacStr[j];
		++j;
		wifiMacStr[j] = '0' + (wifiMacBuf[i] & 0x0f);
		if (wifiMacStr[j] > '9')
			wifiMacStr[j] += 7;
		if (i>2)
			apSSID[k++] = wifiMacStr[j];
		j+=2;
	}
}

void wifiAPInit() {
	IPAddress ip(192, 168, AP_SUBNET, 1);
	IPAddress mask(255,255,255,0);
	WiFi.mode(WIFI_AP_STA);
	WiFi.softAPConfig(ip,ip,mask);
	WiFi.softAP(apSSID);
	delay(2000);
	wifiAPmode = true;
}

bool wifiConnect(int retry) {
	if (wifiAPmode || wifiStatus == WL_CONNECTED)
		return true;
	Serial.print("Connecting to: ");
	Serial.println(wifiSSID);
	wifiStatus = WiFi.status();
	while (wifiStatus != WL_CONNECTED && retry != 0) {
		Serial.print(".");
		wifiStatus = WiFi.begin(wifiSSID, wifiPASSWD);
		delay(2000);
		if (retry > 0)
			retry--;
	}
	Serial.println();
	return wifiStatus == WL_CONNECTED;
}

/* 
 * serial peer input
 */
void serialInput() {
	unsigned long timeLoopStart,timeLoop;
	int nread = 0;
	timeLoopStart = timeLoop = millis();
	// Serial timeout triggers on full commsBuffer, just wait for first command
	while (nread == 0 && timeLoop - timeLoopStart < SERIAL_PEER_DELAY_MS) {
		// drop older contents on buffer overflow
		while ((nread = Serial.readBytes(commsBuffer, COMMS_BUFFER_SIZE)) == COMMS_BUFFER_SIZE);
		timeLoop = millis();
	}		
	commsBuffer[nread] = 0;
}

void loop() {
	int i, j, readstate;
	if (!wifiConnect(WIFI_CONNECT_RETRY))
		wifiAPInit();
	wifiClient = wifiServer.available();
	if (wifiClient && wifiClient.connected()) {
		delay(100);
		i = j = 0;
		readstate = METHOD;
		commsBuffer[0] = '\0';
		while (wifiClient.available()) {
			char c = wifiClient.read();
			switch (readstate) {
				case METHOD:
					if (c == '/')
						readstate = URI;
					else if (c == '\n')
						readstate = IGNORE;
					break;
				case URI:
					if (c == '\n' || c == ' ') {
						commsBuffer[j++] = '\0';
						readstate = IGNORE;
						break;
					}
					commsBuffer[j++] = c;
					break;
				default:
					break;
			}
		}
		while (Serial.available()) {
			Serial.read();
		}
		Serial.print(commsBuffer);
		wifiClient.println("HTTP/1.1 200 OK");
		wifiClient.println("Content-Type: text/plain");
		wifiClient.println("Connection: close");
		wifiClient.println();
		serialInput();
		wifiClient.println(commsBuffer);
		wifiClient.println();
		delay(WIFI_CLIENT_DELAY);
		wifiClient.stop();
	}
}
