/*

  Udp NTP Client

  Get the time from a Network Time Protocol (NTP) time server
  Demonstrates use of UDP sendPacket and ReceivePacket
  For more on NTP time servers and the messages needed to communicate with them,
  see http://en.wikipedia.org/wiki/Network_Time_Protocol

  created 4 Sep 2010
  by Michael Margolis
  modified 9 Apr 2012
  by Tom Igoe
  updated for the ESP8266 12 Apr 2015
  by Ivan Grokhotkov

  This code is in the public domain.

  Changed by JAS 2019-08-24:
  Get the time once from the time server,  This will then be used to set a clock scetch.
  Now the clock code is added using a Mini D1 Wifi and a OLED shield v2.0.0 (D1 form factor).
  The NTP client is used once to set the clock time then it runs on it's own.  NOT a ROLEX! lol

  JAS 2019-8-26
  Added "Waiting" message on oled.
  Added if (debug)
  Changed the delay's for Wifi and NTP server.
  Still need to add a couple of time-outs for wifi and ntp server.
  JAS 2019-09-09
  Totaly changes this version to a "big" character Digital clock.
  JAS 2019-11-09
  Corrected some 24 th hour issues.  And reduced the code size.

*/

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
//#include <Wire.h>  // Include Wire if you're using I2C, I am not!
#include "MySID.h"  // My SID and password details !!!

// JAS This libery was set to 1.2.4 as I had problems with 1.2.7
#include <SFE_MicroOLED.h>  // Include the SFE_MicroOLED library

#define PIN_RESET 255  // No reset pin

// The solder jumper on the back of the shield
#define DC_JUMPER 0  // I2C Addres: 0 - 0x3C, 1 - 0x3D

bool summerTime = true;  // Must check the NTP Protocol!!
bool debug = true;

const char * ssid = STASSID; // your network SSID (name)
const char * pass = STAPSK;  // your network password

unsigned int localPort = 2390;  // local port to listen for UDP packets

IPAddress timeServerIP; // time.nist.gov NTP server address

// Select your server or add another one.

//const char* ntpServerName = "time.nist.gov";  // US server
const char* ntpServerName = "ntp2c.mcc.ac.uk";  // UK server

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

//////////////////////////////////
// MicroOLED Object Declaration //
//////////////////////////////////

MicroOLED oled(PIN_RESET, DC_JUMPER);  // I2C Example

// Use these variables to set the initial time
int hours = 0;
int minutes = 0;
int seconds = 0;

int cb = 0;

// How fast do you want the clock to increment? Set this to 1 for fun.
// Set this to 1000 to get _about_ 1 second timing.
const int CLOCK_SPEED = 1000;

unsigned long lastDraw = 0;

void setup() {

  if (debug) {
    Serial.begin(9600);
  }

  oled.begin();     // Initialize the OLED
  oled.clear(ALL);  // Clear the library's display buffer
  oled.clear(PAGE); // Clear the display's internal memory
  oled.setFontType(0); // set font type 0, please see declaration in SFE_MicroOLED.cpp
  oled.setCursor(0, 0); // points cursor to x0 y0 OR NOT as 0,0 is NOT my top left!!
  oled.println("Waiting on");
  oled.println();
  oled.println("WiFi/NTP.");
  oled.display(); // display the memory buffer drawn

  // We start by connecting to a WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  // Need to add a timeout!
  while (WiFi.status() != WL_CONNECTED) {
    if (debug) Serial.println("Wifi");
    delay(500);
  }

  udp.begin(localPort);

  WiFi.hostByName(ntpServerName, timeServerIP);

  sendNTPpacket(timeServerIP); // send an NTP packet to a time server

  // wait to see if a reply is available. Need to add a timeout!
  while (!cb) {
    if (debug) Serial.println("NTP");
    cb = udp.parsePacket();
    delay(100);
  }

  // We've received a packet, read the data from it
  
  udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
  unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
  unsigned long secsSince1900 = highWord << 16 | lowWord;
  const unsigned long seventyYears = 2208988800UL;
  unsigned long epoch = secsSince1900 - seventyYears;
  
  if (summerTime) {
    hours = (((epoch  % 86400L) / 3600) + 1); // print the hour (86400 equals secs per day)
  }
  else {
    hours = ((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
  }
  if (hours == 24){
    hours = 0;
  }
  minutes = ((epoch  % 3600) / 60);
  seconds = (epoch % 60);

  if (debug) {
    Serial.print(hours);
    Serial.print(":");
    Serial.print(minutes);
    Serial.print(":");
    Serial.println(seconds);
  }

  // Clear my Waiting message
  oled.clear(PAGE); // Clear the display's internal memory
  oled.display(); // display the memory buffer drawn
}

void loop() {
  // Check if we need to update seconds, minutes, hours:
  if (lastDraw + CLOCK_SPEED < millis()) {
    lastDraw = millis();
    // Add a second, update minutes/hours if necessary and display time.
    updateTime();
  }
}

// send an NTP request to the time server at the given address

void sendNTPpacket(IPAddress& address) {

  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}



// Simple function to increment seconds and then increment minutes
// and hours if necessary.  And display the digital time.
// Set for a 24 hour clock, is there any other?

void updateTime()
{
  seconds++;  // Increment seconds
  if (seconds >= 60)  // If seconds overflows (>=60)
  {
    seconds = 0;  // Set seconds back to 0
    minutes++;    // Increment minutes
    if (minutes >= 60)  // If minutes overflows (>=60)
    {
      minutes = 0;  // Set minutes back to 0
      hours++;      // Increment hours
      if (hours >= 24)  // If hours overflows (>=24)
      {
        hours = 0;  // Set hours back to 0
      }
    }

  }
  if (debug) {
    Serial.print(hours);
    Serial.print(":");
    Serial.print(minutes);
    Serial.print(":");
    Serial.println(seconds);
  }

  oled.setFontType(2); // set font type 2, the largest I can use on a 32x24 oled
  oled.setCursor(5, 6); // points cursor to x5 y6
  if (hours < 10) oled.print("0");  // Add leading 0 if required
  oled.print(hours);
  oled.print(":");
  if (minutes < 10) oled.print("0");  // Add leading 0 if required
  oled.println(minutes);
  oled.setCursor(22, 28);
  if (seconds < 10) oled.print("0");  // Add leading 0 if required
  oled.print(seconds);
  oled.display(); // display the memory buffer drawn
}
