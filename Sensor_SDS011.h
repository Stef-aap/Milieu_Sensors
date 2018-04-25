// ***********************************************************************************
// This file implements teh sensor library, as needed by Luftdaten, as a class.
// All data and functions not needed by the end user are made private.
//
// The easiest way to use the module :
//   - create an instance of the SDS011 object
//   - call on a regular base (best at least once a second) Get_Data. You'll always get the latest valid data back.
//
// Public Functions implemented :
//     _Sensor_SDS011 ( int RX, int TX ) {  // Constructor
//     String Get_Data () {                 // Get Data as JSON string
//     String Get_Version () {              
//     void Set_Parameters ( int Working_Time_msec, int Pause_Time_msec, int Sample_Time_msec, int Start_Sample_N ) {
//
// WARNING: we assume that only experts will change the parameters, 
//          therefor there's no check if the changed times are valid.
//
// instead of using Set_Paramaters to change settings, you can also directly address the parameters of this class.
//
// As I don't like redundancy, -h and -cpp files are combined and no separate definitions are made.
// ***********************************************************************************

// ***************************
// To prevent multiple imports
// ***************************
#ifndef _Sensor_SDS011_h
#define _Sensor_SDS011_h

// ***********************************************************************************
// ***********************************************************************************
// Version 0.2, 23-04-2018, SM
//    - added statistics: SD and average sloop
//    - added CSV output through serial port
//    - Start_Sample added
//
// Version 0.1, 20-04-2018, SM
//    - initial version
//    - debug_out is used as in the orginal software
// ***********************************************************************************
String _Sensor_SDS011_Version      = "0.2" ;
String _Sensor_SDS011_Version_Date = "23-04-2018" ;
String _Sensor_SDS011_Version_By   = "SM" ;
// ***********************************************************************************

// ********************************
// specific imports for this module
// ********************************
#include <Arduino.h>
#include <SoftwareSerial.h>

#include "LuftDaten.h"

// ***********************************************************************************
// The Class Name should always start with "_Sensor_" followed by the sensortype
// ***********************************************************************************
class _Sensor_SDS011 {
  
  public:
    uint16_t Device_ID       = -1 ;
    String   Device_Firmware = "unknown" ;
    int      Pause_Time_ms   = 120000 ;     // the time the sensor is set to sleep (and thus no real measurements)
    int      Working_Time_ms = 20000 ;      // the time the sensor and thus the laserdiode is active
                                            // it's important to realize that the laserdiode has 
                                            // an expected lifetime of about 8000 hours
                                            // which is just somewhat less than a year.
                                            // So if you make the pause 5 times larger than the working time,
                                            // the expected lifetime of the laser will be about 6 years.
    int      Sample_Time_ms  = 1000 ;       // The time between consecutive samples
                                            // During workingtime, samples are gathered in an array and
                                            // at the end of the workingtime, 
                                            // the mean value (and some other statistics) are calculated.
                                            // after that a new JSON string is build
    int      Start_Sample    = 1 ;          // The first few samples of the workingtime might be better ignored.
    float    PM_2_5          = 0 ;
    float    PM_10           = 0 ;
  
    // ***********************************************************************
    // The sensor uses sofware serial port, 
    //   so you have to provide the hardware pins
    // Here an instance of the sensor is created and 
    //   the software serial port is also created and started right away.
    // ***********************************************************************
    _Sensor_SDS011 ( int RX, int TX ) {
      const bool Inverted = false ;
      _serialSDS          = new SoftwareSerial  ( RX, TX, Inverted, 128 ) ;
      _serialSDS          -> begin ( 9600 ) ;
    }

    // ***********************************************************************
    // ***********************************************************************
    void Set_Parameters ( int Working_Time_msec, int Pause_Time_msec, int Sample_Time_msec, int Start_Sample_N ) {
      Pause_Time_ms   = Pause_Time_msec ;
      Working_Time_ms = Working_Time_msec ;
      Sample_Time_ms  = Sample_Time_msec ;
      Start_Sample    = Start_Sample_N ;
    }

