//--------------------------------------------------
//! \file		robomowtrak.ino
//! \brief		Full configurable by SMS.
//! \brief		SMS alert only.
//! \brief		Read voltage input and can set an alarm on low power.
//! \brief		Monitor LiPo cell voltage and can set an alarm on low power.
//! \brief		Serial message are for debug purpose only.
//! \brief		Over The Air by GPRS sketche update
//! \brief		NOT USED YET : Wifi for tracking/logging position while in my garden
//! \date		2015-May
//! \author		minbiocabanon
//--------------------------------------------------


//--------------------------------------------------
//! some notes here :
//! google url to send by sms : https://www.google.com/maps?q=<lat>,<lon>
//! millis() overflow/reset over 50 days
//--------------------------------------------------
#include <LTask.h>
#include <vmthread.h>
#include <stddef.h>
#include <LGPS.h>
#include <LGSM.h>
#include <LBattery.h>
#include <math.h>
#include <LEEPROM.h>
#include <LDateTime.h>
#include <LStorage.h>
#include <LFlash.h>
#include <LGPRSClient.h>
#include <LGPRS.h>
#include "RunningMedian.h"
#include "OTAUpdate.h"
#include "OTAUtils.h"

#include "EEPROMAnything.h"
#include "myprivatedata.h"
#include "rfc.h"

//--------------------------------------------------
//! \version	
//--------------------------------------------------
#define	FWVERSION	3

#define PERIOD_LIPO_INFO		120000		// 2 min. ,interval between 2 battery level measurement, in milliseconds
#define PERIOD_READ_ANALOG		120000		// 2 min. ,interval between 2 analog input read (external supply), in milliseconds
#define PERIOD_CHECK_ANALOG_LEVEL 1200000	// 20 min. , interval between 2 analog level check (can send an SMS alert if level are low)
#define PERIOD_CHECK_SMS		1000		// 1 sec., interval between 2 SMS check, in milliseconds
#define TIMEOUT_SMS_MENU		300000		// 5 min., when timeout, SMS menu return to login (user should send password again to log), in milliseconds

#define PERIODIC_STATUS_SMS		60000		// 1 min. (DO NOT CHANGE) : interval between two Hour+Minute check of periodic time (see after)
#define PERIODIC_STATUS_SMS_H	12			// Hour for time of periodic status
#define PERIODIC_STATUS_SMS_M	00			// Minute for time of periodic status

#define PERIODIC_CHECK_FW		60000		// 1 min. (DO NOT CHANGE) : interval between two Hour+Minute check of periodic time (see below)
#define PERIODIC_CHECK_FW_H		12			// Hour for time of periodic check of firmware
#define PERIODIC_CHECK_FW_M		30			// Minute for time of periodic check of firmware

// SMS menu architecture
#define TXT_MAIN_MENU	"Main Menu\r\n1 : Status\r\n2 : Alarm ON\r\n3 : Alarm OFF\r\n4 : Params\r\n0 : Exit"
#define TXT_PARAMS_MENU "Params Menu\r\n5 : Change default num.\r\n6 : Change coord.\r\n7 : Change radius\r\n8 : Change secret\r\n9 : Periodic status ON\r\n10 : Periodic status OFF\r\n11 : Low power alarm ON\r\n12 : Low power alarm OFF\r\n13 : Change low power trig.\r\n14 : Update firmware\r\n15 : Restore factory settings"

// Led gpio definition
#define LEDGPS  				13
#define LEDALARM  				12
// Other gpio
#define FLOODSENSOR  			8			// digital input where flood sensor is connected
#define FLOODSENSOR_ACTIVE		0			// 0 or 1 ,Set level when flood sensor is active (water detected)

// Analog input
#define NB_SAMPLE_ANALOG		16
#define VOLT_DIVIDER_INPUT		5.964 		// Voltage divider ratio for mesuring input voltage. 
#define MAX_DC_IN				36			// Max input voltage
#define MIN_DC_IN				9			// Minimum input voltage
// Lipo
// battery level trigger for alarm , in % , WARNING, LIPO level are only 100,66 and 33%
// Do not use value < 33% because linkitone will not give you another value until 0% ...
#define LIPO_LEVEL_TRIG	33	// in % , values available 33 - 66 (0 and exlucded logically)

// Median computation
RunningMedian samples = RunningMedian(NB_SAMPLE_ANALOG);

// Miscalleneous 
char buff[512];
unsigned long taskTestGeof;
unsigned long taskGetLiPo;
unsigned long taskGetAnalog;
unsigned long taskCheckInputVoltage;
unsigned long taskCheckSMS;
unsigned long taskStatusSMS;
unsigned long TimeOutSMSMenu;
unsigned long taskCheckFW;



//----------------------------------------------------------------------
//!\brief	return position of the comma number 'num' in the char array 'str'
//!\return  char
//----------------------------------------------------------------------
static unsigned char getComma(unsigned char num,const char *str){
	unsigned char i,j = 0;
	int len=strlen(str);
	for(i = 0;i < len;i ++){
		if(str[i] == ',')
			j++;
		if(j == num)
			return i + 1; 
		}
	return 0; 
}


//----------------------------------------------------------------------
//!\brief	convert char buffer to float
//!\return  float
//----------------------------------------------------------------------
static float getFloatNumber(const char *s){
	char buf[10];
	unsigned char i;
	float rev;

	i=getComma(1, s);
	i = i - 1;
	strncpy(buf, s, i);
	buf[i] = 0;
	rev=atof(buf);
	return rev; 
}


//----------------------------------------------------------------------
//!\brief	convert char buffer to int
//!\return  float
//----------------------------------------------------------------------
static float getIntNumber(const char *s){
	char buf[10];
	unsigned char i;
	float rev;

	i=getComma(1, s);
	i = i - 1;
	strncpy(buf, s, i);
	buf[i] = 0;
	rev=atoi(buf);
	return rev; 
}


