/*
 * stepperDriver.h
 *
 * Created: 2020-02-15 6:32:19 PM
 *  Author: Connor
 */ 


#ifndef STEPPERDRIVER_H_
#define STEPPERDRIVER_H_


 #define DEGREES_PER_STEP 1.8
 #define STEPS_PER_REV 200

 typedef enum {			//Enum statement to define motor modes
	 Stepper_BrakeVCC = 0b111111,	//Both brake functions are equivalent, but they will not hold position
	 Stepper_BrakeGND = 0b100100,	//Instead. power through a pole to hold position (S1/2/3/4)
	 S1 = 0b110000,
	 S3 = 0b101000,			//Lab manual was wrong - fixed S3/S2 transpose
	 S2 = 0b000110,
	 S4 = 0b000101
	 //	(EN0/L1/L2/EN1/L3/L4)
 } StepperMode_t;
 StepperMode_t StepperMode;

 struct Stepper_Control{
	 int pos_degrees, pos_steps;
	 int dest_degrees, dest_steps;
	 int stepCount, totalStepCount;
	 char sDirection;
 } SM_DC;



 char stepperRUNNINGflag;
 char stepperENABLEDflag;

	

 void curveAcceleration(int step, int total);
 void initStepper();
 void enableStepper();
 void disableStepper();
 void turnStepper(int degrees);
 void calibrateStepper(int degrees);
  //Note: when called, turnStepper will override a previous command, not queue.


#endif /* STEPPERDRIVER_H_ */