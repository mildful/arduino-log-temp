#include <WiFi.h>
#include <SLFS.h>
#include "Adafruit_TMP006.h"
#include "utility/udma_if.h"
#include "utility/simplelink.h"
#include "driverlib/prcm.h"
// MQTT
#include <WifiIPStack.h>
#include <Countdown.h>
#include <MQTTClient.h>

// FileSystem
#define CONF_FILE "/storage/conf_6.txt"

// Wifi
WiFiServer myServer(80);
uint8_t oldCountClients = 0;
uint8_t countClients = 0;
char wifi_name[] = "PolyIoT";
char wifi_password[] = "foobar123";

// Temperature sensor
Adafruit_TMP006 tmp006(0x41);
unsigned long temp_previousMillis = 0;
const int temp_interval = 1000;
const int temp_blinkRate = 1000;
// button
const int push_interval = 2000;
unsigned long push_lastMillis = 0;
boolean readyToReset = false;
int resetBlinkRate = 200;

// MQTT
const char* topic = "temperature";
char mqtt_address[] = "192.168.1.32";
int mqtt_port = 1883;
WifiIPStack ipstack;
MQTT::Client<WifiIPStack, Countdown> client = MQTT::Client<WifiIPStack, Countdown>(ipstack);

void createConf(String ssid, String pwd) {
	int32_t retval = SerFlash.open(CONF_FILE,
			FS_MODE_OPEN_CREATE(128, _FS_FILE_OPEN_FLAG_COMMIT));

	if (retval != SL_FS_OK)
	{
		Serial.print("Error creating file file, error code: ");
		Serial.println(SerFlash.lastErrorString());
		Serial.flush();  // flush pending serial output before entering suspend()
		SerFlash.close();
		while(1) ;  // halting (suspend() isn't support by Energia MT at the moment FYI)
	}

	SerFlash.print("ssid=");
	SerFlash.print(ssid);
	SerFlash.print("&");
	SerFlash.print("pwd=");
	SerFlash.print(pwd);

	SerFlash.close();
    SerFlash.freeString();
}

void deleteConf() {
	Serial.println("Deleting...");
	int32_t retval = SerFlash.del(CONF_FILE);
	Serial.print("Deleting file return code: ");
	Serial.println(SerFlash.lastErrorString());
	Serial.println("Conf deleted !");
}

String getSSIDFromString(String str) {
	char ssidChar[150] = { 0 };
	int k = 0;
	int ssidLastIndex = str.indexOf("ssid=") + 5;
	int maxLength = str.length();
	for (ssidLastIndex; ssidLastIndex < maxLength; ssidLastIndex++)
	{
		if (str.charAt(ssidLastIndex) != '&')
		{
			ssidChar[k] = str.charAt(ssidLastIndex);
			k++;
		}
		else
		{
			ssidLastIndex = str.length();
		}
	}
	return ssidChar;
}

String getPwdFromString(String str) {
	char pwd[150] = { 0 };
	int k = 0;
	int pwdLastIndex = str.lastIndexOf("pwd=") + 4;
	int maxLength = str.length();
	for (pwdLastIndex; pwdLastIndex < maxLength; pwdLastIndex++)
	{
		if (str.charAt(pwdLastIndex) != ' ')
		{
			pwd[k] = str.charAt(pwdLastIndex);
			k++;
		}
		else
		{
			pwdLastIndex = str.length();
		}
	}
	return pwd;
}

void APMode() {
	WiFi.beginNetwork(wifi_name, wifi_password);
    while (WiFi.localIP() == INADDR_NONE)
    {
        // print dots while we wait for the AP config to complete
        Serial.print('.');
        delay(300);
    }

    Serial.println("AP mode enabled.");
    Serial.print("LAN name = ");
    Serial.println(wifi_name);
    Serial.print("WPA password = ");
    Serial.println(wifi_password);

    Serial.print("Web-server port = ");
    myServer.begin();  // start the web server on port 80
    Serial.println("80");
}

// todo : class
int ledState = LOW;
unsigned long led_previousMillis = 0;
// logic
void blinkRed(unsigned long currentMillis, int interval) {
	if (currentMillis - led_previousMillis >= interval) {
		// save the last time you blinked the LED
		led_previousMillis = currentMillis;

		// if the LED is off turn it on and vice-versa:
		if (ledState == LOW) {
		  ledState = HIGH;
		} else {
		  ledState = LOW;
		}

		// set the LED with the ledState of the variable:
		digitalWrite(RED_LED, ledState);
	}
}