//----------------------------------------------------------------------
//!\brief	Get analog voltage of DC input (can be an external battery)
//!\return  -
//----------------------------------------------------------------------
void GetAnalogRead(void){
	// if it's time to get analog input for monitoring external supply
	if(MyFlag.taskGetAnalog){
		Serial.println("-- Analog input read --");
		MyFlag.taskGetAnalog = false;
		// // read 16 times and average
		// unsigned int i = 0;
		// //on fait plusieurs mesures
		// for( i = 0; i < NB_SAMPLE_ANALOG; i++){
			// //read analog input
			// long x  = analogRead(A0);	//gives value between 0 to 1023
			// samples.add(x);
			// delay(10);
		// }
		// //ocompute median value
		// MyExternalSupply.raw = samples.getMedian();
		MyExternalSupply.raw = analogRead(A0);	//gives value between 0 to 1023
		sprintf(buff," Analog raw input = %d\r\n", MyExternalSupply.raw );
		Serial.print(buff);
		// convert raw data to voltage
		MyExternalSupply.analog_voltage = MyExternalSupply.raw * 5.0 / 1024.0;
		sprintf(buff," Analog voltage= %2.2fV\r\n", MyExternalSupply.analog_voltage );
		Serial.print(buff);
		// compute true input voltage
		MyExternalSupply.input_voltage = MyExternalSupply.analog_voltage * VOLT_DIVIDER_INPUT + 0.41; // +0.41V for forward voltage of protection diode
		sprintf(buff," Input voltage= %2.1fV\r\n", MyExternalSupply.input_voltage );
		Serial.println(buff);
	}	
}


//----------------------------------------------------------------------
//!\brief	Grab LiPo battery level and status
//!\return  -
//----------------------------------------------------------------------
void GetLiPoInfo(void){
	// if it's time to get LiPo voltage and status
	if(MyFlag.taskGetLiPo){
		Serial.println("-- Battery Info --");
		MyFlag.taskGetLiPo = false;
		MyBattery.LiPo_level = LBattery.level();
		MyBattery.charging_status  = LBattery.isCharging();
		sprintf(buff," battery level = %d%%", MyBattery.LiPo_level );
		Serial.print(buff);
		//convert charging direction status
		char chargdir[24];
		sprintf(chargdir,"discharging");
		//convert bit to string
		if(MyBattery.charging_status)
			sprintf(chargdir,"charging");		
		sprintf(buff," is %s\r\n", chargdir );
		Serial.println(buff);
	}
}


//----------------------------------------------------------------------
//!\brief	Verify if SMS is incoming
//!\return  -
//----------------------------------------------------------------------
void CheckSMSrecept(void){
	// Check if there is new SMS
	if(MyFlag.taskCheckSMS && LSMS.available()){
		MyFlag.taskCheckSMS = false;
		char buf[20];
		int v, i = 0;
		//flush buffer before writing in it
		memset(&MySMS.message[0], 0, sizeof(MySMS.message));
		Serial.println("--- SMS received ---");
		// display Number part
		LSMS.remoteNumber(buf, 20);
		size_t destination_size = sizeof (MySMS.incomingnumber);
		snprintf(MySMS.incomingnumber, destination_size, "%s", buf);
		Serial.print("Number:");
		Serial.println(MySMS.incomingnumber);
		// display Content part
		Serial.print("Content:");
		//copy SMS to buffer
		while(true){
			v = LSMS.read();
			if(v < 0)
				break;
			MySMS.message[i] = (char)v;
			Serial.print(MySMS.message[i]);
			i++;
		}
		Serial.println();
		// delete message
		LSMS.flush();
		// set flag to analyse this SMS
		MyFlag.SMSReceived = true;
	}
}

//----------------------------------------------------------------------
//!\brief	Proceed to change number in EEPROM from sms command
//!\brief	MySMS.message should contain : +33---old---,+33---new---
//!\return  -
//----------------------------------------------------------------------
void ProcessChgNum(){

	//check lengh before split
	if( strlen(MySMS.message) == 25 ){
		// Read each command pair 
		char* command = strtok(MySMS.message, ",");
		sprintf(buff, "old num : %s\n",command);
		Serial.println(buff);
		
		//compare old number with the one stored in EEPROM
		if( strcmp(command, MyParam.myphonenumber) == 0){
			// old is OK , we can store new number in EEPROM
			// Find the next command in input string
			command = strtok (NULL, ",");
			sprintf(buff, "new num : %s\n", command);
			Serial.println(buff);
			size_t destination_size = sizeof (MyParam.myphonenumber);
			snprintf(MyParam.myphonenumber, destination_size, "%s", command);
			//Save change in EEPROM
			EEPROM_writeAnything(0, MyParam);
			Serial.println("New number saved in EEPROM");
			sprintf(buff, "New phone number saved : %s", MyParam.myphonenumber); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);	
			//change state machine to Main_menu
			MySMS.menupos = SM_MENU_MAIN;			
		}
		else{
			sprintf(buff, "Error in old phone number : %s.", command); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);	
			//change state machine to Main_menu
			MySMS.menupos = SM_MENU_MAIN;			
		}
	}
	else{
		sprintf(buff, "Error in size parameters (%d): %s.", strlen(MySMS.message),MySMS.message); 
		Serial.println(buff);
		//send SMS
		SendSMS(MySMS.incomingnumber, buff);	
		//change state machine to Main_menu
		MySMS.menupos = SM_MENU_MAIN;
	}
}


//----------------------------------------------------------------------
//!\brief	Proceed to change secret code in EEPROM from sms command
//!\brief	MySMS.message should contain : oldcode,newcode
//!\return  -
//----------------------------------------------------------------------
void ProcessChgSecret(){
	//check lengh before split
	if( strlen(MySMS.message) == 9 ){
		// Read each command pair 
		char* command = strtok(MySMS.message, ",");
		sprintf(buff, "old code : %s\n",command);
		Serial.println(buff);
		
		//compare old number with the one stored in EEPROM
		if( strcmp(command, MyParam.smssecret) == 0){
			// old is OK , we can store new code in EEPROM
			// Find the next command in input string
			command = strtok (NULL, ",");
			sprintf(buff, "new code : %s\n",command);
			Serial.println(buff);
			size_t destination_size = sizeof (MyParam.smssecret);
			snprintf(MyParam.smssecret, destination_size, "%s", command);
			//Save change in EEPROM
			EEPROM_writeAnything(0, MyParam);
			Serial.println("New secret code saved in EEPROM");
			sprintf(buff, "New secret code saved : %s", MyParam.smssecret); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);	
			//change state machine to Main_menu
			MySMS.menupos = SM_MENU_MAIN;			
		}
		else{
			sprintf(buff, "Error in old secret code : %s.", command); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);	
			//change state machine to Main_menu
			MySMS.menupos = SM_MENU_MAIN;			
		}
	}
	else{
		sprintf(buff, "Error in size parameters (%d): %s.", strlen(MySMS.message), MySMS.message); 
		Serial.println(buff);
		//send SMS
		SendSMS(MySMS.incomingnumber, buff);	
		//change state machine to Main_menu
		MySMS.menupos = SM_MENU_MAIN;
	}
}


