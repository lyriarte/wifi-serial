/*
 * Copyright (c) 2017, Luc Yriarte
 * License: BSD <http://www.opensource.org/licenses/bsd-license.php>
 */


#include <ESP8266WiFi.h>


/* **** **** **** **** **** ****
 * Constants
 * **** **** **** **** **** ****/

#define BPS_HOST 9600
#define COMMS_BUFFER_SIZE 24576
#define SERIAL_PEER_DELAY_MS 15000
#define SERIAL_DUMP_DELAY 5000

#define serverPORT 80
#define AP_SUBNET 64
#define WIFI_CLIENT_DELAY 500
#define WIFI_CONNECT_DELAY 2000
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

char * wifiSSIDs[] = {"SSID1", "SSID2", "SSID3"};
char * wifiPASSWDs[] = {"pwd1", "pwd2", "pwd3"};
int wifiStatus = WL_IDLE_STATUS;
bool wifiAPmode = false;
char apSSID[] = "ESP_XXXXXX";
char wifiMacStr[] = "00:00:00:00:00:00";
byte wifiMacBuf[6];

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
	if (!wifiConnect(WIFI_CONNECT_RETRY))
		wifiAPInit();
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
	Serial.print("WiFi.softAP: ");
	Serial.println(apSSID);
	Serial.print("WiFi server IP Address: ");
	Serial.println(ip);
	delay(WIFI_CONNECT_DELAY);
	wifiAPmode = true;
}

bool wifiConnect(int retry) {
	int i,n;
	if (wifiAPmode || wifiStatus == WL_CONNECTED)
		return true;
	n = sizeof(wifiSSIDs) / sizeof(char *);
	for (i=0; i<n; i++) {
		if (wifiConnectSSID(wifiSSIDs[i], wifiPASSWDs[i], retry))
			return true;
	}
	return false;
}

bool wifiConnectSSID(char * wifiSSID, char * wifiPASSWD, int retry) {
	Serial.print("Connecting to: ");
	Serial.println(wifiSSID);
	wifiStatus = WiFi.status();
	while (wifiStatus != WL_CONNECTED && retry != 0) {
		if (wifiStatus != WL_IDLE_STATUS) {
			wifiStatus = WiFi.begin(wifiSSID, wifiPASSWD);
			Serial.print("trying..");
			if (retry > 0)
				retry--;
		}
		Serial.print(".");
		delay(WIFI_CONNECT_DELAY);
	}
	Serial.println();
	if (wifiStatus == WL_CONNECTED) {
		Serial.print("WiFi client IP Address: ");
		Serial.println(WiFi.localIP());
	}
	return wifiStatus == WL_CONNECTED;
}

/* 
 * serial peer input
 */
int serialInput() {
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
	return nread;
}

void serialDump() {
	int nread = 0;
	while (nread = Serial.readBytes(commsBuffer, COMMS_BUFFER_SIZE-1)) {
		commsBuffer[nread] = 0;
		wifiClient.print(commsBuffer);
		// wait for more content
		delay(SERIAL_DUMP_DELAY);
	}
}

void serialCmd(char *cmd) {
	// consume older contents
	while (Serial.available()) {
		Serial.read();
	}
	// send command
	Serial.print(cmd);
}

void loop() {
	int i, j, nread, readstate;
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
					i=1;
					commsBuffer[j++] = c;
					break;
				default:
					break;
			}
		}
		// If a command was passed on URI send it
		if (i)
			serialCmd(commsBuffer);
		wifiClient.println("HTTP/1.1 200 OK");
		wifiClient.println("Content-Type: text/plain");
		wifiClient.println("Access-Control-Allow-Origin: *");
		wifiClient.println("Connection: close");
		wifiClient.println();
		// If a command was passed on URI print the result
		if (i) {
			while (true) {
				nread = serialInput();
				wifiClient.println(commsBuffer);
				// continue printing the results until the next prompt
				if (nread > 2 && commsBuffer[nread-1] == ' ' && commsBuffer[nread-2] == ':') {
					break;
				}
			}
		}
		// otherwise dump serial buffer contents
		else {
			serialDump();
		}
		wifiClient.println();
		delay(WIFI_CLIENT_DELAY);
		wifiClient.stop();
	}
}