void setup() {
	// put your setup code here, to run once:
	Serial.begin(115200);
	Serial.println("====== setup start ======");

	String ssid;
	String password;
	boolean hasConf = false;

	pinMode(RED_LED, OUTPUT);
	pinMode(GREEN_LED, OUTPUT);
	pinMode(YELLOW_LED, OUTPUT);
	pinMode(PUSH2, INPUT_PULLUP);

	digitalWrite(RED_LED, LOW);
	digitalWrite(GREEN_LED, LOW);
	digitalWrite(YELLOW_LED, LOW);

	Serial.print("CONF_FILE: ");
	Serial.println(CONF_FILE);
	Serial.println();

	SerFlash.begin();

	//deleteConf();

	// checking existing conf
	Serial.println("Checkinf if conf exists...");
	int32_t retval;
	retval = SerFlash.open(CONF_FILE, FS_MODE_OPEN_READ);
	if (retval == SL_FS_OK)
	{
		Serial.println("Conf exists.");
		Serial.println("Retrieving data from file...");
		String content = SerFlash.readBytes();
	    Serial.print("Read ");
	    Serial.print(content.length());
	    Serial.println(" bytes from file - contents:");
	    Serial.println(content);
	    Serial.println();
	    SerFlash.close();
	    SerFlash.freeString();

	    ssid = getSSIDFromString(content);
	    password = getPwdFromString(content);
	    hasConf = true;
	}
	else
	{
	    SerFlash.close();
		Serial.print("Error opening file for reading, error code: ");
		Serial.println(SerFlash.lastErrorString());
		Serial.flush();  // flush pending serial output before entering suspend()
	}

	while(!hasConf)
	{
		Serial.println("Conf does not exists.");
		Serial.println();
		Serial.println("Entering AP mode...");

		APMode();

		Serial.println();

		boolean stopServer = false;
		while (!stopServer)
		{
			countClients = WiFi.getTotalDevices();

			// Did a client connect/disconnect since the last time we checked?
			if (countClients != oldCountClients)
			{
				if (countClients > oldCountClients)
				{  // Client connect
					Serial.println("Client connected to AP");
					for (uint8_t k = 0; k < countClients; k++)
					{
						Serial.print("Client #");
						Serial.print(k);
						Serial.print(" at IP address = ");
						Serial.print(WiFi.deviceIpAddress(k));
						Serial.print(", MAC = ");
						Serial.println(WiFi.deviceMacAddress(k));
						Serial.println("CC3200 in AP mode only accepts one client.");
					}
				}
				else
				{  // Client disconnect
					Serial.println("Client disconnected from AP.");
					Serial.println();
				}
				oldCountClients = countClients;
			}

			WiFiClient myClient = myServer.available();

			// we get a client
			if(myClient)
			{
				Serial.println();
				Serial.println("Client connected to server");
				Serial.println();
				char buffer[150] = {0}; // make a buffer to hold incoming data
				int8_t i = 0;

				while (myClient.connected())
				{
					if (myClient.available())
					{
						// if there is bytes to read from the client
						char c = myClient.read(); // read ONE byte, then
						//Serial.write(c);

						if (c == '\n')
						{
							// if byte is a newline character
							// if the current line is blank, you got two newline characters in a row.
							// that's the end of the client HTTP request, so send a response:
							if (strlen(buffer) == 0)
							{
								// HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
								// and a content-type so the client knows what's coming, then a blank line:
								myClient.println("HTTP/1.1 200 OK");
								myClient.println("Content-type:text/html");
								myClient.println();

								// the content of the HTTP response follows the header:
								myClient.print("<html><head><title>Energia CC3200 WiFi Web-Server in AP Mode</title></head><body align=center>");
								myClient.print("<h1 align=center><font color=\"red\">LaunchPad CC3200 WiFi Web-Server in AP Mode</font></h1>");
								myClient.print("<form method=\"GET\" action=\"connect\">");
								myClient.print("SSID: <input type=\"text\" name=\"ssid\"><br/>");
								myClient.print("PWD: <input type=\"password\" name=\"pwd\"><br/>");
								myClient.print("<input type=\"submit\" value=\"connect\">");
								myClient.print("</form>");

								// The HTTP response ends with another blank line:
								myClient.println();
								// break out of the while loop:
								break;
							}
							else
							{      // if you got a newline, then clear the buffer:
								memset(buffer, 0, 150);
								i = 0;
							}
						}
						else if (c != '\r')
						{    // if you got anything else but a carriage return character,
							buffer[i++] = c;      // add it to the end of the currentLine
						}

						String text = buffer;
						if (text.startsWith("GET /connect") && text.endsWith("HTTP/1.1"))
						{
							Serial.println(text);
							Serial.println("Retrieving data...");
							// ssid
							ssid = getSSIDFromString(text);

							// password
							password = getPwdFromString(text);

							Serial.println("Data acquired !");
							Serial.println();
							// overwrite config
							Serial.println("Gonna overwrite previous conf.");
							deleteConf();
							Serial.println();
							Serial.println("Creating new configuration...");
							createConf(ssid, password);
							Serial.println("Configuration created!");
							Serial.println();
							// stop server
							stopServer = true;
							hasConf = true;
						}
					}
				}

				 // close the connection:
				 myClient.stop();
				 Serial.println("Client disconnected from server");
				 Serial.println();
			}
		}
	} // end no default conf file

	Serial.println("Enable station mode with:");
	Serial.print("SSID: ");
	Serial.println(ssid);
	Serial.print("password: ");
	Serial.println(password);
	Serial.println();

	// clear connection
    UDMAInit();
    sl_Start(NULL, NULL, NULL);
    sl_WlanDisconnect();
    sl_NetAppMDNSUnRegisterService(0, 0);
    sl_WlanRxStatStart();
    sl_WlanSetMode(ROLE_STA);
    sl_Stop(0xFF);
    sl_Start(0, 0, 0);

	Serial.println("Connecting...");

	int ssidLen = ssid.length() + 1;
	char ssidBuffer[ssidLen];
	ssid.toCharArray(ssidBuffer, ssidLen);

	password.replace("%21", "!");
	int pwdLen = password.length() + 1;
	char pwdBuffer[pwdLen];
	password.toCharArray(pwdBuffer, pwdLen);

	WiFi.begin(ssidBuffer, pwdBuffer);
	while (WiFi.status() != WL_CONNECTED)
	{
		// print dots while we wait to connect
		Serial.print(".");
		delay(300);
	}

    Serial.println("\nYou're connected to the network");
    Serial.println("Waiting for an ip address");

    while (WiFi.localIP() == INADDR_NONE)
    {
        // print dots while we wait for an ip addresss
        Serial.print(".");
        delay(300);
    }

    Serial.println("\nIP Address obtained");
    Serial.println("Starting temperature sensor...");

	if (! tmp006.begin()) {
		Serial.println("No sensor found. ! Halting !");
		while (1);
	}

	Serial.println("Sensor found.");
	Serial.println("====== setup end ======");
}