    // ***********************************************************************
    // Get all the sampled data as a JSON string
    // ***********************************************************************
    String Get_JSON_Data () {
      return _JSON_Sample ;
    } ;
    
    // ***********************************************************************
    // Gets the data from the sensor.
    // This function schould be called in a loop, as often as possible, preferable at least each second.
    // It will gather all the data, perform some filtering and statistics on the data.
    // When it has sampled long enough, it will produce a new JSON string.
    // ***********************************************************************
    void loop () {
      unsigned long Now = millis ();
      // *******************************
      // State = 0 is the sleeping state
      // *******************************
      if ( _State == 0 ) {

        // *****************************************************
        // test if pause time is passed, go to the working state
        // *****************************************************
        if ( ( Now - _Last_Get_Data ) > Pause_Time_ms ){
          _Last_Get_Data += Pause_Time_ms ;
          _State = 1 ;

          // ***************************************************************************
          // this command starts the sensor (i.e. the laserdiode and the ventilator) and
          // while the sensor is working, it will send continuously the measured data
          // ***************************************************************************
          uint8_t Start_cmd[] = {0xAA, 0xB4, 0x06, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x06, 0xAB};
          _serialSDS -> write ( Start_cmd, sizeof ( Start_cmd ) ); 
        }
      }
      else {
        // ***************************************************************
        // This is the working state
        // Test if sample time has passed, and if so, 
        // take a sample and test if this is the end of the working period
        // ***************************************************************
        if ( ( Now - _Last_Sample_Time ) > Sample_Time_ms ){
          _Last_Sample_Time += Sample_Time_ms ;

          // ************* 
          // take a sample
          // ************* 
          _Get_Response ( 0x04 );
          PM_2_5 = 256 * _Data[1] + _Data[0] ;
          PM_10  = 256 * _Data[3] + _Data[2] ;
          //Print_CMD ( _Data, _Data_Len ) ;

          // ****************************
          // Store sample in sample array
          // ****************************
          _Sample_Array_PM_2_5 [ _N_Sample ] = PM_2_5 ;
          _Sample_Array_PM_10  [ _N_Sample ] = PM_10  ;
          _N_Sample += 1 ;
  
          // ***************************************************
          // test of the working period has passed
          // and if so, do calculation and go to the pause state
          // ***************************************************
          if ( ( Now - _Last_Get_Data ) > Working_Time_ms  ){
            //_Last_Get_Data += Working_Time_ms ;   
            _Last_Get_Data = Now ;
            _State = 0 ;
            int N_Sample = _N_Sample - Start_Sample ;

            // ********************** 
            // stop the SDS011 sensor
            // ********************** 
            uint8_t Stop_cmd[]  = {0xAA, 0xB4, 0x06, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x05, 0xAB};
            _serialSDS -> write ( Stop_cmd, sizeof ( Stop_cmd ) ); 

            // **********************************************
            // if too few samples, ignore this working period
            // **********************************************
            if ( _N_Sample < ( Start_Sample + 1 ) ) {
              _N_Sample = 0 ;
            }

            // ****************************
            // build new _JSON_String and
            // calculate statistics
            // ****************************
            int Max_PM_2_5 = 0 ;
            int Min_PM_2_5 = 1000000 ;
            int Sum_PM_2_5 = 0 ;
            int Max_PM_10  = 0 ;
            int Min_PM_10  = 1000000 ;
            int Sum_PM_10  = 0 ;

            // *************************************************************
            // the first few samples, indicated by Start_Sample, are ignored
            // calculate mean, min, max
            // *************************************************************
            for ( int i = Start_Sample; i < _N_Sample; i++ ) {
              //Max_PM_2_5 = max ( Max_PM_2_5, _Sample_Array_PM_2_5 [i] ) ;   DOESNT WORK HERE ???
              //Min_PM_2_5 = min ( Min_PM_2_5, _Sample_Array_PM_2_5 [i] ) ;
              if ( _Sample_Array_PM_2_5 [i] > Max_PM_2_5 ) { Max_PM_2_5 = _Sample_Array_PM_2_5 [i] ; }
              if ( _Sample_Array_PM_2_5 [i] < Min_PM_2_5 ) { Min_PM_2_5 = _Sample_Array_PM_2_5 [i] ; }
              Sum_PM_2_5 += _Sample_Array_PM_2_5 [i] ;
              
              //max_PM_10  = max ( max_PM_10, _Sample_Array_PM_10 [i] ) ;
              //min_PM_10  = min ( min_PM_10, _Sample_Array_PM_10 [i] ) ;
              if ( _Sample_Array_PM_10 [i] > Max_PM_10 ) { Max_PM_10 = _Sample_Array_PM_10 [i] ; }
              if ( _Sample_Array_PM_10 [i] < Min_PM_10 ) { Min_PM_10 = _Sample_Array_PM_10 [i] ; }
              Sum_PM_10  += _Sample_Array_PM_10 [i] ;
            }

            // ******************************************************
            // now we have mean, we can calulate the linear regression
            //    y = B0 + B1 * x
            //    B1 = SUM ( ( x[i] - MEAN(x) ) * ( y[i] - MEAN(y) ) )
            //    B0 = MEAN(y) - ( B1 * MEAN(x) )
            //    B0, we don't need it, because it's directly correlated with the mean of the signal
            // and the standaard deviation
            //    SD = SQRT ( VARIANCE )
            //    VARIANCE = SUM ( ( y[i] - MEAN(Y) ) ^ 2 )
            // and ....  ?
            // ******************************************************
            //float B0_PM_2_5 ;
            float B1_PM_2_5      = 0 ;
            float SD_PM_2_5      = 0 ;
            float Mean_x_PM_2_5  = N_Sample / 2 ;
            float Mean_y_PM_2_5  = 0.1 * Sum_PM_2_5 / N_Sample ;
            float Sum_dx2_PM_2_5 = 0 ;

            float B1_PM_10      = 0 ;
            float SD_PM_10      = 0 ;
            float Mean_x_PM_10  = N_Sample / 2 ;
            float Mean_y_PM_10  = 0.1 * Sum_PM_10 / N_Sample ;
            float Sum_dx2_PM_10 = 0 ;

            for ( int i = Start_Sample; i < _N_Sample; i++ ) {
              B1_PM_2_5      +=    ( i - Start_Sample - Mean_x_PM_2_5 ) * ( 0.1 * _Sample_Array_PM_2_5 [i] - Mean_y_PM_2_5 ); 
              Sum_dx2_PM_2_5 += sq ( i - Start_Sample - Mean_x_PM_2_5 ) ;
              SD_PM_2_5      += sq ( 0.1 * _Sample_Array_PM_2_5 [i] - Mean_y_PM_2_5 ) ;

              B1_PM_10       +=    ( i - Start_Sample - Mean_x_PM_10 ) * ( 0.1 * _Sample_Array_PM_10 [i] - Mean_y_PM_10 ); 
              Sum_dx2_PM_10  += sq ( i - Start_Sample - Mean_x_PM_10 ) ;
              SD_PM_10       += sq ( 0.1 * _Sample_Array_PM_10 [i] - Mean_y_PM_10 ) ;
            }
            B1_PM_2_5 /= Sum_dx2_PM_2_5 ;
            //B0_PM_2_5 = Mean_y_PM_2_5 - ( B1_PM_2_5 * Mean_x_PM_2_5 ) ;
            SD_PM_2_5 /=  N_Sample ;
            SD_PM_2_5  = sqrt ( SD_PM_2_5 ) ;
            
            B1_PM_10 /= Sum_dx2_PM_10 ;
            SD_PM_10 /= N_Sample ;
            SD_PM_10  = sqrt ( SD_PM_10 ) ;

            // **********************************************************************
            // print a tab delimited string containing all relevant values
            // you can use this data from a serial monitor and use this as a csv file
            // **********************************************************************
            sprintf ( msg, "%d\t%d\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f", 
                           Now, N_Sample, 
                           Mean_y_PM_2_5, Mean_y_PM_10, 
                           SD_PM_2_5, SD_PM_10, 
                           0.1 * Min_PM_2_5, 0.1 * Max_PM_2_5, 
                           0.1 * Min_PM_10, 0.1 * Max_PM_10, 
                           B1_PM_2_5, B1_PM_10 ) ;
            debug_out ( msg, DEBUG_WARNING, true );
            
            // *************************************************
            // and here the JSON string to send to all web api's
            // *************************************************
            sprintf ( msg, "{\"value_type\":\"SDS_P1\",\"value\":\"%.2f\"},{\"value_type\":\"SDS_P2\",\"value\":\"%.2f\"},",
                       Mean_y_PM_10, Mean_y_PM_2_5 ) ;
            _JSON_Sample = msg ;

            // ****************************************
            // Don't forget to reset the buffer pointer
            // ****************************************
            _N_Sample = 0;
          }
        }
      }
    }
    
