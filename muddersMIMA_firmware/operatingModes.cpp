//Copyright 2022-2023(c) John Sullivan

//Send MCM control data, depending on IMA mode

#include "muddersMIMA.h"

uint8_t joystick_percent_stored = JOYSTICK_NEUTRAL_NOM_PERCENT;
bool useStoredJoystickValue = NO; //JTS2doLater: I'm not convinced this is required

// Variables to track clutch state, release time, and ramp up
bool clutchPressed = false;
uint32_t clutchReleaseTime = 0;
bool rampingUp = false;
uint32_t rampStartTime = 0;
bool derating = false;
uint32_t derateStartTime = 0;

/////////////////////////////////////////////////////////////////////////////////////////////

void mode_OEM(void)
{
	brakeLights_setControlMode(BRAKE_LIGHT_OEM); 
	mcm_passUnmodifiedSignals_fromECM();
}

/////////////////////////////////////////////////////////////////////////////////////////////

//PHEV mode
//JTS2doNow: implement manual regen
void mode_INWORK_manualRegen_autoAssist(void)
{
	brakeLights_setControlMode(BRAKE_LIGHT_OEM);

	if(ecm_getMAMODE1_state() == MAMODE1_STATE_IS_REGEN) { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE, JOYSTICK_NEUTRAL_NOM_PERCENT); } //ignore regen request
	else /* (ECM not requesting regen) */                { mcm_passUnmodifiedSignals_fromECM(); } //pass all other signals through
}

/////////////////////////////////////////////////////////////////////////////////////////////

//LiControl completely ignores ECM signals (including autostop, autostart, prestart, etc)
void mode_manualAssistRegen_ignoreECM(void)
{
	brakeLights_setControlMode(BRAKE_LIGHT_AUTOMATIC);

	uint16_t joystick_percent = adc_readJoystick_percent();

	if     (joystick_percent < JOYSTICK_MIN_ALLOWED_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE,   JOYSTICK_NEUTRAL_NOM_PERCENT); } //signal too low
	else if(joystick_percent < JOYSTICK_NEUTRAL_MIN_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_REGEN,  joystick_percent);             } //manual regen
	else if(joystick_percent < JOYSTICK_NEUTRAL_MAX_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE,   joystick_percent);             } //standby
	else if(joystick_percent < JOYSTICK_MAX_ALLOWED_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_ASSIST, joystick_percent);             } //manual assist
	else                                                     { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE,   JOYSTICK_NEUTRAL_NOM_PERCENT); } //signal too high
}

/////////////////////////////////////////////////////////////////////////////////////////////

void mode_manualAssistRegen_withAutoStartStop(void)
{
	brakeLights_setControlMode(BRAKE_LIGHT_AUTOMATIC);

	if( (ecm_getMAMODE1_state() == MAMODE1_STATE_IS_REGEN ) ||
		(ecm_getMAMODE1_state() == MAMODE1_STATE_IS_IDLE  ) ||
		(ecm_getMAMODE1_state() == MAMODE1_STATE_IS_ASSIST)  )
	{
		//ECM is sending assist, idle, or regen signal...
		//but we're in manual mode, so use joystick value instead (either previously stored or value right now)

		uint16_t joystick_percent = adc_readJoystick_percent();

		if(gpio_getButton_momentary() == BUTTON_PRESSED)
		{
			//store joystick value when button is pressed
			joystick_percent_stored = joystick_percent;
			useStoredJoystickValue = YES;
		}

		//disable stored joystick value if user is braking
		//JTS2doLater: Add clutch disable
		if(gpio_getBrakePosition_bool() == BRAKE_LIGHTS_ARE_ON)
		{
			useStoredJoystickValue = NO;
			joystick_percent_stored = JOYSTICK_NEUTRAL_NOM_PERCENT;
		} 

		//use stored joystick value if conditions are right
		if( (useStoredJoystickValue == YES                ) && //user previously pushed button
			(joystick_percent > JOYSTICK_NEUTRAL_MIN_PERCENT) && //joystick is neutral
			(joystick_percent < JOYSTICK_NEUTRAL_MAX_PERCENT)  ) //joystick is neutral
		{
			//replace actual joystick position with previously stored value
			joystick_percent = joystick_percent_stored;
		}
		
		//send assist/idle/regen value to MCM
		if     (joystick_percent < JOYSTICK_MIN_ALLOWED_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE,   JOYSTICK_NEUTRAL_NOM_PERCENT); } //signal too low
		else if(joystick_percent < JOYSTICK_NEUTRAL_MIN_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_REGEN,  joystick_percent);             } //manual regen
		else if(joystick_percent < JOYSTICK_NEUTRAL_MAX_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE,   joystick_percent);             } //standby
		else if(joystick_percent < JOYSTICK_MAX_ALLOWED_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_ASSIST, joystick_percent);             } //manual assist
		else                                                     { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE,   JOYSTICK_NEUTRAL_NOM_PERCENT); } //signal too high

	}
	else if(ecm_getMAMODE1_state() == MAMODE1_STATE_IS_PRESTART)
	{
		//prevent DCDC disable when user regen-stalls car

		//DCDC converter must be disabled when the key first turns on.
		//Otherwise, the DCDC input current prevents the HVDC capacitors from pre-charging through the precharge resistor.
		//This can cause intermittent P1445, particularly after rapidly turning key off and on.

		//Therefore, we need to honor the ECM's PRESTART request for the first few seconds after keyON (so the HVDC bus voltage can charge to the pack voltage).

		//JTS2doNow: if SoC too low (get from LiBCM), pass through unmodified signal (which will disable DCDC) 
		if(millis() < (time_latestKeyOn_ms() + PERIOD_AFTER_KEYON_WHERE_PRESTART_ALLOWED_ms)) { mcm_passUnmodifiedSignals_fromECM(); } //key hasn't been on long enough
		else { mcm_setAllSignals(MAMODE1_STATE_IS_AUTOSTOP, JOYSTICK_NEUTRAL_NOM_PERCENT); } //JTS2doLater: This prevents user from manually assist-starting IMA

		//clear stored assist/idle/regen setpoint
		joystick_percent_stored = JOYSTICK_NEUTRAL_NOM_PERCENT;
		useStoredJoystickValue = NO;
	}
	else //ECM is sending autostop, start, or undefined signal
	{
		//pass these signals through unmodified (so autostop works properly)
		mcm_passUnmodifiedSignals_fromECM();

		//clear stored assist/idle/regen setpoint
		joystick_percent_stored = JOYSTICK_NEUTRAL_NOM_PERCENT;
		useStoredJoystickValue = NO;
	}

	//JTS2doLater: New feature: When the key is on and the engine is off, pushing momentary button starts engine.
}

