/*
 * dcDriver.h
 *
 * Created: 2020-02-15 5:20:59 PM
 *  Author: Connor
 */ 


#ifndef DCDRIVER_H_
#define DCDRIVER_H_

typedef enum {			//Enum statement to define motor modes
 BrakeVCC = 0b1111,
 BrakeGND = 0b0011,
 CW =		0b1011,
 CCW =		0b0111
 //		INA,INB,ENA,ENB
 } dcm;
 
 #define DCM_PIN_OFFSET 3

 char DCmotorRUNNING;

 void initDCmotor();
 void enableDCmotor();
 void setDCspeed(signed int speed);
 void stopDCmotor();


#endif /* DCDRIVER_H_ */