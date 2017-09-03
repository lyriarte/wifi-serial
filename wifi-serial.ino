/*
 * Copyright (c) 2017, Luc Yriarte
 * License: BSD <http://www.opensource.org/licenses/bsd-license.php>
 */


#include <ESP8266WiFi.h>


/* **** **** **** **** **** ****
 * Constants
 * **** **** **** **** **** ****/

#define BPS_HOST 9600
#define COMMS_BUFFER_SIZE 1024

#define wifiSSID "SSID"
#define wifiPASSWD "password"
#define serverPORT 80

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
	if (!wifiConnect(5)) {
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
	IPAddress ip(192, 168, 64, 1);
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
 * userIO
 */
char * userInput(char * message) {
	char * input = NULL;
	int nread = 0;
	if (message != NULL)
		Serial.print(message);
	while (!(nread = Serial.readBytes(commsBuffer, COMMS_BUFFER_SIZE)));
	if (nread == COMMS_BUFFER_SIZE)
		return input; // buffer overflow
	commsBuffer[nread] = 0;
	input = commsBuffer;
	return input;
}

void loop() {
	int i, j, readstate;
	if (!wifiConnect(5))
		wifiAPInit();
	wifiClient = wifiServer.available();
	if (wifiClient && wifiClient.connected()) {
		delay(100);
		i = j = 0;
		readstate = METHOD;
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
		Serial.println(commsBuffer);
		delay(1500);
		wifiClient.println("HTTP/1.1 200 OK");
		wifiClient.println("Content-Type: text/html");
		wifiClient.println("Connection: close");
		wifiClient.println();
		wifiClient.println("<!DOCTYPE HTML>");
		wifiClient.println("<html>");
		wifiClient.print("<center><h1>");
		wifiClient.print(apSSID);
		wifiClient.println("</h1></center>");
		wifiClient.print("<h2>");
		wifiClient.print(wifiMacStr);
		wifiClient.println("</h2>");
		if (Serial.available()) {
			wifiClient.println("<pre>");
			if (userInput(NULL) != NULL)
				wifiClient.println(commsBuffer);
			wifiClient.println("</pre>");
		}
		wifiClient.println("</html>");
		delay(500);
		wifiClient.stop();
	}
}
