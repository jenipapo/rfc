//--------------------------------------------------
//! \file		rfc.ino
//! \brief		SMS alert only.
//! \brief		Read voltage input and can set an alarm on low power.
//! \brief		Monitor LiPo cell voltage and can set an alarm on low power.
//! \brief		Serial message are for debug purpose only.
//! \brief		Over The Air by GPRS sketche update
//! \date		2015-May
//! \author		minbiocabanon
//--------------------------------------------------

//--------------------------------------------------
//! some notes here :
//! millis() overflow/reset over 50 days
//--------------------------------------------------
#include <LTask.h>
#include <vmthread.h>
#include <stddef.h>
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
#include "myperiod.h"

//--------------------------------------------------
//! \version	
//--------------------------------------------------
#define	FWVERSION	1


// SMS menu architecture
#define TXT_MAIN_MENU	"Menu principal\r\n1 : Chauffage ON\r\n2 : Chauffage OFF\r\n3 : Params\r\n0 : Exit"
#define TXT_PARAMS_MENU "Menu Params \r\n 4 : Changer num. gsm\r\n 5 : Changer code secret\r\n 6 : Alarme batt. faible ON\r\n 7 : Alarme batt. faible OFF\r\n 8 : Changer seuil batt.\r\n9 : Changer seuil entree\r\n10 : MaJ firmware\r\n11 : restaurer config. usine"

// Led gpio definition
#define LEDALARM  				12

// Median computation
RunningMedian samples_A0 = RunningMedian(NB_SAMPLE_ANALOG);
RunningMedian samples_A1 = RunningMedian(NB_SAMPLE_ANALOG);
RunningMedian samples_A2 = RunningMedian(NB_SAMPLE_ANALOG);

// Miscalleneous 
char buff[512];
int incomingByte = 0;   // for incoming serial data
unsigned long taskGetLiPo;
unsigned long taskGetAnalog;
unsigned long taskCheckInputVoltage;
unsigned long taskCheckSMS;
unsigned long taskCheckFlood;
unsigned long taskStatusSMS;
unsigned long TimeOutSMSMenu;

//----------------------------------------------------------------------
//!\brief	Get analog voltage of DC input (can be an external battery)
//!\return  -
//----------------------------------------------------------------------
void GetAnalogRead(void){
	// if it's time to get analog input for monitoring external supply
	if(MyFlag.taskGetAnalog){
		Serial.println("-- Analog input read --");
		MyFlag.taskGetAnalog = false;
		long x;
		// read 16 times and average
		for( unsigned int i = 0; i < NB_SAMPLE_ANALOG; i++){
			//read analog input voltage
			x  = analogRead(A0);	//gives value between 0 to 1023
			samples_A0.add(x);
			delay(5);
			//read analog input voltage divider flood sensor
			x  = analogRead(A1);	//gives value between 0 to 1023
			samples_A1.add(x);
			delay(5);
			//read analog input raw flood sensor
			x  = analogRead(A2);	//gives value between 0 to 1023
			samples_A2.add(x);
			delay(5);			
		}
		// compute median value input voltage
		MyExternalSupply.raw = samples_A0.getMedian();
		sprintf(buff," Analog raw input = %2.2f\r\n", MyExternalSupply.raw );
		Serial.print(buff);
		// convert raw data to voltage
		MyExternalSupply.analog_voltage = MyExternalSupply.raw * VCC_5V / 1024.00;
		sprintf(buff," Analog voltage= %2.2fV\r\n", MyExternalSupply.analog_voltage );
		Serial.print(buff);
		// compute true input voltage
		MyExternalSupply.input_voltage = MyExternalSupply.analog_voltage * VOLT_DIVIDER_INPUT + 0.61; // +0.61V for forward voltage of protection diode
		sprintf(buff," Input voltage= %2.2fV\r\n", MyExternalSupply.input_voltage );
		Serial.println(buff);
		
		// compute median value for flood sensor voltage divider
		MyFloodSensor.raw_divider = samples_A1.getMedian();
		sprintf(buff," Flood sensor divider raw value = %2.1f\r\n", MyFloodSensor.raw_divider );
		Serial.print(buff);
		// convert raw data to voltage
		MyFloodSensor.analog_voltage_divider = MyFloodSensor.raw_divider * VCC_5V / 1024.00;
		sprintf(buff," Flood sensor divider voltage = %3.2fV\r\n", MyFloodSensor.analog_voltage_divider );
		Serial.print(buff);
		
		// compute median value for flood sensor raw voltage
		MyFloodSensor.raw = samples_A2.getMedian();
		sprintf(buff," Flood sensor raw value= %3.2f\r\n", MyFloodSensor.raw );
		Serial.print(buff);
		// convert raw data to voltage
		MyFloodSensor.analog_voltage = MyFloodSensor.raw * VCC_5V / 1024.00;
		sprintf(buff," Flood sensor raw voltage = %3.2fV\r\n", MyFloodSensor.analog_voltage );
		Serial.print(buff);		
		
		Serial.println();
	}	
}

