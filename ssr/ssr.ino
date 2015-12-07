/**
 * @file ssr.ino
 *
 * solid state relay server
 * 
 * started from original code by
 * @author Pascal Gollor (http://www.pgollor.de/cms/)
 * @date 2015-09-18
 * 
 * changelog:
 * 2015-10-22: 
 * - Use new ArduinoOTA library.
 * - loadConfig function can handle different line endings
 * - remove mDNS studd. ArduinoOTA handle it.
 * 
 * 2015-12-08: 
 * - modified for SSR support
 */

// includes
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <FS.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <DHT.h>

#define DHTTYPE DHT22
#define DHTPIN  13

/**
 * @brief mDNS and OTA Constants
 * @{
 */
#define HOSTNAME "kabel-" ///< Hostename. The setup function adds the Chip ID at the end.
/// @}

/**
 * @brief Default WiFi connection information.
 * @{
 */
const char* ap_default_ssid = "kabel"; ///< Default SSID.
const char* ap_default_psk = "produzni"; ///< Default PSK.
/// @}
const char* config_name = "./ssr.conf";

String current_ssid = ap_default_ssid;
String current_psk = ap_default_psk;

float humidity = 50.0, temp_c = 20.0;  // readings from sensor

int ssr_cols = 2, ssr_rows = 3; // ssr display shown as table 2x3
#define SSR_N 6
#define NORMAL 1
#define INVERTED 0

uint8_t relay_state[] = 
{
  0, 0,
  0, 0,
  0, 0,
};

uint8_t relay_logic[] = 
{
  NORMAL,   NORMAL,
  INVERTED, NORMAL,
  NORMAL,   INVERTED,
};

String message = "";
ESP8266WebServer server(80);
String webString="";     // String to display (runtime modified)

/// Uncomment the next line for verbose output over UART.
#define SERIAL_VERBOSE

// greate html table
// that displays ssr state
// in color and has submit buttons
void create_message()
{
  int n = 0;
  message = "<a href=\"login\">login</a><p/>"
            "<form action=\"/update\" method=\"get\" autocomplete=\"off\">"
            "<table>";
  String input_name = "";
  for(int y = 0; y < ssr_rows; y++)
  {
    message += "<tr>";
    for(int x = 0; x < ssr_cols; x++)
    {
      input_name = "name=\"check" + String(n) + "\"";
      message += String("<td bgcolor=\"") + String(relay_state[n] ? "#00FF00" : "#FF0000") + "\">";
      message += String("<input type=\"checkbox\" ") + input_name + String(relay_state[n] ? " checked" : "") + "> </input>";
      message += "<button type=\"submit\" name=\"button" 
               + String(n) 
               + "\" value=\"" 
               + String(relay_state[n] ? "0" : "1") 
               + "\">" // toggle when clicked 
               + String(relay_state[n] ? "ON" : "OFF") // current state
               + "</button>";
      message += "</td>";
      n++; // increment ssr number
    }
    message += "</tr>";
  }
  message += "</table>";
  message += "<button type=\"submit\" name=\"apply\" value=\"1\">Apply</button>";
  message += "<button type=\"submit\" name=\"save\" value=\"1\">Save</button>";
  message += "</form>";
}

/**
 * @brief Read WiFi connection information from file system.
 * @param ssid String pointer for storing SSID.
 * @param pass String pointer for storing PSK.
 * @return True or False.
 * 
 * The config file have to containt the WiFi SSID in the first line
 * and the WiFi PSK in the second line.
 * Line seperator can be \r\n (CR LF) \r or \n.
 */
