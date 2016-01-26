//--------------------------------------------------
//! \file		myperiod.h
//! \brief		header file for period tuning
//! \date		2015-Mar
//! \author		minbiocabanon
//--------------------------------------------------

//----------------------------------------------------------------------
//!\brief	These are DEFAULTS parameters !
//----------------------------------------------------------------------

//#define PERIOD_CHECK_FLOOD		3600000		// 60min. ,interval between 2 flood sensor check (can send an SMS alert if water is detected)
#define PERIOD_CHECK_FLOOD			30000		// 60min. ,interval between 2 flood sensor check (can send an SMS alert if water is detected)
#define PERIOD_CHECK_ANALOG_LEVEL 	3600000		// 60min. , interval between 2 analog level check (can send an SMS alert if level are low)

// !!! DO  NOT MODIFY !!!!
#define PERIOD_LIPO_INFO		120000		// 2 min. ,interval between 2 battery level measurement, in milliseconds
#define PERIOD_READ_ANALOG		120000		// 2 min. ,interval between 2 analog input read (external supply), in milliseconds
#define PERIOD_CHECK_SMS		1000		// 1 sec., interval between 2 SMS check, in milliseconds
#define TIMEOUT_SMS_MENU		300000		// 5 min., when timeout, SMS menu return to login (user should send password again to log), in milliseconds