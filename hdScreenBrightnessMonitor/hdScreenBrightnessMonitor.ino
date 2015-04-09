#include <SPI.h>
#include <Ethernet.h>

#include "Config.h"

byte mac[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }; // Must be unique on local network
const int second = 1000; // 1 second = 1000 millis
const int updateThingSpeakInterval = 15 * second;      // Time interval in milliseconds to update ThingSpeak (number of seconds * 1000 = interval)
unsigned long lastConnectionTime = 0;

byte networkFailedCounter = 0;

int ledPin = 8;
int ledStatus = LOW;

void setup()
{
	delay(250);

	// Start Serial for debugging on the Serial Monitor
	Serial.begin(9600);

	// Start Ethernet on Arduino
	startEthernet();

	pinMode(ledPin, OUTPUT);
	ledStatus = LOW;
}

void processCommand(String command)
{
	if (command == "ON")
		digitalWrite(ledPin, ledStatus = HIGH);
	else if (command == "OFF")
		digitalWrite(ledPin, ledStatus = LOW);
}

String readHttpResponseWithoutHeaders(EthernetClient &client)
{
	String response;

	// Wait a moment for the server to generate response.
	// Value to wait was chosen quite randomly and may need to be adjusted. How much to wait? Well, it depends on the server load, network performance and many other factors
	delay(500);

	byte crlf_found = 0; // The http response divides header and body by two CRLF so we count them

	while (true)
	{
		delay(100); // I don't believe Arduino process the data faster than it arrives, but in case it happen then wait for some data
		if (!client.available())
			break;

		while (client.available())
		{
			char c = client.read();
			//Serial.print(c);
			
			if (crlf_found >= 4) // Header end
			{
				if (crlf_found == 6) // Also omit the first line which is the body length (we should read it, but whatever)
					response += c;
				else if (c == '\r' || c == '\n')
					crlf_found++;
			}
			else if (c == '\r')
			{
				crlf_found++;
				if (crlf_found != 1 && crlf_found != 3)
					crlf_found = 0;
			}
			else if (c == '\n')
			{
				crlf_found++;
				if (crlf_found != 2 && crlf_found != 4)
					crlf_found = 0;
			}
			else
				crlf_found = 0;
		}
		
		if (response.length() <= 2) // 'empty' response (usually "\r\n")
			response = "";
		else
			response = response.substring(0, response.length() - 7); // Remove last five character ("0\r\n\r\n")
		return response;
	}
}

String getNextTalkBackCommand(String apiKey, String talkbackId)
{
	EthernetClient client;
	if (client.connect(thingSpeakAddress, 80))
	{
		networkFailedCounter = 0;

		client.println("POST /talkbacks/" + talkbackId + "/commands/execute HTTP/1.1");
		client.println("Host: api.thingspeak.com");
		client.println("Connection: close");
		client.println("Content-Type: application/x-www-form-urlencoded");
		String post = "api_key=" + apiKey;
		client.println("Content-Length: " + String(post.length(), DEC));
		client.println();
		client.print(post);

		String response = readHttpResponseWithoutHeaders(client);
		
		// Disconnect from ThingSpeak
		if (client.connected())
			Serial.println("[getNextTalkBackCommand] Error: Arduino is still connected");
		client.stop();
		return response;
	}
	else
	{
		networkFailedCounter++;
		Serial.println("[getNextTalkBackCommand] ERROR: Cannot connect to ThingSpeak (" + String(networkFailedCounter, DEC) + ")");
		return "";
	}
}

void loop()
{
	// Check if Arduino Ethernet may need to be restarted
	if (networkFailedCounter > 3)
	{
		startEthernet();
		networkFailedCounter = 0;
	}

	String command = getNextTalkBackCommand(THINGSPEAK_LED_TALKBACK_API_KEY, THINGSPEAK_LED_TALKBACK_ID);
	if (command != "")
		processCommand(command);
	delay(500);

	unsigned long timeElapsedSinceLastConnection = millis() - lastConnectionTime;
	if (timeElapsedSinceLastConnection > updateThingSpeakInterval)
	{
		// Calculate some average based on many reads
		int light = 0;
		for (int i = 0; i < 20; ++i)
		{
			light += analogRead(A0);
			delay(50);
		}
		light /= 20;

		String sLight = String(light, DEC);
		Serial.println("<LIGHT FEED " + sLight + ">");
		updateThingSpeakFeed(THINGSPEAK_LIGHT_FEED_API_KEY, "field1=" + sLight);

		String sLed = String(ledStatus, DEC);
		Serial.println("<LED FEED " + sLed + ">");
		updateThingSpeakFeed(THINGSPEAK_LED_FEED_API_KEY, "field1=" + sLed);

		lastConnectionTime = millis();
	}
}

void updateThingSpeakFeed(String apiKey, String tsData)
{
	EthernetClient client;
	if (client.connect(thingSpeakAddress, 80))
	{
		networkFailedCounter = 0;

		client.println("POST /update HTTP/1.1");
		client.println("Host: api.thingspeak.com");
		client.println("Connection: close");
		client.println("X-THINGSPEAKAPIKEY: " + apiKey);
		client.println("Content-Type: application/x-www-form-urlencoded");
		client.println("Content-Length: " + String(tsData.length()));
		client.println();
		client.print(tsData);

		String response = readHttpResponseWithoutHeaders(client);

		if (client.connected())
			Serial.println("[updateThingSpeakFeed] Error: Arduino is still connected");
		client.stop();
	}
	else
	{
		networkFailedCounter++;
		Serial.println("[updateThingSpeakFeed] Error: Cannot connect to ThingSpeak (" + String(networkFailedCounter, DEC) + ")");
	}
}

bool startEthernet()
{
	Serial.print("Connecting Arduino to network... ");
	if (Ethernet.begin(mac))
		Serial.println("[OK]");
	else
		Serial.println("[ERROR] DHCP Failed, reset Arduino to try again"); 
	Serial.println();
}