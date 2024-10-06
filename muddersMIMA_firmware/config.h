//Copyright 2022-2023(c) John Sullivan


//config.h - compile time configuration parameters

#ifndef config_h
	#define config_h
	#include "muddersMIMA.h"  //For Arduino IDE compatibility

	#define FW_VERSION "0.2.0"
    #define BUILD_DATE "2024JUN05"

	#define CPU_MAP_ATMEGA328p
    
    #define HW_REVA_REVB

	#define INVERT_JOYSTICK_DIRECTION //comment to mirror joystick assist and regen directions

  //Adjusts output for sliders that output 20-84% range rather than 5-95%. Added for Balto.
  //#define SLIDER_IS_INSTALLED

  //Maximum engine RPM before assist is disabled
  const uint16_t MAX_RPM = 5500; 

  //Configurable delay after clutch depressed before assist re-enabled (in ms)
  const uint16_t CLUTCH_DELAY = 500; 

  //RPM under which the IMA will derate output to avoid errors
  const uint16_t DERATE_UNDER_RPM = 2000; 

  //Output percent to derate to if under DERATE_UNDER_RPM
  const uint8_t DERATE_PERCENT = 75; 

  //Time in ms to ramp from 0 to joystick value
  const uint16_t RAMP_UP_DURATION = 0;

	//choose behavior when three position switch...
	//...is in the '0' position
		  #define MODE0_BEHAVIOR() mode_OEM()
		//#define MODE0_BEHAVIOR() mode_manualAssistRegen_withAutoStartStop();
		//#define MODE0_BEHAVIOR() mode_manualAssistRegen_ignoreECM();
		//#define MODE0_BEHAVIOR() mode_INWORK_PHEV_mudder();

	//...is in the '1' position
		//#define MODE1_BEHAVIOR() mode_OEM()
	  	//#define MODE1_BEHAVIOR() mode_manualAssistRegen_withAutoStartStop();
		//#define MODE1_BEHAVIOR() mode_manualAssistRegen_ignoreECM();
		//#define MODE1_BEHAVIOR() mode_INWORK_PHEV_mudder();
    	#define MODE1_BEHAVIOR() mode_INWORK_PHEV_AfterEffect();

	//...is in the '2' position
		//#define MODE2_BEHAVIOR() mode_OEM()
		//#define MODE2_BEHAVIOR() mode_manualAssistRegen_withAutoStartStop();
	  	  #define MODE2_BEHAVIOR() mode_manualAssistRegen_ignoreECM();
		//#define MODE2_BEHAVIOR() mode_INWORK_PHEV_mudder();

#endif