    // ***********************************************************************
    // Get version of device-firmware and the unique device-ID 
    // these values ar stored in Device_Firmware and Device_ID
    // The function itself returns a string containing both in a readable way.
    // ***********************************************************************
    String Get_Version () {
      uint8_t Version_Cmd[] = {0xAA, 0xB4, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x05, 0xAB};

      // *******************************
      // The next delay is necessary to 
      // let this function work correctly if it is called from the setup part.
      // *******************************
      delay ( 500 ) ;
      
      _serialSDS -> write ( Version_Cmd, sizeof ( Version_Cmd )); 
    
      // *************************************************
      // met een te kleine delay (0, 10, 50, 70) lukt het niet
      //   80 doet het meestal wel
      // ===>>>  100 msec doet het zeker
      // grote delays (maar wat is de zin hiervan) 
      //     doen het ook (uitgeprobeerd tot 2000)
      // *************************************************
      delay ( 100 ) ;
      _Get_Response ( Version_Cmd[2] );

      _Print_CMD ( _Data, _Data_Len ) ;

      Device_Firmware = "20" ;
      Device_Firmware += String ( _Data[1], 10 ) + "." ;
      Device_Firmware += String ( _Data[2], 10 ) + "." ;
      Device_Firmware += String ( _Data[3], 10 ) ;

      Device_ID = 256 * _Data[4] + _Data[5] ;

      sprintf ( msg, "Device-Firmware = %s    Device-ID = %s", Device_Firmware.c_str(),
                                                               String ( Device_ID, HEX ).c_str() ) ;
      debug_out ( msg, DEBUG_MAX_INFO, true );
      return msg ;
    }