bool loadConfig(String *ssid, String *pass)
{
  // open file for reading.
  File configFile = SPIFFS.open(config_name, "r");
  if (!configFile)
  {
    Serial.print("Failed to load ");
    Serial.println(config_name);

    return false;
  }

  // Read content from config file.
  String content = configFile.readString();
  configFile.close();
  
  content.trim();

  // Check if there is a second line available.
  int8_t pos = content.indexOf("\r\n");
  uint8_t le = 2;
  // check for linux and mac line ending.
  if (pos == -1)
  {
    le = 1;
    pos = content.indexOf("\n");
    if (pos == -1)
    {
      pos = content.indexOf("\r");
    }
  }

  // If there is no second line: Some information is missing.
  if (pos == -1)
  {
    Serial.println("Infvalid content.");
    Serial.println(content);

    return false;
  }

  // check for the third line
  // Check if there is a second line available.
  int8_t pos2 = content.indexOf("\r\n", pos + le + 1);
  uint8_t le2 = 2;
  // check for linux and mac line ending.
  if (pos2 == -1)
  {
    le2 = 1;
    pos2 = content.indexOf("\n", pos + le + 1);
    if (pos2 == -1)
    {
      pos2 = content.indexOf("\r", pos + le + 1);
    }
  }
  
  // If there is no third line: Some information is missing.
  if (pos2 == -1)
  {
    Serial.println("Infvalid content.");
    Serial.println(content);

    return false;
  }

  // Store SSID and PSK into string vars.
  *ssid = content.substring(0, pos);
  *pass = content.substring(pos + le, pos2);

  // get relay state
  String ssr_state = content.substring(pos2 + le2);
  for(int i = 0; i < ssr_state.length() && i < SSR_N; i++)
  {
    //Serial.print(ssr_state.substring(i, i+1));
    relay_state[i] = (ssr_state.substring(i,i+1) == "1" ? 1 : 0);
    //Serial.println(relay_state[i], DEC);
  }
  ssid->trim();
  pass->trim();

#ifdef SERIAL_VERBOSE
  Serial.println("----- file content -----");
  Serial.println(content);
  Serial.println("----- file content -----");
  Serial.println("ssid: " + *ssid);
  Serial.println("psk:  " + *pass);
  Serial.println("ssr:  " +  ssr_state);
#endif

  return true;
} // loadConfig


/**
 * @brief Save WiFi SSID and PSK to configuration file.
 * @param ssid SSID as string pointer.
 * @param pass PSK as string pointer,
 * @return True or False.
 */
bool saveConfig(String *ssid, String *pass)
{
  // Open config file for writing.
  File configFile = SPIFFS.open(config_name, "w");
  if (!configFile)
  {
    Serial.print("Failed to save ");
    Serial.println(config_name);

    return false;
  }

  // Save SSID and PSK.
  configFile.println(*ssid);
  configFile.println(*pass);

  for(int i = 0; i < SSR_N; i++)
    configFile.print(relay_state[i] ? "1" : "0");
  configFile.println("");

  configFile.close();
  
  return true;
} // saveConfig

// format filesystem (erase everything)
// place default password file
void format_filesystem(void)
{
  String station_ssid = ap_default_ssid;
  String station_psk = ap_default_psk;

  Serial.println("Formatting"); // erase everything
  SPIFFS.format();
  
  Serial.println("Saving factory default");
  saveConfig(&station_ssid, &station_psk);
}

/**
 * @brief Arduino setup function.
 */
void setup()
{
  String station_ssid = "";
  String station_psk = "";

  Serial.begin(115200);
  delay(100);

  Serial.println("\r\n");
  Serial.print("Chip ID: 0x");
  Serial.println(ESP.getChipId(), HEX);

  // Set Hostname.
  String hostname(HOSTNAME);
  hostname += String(ESP.getChipId(), HEX);
  WiFi.hostname(hostname);

  // Print hostname.
  Serial.println("Hostname: " + hostname);
  //Serial.println(WiFi.hostname());

  // Initialize file system.
  if (!SPIFFS.begin())
  {
    Serial.println("Failed to mount file system");
    return;
  }
  
  // Load wifi connection information.
  if (! loadConfig(&station_ssid, &station_psk))
  {
    station_ssid = "";
    station_psk = "";

    Serial.println("No WiFi connection information available.");
    format_filesystem();
    Serial.println("Trying again");
    
    if (! loadConfig(&station_ssid, &station_psk))
    {
      station_ssid = "";
      station_psk = "";

      Serial.println("Second time failed. Cannot create filesystem.");
    }
  }

  // Check WiFi connection
  // ... check mode
  if (WiFi.getMode() != WIFI_STA)
  {
    WiFi.mode(WIFI_STA);
    delay(10);
  }

  // ... Compare file config with sdk config.
  if (WiFi.SSID() != station_ssid || WiFi.psk() != station_psk)
  {
    Serial.println("WiFi config changed.");

    // ... Try to connect to WiFi station.
    WiFi.begin(station_ssid.c_str(), station_psk.c_str());

    // ... Pritn new SSID
    Serial.print("new SSID: ");
    Serial.println(WiFi.SSID());

    // ... Uncomment this for debugging output.
    //WiFi.printDiag(Serial);
  }
  else
  {
    // ... Begin with sdk config.
    WiFi.begin();
  }

  Serial.println("Wait for WiFi connection.");

  // ... Give ESP 10 seconds to connect to station.
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000)
  {
    Serial.write('.');
    //Serial.print(WiFi.status());
    delay(500);
  }
  Serial.println();

  // Check connection
  if(WiFi.status() == WL_CONNECTED)
  {
    // ... print IP Address
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("No connection to remote AP. Becoming AP itself.");
    Serial.println(ap_default_ssid);
    Serial.println(ap_default_psk);
    // Go into software AP mode.
    WiFi.mode(WIFI_AP);

    delay(10);

    WiFi.softAP(ap_default_ssid, ap_default_psk);

    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());
  }
  current_ssid = station_ssid;
  current_psk = station_psk;

  // Start OTA server.
  ArduinoOTA.setHostname((const char *)hostname.c_str());
  ArduinoOTA.begin();

  server.on("/", handle_root);
  server.on("/read", handle_read);
  server.on("/login", handle_login);
  server.on("/update", handle_update);
  create_message();
  server.begin();
  Serial.println("HTTP server started");
}

