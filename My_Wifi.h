
// *********************************************************************************************
/* This is not meant a s library,
 * but just some difficult parts that are always identical
 * so now evry program can benifit from small improvements.
 */
// *********************************************************************************************


extern "C" {
#include "user_interface.h"      //system_get_sdk_version()
}


// *********************************************************************************************
// prototype
// *********************************************************************************************
void Wifi_Connect () ;
void Wifi_Connect ( uint8_t* My_IP_Address ) ;

// *********************************************************************************************
// *********************************************************************************************
void Wifi_Connect () {
  uint8_t Unknown_IP_Address [] = { 0, 0, 0, 0 } ;
  Wifi_Connect ( Unknown_IP_Address ) ;
}

// *********************************************************************************************
// *********************************************************************************************
void Wifi_Connect ( uint8_t* My_IP_Address ) { 
  //******************************************************
  //******************************************************
  #define WIFI_TX_POWER 82
  system_phy_set_max_tpw ( WIFI_TX_POWER );   // set TX power [ 0..82 ], 0.25 dBm per step
  //wifi_set_phy_mode ( PHY_MODE_11N ) ;      // alleen N werkt hier !!
  wifi_set_opmode ( 0x01 ) ;                  // 1= Station,  2=SoftAP,  3=Station + SoftAP

  //******************************************************
  //******************************************************
  if ( My_IP_Address[0] > 0 ) {
    IPAddress ip      ( My_IP_Address ) ;       // desired IP address
    IPAddress gateway ( 192, 168, 0,   1 ) ;    // IP address of the router
    IPAddress subnet  ( 255, 255, 255, 0 ) ;
    WiFi.config ( ip, gateway, subnet ) ;
  }

  //******************************************************
  //******************************************************
  int Connect_Count = 200 ;
  WiFi.begin ( Wifi_User, Wifi_Pwd ) ;
  while ( ( WiFi.status() != WL_CONNECTED ) && ( Connect_Count > 0 ) ) {
    delay ( 100 ) ;
    Connect_Count -= 1 ;
    if ( Verbose > 1 ) {
      Serial.print ( "." ) ;
    }
  }

  //******************************************************
  //******************************************************
  if ( WiFi.status() == WL_CONNECTED ) {
    if ( Verbose > 3 ) {
      Serial.println () ;
      Serial.print   ( "IP          = " ) ;
      Serial.println ( WiFi.localIP() ) ;
      //Serial.print   ( "Subnet      = " ) ;
      //Serial.println ( WiFi.subnetMask());
      Serial.print   ( "Gateway     = " ) ;
      Serial.println ( WiFi.gatewayIP() ) ;
      Serial.print   ( "SDK version = " ) ;
      Serial.println ( system_get_sdk_version() ) ;
    }
    client.setServer ( Broker_IP, Broker_Port ) ;
  }
}


// *********************************************************************************************
// *********************************************************************************************
void MQTT_Connect () {

  int MQTT_Count = 25;

  while ( !client.connected() && ( MQTT_Count > 0 ) ) {

    if ( client.connect ( MQTT_ID.c_str(), MQTT_User, MQTT_Pwd, 
                          Subscription_Out.c_str(), 1, 1, LWT.c_str() )) {
      // resubscribe, LET OP: MQTTQOS1 lijkt de zaak op te hangen
      client.subscribe ( Subscription.c_str(), MQTTQOS0 ) ;
      // and publish ALIVE
      //client.publish ( Subscription_Out.c_str(), ALIVE.c_str() );
      Serial.println ( "MQTT subscribed" );
    } 
    else {
      if ( Verbose > 0 ) {
        Serial.print   ( "failed, rc = " ) ;
        Serial.println ( client.state() ) ;
      }
      // Wait 0.1 seconds before retrying
      delay ( 100 ) ;
      MQTT_Count -= 1 ;
    }
  }
  if ( client.connected () ) {
    client.loop ();
  }

}


