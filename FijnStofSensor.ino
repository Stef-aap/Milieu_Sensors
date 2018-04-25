
// ***********************************************************************************
// ***********************************************************************************
// ToDo :
//    - millis counters are unsigned long, for which yields
//         4294967295/(1000*60*60*24) = 49.7103 dagen
//         so after about 50 days, none of the loops will be entered anymore
//    - IP-address (or at least the last part), store it in RTC memory (inifile is also possible)
//    - as soon as a second sensor is added, dynamically select available sensors
//    - support of other API's
//    - Web Interface
//    - store/recover all settings via inifile
//    - min/max bug uitzoeken ("not definied in this scope" ??? )
//    - merge of verbose and debug_out
//    - reprogram the device through wifi: #include <ArduinoOTA.h>
//
// Version 0.1, 23-04-2018, SM, checked by RM
//    - initial version
//    - only supports SDS011, with outputs to Luftdaten, Madavi, MQTT
// ***********************************************************************************
String _FijnStofSensor_Version      = "0.1" ;
String _FijnStofSensor_Version_Date = "25-04-2018" ;
String _FijnStofSensor_Version_By   = "SM" ;
// ***********************************************************************************

#include "ext_def.h"
#include <ESP8266WiFi.h>
#include "PubSubClient.h"
#include "Wifi_Settings.h"

// ********************************
// during initialisation phase
// we want a maximum of information
// ********************************
int debug = DEBUG_MAX_INFO ;

// ***********************************************
// espid is needed as login name for the web-api's
// ***********************************************
String esp_chipid ;

#include "LuftDaten.h"

// *************************************************************************
//  GLOBALS
// *************************************************************************
// General
const int  Verbose   = 10 ;   // should be replaced by debug_out

//  Wifi 
//uint8_t My_IP_Address [] = {0,0,0,0} ;       // to find out the correct IP
uint8_t My_IP_Address [] = {192, 168,0,10} ;       // LOLIN + SDS011 semsor
//uint8_t My_IP_Address [] = {192, 168,0,30} ;       // WEMOS

// MQTT
String MQTT_ID          = "FijnStof_2" ;
String Subscription     = "prefix/" + MQTT_ID ;
String Subscription_Out = Subscription + "_" ;          
String LWT              = "\"$$Dead " + MQTT_ID + "\"" ;
//String ALIVE            = "\"$$Alive " + MQTT_ID + "\"" ;
String Version          = "Fijnstof V 0.1" ;

// *************************************************************************
//  GLOBALS
// *************************************************************************
WiFiClient espClient ;
PubSubClient client ( espClient ) ;

#include "My_Wifi.h" ;

char msg[1000] ;



// ************************************************
// Create an instance of the SDS011 particle sensor
// This class requires a soft serial port, 
//   so you have to specify the hardware pins
// ************************************************
const int  RX = D1 ;  
const int  TX = D2 ;
#include "Sensor_SDS011.h"
_Sensor_SDS011 Sensor_1 ( RX, TX ) ;


// ***********************************************************************
//  Initialisation
// ***********************************************************************
void setup() {
  
  // ***********************************************************************************
  // Open hardware serial communication port (USB connection) and wait for port to open:
  // ***********************************************************************************
  Serial.begin ( 115200 ) ;
  while ( !Serial ) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  //******************************************************
  // Connect to MQTT broker
  //******************************************************
  Wifi_Connect ( My_IP_Address ) ;
  if ( WiFi.status() == WL_CONNECTED ) {
    MQTT_Connect () ;
  }

  // *************************
  // get version, what for ???
  // *************************
  String Version_ID = Sensor_1.Get_Version ();
  //Serial.println ( Version_ID ) ;

  // ***************************************************
  // get chipid, needed for sending data to web api's
  // maybe this should be done in the sensor library ???
  // ***************************************************
  esp_chipid = String ( ESP.getChipId() ) ;

  // **************************************************
  // print header fro the CSV output on the serial port
  // **************************************************
  sprintf ( msg, "\n\nMillis\tN\tPM2.5\tPM10\tSD_2.5\tSD_10\tmin2_5\tmax2_5\tmin_10\tmax_10\tB1_2.5\tB1_10" ) ;
  debug_out ( msg, DEBUG_WARNING, true );
}


// ***********************************************************************
//  Global Parameters for the main loop
// ***********************************************************************
unsigned long Send_Last_Time     = 0 ;
int           Send_Sample_Period = 150000 ;

unsigned long Sample_Count = 0 ;


// ***********************************************************************
//  Main loop
// ***********************************************************************
void loop() {

  // ************************************************
  // get the current time and update the loop counter
  // ************************************************
  unsigned long Now = millis() ;
  Sample_Count     += 1 ;

  // *********************************************
  //  Ensure we've low info during continuous loop
  // *********************************************
  debug = DEBUG_WARNING ;

  // **************************************************
  // These are the most important calls:
  // Let all the sensor modules do their work 
  // should preferable be called at least once a second
  // **************************************************
  Sensor_1.loop () ;

  // ***************************************************
  // Test if it's time to send new data to all the api's
  // ***************************************************
  if ( ( Now - Send_Last_Time ) > Send_Sample_Period ) {
    Send_Last_Time += Send_Sample_Period ; 

    String SDS = Sensor_1.Get_JSON_Data () ;

    // ********************************************
    // Luftdaten
    // ********************************************
    sendLuftdaten ( SDS, SDS_API_PIN, host_dusti, httpPort_dusti, url_dusti, "SDS_");

    // ********************************************************
    // for all other API's we need to construct a total message
    // ********************************************************
    String data ;
    String data_sample_times ;
    int    signal_strength = WiFi.RSSI();

    // **********************************************
    // don't know what the function of micros is ????
    // **********************************************
    data_sample_times  = Value2Json("samples", String(  long ( Sample_Count )));
    //data_sample_times += Value2Json("min_micro", String(long(min_micro)));
    //data_sample_times += Value2Json("max_micro", String(long(max_micro)));
    data_sample_times += Value2Json ( "signal", String(signal_strength) );
    
    data = data_first_part;
    data += SDS ;
    data += data_sample_times;

    // **********************
    // remove the final comma
    // **********************
    //if ( data [ data.length() - 1 ] == "," ) {   //ERROR: ISO C++ forbids comparison between pointer and integer [-fpermissive]
    if ( data.lastIndexOf (',') == ( data.length() - 1 ) ) {
      data.remove(data.length() - 1);
    }
    data += "]}";
    
    // ********************************************
    // MADAVI
    // ********************************************
    sendData(data, 0, host_madavi, httpPort_madavi, url_madavi, "", FPSTR(TXT_CONTENT_TYPE_JSON));

    // **********************
    // send it to MQTT broker
    // **********************
    sprintf ( msg, "[%s,\"%s\",%d,%d]", \
                   SDS.c_str(), \
                   Version.c_str(), \
                   WiFi.RSSI(), ESP.getFreeHeap() ) ;
    // ********************************************************************               
    // MQTT connection will sometimes get lost, so if necessairy, reconnect
    // ********************************************************************               
    if ( ! client.connected() ) {
      MQTT_Connect () ;
    }
    if ( client.connected() ) {
      client.publish ( Subscription_Out.c_str(), msg ) ;
    }
  }
}

