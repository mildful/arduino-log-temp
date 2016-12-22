#line 1 "D:/ti_workspace/log_temp/log_temp.ino"
#include <WiFi.h>
#include <SLFS.h>
#include "Adafruit_TMP006.h"
#include "utility/udma_if.h"
#include "utility/simplelink.h"
#include "driverlib/prcm.h"


#define CONF_FILE "/storage/conf_6.txt"


#include "Energia.h"

void createConf(String ssid, String pwd);
void deleteConf();
String getSSIDFromString(String str);
String getPwdFromString(String str);
void APMode();
void blinkRed(unsigned long currentMillis, int interval);
void setup();
void loop();

#line 12
WiFiServer myServer(80);
uint8_t oldCountClients = 0;
uint8_t countClients = 0;
char wifi_name[] = "PolyIoT";
char wifi_password[] = "foobar123";


Adafruit_TMP006 tmp006(0x41);
unsigned long temp_previousMillis = 0;
const int temp_interval = 500;
const int temp_blinkRate = 1000;

const int push_interval = 2000;
unsigned long push_lastMillis = 0;
boolean readyToReset = false;
int resetBlinkRate = 200;


char mqtt_hostname[] = "192.168.1.22";
int mqtt_port = 1883;

void createConf(String ssid, String pwd) {
	int32_t retval = SerFlash.open(CONF_FILE,
			FS_MODE_OPEN_CREATE(128, _FS_FILE_OPEN_FLAG_COMMIT));

	if (retval != SL_FS_OK)
	{
		Serial.print("Error creating file file, error code: ");
		Serial.println(SerFlash.lastErrorString());
		Serial.flush();  
		SerFlash.close();
		while(1) ;  
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
        
        Serial.print('.');
        delay(300);
    }

    Serial.println("AP mode enabled.");
    Serial.print("LAN name = ");
    Serial.println(wifi_name);
    Serial.print("WPA password = ");
    Serial.println(wifi_password);

    Serial.print("Web-server port = ");
    myServer.begin();  
    Serial.println("80");
}


int ledState = LOW;
unsigned long led_previousMillis = 0;

void blinkRed(unsigned long currentMillis, int interval) {
	if (currentMillis - led_previousMillis >= interval) {
		
		led_previousMillis = currentMillis;

		
		if (ledState == LOW) {
		  ledState = HIGH;
		} else {
		  ledState = LOW;
		}

		
		digitalWrite(RED_LED, ledState);
	}
}

void setup() {
	
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
		Serial.flush();  
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

			
			if (countClients != oldCountClients)
			{
				if (countClients > oldCountClients)
				{  
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
				{  
					Serial.println("Client disconnected from AP.");
					Serial.println();
				}
				oldCountClients = countClients;
			}

			WiFiClient myClient = myServer.available();

			
			if(myClient)
			{
				Serial.println();
				Serial.println("Client connected to server");
				Serial.println();
				char buffer[150] = {0}; 
				int8_t i = 0;

				while (myClient.connected())
				{
					if (myClient.available())
					{
						
						char c = myClient.read(); 
						

						if (c == '\n')
						{
							
							
							
							if (strlen(buffer) == 0)
							{
								
								
								myClient.println("HTTP/1.1 200 OK");
								myClient.println("Content-type:text/html");
								myClient.println();

								
								myClient.print("<html><head><title>Energia CC3200 WiFi Web-Server in AP Mode</title></head><body align=center>");
								myClient.print("<h1 align=center><font color=\"red\">LaunchPad CC3200 WiFi Web-Server in AP Mode</font></h1>");
								myClient.print("<form method=\"GET\" action=\"connect\">");
								myClient.print("SSID: <input type=\"text\" name=\"ssid\"><br/>");
								myClient.print("PWD: <input type=\"password\" name=\"pwd\"><br/>");
								myClient.print("<input type=\"submit\" value=\"connect\">");
								myClient.print("</form>");

								
								myClient.println();
								
								break;
							}
							else
							{      
								memset(buffer, 0, 150);
								i = 0;
							}
						}
						else if (c != '\r')
						{    
							buffer[i++] = c;      
						}

						String text = buffer;
						if (text.startsWith("GET /connect") && text.endsWith("HTTP/1.1"))
						{
							Serial.println(text);
							Serial.println("Retrieving data...");
							
							ssid = getSSIDFromString(text);

							
							password = getPwdFromString(text);

							Serial.println("Data acquired !");
							Serial.println();
							
							Serial.println("Gonna overwrite previous conf.");
							deleteConf();
							Serial.println();
							Serial.println("Creating new configuration...");
							createConf(ssid, password);
							Serial.println("Configuration created!");
							Serial.println();
							
							stopServer = true;
							hasConf = true;
						}
					}
				}

				 
				 myClient.stop();
				 Serial.println("Client disconnected from server");
				 Serial.println();
			}
		}
	} 

	Serial.println("Enable station mode with:");
	Serial.print("SSID: ");
	Serial.println(ssid);
	Serial.print("password: ");
	Serial.println(password);
	Serial.println();

	
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
		
		Serial.print(".");
		delay(300);
	}

    Serial.println("\nYou're connected to the network");
    Serial.println("Waiting for an ip address");

    while (WiFi.localIP() == INADDR_NONE)
    {
        
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
	    float diet = tmp006.readDieTempC();
	    Serial.println(diet);

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
	}

	int pushState = digitalRead(PUSH2);

	if (pushState == HIGH) {
		push_lastMillis = push_lastMillis != 0 ? push_lastMillis : currentMillis;
		
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