//----------------------------------------------------------------------
//!\brief	Proceed to change voltage of low power trigger alarm
//!\brief	MySMS.message should contain a tension like  11.6
//!\return  -
//----------------------------------------------------------------------
void ProcessLowPowTrig(){
	//check lengh before getting data
	if( strlen(MySMS.message) <= 5 ){
		// convert SMS to float
		float value_sms = atof(MySMS.message);
		sprintf(buff, "SMS content as volt : %2.1f\n",value_sms);
		Serial.println(buff);
		
		// check that it is a value inside the range
		if( value_sms > MIN_DC_IN and value_sms <= MAX_DC_IN ){
			// value is OK , we can store it in EEPROM
			MyParam.trig_input_level = value_sms;
			//Save change in EEPROM
			EEPROM_writeAnything(0, MyParam);
			Serial.println("New value saved in EEPROM");
			sprintf(buff, "New trigger value for low power voltage saved : %2.1fV", MyParam.trig_input_level); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);	
			//change state machine to Main_menu
			MySMS.menupos = SM_MENU_MAIN;			
		}
		else{
			sprintf(buff, "Error, value is outside rangee : %2.1fV", value_sms); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);	
			//change state machine to Main_menu
			MySMS.menupos = SM_MENU_MAIN;			
		}
	}
	else{
		sprintf(buff, "Error in size parameters (%d): %s.", strlen(MySMS.message), MySMS.message); 
		Serial.println(buff);
		//send SMS
		SendSMS(MySMS.incomingnumber, buff);	
		//change state machine to Main_menu
		MySMS.menupos = SM_MENU_MAIN;
	}
}


//----------------------------------------------------------------------
//!\brief	Proceed to restore all factory settings 
//!\brief	MySMS.message should contain : y or n (Y or N)
//!\return  -
//----------------------------------------------------------------------
void ProcessRestoreDefault(){
	//check lengh , message should contain y or n (Y or N) 
	if( strlen(MySMS.message) == 1 ){
		// Read sms content
		sprintf(buff, " response : %s\n", MySMS.message);
		Serial.println(buff);
		
		//If restoration id confirmed
		if( (strcmp("Y", MySMS.message) == 0) || (strcmp("y", MySMS.message) == 0) ){
			// then revert all parameters to default !
			
			//little trick : use LoadParamEEPROM function with the flat_data_written forced to false
			// this will act as the very first boot 
			MyParam.flag_data_written = false;
			//SAVE IN EEPROM !
			EEPROM_writeAnything(0, MyParam);
			// then load default param in EEPROM
			LoadParamEEPROM();
			//print structure
			PrintMyParam();	
			
			//prepare an sms to confirm and display default password (it may has been changed)
			sprintf(buff, "Parameters restored to factory settings!!\r\nSecret code: %s", MyParam.smssecret); 
			Serial.println(buff);			
		}
		else{ 
			//prepare an sms to confirm
			sprintf(buff, "Restoration aborted, message received : %s.\r\n", MySMS.message); 
			Serial.println(buff);			
		}
	}
	else{
		sprintf(buff, "Error in message received : %s , size(%d).\r\n", MySMS.message, strlen(MySMS.message)); 
		Serial.println(buff);		
	}
	
	//send SMS
	SendSMS(MySMS.incomingnumber, buff);			
	//change state machine to Main_menu
	MySMS.menupos = SM_MENU_MAIN;	
}


