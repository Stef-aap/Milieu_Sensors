#ifndef LuftDaten_h
#define LuftDaten_h

#include <Arduino.h>

#include "html-content.h"

#define SOFTWARE_VERSION "NRZ-2017-100"

//String data_first_part = "{\"software_version\": \"" + String(SOFTWARE_VERSION) + "\"FEATHERCHIPID, \"sensordatavalues\":[";
//data_first_part.replace("FEATHERCHIPID", "");
String data_first_part = "{\"software_version\": \"" + String(SOFTWARE_VERSION) + "\", \"sensordatavalues\":[";



const char* host_madavi = "api-rrd.madavi.de";
const char* url_madavi = "/data.php";
int httpPort_madavi = 443;
const char* update_host = "www.madavi.de";
const char* update_url = "/sensor/update/firmware.php";
const int update_port = 80;

const char* host_dusti = "api.luftdaten.info";
const char* url_dusti = "/v1/push-sensor-data/";
int httpPort_dusti = 443;


/*****************************************************************
/* convert value to json string                                  *
/*****************************************************************/
String Value2Json(const String& type, const String& value) {
  String s = F("{\"value_type\":\"{t}\",\"value\":\"{v}\"},");
  s.replace("{t}", type); s.replace("{v}", value);
  return s;
}

/*****************************************************************
/* Debug output                                                  *
/*****************************************************************/
void debug_out(const String& text, const int level, const bool linebreak) {
  if (level <= debug) {
    if (linebreak) {
      Serial.println(text);
    } else {
      Serial.print(text);
    }
  }
}

/*****************************************************************
/* send data to rest api                                         *
/*****************************************************************/
void sendData(const String& data, const int pin, const char* host, const int httpPort, const char* url, const char* basic_auth_string, const String& contentType) {
#if defined(ESP8266)

  debug_out(F("Start connecting to "), DEBUG_MIN_INFO, 0);
  debug_out(host, DEBUG_MIN_INFO, 1);

  String request_head = F("POST "); request_head += String(url); request_head += F(" HTTP/1.1\r\n");
  request_head += F("Host: "); request_head += String(host) + "\r\n";
  request_head += F("Content-Type: "); request_head += contentType + "\r\n";
  if (basic_auth_string != "") { request_head += F("Authorization: Basic "); request_head += String(basic_auth_string) + "\r\n";}
  request_head += F("X-PIN: "); request_head += String(pin) + "\r\n";
  request_head += F("X-Sensor: esp8266-"); request_head += esp_chipid + "\r\n";
  request_head += F("Content-Length: "); request_head += String(data.length(), DEC) + "\r\n";
  request_head += F("Connection: close\r\n\r\n");

  // Use WiFiClient class to create TCP connections

  if (httpPort == 443) {

    WiFiClientSecure client_s;

    client_s.setNoDelay(true);
    client_s.setTimeout(20000);

    if (!client_s.connect(host, httpPort)) {
      debug_out(F("connection failed"), DEBUG_ERROR, 1);
      return;
    }

    debug_out(F("Requesting URL: "), DEBUG_MIN_INFO, 0);
    debug_out(url, DEBUG_MIN_INFO, 1);
    debug_out(esp_chipid, DEBUG_MIN_INFO, 1);
    debug_out(data, DEBUG_MIN_INFO, 1);

    // send request to the server

    client_s.print(request_head);

    client_s.println(data);

    delay(10);

    // Read reply from server and print them
    while(client_s.available()) {
      char c = client_s.read();
      debug_out(String(c), DEBUG_MAX_INFO, 0);
    }

    debug_out(F("\nclosing connection\n------\n\n"), DEBUG_MIN_INFO, 1);

  } else {

    WiFiClient client;

    client.setNoDelay(true);
    client.setTimeout(20000);

    if (!client.connect(host, httpPort)) {
      debug_out(F("connection failed"), DEBUG_ERROR, 1);
      return;
    }

    debug_out(F("Requesting URL: "), DEBUG_MIN_INFO, 0);
    debug_out(url, DEBUG_MIN_INFO, 1);
    debug_out(esp_chipid, DEBUG_MIN_INFO, 1);
    debug_out(data, DEBUG_MIN_INFO, 1);

    client.print(request_head);

    client.println(data);

    delay(10);

    // Read reply from server and print them
    while(client.available()) {
      char c = client.read();
      debug_out(String(c), DEBUG_MAX_INFO, 0);
    }

    debug_out(F("\nclosing connection\n------\n\n"), DEBUG_MIN_INFO, 1);

  }

  debug_out(F("End connecting to "), DEBUG_MIN_INFO, 0);
  debug_out(host, DEBUG_MIN_INFO, 1);

  wdt_reset(); // nodemcu is alive
  yield();
#endif
}


/*****************************************************************
/* send single sensor data to luftdaten.info api                 *
/*****************************************************************/
void sendLuftdaten(const String& data, const int pin, const char* host, const int httpPort, const char* url, const char* replace_str) {
  String data_4_dusti = "";
  data_4_dusti  = data_first_part + data;
  data_4_dusti.remove(data_4_dusti.length() - 1);
  data_4_dusti.replace(replace_str, "");
  data_4_dusti += "]}";
  if (data != "") {
    sendData(data_4_dusti, pin, host, httpPort, url, "", FPSTR(TXT_CONTENT_TYPE_JSON));
  } else {
    debug_out(F("No data sent..."), DEBUG_MIN_INFO, 1);
  }
}




#endif
