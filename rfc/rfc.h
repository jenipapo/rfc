//--------------------------------------------------
//! \file		pepettebox.h
//! \brief		header file for structures and enumerate
//! \date		2015-Mar
//! \author		minbiocabanon
//--------------------------------------------------

// Analog input
#define NB_SAMPLE_ANALOG		16
#define VCC_5V					4.97
#define VOLT_DIVIDER_INPUT		10.54123	// Voltage divider ratio for mesuring input voltage. 
#define MIN_DC_IN				9			// Minimum input voltage
#define MAX_DC_IN				36			// Max input voltage


// Lipo
// battery level trigger for alarm , in % , WARNING, LIPO level are only 100,66 and 33%
// Do not use value < 33% because linkitone will not give you another value until 0% ...
#define LIPO_LEVEL_TRIG	33	// in % , values available 33 - 66 (0 and exlucded logically)

// flood sensor
#define FLOODSENSOR  			8			// Analog input where flood sensor is connected
#define FLOODSENSOR_ACTIVE		0			// 0 or 1 ,Set level when flood sensor is active (water detected)
#define MIN_FLOOR				0			// Minimum value for flood sensor trigger
#define MAX_FLOOR				1000		// Maximum value for flood sensor trigger



//----------------------------------------------------------------------
//!\brief	Structure where user parameters are stored in EEPROM (tuned by SMS)
//!\brief	DO NOT MODIFY THIS !
//----------------------------------------------------------------------
struct EEPROM_param {
	char smssecret[5];
	bool flag_data_written;				// when true, this structure contains data. Should be false only at the very first start
	bool flag_alarm_low_bat;			// 1 = send SMS alarm when low voltage detected at input voltage (can be an external batt.) ; 0 = do not check input voltage
	bool flag_alarm_flood;				// 1 = send alarm when water detected ; 0 = don't care about water
	char myphonenumber[13];				// Default phone number where to send messages
	unsigned int lipo_level_trig;		// battery level, when trigged, should send an alarm
	float trig_input_level;				// trigger alarm for low level input 
	float flood_sensor_trig;				// trigger alarm for low level input 
}MyParam;

//----------------------------------------------------------------------
//!\brief	Other structures used in programm
//----------------------------------------------------------------------
struct Battery {
	unsigned int LiPo_level;
	unsigned int charging_status;
	}MyBattery;
	
struct AnalogInput {
	double raw;
	double analog_voltage;
	double input_voltage;
	}MyExternalSupply;	

struct DigitalSensor {
	unsigned int value;
	double raw_divider; 
	double analog_voltage_divider;
	double raw; 
	double analog_voltage;
	}MyFloodSensor;	
	
struct SMS {
	char message[256];
	char incomingnumber[13];
	int menupos;
	int menulevel;
	}MySMS;
	
struct FlagReg {
	bool taskGetLiPo;	// flag to indicate that we have to get battery level and charging status
	bool taskGetAnalog;	// flag to indicate that we have to read analog input of external supply
	bool taskCheckSMS;	// flag to indicate when check SMS
	bool taskCheckFlood;// flag to indicate when check Flood sensor
	bool SMSReceived;	// flag to indicate that an SMS has been received
	bool taskCheckInputVoltage;	// flag to indicate when do an input voltage check
	bool taskCheckFW;	// flag to indicate when it's time to check if is FW hour check !
	bool ForceFWUpdate; // flag to indicate that a manual force update is asked by SMS		
	}MyFlag;

enum SMSMENU{
	SM_NOPE,		//0
	SM_LOGIN,		//1
	SM_MENU_MAIN,	//2
	SM_CHG_NUM,		//3
	SM_CHG_SECRET,	//4
	SM_RESTORE_DFLT, //5
	SM_CHG_LOWPOW_TRIG, 	//6
	SM_CHG_FLOODSENSORTRIG,	//7
	};

// CMD must have same number as SMS menu number
enum CMDSMS{
	CMD_EXIT,			//0
	CMD_CHFG_ON,		//1
	CMD_CHFG_OFF,		//2
	CMD_PARAMS,			//3
	CMD_CHG_NUM,		//4
	CMD_CHG_SECRET,		//5
	CMD_LOWPOWER_ON,	//6
	CMD_LOWPOWER_OFF,	//7
	CMD_CHG_LOWPOW_TRIG,//8
	CMD_CHG_FLOOD_TRIG,	//9
	CMD_UPDATE_FW,		//10
	CMD_RESTORE_DFLT	//11
	};	