/////////////////////////////////////////////////////////////////////////////////////////////

//GOAL: All OEM signals are passed through unmodified, except:
//CMDPWR assist
	//LiControl uses strongest assist request (user or ECM), except that;
	//pressing the momentary button stores the joystick position (technically the value is stored on button release)
	//after pressing the momentary button, all ECM assist requests are ignored until the user either brakes or (temporarily) changes modes   
	//manual joystick assist requests are allowed even after pushing momentary button; stored value resumes once joystick is neutral again
//CMDPWR regen
	//LiControl ignores ECM regen requests, unless user is braking
	//when braking and joystick is neutral, LiControl uses ECM regen request
	//manual joystick regen request always overrides ECM regen request
//MAMODE1 prestart
	//modified to always enable DCDC when key is on
void mode_INWORK_PHEV_mudder(void)
{
	brakeLights_setControlMode(BRAKE_LIGHT_MONITOR_ONLY); //JTS2doLater: if possible, add strong regen brake lights

	if( (ecm_getMAMODE1_state() == MAMODE1_STATE_IS_REGEN ) ||
		(ecm_getMAMODE1_state() == MAMODE1_STATE_IS_IDLE  ) ||
		(ecm_getMAMODE1_state() == MAMODE1_STATE_IS_ASSIST)  )
	{
		//ECM is sending assist, idle, or regen signal

		uint8_t joystick_percent = adc_readJoystick_percent();
		uint8_t ECM_CMDPWR_percent = ecm_getCMDPWR_percent();

		if (ECM_CMDPWR_percent > joystick_percent) { joystick_percent = ECM_CMDPWR_percent; } //choose strongest assist request (user or ECM)

		if(gpio_getButton_momentary() == BUTTON_PRESSED)
		{
			//store joystick value when button is pressed
			joystick_percent_stored = joystick_percent;
			useStoredJoystickValue = YES;
		}

		//disable stored joystick value if user is braking
		//JTS2doLater: Add clutch disable
		if(gpio_getBrakePosition_bool() == BRAKE_LIGHTS_ARE_ON)
		{
			useStoredJoystickValue = NO;
			joystick_percent_stored = JOYSTICK_NEUTRAL_NOM_PERCENT;	
		} 

		//Use ECM regen request when user is braking AND joystick is neutral
		if ((joystick_percent > JOYSTICK_NEUTRAL_MIN_PERCENT)     && //joystick is neutral
			(joystick_percent < JOYSTICK_NEUTRAL_MAX_PERCENT)     && //joystick is neutral
			(gpio_getBrakePosition_bool() == BRAKE_LIGHTS_ARE_ON)  ) //user is braking
		{
			//while braking, replace neutral joystick position with ECM regen request
			joystick_percent = ecm_getCMDPWR_percent();
		}

		//use stored joystick value if conditions are right
		if( (useStoredJoystickValue == YES                  ) && //user previously pushed button
			(joystick_percent > JOYSTICK_NEUTRAL_MIN_PERCENT) && //joystick is neutral
			(joystick_percent < JOYSTICK_NEUTRAL_MAX_PERCENT)  ) //joystick is neutral
		{
			//replace actual joystick position with previously stored value
			joystick_percent = joystick_percent_stored;
		}
		
		//send assist/idle/regen value to MCM
		if     (joystick_percent < JOYSTICK_MIN_ALLOWED_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE,   JOYSTICK_NEUTRAL_NOM_PERCENT); } //signal too low
		else if(joystick_percent < JOYSTICK_NEUTRAL_MIN_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_REGEN,  joystick_percent);             } //manual regen
		else if(joystick_percent < JOYSTICK_NEUTRAL_MAX_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE,   joystick_percent);             } //standby
		else if(joystick_percent < JOYSTICK_MAX_ALLOWED_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_ASSIST, joystick_percent);             } //manual assist
		else                                                     { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE,   JOYSTICK_NEUTRAL_NOM_PERCENT); } //signal too high

	}
	else if(ecm_getMAMODE1_state() == MAMODE1_STATE_IS_PRESTART)
	{
		//prevent DCDC disable when user regen-stalls car

		//DCDC converter must be disabled when the key first turns on.
		//Otherwise, the DCDC input current prevents the HVDC capacitors from pre-charging through the precharge resistor.
		//This can cause intermittent P1445, particularly after rapidly turning key off and on.

		//Therefore, we need to honor the ECM's PRESTART request for the first few seconds after keyON (so the HVDC bus voltage can charge to the pack voltage).

		//JTS2doNow: if SoC too low (get from LiBCM), pass through unmodified signal (which will disable DCDC) 
		if(millis() < (time_latestKeyOn_ms() + PERIOD_AFTER_KEYON_WHERE_PRESTART_ALLOWED_ms)) { mcm_passUnmodifiedSignals_fromECM(); } //key hasn't been on long enough
		else { mcm_setAllSignals(MAMODE1_STATE_IS_AUTOSTOP, JOYSTICK_NEUTRAL_NOM_PERCENT); } //JTS2doLater: This prevents user from manually assist-starting IMA

		//clear stored assist/idle/regen setpoint
		joystick_percent_stored = JOYSTICK_NEUTRAL_NOM_PERCENT;
		useStoredJoystickValue = NO;
	}
	else //ECM is sending autostop, start, or undefined signal
	{
		//pass these signals through unmodified (so autostop works properly)
		mcm_passUnmodifiedSignals_fromECM();

		//clear stored assist/idle/regen setpoint
		joystick_percent_stored = JOYSTICK_NEUTRAL_NOM_PERCENT;
		useStoredJoystickValue = NO;
	}

	//JTS2doLater: New feature: When the key is on and the engine is off, pushing momentary button starts engine.
}