//----------------------------------------------------------------------
//!\brief	Does action selected by user in the main menu
//!\return  -
//----------------------------------------------------------------------
void ProcessMenuMain(void){
	int val = atoi(MySMS.message);
	char flagalarm[4];
	switch(val){
		case CMD_EXIT:
			Serial.println(" Exit !");
			// Force to return to SM_LOGIN state -> need to receive secret code 
			MySMS.menupos = SM_LOGIN;
			break;	
		case CMD_STATUS:		//status
			Serial.println("Status required ");
			
			//convert flag_alarm_onoff
			sprintf(flagalarm,"OFF");
			//convert bit to string
			if(MyParam.flag_alarm_onoff)
				sprintf(flagalarm,"ON");

			//convert flag_periodic_status_onoff
			char flagalarm_period[4];
			sprintf(flagalarm_period,"OFF");
			//convert bit to string
			if(MyParam.flag_periodic_status_onoff)
				sprintf(flagalarm_period,"ON");

			//convert flag_periodic_status_onoff
			char flagalarm_lowbat[4];
			sprintf(flagalarm_lowbat,"OFF");
			//convert bit to string
			if(MyParam.flag_alarm_low_bat)
				sprintf(flagalarm_lowbat,"ON");

			//convert flag_periodic_status_onoff
			char flagalarm_flood[4];
			sprintf(flagalarm_flood,"OFF");
			//convert bit to string
			if(MyParam.flag_alarm_flood)
				sprintf(flagalarm_flood,"ON");
				
			//convert charging direction status
			char chargdir[24];
			sprintf(chargdir,"discharging");
			//convert bit to string
			if(MyBattery.charging_status)
				sprintf(chargdir,"charging");				
			//if GPS is fixed , prepare a complete message
			if(MyFlag.fix3D == true){
				sprintf(buff, "Status : \r\nCurrent position is : https://www.google.com/maps?q=%2.6f%c,%3.6f%c \r\nLiPo = %d%%, %s\r\nExternal supply : %2.1fV\r\nGeofencing alarm is %s.\r\nPeriodic SMS is %s.\r\nLow input voltage alarm is %s.\r\nFlood alarm is %s\r\nFw v%d.", MyGPSPos.latitude, MyGPSPos.latitude_dir, MyGPSPos.longitude, MyGPSPos.longitude_dir, MyBattery.LiPo_level, chargdir, MyExternalSupply.input_voltage, flagalarm, flagalarm_period, flagalarm_lowbat, flagalarm_flood, FWVERSION); 
			}
			// else, use short form message
			else{
				sprintf(buff, "Status : \r\nNO position fix.\r\nLiPo = %d%%, %s\r\nExternal supply : %2.1fV\r\nGeofencing alarm is %s.\r\nPeriodic SMS is %s.\r\nLow input voltage alarm is %s.\r\nFlood alarm is %s\r\nFw v%d.", MyBattery.LiPo_level, chargdir, MyExternalSupply.input_voltage, flagalarm, flagalarm_period, flagalarm_lowbat, flagalarm_flood, FWVERSION); 
			}
			Serial.println(buff);
			SendSMS(MySMS.incomingnumber, buff);
			break;
			
		case CMD_ALM_ON:		// alarm ON
			Serial.println("Alarm ON required");
			MyParam.flag_alarm_onoff = true;
			//convert flag_alarm_onoff
			snprintf(flagalarm,3,"ON");	
			//prepare SMS content
			sprintf(buff, "Alarm switch to %s state", flagalarm); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);
			//Save change in EEPROM
			EEPROM_writeAnything(0, MyParam);
			Serial.println("Data saved in EEPROM");
			break;
			
		case CMD_ALM_OFF:		// alarm OFF
			Serial.println("Alarm OFF required");
			MyParam.flag_alarm_onoff = false;
			//convert flag_alarm_onoff
			snprintf(flagalarm,4,"OFF");
			//prepare SMS content
			sprintf(buff, "Alarm switch to %s state", flagalarm); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);
			//Save change in EEPROM
			EEPROM_writeAnything(0, MyParam);
			Serial.println("Data saved in EEPROM");	
			break;
			
		case CMD_PARAMS:	//go to sub menu params
			sprintf(buff, TXT_PARAMS_MENU); 
			Serial.println(buff);
			SendSMS(MySMS.incomingnumber, buff);
			break;
			
		case CMD_CHG_NUM:
			Serial.println("Change number");
			//prepare SMS content
			sprintf(buff, "Send : +336--old---,+336--new---"); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);
			MySMS.menupos = SM_CHG_NUM;
			break;
			
		case CMD_CHG_COORD:
			Serial.println("Change coordinates");
			//prepare SMS content
			sprintf(buff, "Send : 49.791489,N,179.1077,E\r\nor 'Here', to set current position as new coord."); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);
			MySMS.menupos = SM_CHG_COORD;		
			break;
			
		case CMD_CHG_RADIUS:
			// TO DO !!!
			Serial.println("Change radius for geofencing");
			//prepare SMS content
			sprintf(buff, "Send radius in meter (1-10000).\r\nActual radius is %d m", MyParam.radius); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);
			MySMS.menupos = SM_CHG_RADIUS;				
			break;
			
		case CMD_CHG_SECRET:
			Serial.println("Change secret code");
			//prepare SMS content
			sprintf(buff, "Send : oldcode,newcode\r\nLimit to 4 caracters."); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);
			MySMS.menupos = SM_CHG_SECRET;		
			break;
			
		case CMD_PERIODIC_STATUS_ON:
			Serial.println("Periodic status ON required");
			MyParam.flag_periodic_status_onoff = true;
			//convert flag_periodic_status_onoff
			snprintf(flagalarm,4,"ON");
			//prepare SMS content
			sprintf(buff, "Periodic status switched to %s state", flagalarm); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);
			//Save change in EEPROM
			EEPROM_writeAnything(0, MyParam);
			Serial.println("Data saved in EEPROM");	
			break;

		case CMD_PERIODIC_STATUS_OFF:
			Serial.println("Periodic status OFF required");
			MyParam.flag_periodic_status_onoff = false;
			//convert flag_periodic_status_onoff
			snprintf(flagalarm,4,"OFF");
			//prepare SMS content
			sprintf(buff, "Periodic status switched to %s state", flagalarm); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);
			//Save change in EEPROM
			EEPROM_writeAnything(0, MyParam);
			Serial.println("Data saved in EEPROM");	
			break;	

		case CMD_LOWPOWER_ON:
			Serial.println("Low power alarm ON required");
			MyParam.flag_alarm_low_bat = true;
			//convert flag_alarm_low_bat
			snprintf(flagalarm,4,"ON");
			//prepare SMS content
			sprintf(buff, "Low power alarm switched to %s state", flagalarm); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);
			//Save change in EEPROM
			EEPROM_writeAnything(0, MyParam);
			Serial.println("Data saved in EEPROM");	
			break;

		case CMD_LOWPOWER_OFF:
			Serial.println("Low power alarm OFF required");
			MyParam.flag_alarm_low_bat = false;
			//convert flag_alarm_low_bat
			snprintf(flagalarm,4,"OFF");
			//prepare SMS content
			sprintf(buff, "Low power alarm switched to %s state", flagalarm); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);
			//Save change in EEPROM
			EEPROM_writeAnything(0, MyParam);
			Serial.println("Data saved in EEPROM");	
			break;			
		
		case CMD_CHG_LOWPOW_TRIG:
			Serial.println("Change low power trigger level");
			//prepare SMS content
			sprintf(buff, "Send tension in volt, ex. :  11.6\r\nActual trig. is %2.1fV", MyParam.trig_input_level); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);
			MySMS.menupos = SM_CHG_LOWPOW_TRIG;		
			break;

		case CMD_UPDATE_FW:
			//prepare SMS content
			sprintf(buff, "FIRMWARE UPDATE WILL PROCEED IF A NEW VERSION IS AVAILABLE."); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);
			// set flag to force firmware update
			MyFlag.ForceFWUpdate = true;
			MyFlag.taskCheckFW = true;
			break;
			
		case CMD_RESTORE_DFLT:
			//prepare SMS content
			sprintf(buff, "CONFIRM RESTORE DEFAULT SETTINGS Y/N ?"); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);
			MySMS.menupos = SM_RESTORE_DFLT;		
			break;
			
		default:
			//prepare SMS content
			sprintf(buff, "Error"); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);		
			break;
	}
}