// when user requests root web page
void handle_root() {
  server.send(200, "text/html", message);
}

// when user requests /read web page
void handle_read() {
  webString = message 
            + "Temperature: " + String((int)temp_c)+"&deg;C"
            + " Humidity: "   + String((int)humidity)+"%";   // Arduino has a hard time with float to string
  server.send(200, "text/html", webString);            // send to someones browser when asked
}

void handle_login() {
  String new_ssid = "", new_psk = "";
  webString = "<form action=\"/login\" method=\"get\" autocomplete=\"off\">"
              "Access point: <input type=\"text\" name=\"ssid\"><br>"
              "Password: <input type=\"text\" name=\"psk\"><br>"
              "<button type=\"submit\" name=\"apply\" value=\"1\">Apply</button>"
              "<button type=\"submit\" name=\"cancel\" value=\"1\">Cancel</button>"
              "</form>";
  for(int i = 0; i < server.args(); i++)
  {
    if(server.argName(i) == "cancel")
    {
      loadConfig(&current_ssid, &current_psk);
      create_message();
      webString = message;
    }
    if(server.argName(i) == "apply")
    {
      for(int j = 0; j < server.args(); j++)
      {
        if(server.argName(j) == "ssid")
        {
          new_ssid = server.arg(j);
        }
        if(server.argName(j) == "psk")
        {
          new_psk = server.arg(j);
        }
      }
      if(new_ssid.length() > 0 && new_psk.length() >= 8)
      {
        //Serial.println("Save config");
        current_ssid = new_ssid;
        current_psk = new_psk;
        //saveConfig(&current_ssid, &current_psk);
        //reboot = 1;
        //loadConfig(&current_ssid, &current_psk);
        create_message();
        webString = message 
          + String("Click Save for new login:<p/>")
          + "Access point: " + current_ssid + "<br/>"
          + "Password: " + current_psk + "<p/>"
          + "Settings will be active after next power up.";
      }
    }
  }
  server.send(200, "text/html", webString);            // send to someones browser when asked
}

void handle_update() {
  // Apply or Save button
  for(int i = 0; i < server.args(); i++)
  {
    if(server.argName(i) == "apply" || server.argName(i) == "save")
    {
      // assume all are off
      for(int j = 0; j < SSR_N; j++)
        relay_state[j] = 0;
      // checkboxes on
      for(int j = 0; j < server.args(); j++)
      {
        if(server.argName(j).startsWith("check"))
        {
          int n = server.argName(j).substring(5).toInt();
          if(n >= 0 && n < SSR_N)
            if(server.arg(j) == "on")
              relay_state[n] = 1;
        }
      }
    }
    if(server.argName(i) == "save")
      saveConfig(&current_ssid, &current_psk);
  }
  // ON/OFF buttons
  for(int i = 0; i < server.args(); i++)
  {
    if(server.argName(i).startsWith("button"))
    {
      int n = server.argName(i).substring(6).toInt();
      if(n >= 0 && n < SSR_N)
        relay_state[n] = server.arg(i).toInt();
    }
  };
  create_message();
  webString = message;
  #if 0
  // some debugging print post/get messages
  webString += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  for (int i = 0; i < server.args(); i++ )
  {
    webString += " " + server.argName(i) + ": " + server.arg(i);
  };
  #endif
  server.send(200, "text/html", webString);
}

/**
 * @brief Arduino loop function.
 */
void loop()
{
  // Handle OTA server.
  ArduinoOTA.handle();
  // Handle web server
  server.handleClient();
  yield();
}
