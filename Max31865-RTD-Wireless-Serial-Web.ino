/*************************************************** 
  This is a library for the Adafruit PT100/P1000 RTD Sensor w/MAX31865

  Designed specifically to work with the Adafruit RTD Sensor
  ----> https://www.adafruit.com/products/3328

  This sensor uses SPI to communicate, 4 pins are required to  
  interface
  Adafruit invests time and resources providing this open source code, 
  please support Adafruit and open-source hardware by purchasing 
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.  
  BSD license, all text above must be included in any redistribution

 
 ****************************************************/

 #include <ESP8266WiFi.h>
 
const char* ssid     = "LAB_Devices";
const char* password = "";
 
#include <Adafruit_MAX31865.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>

#include "ESP8266WebServer.h"

// Start a TCP Telnet Server defaul port (23)
//#define MAX_SVR_CLIENTS 1
WiFiServer server(23);
WiFiClient serverClients;
ESP8266WebServer WEBserver(80);

//TCP port

//WiFiClient client;

// Use software SPI:         CS, DI, DO, CLK
//NodeMCU pins connected to: D0, D1, D2, D3
Adafruit_MAX31865 max1 = Adafruit_MAX31865(16, 5, 4, 0);
// use hardware SPI, just pass in the CS pin
//Adafruit_MAX31865 max1 = Adafruit_MAX31865(14);

// The value of the Rref resistor. Use 430.0!
#define RREF 430.0
//FACTORY CALIBRATED CONSTANT -- CHIP & RTD DEPENDENT (first time calibration only) use DATA fit
//Temp = C0+(R*(C1+R*(C2+R*(C3+C4*R))))/(1+R*(C5+R*(C6+C7*R)))
float C[8]={-262.0325346,-2.181174022,-0.09730651673,0.005051326571,0.000004755698497,0.001009492187,0.001352944965,0.0000001407667341};
float Ri; //actual resistance
uint16_t rtd1;
uint16_t rtd1_avg = 0;


int WiFiStatus;
int WiFiLastStatus;

int noDisconnect=0;
bool con=false;

float R;
char buffer[128];
char webpage[512];

void setup() {
  //SETUP SERIAL PORT
  Serial.begin(115200);
  Serial.println("INFO, PRCN, Wireless RTD, RTD200, v0.0.1");

  //INIT MAX31865
  max1.begin(MAX31865_4WIRE);  //set 4WIRE as necessary
  rtd1 = max1.readRTD();//READ RTD resistance (raw)
  R = (float)((RREF/32768.0)*rtd1);  //convert to actual resistance (floating point number)
  
  

  //INIT WIFI
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");  Serial.println(ssid);

  WiFi.softAPdisconnect (true);  //DISABLE ACCESS POINT (AP MODE)
  WiFi.begin(ssid, password);
  /*
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
 
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: "); Serial.println(WiFi.localIP());
  Serial.print("Netmask: "); Serial.println(WiFi.subnetMask());
  Serial.print("Gateway: "); Serial.println(WiFi.gatewayIP());
  */
  server.begin();
  server.setNoDelay(true);
  /* NOTE: ESP will attempt to reconnect to WiFi network whenever it is disconnected. 
   You don't need to handle this manually. A good way to simulate disconnection from
   an AP would be to reset the AP. ESP will report disconnection, and it will try to
   reconnect automatically.*/

  WEBserver.on("/other", []() {   //Define the handling function for the path
 
  WEBserver.send(200, "text / plain", "Other URL");});
  WEBserver.on("/", handleRootPath);    //Associate the handler function to the path
  //WEBserver.onNotFound([](){ WEBserver.send(404, "text/plain", "Are you lost?");});
  WEBserver.begin();
   
  WiFiLastStatus=WiFi.status();  
}



void handleRootPath() {            //Handler for the rooth path
  String htmlPage = 
  String ("<html>\n\r  <head>\n\r    <title>PRCN-RTD200</title>\n\r    <meta http-equiv=\"refresh\" content=\"5\">\n\r  </head>\n\r  <body>\n\r") +
          buffer + "</body>\n\r</html>";
  
  WEBserver.send(200, "text/html", htmlPage);
}



//Calibrated temp, raw resitance, wifi status, client status
void loop() {
  rtd1 = max1.readRTD();//READ RTD resistance (raw)
  Ri = (float)((RREF/32768.0)*rtd1);  //convert to actual resistance (floating point number)
  
  R=((R*19.0)+Ri)/20.0;  //Take an average
  WiFiStatus=WiFi.status();

  if((WiFiStatus!=WL_CONNECTED)&&(WiFiLastStatus!=WiFiStatus)) noDisconnect++; //count disconnection from WIFI
  WiFiLastStatus = WiFiStatus;
  
  //Calculate Temp. in °F using rational polynomial
  float Temp = C[0] + (R * (C[1] + R*(float)(C[2] + R*(float)(C[3] + (float)R*C[4]))))/(float)(1.0+R*(float)(C[5]+R*(float)(C[6]+R*C[7])));
  dtostrf(Temp, -10, 8, buffer);//convert to string
  
  Serial.print (Temp, 6); Serial.print (", "); Serial.print (R, 6), Serial.print (", ");
  if (WiFiStatus==WL_CONNECTED) Serial.print("WiFi Connected, ");
  else Serial.print ("WiFi Disconnected, ");
  Serial.print (WiFi.localIP());
  if (serverClients.connected()) Serial.print (", Client connected");
  else Serial.print (", No Client");

  Serial.print(", "); Serial.println (noDisconnect);

  WEBserver.handleClient();
  
  // listen for incoming clients
  if (server.hasClient())//new client avaliable
  {
    if(!serverClients.connected())
    {
      serverClients = server.available();
      Serial.println("Client connected");
    }
    else
    {
      server.available().println("busy"); 
      Serial.println ("New client, but server is busy.");
    }
  }
  
  
  //WiFiClient client = server.available();

  //NOTE: must read all client buffer, otherwise serverClients.connected return TRUE
  if (serverClients.connected())
  {
    serverClients.read();
    serverClients.println(buffer);
  }
  else serverClients.stop();
    
  delay(50);


  // Check and print any faults
  uint8_t fault = max1.readFault();
  if (fault) {
    Serial.print("Fault 0x"); Serial.println(fault, HEX);
    if (fault & MAX31865_FAULT_HIGHTHRESH) {
      Serial.println("RTD High Threshold"); 
    }
    if (fault & MAX31865_FAULT_LOWTHRESH) {
      Serial.println("RTD Low Threshold"); 
    }
    if (fault & MAX31865_FAULT_REFINLOW) {
      Serial.println("REFIN- > 0.85 x Bias"); 
    }
    if (fault & MAX31865_FAULT_REFINHIGH) {
      Serial.println("REFIN- < 0.85 x Bias - FORCE- open"); 
    }
    if (fault & MAX31865_FAULT_RTDINLOW) {
      Serial.println("RTDIN- < 0.85 x Bias - FORCE- open"); 
    }
    if (fault & MAX31865_FAULT_OVUV) {
      Serial.println("Under/Over voltage"); 
    }
    max1.clearFault();
  }
  Serial.println();
  
}