//----------------------------------------------------------------------
//!\brief	Manage SMS menu
//!\return  -
//----------------------------------------------------------------------
void MenuSMS(void){
	// if a new message is received
	if( MyFlag.SMSReceived == true ){
		Serial.println("--- SMS Menu manager ---");
		switch(MySMS.menupos){
			default:			
			case SM_LOGIN:
				//compare secret code with received sms code
				if( strcmp(MySMS.message, MyParam.smssecret) == 0 ){
					// password is OK
					Serial.println("Password is OK.");
					// password is OK, we can send main menu
					MySMS.menupos = SM_MENU_MAIN;
					sprintf(buff, TXT_MAIN_MENU); 
					Serial.println(buff);
					SendSMS(MySMS.incomingnumber, buff);
					// start timer to auto-logout when no action occurs
					TimeOutSMSMenu = millis();
				}
				else{
					Serial.println("Wrong Password. Do nothing !");
				}
				break;
			
			case SM_MENU_MAIN:
				// reload timer to avoid auto-logout
				TimeOutSMSMenu = millis();
				Serial.println("Menu Main ");
				ProcessMenuMain();
				break;
			
			case SM_CHG_NUM:
				// reload timer to avoid auto-logout
				TimeOutSMSMenu = millis();
				Serial.println("Proceed to change number");
				ProcessChgNum();
				break;
				
			case SM_CHG_COORD:
				// reload timer to avoid auto-logout
				TimeOutSMSMenu = millis();
				Serial.println("Proceed to change coordinates");
				//ProcessChgCoord();
				break;	
				
			case SM_CHG_RADIUS:
				// reload timer to avoid auto-logout
				TimeOutSMSMenu = millis();
				Serial.println("Proceed to change geofencing radius");
				//ProcessChgRadius();
				break;
				
			case SM_CHG_SECRET:
				// reload timer to avoid auto-logout
				TimeOutSMSMenu = millis();
				Serial.println("Proceed to change secret code");
				ProcessChgSecret();
				break;
			
			case SM_RESTORE_DFLT:
				// reload timer to avoid auto-logout
				TimeOutSMSMenu = millis();
				Serial.println("Proceed to restore default settings");
				ProcessRestoreDefault();				
				break;

			case SM_CHG_LOWPOW_TRIG:
				// reload timer to avoid auto-logout
				TimeOutSMSMenu = millis();
				Serial.println("Proceed to change low power level");
				ProcessLowPowTrig();
				break;
		}
	
		// SMS read reset flag
		MyFlag.SMSReceived = false;
	}
}

//----------------------------------------------------------------------
//!\brief	Send an SMS with specific number and message
//!\param	phonenumber (char[]) 
//!\param  	message (char[120])
//!\return  true or false
//----------------------------------------------------------------------
bool SendSMS( const char *phonenumber, const char *message ){
	Serial.println("  Sending SMS ...");
	LSMS.beginSMS(phonenumber);
	LSMS.print(message);
	bool ret;
	if(LSMS.endSMS()){
		Serial.println("  SMS sent");
		ret = true;
	}
	else{
		Serial.println("  SMS not sent");
		ret = false;
	}
	return ret;
}