void loop() {
	unsigned long currentMillis = millis();

	if (currentMillis - temp_previousMillis > temp_interval) {
		temp_previousMillis = currentMillis;
	    float diet = tmp006.readDieTempC();
		// LED
	    if(!readyToReset) {
	    	if (diet > 40)
			{
				digitalWrite(RED_LED, HIGH);
			}
			else
			{
				blinkRed(currentMillis, temp_blinkRate);
			}
	    }
	    // MQTT
	    int rc = -1;
	    // Connect
	    if(!client.isConnected()) {
	    	Serial.println("Connecting to MQTT...");

	    	while (rc != 0) {
	    		rc = ipstack.connect(mqtt_address, mqtt_port);
	    	}

	    	MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;
			connectData.MQTTVersion = 3;
			connectData.clientID.cstring = (char*)"poly-launchpad";

			rc = -1;
		    while ((rc = client.connect(connectData)) != 0)
		      ;
		    Serial.println("Connected to MQTT !\n");
	    }
	    // Publish
	    char json[50];
	    sprintf(json, "%f", diet);
	    MQTT::Message message;
	    message.qos = MQTT::QOS0;
	    message.retained = false;
	    message.dup = false;
	    message.payload = (void*) json;
	    message.payloadlen = strlen(json) + 1;
	    rc = client.publish(topic, message);
	    Serial.print(diet);
	    Serial.print(" - ");
	    Serial.println(json);
	}

	int pushState = digitalRead(PUSH2);

	if (pushState == HIGH) {
		push_lastMillis = push_lastMillis != 0 ? push_lastMillis : currentMillis;
		// used to blink the led
		if (!readyToReset && currentMillis - push_lastMillis >= push_interval) {
			readyToReset = true;
		}
	} else if (push_lastMillis != 0){
		if (currentMillis - push_lastMillis >= push_interval) {
			Serial.println("Resetting configuration !");
			readyToReset = false;
			deleteConf();
		} else {
			Serial.println("Rebooting device !");
			Serial.println();
			sl_Stop(100);
			MAP_PRCMHibernateIntervalSet(330);
			MAP_PRCMHibernateWakeupSourceEnable(PRCM_HIB_SLOW_CLK_CTR);
			MAP_PRCMHibernateEnter();
		}
		push_lastMillis = 0;
	}

	if (readyToReset) {
		blinkRed(currentMillis, resetBlinkRate);
	}
}