/////////////////////////////////////////////////////////////////////////////////////////////



//Heavily based on Mudders code above. Added a max RPM to prevent redline, derating logic under 2k RPM and ramp-up logic to smoothly transition between states. 
void mode_INWORK_PHEV_AfterEffect(void)
{
    brakeLights_setControlMode(BRAKE_LIGHT_MONITOR_ONLY);

    // Check if ECM is sending assist, idle, or regen signal
    if( (ecm_getMAMODE1_state() == MAMODE1_STATE_IS_REGEN ) ||
        (ecm_getMAMODE1_state() == MAMODE1_STATE_IS_IDLE  ) ||
        (ecm_getMAMODE1_state() == MAMODE1_STATE_IS_ASSIST)  )
    {
        uint8_t joystick_percent = adc_readJoystick_percent();
        uint8_t ECM_CMDPWR_percent = ecm_getCMDPWR_percent();

        // Prioritize ECM command over joystick if stronger
        if (ECM_CMDPWR_percent > joystick_percent) { 
            joystick_percent = ECM_CMDPWR_percent; 
        }

        // Handle clutch interaction
        if (gpio_getClutchPosition() == CLUTCH_PEDAL_PRESSED)
        {
            clutchPressed = true;
            clutchReleaseTime = millis();
        }
        else if (clutchPressed && (millis() - clutchReleaseTime > CLUTCH_DELAY))
        {
            clutchPressed = false;
        }

        // Disable assist if clutch is pressed
        if (clutchPressed)
        {
            joystick_percent = JOYSTICK_NEUTRAL_NOM_PERCENT; // No assist when clutch is pressed
        }

        // Handle maximum RPM logic
        uint16_t currentRPM = engineSignals_getLatestRPM();
        if (currentRPM >= MAX_RPM)
        {
            joystick_percent = JOYSTICK_NEUTRAL_NOM_PERCENT; // Disable assist at max RPM
        }
        else if (currentRPM < DERATE_UNDER_RPM && !derating)
        {
            // Apply derating when RPM is under threshold
            joystick_percent = (joystick_percent * DERATE_PERCENT) / 100; // Derating joystick value
            ECM_CMDPWR_percent = (ECM_CMDPWR_percent * DERATE_PERCENT) / 100; // Derating OEM signal
            derating = true;
            derateStartTime = millis();
        }
        else if (currentRPM >= DERATE_UNDER_RPM && derating)
        {
            // Re-enable assist with ramp-up when RPM exceeds DERATE_UNDER_RPM
            derating = false;
            rampingUp = true;
            rampStartTime = millis();
        }

        // Ramp up assist after clutch is released or exiting derating
        if (!clutchPressed && rampingUp && joystick_percent > JOYSTICK_NEUTRAL_NOM_PERCENT)
        {
            uint32_t rampDuration = millis() - rampStartTime;
            if (rampDuration < RAMP_UP_DURATION)
            {
                // Gradually increase joystick percent
                joystick_percent = (joystick_percent * rampDuration) / RAMP_UP_DURATION;
            }
            else
            {
                rampingUp = false; // Ramp-up complete
            }
        }

        // Use ECM regen request when the user is braking and joystick is neutral
        if ((joystick_percent > JOYSTICK_NEUTRAL_MIN_PERCENT)     && // Joystick is neutral
            (joystick_percent < JOYSTICK_NEUTRAL_MAX_PERCENT)     && // Joystick is neutral
            (gpio_getBrakePosition_bool() == BRAKE_LIGHTS_ARE_ON)  ) // User is braking
        {
            // Replace neutral joystick position with ECM regen request while braking
            joystick_percent = ecm_getCMDPWR_percent();
        }

        // Send assist/idle/regen value to MCM based on either braking or joystick position
        if ((joystick_percent < JOYSTICK_NEUTRAL_MIN_PERCENT) || (gpio_getBrakePosition_bool() == BRAKE_LIGHTS_ARE_ON)) 
        {
            // If the joystick is below neutral (regen) OR the brake is pressed, send regen signal
            mcm_setAllSignals(MAMODE1_STATE_IS_REGEN, joystick_percent);
        }
        else if (joystick_percent < JOYSTICK_NEUTRAL_MAX_PERCENT)
        {
            // If joystick is in neutral range and brake is not pressed, go to idle
            mcm_setAllSignals(MAMODE1_STATE_IS_IDLE, joystick_percent);
        }
        else if (joystick_percent < JOYSTICK_MAX_ALLOWED_PERCENT)
        {
            // If joystick is above neutral, send assist signal
            mcm_setAllSignals(MAMODE1_STATE_IS_ASSIST, joystick_percent);
        }
        else
        {
            // Invalid signal (joystick percent too high), fallback to idle
            mcm_setAllSignals(MAMODE1_STATE_IS_IDLE, JOYSTICK_NEUTRAL_NOM_PERCENT);
        }
    }
    else if (ecm_getMAMODE1_state() == MAMODE1_STATE_IS_PRESTART)
    {
        // Handle prestart logic
        if (millis() < (time_latestKeyOn_ms() + PERIOD_AFTER_KEYON_WHERE_PRESTART_ALLOWED_ms)) { 
            mcm_passUnmodifiedSignals_fromECM(); 
        } else { 
            mcm_setAllSignals(MAMODE1_STATE_IS_AUTOSTOP, JOYSTICK_NEUTRAL_NOM_PERCENT); 
        }

        // Clear stored joystick value
        joystick_percent_stored = JOYSTICK_NEUTRAL_NOM_PERCENT;
        useStoredJoystickValue = NO;
    }
    else
    {
        // Pass through other signals unmodified
        mcm_passUnmodifiedSignals_fromECM();

        // Clear stored joystick value
        joystick_percent_stored = JOYSTICK_NEUTRAL_NOM_PERCENT;
        useStoredJoystickValue = NO;
    }
}





void operatingModes_handler(void)
{
	uint8_t toggleState = gpio_getButton_toggle();
	static uint8_t toggleState_previous = TOGGLE_UNDEFINED;

	if(toggleState != toggleState_previous)
	{
		//clear previously stored joystick value (from the last time we were in manual mode)
		joystick_percent_stored = JOYSTICK_NEUTRAL_NOM_PERCENT;
		useStoredJoystickValue = NO;
	}

	if     (toggleState == TOGGLE_POSITION0) { MODE0_BEHAVIOR(); } //see #define substitutions in config.h
	else if(toggleState == TOGGLE_POSITION1) { MODE1_BEHAVIOR(); }
	else if(toggleState == TOGGLE_POSITION2) { MODE2_BEHAVIOR(); }
	else /* hidden 'mode3' (unsupported) */  { MODE0_BEHAVIOR(); }

	toggleState_previous = toggleState;
}