//----------------------------------------------------------------------
//!\brief	Manage alert when occurs
//!\return  -
//----------------------------------------------------------------------
void AlertMng(void){
	// if alarm is allowed AND position is outside autorized area
	if ( MyParam.flag_alarm_onoff && MyFlag.PosOutiseArea){
		MyFlag.PosOutiseArea = false;
		Serial.println("--- AlertMng : start sending SMS"); 		
		//convert charging direction status
		char chargdir[24];
		sprintf(chargdir,"discharging");
		//convert bit to string
		if(MyBattery.charging_status)
			sprintf(chargdir,"charging");		
		sprintf(buff, "Robomow Alert !! Current position is : https://www.google.com/maps?q=%2.6f%c,%3.6f%c \r\nLiPo = %d%%, %s\r\nExternal supply : %2.1fV", MyGPSPos.latitude, MyGPSPos.latitude_dir, MyGPSPos.longitude, MyGPSPos.longitude_dir, MyBattery.LiPo_level, chargdir, MyExternalSupply.input_voltage); 
		Serial.println(buff);
		SendSMS(MyParam.myphonenumber, buff);
	}
	
	// each minute : if periodic status sms is required
	if ( MyParam.flag_periodic_status_onoff && MyFlag.taskStatusSMS){
		// reset flag
		MyFlag.taskStatusSMS = false;
		// check if hour + minute is reach 
		if ( MyGPSPos.hour == PERIODIC_STATUS_SMS_H && MyGPSPos.minute == PERIODIC_STATUS_SMS_M){
			// It's time to send a status SMS !!
			Serial.println("--- AlertMng : periodic status SMS"); 
			sprintf(buff, "  Ring ! it's %d:%d , time to send a periodic status SMS", MyGPSPos.hour, MyGPSPos.minute ); 
			Serial.println(buff);			
			//convert flag_alarm_onoff
			char flagalarm[4];
			sprintf(flagalarm,"OFF");
			//convert bit to string
			if(MyParam.flag_alarm_onoff)
				sprintf(flagalarm,"ON");

			//convert flag_periodic_status_onoff
			char flagalarm_period[4];
			sprintf(flagalarm_period,"OFF");
			//convert bit to string
			if(MyParam.flag_periodic_status_onoff)
				sprintf(flagalarm_period,"ON");

			//convert flag_periodic_status_onoff
			char flagalarm_lowbat[4];
			sprintf(flagalarm_lowbat,"OFF");
			//convert bit to string
			if(MyParam.flag_alarm_low_bat)
				sprintf(flagalarm_lowbat,"ON");

			//convert flag_periodic_status_onoff
			char flagalarm_flood[4];
			sprintf(flagalarm_flood,"OFF");
			//convert bit to string
			if(MyParam.flag_alarm_flood)
				sprintf(flagalarm_flood,"ON");				
				
			//convert charging direction status
			char chargdir[24];
			sprintf(chargdir,"discharging");
			//convert bit to string
			if(MyBattery.charging_status)
				sprintf(chargdir,"charging");			
			sprintf(buff, "Periodic status : \r\nCurrent position is : https://www.google.com/maps?q=%2.6f%c,%3.6f%c \r\nLiPo = %d%%, %s\r\nExternal supply : %2.1fV\r\nGeofencing alarm is %s.\r\nPeriodic SMS is %s.\r\nLow input voltage alarm is %s.\r\nFlood alarm is %s\r\nFw v%d.", MyGPSPos.latitude, MyGPSPos.latitude_dir, MyGPSPos.longitude, MyGPSPos.longitude_dir, MyBattery.LiPo_level, chargdir, MyExternalSupply.input_voltage, flagalarm, flagalarm_period, flagalarm_lowbat, flagalarm_flood, FWVERSION); 
			Serial.println(buff);
			SendSMS(MyParam.myphonenumber, buff);
		}
	}

	// Check input supply level (can be an external battery) and LiPo level
	if (  MyFlag.taskCheckInputVoltage ){
		Serial.println("--- AlertMng : Check input voltage"); 
		// If input voltage is lower than alarm treshold
		if( (MyExternalSupply.input_voltage <= MyParam.trig_input_level) && MyParam.flag_alarm_low_bat ){
			// add some debug and send an alarm SMS
			sprintf(buff, "  LOW BATTERY ALARM\r\n  Input voltage is lower than TRIG_INPUT_LEVEL :\r\n  %2.1fV <= %2.1fV", MyExternalSupply.input_voltage, MyParam.trig_input_level ); 
			Serial.println(buff);
			SendSMS(MyParam.myphonenumber, buff);
		}
		
		Serial.println("--- AlertMng : Check LiPo voltage"); 
		// If LiPo voltage is lower than alarm treshold
		if( MyBattery.LiPo_level <= MyParam.lipo_level_trig ){
			// add some debug and send an alarm SMS
			sprintf(buff, "  LOW VOLTAGE LiPo ALARM\r\n  LiPo voltage is lower than LIPO_LEVEL_TRIG :\r\n  %d%% <= %d%%", MyBattery.LiPo_level, MyParam.lipo_level_trig ); 
			Serial.println(buff);
			SendSMS(MyParam.myphonenumber, buff);
		}
		
		// reset flags
		MyParam.flag_alarm_low_bat = false;
		MyFlag.taskCheckInputVoltage = false;
	}	
	
	// Check if it's time to check for a firmware update on the server
	if (  MyFlag.taskCheckFW ){
		// reset flag
		MyFlag.taskCheckFW = false;
		// check if hour + minute is reach  OR if a manual force update is incoming
		if ( (MyGPSPos.hour == PERIODIC_CHECK_FW_H && MyGPSPos.minute == PERIODIC_CHECK_FW_M) || MyFlag.ForceFWUpdate == true){
			// reset flag
			MyFlag.ForceFWUpdate = false;
			Serial.println("--- AlertMng : FW check on the server");
			// It's time to check if there is a Firmware update on the remote server
			// The following code can take some minutes to proceed ...
			switch(OTAUpdate.checkUpdate()){
				case 1:
					// send a SMS to say that there is no update available
					sprintf(buff, "  update.md5 not found or host not available." ); 
					Serial.println(buff);
					SendSMS(MyParam.myphonenumber, buff);						
					break;
					
				case 2:
					// send a SMS to say that there is an error while parsing md5 file
					sprintf(buff, "  Error while parsing update.md5 file." ); 
					Serial.println(buff);
					SendSMS(MyParam.myphonenumber, buff);
					break;
				case 3:
					// send a SMS to say that update.md5 does not contain a valid firmware name
					sprintf(buff, "  update.md5 does not contain a valid firmware name" ); 
					Serial.println(buff);
					SendSMS(MyParam.myphonenumber, buff);
					break;
				case 4:
					// send a SMS to say that there is an error while downloading update.vxp (update.md5 was well downloaded before)
					sprintf(buff, "  Error while downloading update.vxp (update.md5 was well downloaded before)" ); 
					Serial.println(buff);
					SendSMS(MyParam.myphonenumber, buff);
					break;
				case 5:
					// send a SMS to say that New firmware has a wrong md5sum!
					sprintf(buff, "  New firmware has a wrong md5sum!" ); 
					Serial.println(buff);
					SendSMS(MyParam.myphonenumber, buff);
					break;
				case 6:
					// send a SMS to warn user that is device will be updated
					sprintf(buff, "  A new firmware version is available. Update is running now ... Your device will restart soon." ); 
					Serial.println(buff);
					SendSMS(MyParam.myphonenumber, buff);			
					// DO update
					OTAUpdate.startUpdate();
					break;					
			}
			// if (OTAUpdate.checkUpdate()) {
				// // send a SMS to warn user that is device will be updated
				// sprintf(buff, "  A new firmware version is available. Update is running now ... Your device will restart soon." ); 
				// Serial.println(buff);
				// SendSMS(MyParam.myphonenumber, buff);			
				// // DO update
				// OTAUpdate.startUpdate();
			// }
			// else{
				// // send a SMS to say that there is no update available
				// sprintf(buff, "  No firmware found or host not available." ); 
				// Serial.println(buff);
				// SendSMS(MyParam.myphonenumber, buff);	
			// }
		}
	}	
}