  // ***********************************************************************
  private:
  // ***********************************************************************
    uint8_t       _Data [ 20 ] ;
    uint16_t      _Sample_Array_PM_2_5 [ 1000 ] ;
    uint16_t      _Sample_Array_PM_10  [ 1000 ] ;
    int           _N_Sample         = 0 ;
    int           _Data_Len         = 0 ;
    int           _State            = 1 ;
    unsigned long _Last_Get_Data    = 0 ;
    unsigned long _Last_Sample_Time = 0 ;
    String        _JSON_Sample      = "" ;

    // *****************************************************
    // Because Software Serial is a of abstract type Stream
    //   We have to declare it as pointer
    // *****************************************************
    SoftwareSerial *_serialSDS ;
    // ***********************************************************************

    // ***********************************************************************
    // Print een character buffer als hex characters, if MEDIUM debug info
    // ***********************************************************************
    void _Print_CMD ( uint8_t *Cmd, uint8_t Len ) {
      sprintf ( msg, "\n===  CMD  ===================   %d", Len );
      debug_out ( msg, DEBUG_MED_INFO, true );
      for ( int i = 0; i < Len ; i++ ){
        sprintf ( msg, "%#04x ", Cmd[i] );
        debug_out ( msg, DEBUG_MED_INFO, false );
      }
      debug_out ( "\n---  CMD end  ---------------", DEBUG_MED_INFO, true  ) ;
    }


