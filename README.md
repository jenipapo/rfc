rfc
============

More info about linkitone : 

- http://labs.mediatek.com/
- http://www.seeedstudio.com/depot/LinkIt-ONE-p-2017.html

![Linkitone pic](/docs/Linkitone.jpg)

How it works ?
============

The tracker is composed with a shield plugged in the linkitone. It allows DC/DC conversion (9-36V to 5V) and GPIO interfaces.

 * Manage a GPIO by SMS command.
 * Fully configurable by SMS.
 * SMS alert only.
 * Read voltage input and can set an alarm on low power.
 * Monitor LiPo cell voltage and can set an alarm on low power.
 * Serial messages are for debug purpose only.
 
 Other possibilities :
 
 * Monitor temperature(s), pressure, temperature, shocks ... (choose your sensor !)
 * Wifi for tracking/logging position while local wifi available

Applications
============ 

 * My application : monitor and control my heating system
 

Principles
============

##Analog inputs

The device can monitor some analog inputs :

- LiPo battery : linkitone API only return these levels : 0 - 33 - 66 - 100% . So, by default, an SMS alert is sent to your phone when LiPo voltage is lower than 33%. This trigger is not customizable by SMS.

- Power supply voltage : device read voltage of this input and can send an SMS when is lower than a trigger. This value can be set by SMS. Default value is 11.6V (for a 12V lead battery).

##Digital inputs

The device can monitor digital input.

##Settings

A lot of parameters can bu set by SMS. You need to send your secret code to the device to receive the menu. Default secret code is *1234*.

Available settings are :

- Change phone number where are sent alarms
- Set periodic SMS ON/OFF. When true, the device will send you a daily status SMS.
- Set low power alarm ON/OFF. When true, the device will send you an SMS when it detects low voltage on analog inputs.
- Change low power voltage trigger : you can set voltage for analog input, when voltage is lower than this value, device will send you SMS.
- Restore factory settings.

## Serial port

Serial port is available on micro USB connector. It is only for debugging purpose. No maintenance or configuration messages are available through this serial port.

Instructions
============

Don't forget to set your settings in this file :

	~/robomowtrak/myprivatedata.h
	

Compile files and uploard to linkitone with arduino IDE (follow instructions in Developer's guide)

http://labs.mediatek.com/site/global/developer_tools/mediatek_linkit/sdk_intro/index.gsp

User 'modem port' to check debug message (SIM detected, GPS Fix, ...)

Software
============
SDK version must be greater or equal to 1.0.42 ! (previous version has a bug in SMS librarie).
So do not forget to upgrade your firmware before programming your linkitone !
Software updater is in your arduino directory :

		~\Arduino\hardware\tools\mtk\FirmwareUpdater.exe

# Libraries needed
install this lib :
http://playground.arduino.cc/Main/RunningMedian		
		
Hardware
============
- linkitone
http://www.seeedstudio.com/depot/LinkIt-ONE-p-2017.html



Troubleshooting
============

If data in EEPROM are corrupted, in the function LoadParamEEPROM() please uncomment 

	MyParam.flag_data_written = false;
	
Compile the code, run it once (at least) and recompile the code this line commented.
Corrupted data in EEPROM can do impossible connexion to linkitone by SMS (secret code corrupted!).

### Can't reach host
	Verify your settings in :
	
	<code>OTAUpdate.begin("my.server.ip.adress", "port", "my/dir");</code>
	
	or
	
	<code>LGPRS.attachGPRS("my", NULL, NULL)</code>
	
### md5 file is downloaded but it can't be parsed

	Verify that your linkitone has the /OTA directory in memory. Check throught USB mass storage. If OTA is not present, read the first chapter of this page, you may have miss something...
	