//----------------------------------------------------------------------
//!\brief	Load params from EEPROM
//----------------------------------------------------------------------
void LoadParamEEPROM() {
	
	EEPROM_readAnything(0, MyParam);
	
	//uncomment this line to erase EEPROM parameters with DEFAULT parameters
	// MyParam.flag_data_written = false;
	
	//check if parameters were already written
	if( MyParam.flag_data_written == false ){
		Serial.println("--- !!! Loading DEFAULT parameters from EEPROM ...  --- ");
		//EEPROM is empty , so load default parameters (see myprivatedata.h)
		MyParam.flag_alarm_onoff = FLAG_ALARM_ONOFF;
		MyParam.flag_periodic_status_onoff = FLAG_PERIODIC_STATUS_ONOFF;	
		MyParam.flag_alarm_low_bat = FLAG_ALARM_LOW_BAT;
		MyParam.flag_alarm_flood = FLAG_ALARM_FLOOD;
		size_t destination_size = sizeof (MyParam.smssecret);
		snprintf(MyParam.smssecret, destination_size, "%s", SMSSECRET);
		destination_size = sizeof (MyParam.myphonenumber);
		snprintf(MyParam.myphonenumber, destination_size, "%s", MYPHONENUMBER);
		MyParam.radius = RADIUS;
		MyParam.base_lat = BASE_LAT;
		MyParam.base_lat_dir = BASE_LAT_DIR;
		MyParam.base_lon = BASE_LON;
		MyParam.base_lon_dir = BASE_LON_DIR;
		MyParam.lipo_level_trig = LIPO_LEVEL_TRIG;
		MyParam.trig_input_level = TRIG_INPUT_LEVEL;
		//set flag that default data are stored
		MyParam.flag_data_written = true;
		
		//SAVE IN EEPROM !
		EEPROM_writeAnything(0, MyParam);
		Serial.println("--- !!! DEFAULT parameters stored in EEPROM !!! --- ");
	}
	else{
		Serial.println("--- Parameters loaded from EEPROM --- ");
	}
}

//----------------------------------------------------------------------
//!\brief	Print params of MyParam structure
//----------------------------------------------------------------------
void PrintMyParam() {
	char flag[4];
	Serial.println("--- MyParam contents --- ");
	
	sprintf(flag,"OFF");
	//convert bit to string
	if(MyParam.flag_data_written)
		sprintf(flag,"ON");
	sprintf(buff, "  flag_data_written = %s", flag);
	Serial.println(buff);
	
	sprintf(flag,"OFF");
	//convert bit to string
	if(MyParam.flag_alarm_onoff)
		sprintf(flag,"ON");	
	sprintf(buff, "  flag_alarm_onoff = %s", flag);
	Serial.println(buff);
	
	sprintf(flag,"OFF");
	//convert bit to string
	if(MyParam.flag_periodic_status_onoff)
		sprintf(flag,"ON");	
	sprintf(buff, "  flag_periodic_status_onoff = %s", flag);
	Serial.println(buff);	

	sprintf(flag,"OFF");
	//convert bit to string
	if(MyParam.flag_alarm_low_bat)
		sprintf(flag,"ON");	
	sprintf(buff, "  flag_alarm_low_bat = %s", flag);
	Serial.println(buff);

	sprintf(flag,"OFF");
	//convert bit to string
	if(MyParam.flag_alarm_flood)
		sprintf(flag,"ON");	
	sprintf(buff, "  flag_alarm_flood = %s", flag);
	Serial.println(buff);
	
	sprintf(buff, "  smssecret = %s", MyParam.smssecret);
	Serial.println(buff);
	sprintf(buff, "  myphonenumber = %s", MyParam.myphonenumber);
	Serial.println(buff);
	sprintf(buff, "  radius = %d", MyParam.radius);
	Serial.println(buff);	
	sprintf(buff, "  base_lat = %2.6f", MyParam.base_lat);
	Serial.println(buff);
	sprintf(buff, "  base_lat_dir = %c", MyParam.base_lat_dir);
	Serial.println(buff);
	sprintf(buff, "  base_lon = %3.6f", MyParam.base_lon);
	Serial.println(buff);
	sprintf(buff, "  base_lon_dir = %c", MyParam.base_lon_dir);
	Serial.println(buff);
	sprintf(buff, "  lipo_level_trig = %d%%", MyParam.lipo_level_trig);
	Serial.println(buff);
	sprintf(buff, "  trig_input_level = %2.1fV", MyParam.trig_input_level);
	Serial.println(buff);	
}

//----------------------------------------------------------------------
//!\brief           scheduler()
//----------------------------------------------------------------------
void Scheduler() {

	// if( (millis() - taskGetGPS) > PERIOD_GET_GPS){
		// taskGetGPS = millis();
		// MyFlag.taskGetGPS = true;	
	// }
	
	// if( (millis() - taskTestGeof) > PERIOD_TEST_GEOFENCING){
		// taskTestGeof = millis();
		// MyFlag.taskTestGeof = true;
	// }
	
	if( (millis() - taskGetLiPo) > PERIOD_LIPO_INFO){
		taskGetLiPo = millis();
		MyFlag.taskGetLiPo = true;
	}	
	
	if( (millis() - taskCheckSMS) > PERIOD_CHECK_SMS){
		taskCheckSMS = millis();
		MyFlag.taskCheckSMS = true;
	}
	
	// if( (millis() - taskCheckFlood) > PERIOD_CHECK_FLOOD){
		// taskCheckFlood = millis();
		// MyFlag.taskCheckFlood = true;
	// }	
	
	if( (millis() - taskStatusSMS) > PERIODIC_STATUS_SMS){
		taskStatusSMS = millis();
		MyFlag.taskStatusSMS = true;
	}

	if( (millis() - taskCheckFW) > PERIODIC_CHECK_FW){
		taskCheckFW = millis();
		MyFlag.taskCheckFW = true;
	}	

	if( (millis() - taskGetAnalog) > PERIOD_READ_ANALOG){
		taskGetAnalog = millis();
		MyFlag.taskGetAnalog = true;
	}	

	if( (millis() - taskCheckInputVoltage) > PERIOD_CHECK_ANALOG_LEVEL){
		taskCheckInputVoltage = millis();
		MyFlag.taskCheckInputVoltage = true;
	}
	
	if( ((millis() - TimeOutSMSMenu) > TIMEOUT_SMS_MENU) && MySMS.menupos != SM_LOGIN){
		MySMS.menupos = SM_LOGIN;
		Serial.println("--- SMS Menu manager : Timeout ---");
	}
}