    // ***********************************************************************
    // All responses have the same length
    //   and are roughly build in the same way.
    //   So we can use the same function for alle responses.
    //
    // Code    = comand-ID, which is reflected in third byte of the response 
    //           (except when quering the data)
    // Respons = the second and third byte of teh response
    //
    // Code   Respons   Meaning
    //  02    C5, 02    Set the work-mode Passive = query / active
    //  04    C0        Query the sensor data
    //  05    C5, 05    Set Device-ID
    //  06    C5, 06    Start / Stop the sensor ( laser + ventilator)
    //  07    C5, 07    Get Firmware and Device-ID
    //  08    C5, 08    Set working period
    // ***********************************************************************
    void _Get_Response (  uint8_t Cmd2 ) {
      int    CheckSum   = 0 ;
      int    Answer_Len = 0 ;
    
      while ( _serialSDS->available () > 0 ) {
        char kar = _serialSDS->read() ;
        
        // ***************************************
        // DEBUG INFO if MAX info
        // ***************************************
        if ( Answer_Len > -1 ) { 
          sprintf ( msg, "#%d %s  ", Answer_Len + 1, String ( kar, HEX ).c_str() ) ;
          debug_out ( msg, DEBUG_MAX_INFO, false );
        }

        // ***************************************
        // ***************************************
        switch ( Answer_Len ) {
          case 0 :
            if ( kar == 0xAA ) {
              Answer_Len += 1 ;
            }
            break ;
    
          case 1 :
            if ((( Cmd2 == 0x04 ) && ( kar == 0xC0 )) || (( Cmd2 != 0x04 ) && ( kar == 0xC5 ) ) ) {
              Answer_Len += 1 ;
            }
            else {
              Answer_Len = 0 ;
            }
            break ;
            
          case 2 :
            // *******************************************
            // for all commands, except the data query,
            //   the command ID is reflected in the answer
            //   so check it
            // Because the data is send autonomuously by the sensor
            //    this will happen often ( so not realy an error)
            //    thus we only give a message if MAX info
            // *******************************************
            if  ( ( Cmd2 != 0x04 ) && ( Cmd2 != kar )) {
              debug_out ( "\nUndesired Answer", DEBUG_MAX_INFO, true );
              Answer_Len = 0 ;
              break ;
            }
            // ****************************************
            // checksum is calculated over bytes 2 .. 8
            // ****************************************
            Answer_Len += 1 ;
            CheckSum = kar ;
            break;
            
          case 8 : 
            if ( kar == ( CheckSum % 256 ) ) {
              // ***************************************
              // DEBUG INFO if MAX info
              // ***************************************
              debug_out ( "\nChecksum OK", DEBUG_MAX_INFO, true );
              Answer_Len += 1 ;
            }
            // ********************************************
            // Als Checksum fout, helemaal opnieuw beginnen
            // ********************************************
            else {
              Answer_Len = 0 ;
            }
            break ;
          
          case 9 :
            if ( kar == 0xAB ) {
              _Data_Len = Answer_Len - 2 ;
              return ;
            }
            else {
              sprintf ( msg, " LAST BYTE WRONG#%d %s  ", Answer_Len + 1, String ( kar, HEX ).c_str() ) ;
              debug_out ( msg, DEBUG_MIN_INFO, false );
              Answer_Len = 0 ;
            }
            break;
            
          default :
            Answer_Len += 1 ;
            CheckSum += kar ;
        }
        if ( Answer_Len >= 3 ) {
              _Data [ Answer_Len - 3 ] = kar ;
            }
    
        // ***************************************
        // DEBUG INFO if MAX info
        // ***************************************
        if ( Answer_Len < 3 ) {
          debug_out ( ".", DEBUG_MAX_INFO, false );
        }
      }
      // *****************************************************
      // No more data from sensor, and no good answer received
      // *****************************************************
      _Data_Len = 0 ;
    }

};


#endif
