/*
 * stepperDriver.c
 *
 * Created: 2020-02-11 12:03:34 PM
 *  Author: Connor
 */ 

 #include <avr/io.h>
 #include <avr/interrupt.h>
 #include <stdlib.h>
 #include "stepperDriver.h"



 void initStepper(){
	SM_DC.pos_degrees = 0, SM_DC.pos_steps = 0;
	SM_DC.dest_degrees = 0, SM_DC.dest_steps = 0;
	SM_DC.stepCount = 0, SM_DC.totalStepCount = 0;

	DDRA |= 0x3F;	//Set Port A data direction

	//Initialize pulse timer
	//Google says stepper motors generally operate at <20kHz
	TCCR2A = 0x02;	//CTC mode (WMG bit 2)
	TCCR2B = 0x04;	//prescaler  /64 (15,625Hz)

	//TEST - I'm making it way slower
	//TCCR2B = (1<<CS21) | (1<<CS20);	// prescaler /128
	TCCR2B = (1<<CS22) | (1<<CS21);	// prescaler /256

	OCR2A = 0x7D;	//125 ticks, 

	TCNT2 = 0x00;	//Reset counter
	TIMSK2 = 0x00;	//Disable interrupts
	PORTA = 0x00;	//Set output to nothing (coast)

	stepperENABLEDflag = 1;
 }


 void enableStepper(){
	stepperENABLEDflag = 1;		//enable the output
	TCNT2 = 0x00;	//Reset counter
 }

void disableStepper(){
	stepperENABLEDflag = 0;
	SM_DC.stepCount = 0;
	TIMSK2 = 0x00;
	PORTA = 0x00;	//Disable completely - coasts instead of braking
}

void turnStepper(int degrees){
//Move the stepper motor a specified number of degrees CW (+) or CCW (-)
//Use Timer2 to generate interrupts, which decrement the stepCount, and track the pos_steps

	if ((stepperENABLEDflag == 0) | (degrees == 0)) return;

	if (degrees>0) SM_DC.sDirection = 1;				//Set direction to CW if positive degrees
	if (degrees<0) SM_DC.sDirection = 0;				//Set direction to CCW if negative degrees

	
	SM_DC.stepCount = STEPS_PER_REV*(degrees/360);	//adds revolutions to stepCount
	degrees = degrees % 360;					//Removes revolutions from degrees


	SM_DC.dest_degrees = (SM_DC.pos_degrees + degrees);					//set destination in degrees
	if (SM_DC.dest_degrees  <  0 ) SM_DC.dest_degrees += 360;			//Normalize destination
	if (SM_DC.dest_degrees >= 360) SM_DC.dest_degrees -= 360;
	SM_DC.pos_degrees = SM_DC.dest_degrees;								//update pos_degrees


	//Set # of steps to step
	SM_DC.dest_steps = (int)(SM_DC.dest_degrees / DEGREES_PER_STEP);	//set destination in steps
	int step_dist = SM_DC.dest_steps - SM_DC.pos_steps;					//Find distance in steps
	if (step_dist < 0) step_dist += STEPS_PER_REV;			//Normalize Distance to FWD


	if (SM_DC.sDirection == 1) SM_DC.stepCount += step_dist;		//steps if FWD
	else SM_DC.stepCount += STEPS_PER_REV - step_dist;		//steps if REV
	
	SM_DC.totalStepCount = SM_DC.stepCount;					//Save steps for acc. curve


	//Control the timer for the steps' pulses
	TCNT2 = 0x00;	//Reset Timer 2
	TIMSK2 = 0x02;	//Enable Timer 2 interrupt
	TIFR2 = 0x03;	//Start Timer 2

	//THIS WORKS! (Except for the interrupts)
	//PORTC = totalStepCount;

	stepperRUNNINGflag = 1;	//not evaluated here, just checkable from main
}

ISR(TIMER2_COMPA_vect){

	if(stepperENABLEDflag == 0){		//This should never occur
		disableStepper();			//re-disable in case this ISR is called somehow
		return;
	}

	curveAcceleration(SM_DC.stepCount,SM_DC.totalStepCount);

	if(SM_DC.stepCount-- <= 0){			//stop timer on completion, and hold position
		TIMSK2 = 0x00;
		stepperRUNNINGflag = 0;
	}else {

		if (SM_DC.sDirection == 1){		//FWD cycles forward
			switch (StepperMode){
				case S1: StepperMode = S2; break;
				case S2: StepperMode = S3; break;
				case S3: StepperMode = S4; break;
				case S4: StepperMode = S1; break;

				default: StepperMode = S1; break;
			}
			SM_DC.pos_steps++;
		}else{
			switch (StepperMode){		//REV cycles backward
				case S4: StepperMode = S3; break;
				case S3: StepperMode = S2; break;
				case S2: StepperMode = S1; break;
				case S1: StepperMode = S4; break;

				default: StepperMode = S1; break;
			}
			SM_DC.pos_steps--;
		}
	
		//Normalize position
		SM_DC.pos_steps = SM_DC.pos_steps % STEPS_PER_REV;
		if (SM_DC.pos_steps < 0) SM_DC.pos_steps += STEPS_PER_REV;


		//Set the output to step the motor
		PORTA = StepperMode;

		//PORTC = StepperMode;	//Mode Test	- outputs too fast i think. or its 0
		//PORTC = SM_DC.stepCount;		//Count Test

	}
}

void calibrateStepper(int degrees){
	degrees = degrees % 360;					//Normalize degrees
	SM_DC.pos_degrees = degrees;				//calibrate pos
	SM_DC.pos_steps = degrees/DEGREES_PER_STEP;	//calibrate pos_steps
	SM_DC.dest_degrees = degrees;				//reset dest
}

void curveAcceleration(int step, int total){
//Given our prescaler /64 and a 2-bit period
//15,625Hz to 61Hz	-	78 to 0.305
	
//freq  = (1MHz/prescaler) / OCR2A
	//Use freq (inverse OCR2A) to be linear with respect to speed
	int f_min = 15625 / 0xFF;	//0.305 rev/s
	int f_max = 15625 / 0x15;	//3.72 rev/s
	int ramp_length = 20;	//steps per accel/deccel ramp
	//Note: integral is speed, increase any value to be faster

	int slope = (f_max-f_min)/ramp_length;
	int clipping = total - 2*ramp_length;
	if (clipping <0) ramp_length += (clipping/2);

	int f_out;
	

	if ((total - step) < ramp_length){	//acceleration
		f_out = f_min + (total - step)*slope;

	}else if(step <= ramp_length){	//deceleration
		f_out = f_min + (step-1)*slope;

	}else{								//normal speed
		f_out = f_max;
	}

	OCR2A = (15625/f_out) -1;	//-1 because it gives 256 and overflows

}