//----------------------------------------------------------------------
//!\brief           SETUP()
//----------------------------------------------------------------------
void setup() {
	// put your setup code here, to run once:
	Serial.begin(115200);
	
	// set I/O direction
	pinMode(LEDALARM, OUTPUT);
	pinMode(LEDGPS, OUTPUT);
	pinMode(FLOODSENSOR, INPUT);
	
	delay(5000);
	Serial.println("RoboMowTrak "); 
	// GPS power on
	LGPS.powerOn();
	Serial.println("GPS Powered on.");
	// set default value for GPS (needed for default led status)
	MyGPSPos.fix = Error;
	
	// LTask will help you out with locking the mutex so you can access the global data
    //LTask.remoteCall(createThread1, NULL);
	LTask.remoteCall(createThreadSerialMenu, NULL);
	Serial.println("Launch threads.");
	
	// GSM setup
	while(!LSMS.ready()){
		delay(1000);
		Serial.println("Please insert SIM");
	}
	Serial.println("SIM ready.");	
	
	Serial.println("Deleting SMS received ...");
	//delete ALL sms received while powered off
	while(LSMS.available()){
		LSMS.flush(); // delete message
	}

	Serial.printf("init gprs... \r\n");
	while (!LGPRS.attachGPRS("Free", NULL, NULL)) {
		delay(500);
	}
	OTAUpdate.begin("92.245.144.185", "50150", "OTA_RoboMowTrack");
	
	// load params from EEPROM
	LoadParamEEPROM();
	//print structure
	PrintMyParam();
	
	// init default position in sms menu
	MySMS.menupos = SM_LOGIN;
	
	// for scheduler
	//taskGetGPS = millis();
	//taskTestGeof = millis();
	taskGetLiPo = millis();
	taskGetAnalog = millis();
	taskCheckSMS = millis();
	taskStatusSMS = millis();
	taskCheckFW = millis();	
	
	
	// set this flag to proceed a first LiPO level read (if an SMS is received before timer occurs)
	MyFlag.taskGetLiPo = true;
	// set this flag to proceed a first analog read (external supply)
	MyFlag.taskGetAnalog = true;
	
	//GPIO setup
	pinMode(LEDGPS, OUTPUT);
	pinMode(LEDALARM, OUTPUT);
	
	Serial.println("Setup done.");	
	
	// send an SMS to inform user that the device has boot
	// DON'T DO THAT BECAUSE GSM NETWORK IS NOT REACHABLE AT STARTUP (NEED SOME MINUTES TO LINK GSM NETWORK!!)
	// sprintf(buff, "PepetteBox is running.\r\n Firmware version : %d", FWVERSION); 
	// Serial.println(buff);
	// SendSMS(MyParam.myphonenumber, buff);	
}


//----------------------------------------------------------------------
//!\brief           LOOP()
//----------------------------------------------------------------------
void loop() {
	Scheduler();
	GetLiPoInfo();
	GetAnalogRead();
	//GetDigitalInput();
	CheckSMSrecept();
	MenuSMS();
	// SendGPS2Wifi();
	AlertMng();
}

//----------------------------------------------------------------------
//!\brief           THREAD DECLARATION
//----------------------------------------------------------------------
// boolean createThread1(void* userdata) {
	// // The priority can be 1 - 255 and default priority are 0
	// // the arduino priority are 245
	// vm_thread_create(thread_ledgps, NULL, 255);
    // return true;
// }

boolean createThreadSerialMenu(void* userdata) {
	// The priority can be 1 - 255 and default priority are 0
	// the arduino priority are 245
	vm_thread_create(thread_serialmenu, NULL, 255);
    return true;
}

////----------------------------------------------------------------------
////!\brief           THREAD LED GPS
////---------------------------------------------------------------------- 
//VMINT32 thread_ledgps(VM_THREAD_HANDLE thread_handle, void* user_data){
    //for (;;){
		//switch(MyGPSPos.fix){
			//case Invalid:
				//// blink led as pulse
				//digitalWrite(LEDGPS, HIGH);
				//delay(500);
				//digitalWrite(LEDGPS, LOW);
				//delay(500);
				//break;
			//case GPS:
			//case DGPS:
			//case PPS:
			//case RTK:
			//case FloatRTK:
			//case DR:
			//case Manual:
			//case Simulation:
				//// blink led as slow pulse
				//digitalWrite(LEDGPS, HIGH);
				//delay(150);
				//digitalWrite(LEDGPS, LOW);
				//delay(2850);
				//break;
			//case Error:
				//// Fast blinking led
				//digitalWrite(LEDGPS, HIGH);
				//delay(100);
				//digitalWrite(LEDGPS, LOW);
				//delay(100);
				//break;
		//}
		////DEBUG
		//// sprintf(buff, "MyGPSPos.fix = %d", MyGPSPos.fix);
		//// Serial.println(buff);
		//// delay(1000);
	//}
    //return 0;
//}

//----------------------------------------------------------------------
//!\brief           THREAD THAT MANAGE SERIAL MENU
//!\brief			Read serial console to menu access
//---------------------------------------------------------------------- 
VMINT32 thread_serialmenu(VM_THREAD_HANDLE thread_handle, void* user_data){
    
	char buffth[255];
	String stSerial;
	for (;;){
		// get the string hit on serial port
		stSerial = Serial.readStringUntil('\n');
		// if something has been typed
		if(stSerial != ""){
			// first delete \n
			stSerial.replace("\n","");
			stSerial.replace("\r","");
			// copy serial to SMS structure			
			size_t destination_size = sizeof (stSerial);
			stSerial.toCharArray(MySMS.message, destination_size);
			//DEBUG
			sprintf(buffth, ">> SERIAL get : %s", MySMS.message);
			Serial.println(buffth);
			// Write 0 in phone number to reply
			sprintf(MySMS.incomingnumber, "+33000000000");
			// Do as we have receive an SMS : set flag to analyse this SMS
			MyFlag.SMSReceived = true;
		}
		delay(500);		
  }
  return 0;
}