//----------------------------------------------------------------------
//!\brief	Check if flood sensor is dry or wet 
//!\return  -
//----------------------------------------------------------------------
void CheckFloodSensor(void){	
	// if it's time to get digital input from flood sensor
	if(MyFlag.taskCheckFlood){
		Serial.println("-- Flood sensor input read --");
		MyFlag.taskCheckFlood = false;
		//read digital input

		// For testing pupose
		// MyFloodSensor.value = digitalRead(FLOODSENSOR);
		// sprintf(buff," Digital input = %d\r\n", MyFloodSensor.value );
		// Serial.print(buff);
		// // if input is true, we are diviiiiiing !
		// if ( MyFloodSensor.value == FLOODSENSOR_ACTIVE ){
			// //prepare SMS to warn user
			// sprintf(buff, " Alert! flood sensor has detected water.\r\n Input level is %d.", MyFloodSensor.value); 
			// Serial.println(buff);
			// //send SMS
			// SendSMS(MySMS.incomingnumber, buff);
		// }
		// else{
			// Serial.println(" Flood sensor is dry.");
		// }
		
		// if read value is higher than trigger value, it's mean that there is too much water !
		if ( MyFloodSensor.raw > FLOODSENSOR_TRIG ){
			//prepare SMS to warn user
			sprintf(buff, " Alert! flood sensor has detected water.\r\n Input value is %3.1f\r\n(trigger value is %3.1f).", MyFloodSensor.raw, FLOODSENSOR_TRIG); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);
			// set the flag
			MyFloodSensor.value = FLOODSENSOR_ACTIVE; 
		}
		else{
			sprintf(buff, " Flood sensor is dry.\r\n Input value is %3.1f\r\n(trigger value is %3.1f) \r\n", MyFloodSensor.raw, FLOODSENSOR_TRIG); 
			Serial.println(buff);
			// set the flag
			MyFloodSensor.value = FLOODSENSOR_ACTIVE+1; 
		}
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
	if( strlen(MySMS.message) <= 25 ){
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
			sprintf(buff, "Error, value is outside range : %2.1fV", value_sms); 
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
//!\brief	Proceed to change trigger value for flood sensor alarm
//!\brief	MySMS.message should contain a tension like  11.6
//!\return  -
//----------------------------------------------------------------------
void ProcessChgFloodSensorTrig(){
	//check lengh before getting data
	if( strlen(MySMS.message) <= 5 ){
		// convert SMS to float
		float value_sms = atof(MySMS.message);
		sprintf(buff, "SMS content as value : %3.1f\n",value_sms);
		Serial.println(buff);
		
		// check that it is a value inside the range
		if( value_sms > MIN_FLOOR and value_sms <= MAX_FLOOR ){
			// value is OK , we can store it in EEPROM
			MyParam.flood_sensor_trig = value_sms;
			//Save change in EEPROM
			EEPROM_writeAnything(0, MyParam);
			Serial.println("New value saved in EEPROM");
			sprintf(buff, "New trigger value for flood sensor saved : %3.1f", MyParam.flood_sensor_trig); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);	
			//change state machine to Main_menu
			MySMS.menupos = SM_MENU_MAIN;			
		}
		else{
			sprintf(buff, "Error, value is outside range : %3.1fV", value_sms); 
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
						
		case CMD_CHG_SECRET:
			Serial.println("Change secret code");
			//prepare SMS content
			sprintf(buff, "Send : oldcode,newcode\r\nUse 4 caracters."); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);
			MySMS.menupos = SM_CHG_SECRET;		
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
			sprintf(buff, "Send tension in volt, ex. :  11.6\r\nValue must be  between %d and %d.\r\nActual trig. is %2.1fV", MIN_DC_IN, MAX_DC_IN, MyParam.trig_input_level); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);
			MySMS.menupos = SM_CHG_LOWPOW_TRIG;		
			break;
		
		case CMD_CHG_FLOOD_TRIG:
			Serial.println("Change flood sensor trigger");
			//prepare SMS content
			sprintf(buff, "Send value between %d and %d.\r\nCurrent value is %2.1f\r\nActual trig. is %3.1f", MIN_FLOOR, MAX_FLOOR, MyFloodSensor.analog_voltage, MyParam.flood_sensor_trig); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);
			MySMS.menupos = SM_CHG_FLOODSENSORTRIG;		
			break;
			
		case CMD_UPDATE_FW:
			//prepare SMS content
			sprintf(buff, "FIRMWARE UPDATE WILL PROCEED IF A NEW VERSION IS AVAILABLE."); 
			Serial.println(buff);
			//send SMS
			SendSMS(MySMS.incomingnumber, buff);
			// set flag to force firmware update
			MyFlag.ForceFWUpdate = true;
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
					sprintf(buff, "Wrong password, do nothing. msg[%s] != code[%s] ", MySMS.message, MyParam.smssecret); 
					Serial.println(buff);
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
				
			case SM_CHG_FLOODSENSORTRIG:
				// reload timer to avoid auto-logout
				TimeOutSMSMenu = millis();
				Serial.println("Proceed to change flood sensor trig value");
				ProcessChgFloodSensorTrig();
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
}

