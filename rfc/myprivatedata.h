//--------------------------------------------------
//! \file		myprivatedata.h
//! \brief		header file for private data as GPS coordinate, phone number and wifi credentials
//! \date		2015-Mar
//! \author		minbiocabanon
//--------------------------------------------------

//----------------------------------------------------------------------
//!\brief	These are DEFAULTS parameters !
//----------------------------------------------------------------------

// Alarm allowed ?
#define FLAG_ALARM_LOW_BAT			0	// 1 = send alarm when low voltage, set TRIG_INPUT_LEVEL to define treshol 	; 0 = no check
#define FLAG_ALARM_FLOOD			0	// 1 = send alarm if flood sensor detects water ;  0 = don't care about flooding

#define TRIG_INPUT_LEVEL			11.6	// in volt, when input voltage is lower than this value, an SMS alarm will be sent
					// 11.6V is a good level trig for 12V lead acid battery. Set lower voltage at your own risk !
					// 23.2V is a good level trig for 24V lead acid battery. Set lower voltage at your own risk !

// Params for flood sensor (analog)
#define FLOODSENSOR_TRIG		500.0	// Trigger value on raw data (ADC) on direct ready from flood sensor
										// Dry sensor : 0-10
										// Sensor touched with finger : 100-300
										// wet sensor : ~700-800


// Phone number to call or for SMS
#define MYPHONENUMBER	"+33600000000"		// Default phone number where to send messages

// SMS Menu
#define SMSSECRET	"1234"					// Default secret code to activate menu
