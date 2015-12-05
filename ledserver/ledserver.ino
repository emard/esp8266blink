#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

const char* ssid = "RADIONA";
const char* password = "............";

ESP8266WebServer server(80);

const int led = BUILTIN_LED;

void handleRoot() {
  server.send(200, "text/html", "LED<p/><a href=\"on\">ON</a> <a href=\"off\">OFF</a>");
}

void handleNotFound(){
  //digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    if(server.arg(i)[0]='0')
      digitalWrite(led, 0);
    if(server.arg(i)[0]='1')
      digitalWrite(led, 1);
  }
  server.send(404, "text/plain", message);
  //digitalWrite(led, 0);
}

void setup(void){
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);

  server.on("/on", [](){
    digitalWrite(led, 0);
    server.send(200, "text/html", "LED is ON<p/><a href=\"on\">ON</a> <a href=\"off\">OFF</a>");
  });

  server.on("/off", [](){
    digitalWrite(led, 1);
    server.send(200, "text/html", "LED is OFF<p/><a href=\"on\">ON</a> <a href=\"off\">OFF</a>");
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void){
  server.handleClient();
}