//----------------------------------------------------------------------
//!\brief	Check if a new firmware update is available (by SMS action)
//!\return  -
//----------------------------------------------------------------------
void CheckFirwareUpdate( void ){
	// if we have previously received an SMS a firmware update checking message
	if ( MyFlag.ForceFWUpdate == true ){
		// reset flag
		MyFlag.ForceFWUpdate = false;
		Serial.println("--- CheckFirwareUpdate : FW check on the server");
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
				sprintf(buff, "  Firmware is already up to date." ); 
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
			// sprintf(buff, "  No new firmware found or host not available." ); 
			// Serial.println(buff);
			// SendSMS(MyParam.myphonenumber, buff);	
		// }
	}
}

//----------------------------------------------------------------------
//!\brief	Load params from EEPROM
//----------------------------------------------------------------------
void LoadParamEEPROM() {
	
	EEPROM_readAnything(0, MyParam);
	
	//uncomment this line to erase EEPROM parameters with DEFAULT parameters
	//MyParam.flag_data_written = false;
	
	//check if parameters were already written
	if( MyParam.flag_data_written == false ){
		Serial.println("--- !!! Loading DEFAULT parameters from EEPROM ...  --- ");
		//EEPROM is empty , so load default parameters (see myprivatedata.h)
		MyParam.flag_alarm_low_bat = FLAG_ALARM_LOW_BAT;
		MyParam.flag_alarm_flood = FLAG_ALARM_FLOOD;
		size_t destination_size = sizeof (MyParam.smssecret);
		snprintf(MyParam.smssecret, destination_size, "%s", SMSSECRET);
		destination_size = sizeof (MyParam.myphonenumber);
		snprintf(MyParam.myphonenumber, destination_size, "%s", MYPHONENUMBER);
		MyParam.lipo_level_trig = LIPO_LEVEL_TRIG;
		MyParam.trig_input_level = TRIG_INPUT_LEVEL;
		MyParam.flood_sensor_trig = FLOODSENSOR_TRIG;
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
	sprintf(buff, "  lipo_level_trig = %d%%", MyParam.lipo_level_trig);
	Serial.println(buff);
	sprintf(buff, "  trig_input_level = %2.1fV", MyParam.trig_input_level);
	Serial.println(buff);
	sprintf(buff, "  flood_sensor_trig = %3.1f", MyParam.flood_sensor_trig);
	Serial.println(buff);		
}

//----------------------------------------------------------------------
//!\brief           scheduler()
//----------------------------------------------------------------------
void Scheduler() {
		
	if( (millis() - taskGetLiPo) > PERIOD_LIPO_INFO){
		taskGetLiPo = millis();
		MyFlag.taskGetLiPo = true;
	}	
	
	if( (millis() - taskCheckSMS) > PERIOD_CHECK_SMS){
		taskCheckSMS = millis();
		MyFlag.taskCheckSMS = true;
	}
	
	if( (millis() - taskCheckFlood) > PERIOD_CHECK_FLOOD){
		taskCheckFlood = millis();
		MyFlag.taskCheckFlood = true;
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
	Serial.setTimeout(2000);
	
	// set I/O direction
	pinMode(LEDALARM, OUTPUT);
	pinMode(FLOODSENSOR, INPUT);
	
	delay(5000);
	Serial.println("Pepette.com.Box "); 
	
	// LTask will help you out with locking the mutex so you can access the global data
	LTask.remoteCall(createThreadSerialMenu, NULL);
	Serial.println("Launch threads.");
	
	// GSM setup
	unsigned int nbtry;
	while(!LSMS.ready() && nbtry <= 10){
		delay(1000);
		Serial.println("Please insert SIM");
		nbtry++;
	}
	
	if(LSMS.ready()){
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
	}
	else{
		Serial.println("No SIM Detected.");	
	}
	

	// OTAUpdate.begin("92.245.144.185", "50150", "OTA_pepettebox");
	OTAUpdate.begin("91.224.149.231", "8080", "OTA/OTA_pepettebox");
	
	// load params from EEPROM
	LoadParamEEPROM();
	//print structure
	PrintMyParam();
	
	// init default position in sms menu
	MySMS.menupos = SM_LOGIN;
	
	// for scheduler
	taskGetLiPo = millis();
	taskGetAnalog = millis();
	taskCheckSMS = millis();
	taskStatusSMS = millis();
	
	// set this flag to proceed a first LiPO level read (if an SMS is received before timer occurs)
	MyFlag.taskGetLiPo = true;
	// set this flag to proceed a first analog read (external supply)
	MyFlag.taskGetAnalog = true;
	
	Serial.println("Setup done.");

	// send an SMS to inform user that the device has boot
	sprintf(buff, "RFC is running.\r\n Firmware version : %d", FWVERSION); 
	Serial.println(buff);
	SendSMS(MyParam.myphonenumber, buff);	
}

//----------------------------------------------------------------------
//!\brief           LOOP()
//----------------------------------------------------------------------
void loop() {
	Scheduler();
	GetLiPoInfo();
	//GetAnalogRead();
	//CheckFloodSensor();
	CheckSMSrecept();
	MenuSMS();
	AlertMng();
	CheckFirwareUpdate();
}

//----------------------------------------------------------------------
//!\brief           THREAD DECLARATION
//----------------------------------------------------------------------

boolean createThreadSerialMenu(void* userdata) {
	// The priority can be 1 - 255 and default priority are 0
	// the arduino priority are 245
	vm_thread_create(thread_serialmenu, NULL, 255);
    return true;
}

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