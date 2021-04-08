/*
 * dcDriver.c
 *
 * Created: 2020-02-11 12:03:56 PM
 *  Author: Connor
 */ 

 #include <avr/io.h>
 #include <avr/interrupt.h>
 #include <stdlib.h>
 #include "dcDriver.h"


 void initDCmotor(){
	stopDCmotor();	//Disable motor
	cli();			//disable global interrupts
 

  //DC motor output PORTB pins 3-7 (3/4/5/6 control, 7 PWM)(p.3,80,170)
  DDRB |= 0x80 | (0xF<<DCM_PIN_OFFSET);


 //PWM
 //Frequency = 1MHz / (prescaler*256) because it counts to 0xFF (255)
 //Select PWM frequency of 488 Hz; prescaler = /8
 
	//WGM Fast PWM mode (011)	- bit 3 set to 0 in register B
	//COM0A (10) - OC0A clears on match, and sets at TOP.
	TCCR0A = (0x1<<COM0A1)|(0x1<<WGM01)|(0x1<<WGM00);
			
	
	// CS mode /8 (010)	
	TCCR0B = (0x1<<CS01);	//488Hz  (1MHz / (256*prescaler)

	TCNT0 = 0x00;	//Reset Counter
	OCR0A = 0x80;	//default to 50% duty cycle
	TIMSK0 = 0x03;	//Compare A and overflow interrupts enabled
	
	sei();	//re-enable global interrupts

	TIFR0 = 0x03;	//Clear OCF0A and TOV0 flags, to start timer


 }

 void enableDCmotor(){
	OCR0A = 0xFF;	//Set minimum speed
	TIMSK0 = 0x02;	//Compare A interrupt enabled
	TIFR0 = 0x02;	//Clear OCF0A and TOV0 flags, and start timer
 }

 /*	Do I want to brake in between direction switches? Or is plugging fine? */
 //Plugging is probably not ok. but regenerative braking should be.

 void setDCspeed(signed int speed){
 //		(+/-) = (CW/CCW)
	DCmotorRUNNING = 1;

	 dcm MotorMode;
	 if (speed == 0){		//No speed -> Brake
		MotorMode = BrakeVCC;
		DCmotorRUNNING = 0;
	 }else if(speed > 0){	//Positive speed -> Clockwise direction
		MotorMode = CW;
	 }else{					//Negative speed -> Counter-clockwise direction
		MotorMode = CCW;
		speed = speed * -1;	//Return to positive value
	 }


	 //Set PWM duty cycle (Cast 15 bit to 8 bit number explicitly)
	 OCR0A = speed >> 7;
	 PORTC = speed >> 7;

	
	
	//Set the MotorMode output on the control pins
	PORTB = (PORTB & ~(0xF<<DCM_PIN_OFFSET)) | (MotorMode<<DCM_PIN_OFFSET);

	//PORTC = PORTB;	//displays output as a test
 }

 //This should be run in an ISR for an E-Stop approximation (motor safety - not human safety)
 void stopDCmotor(){
	PORTB &= ~(0xF<<DCM_PIN_OFFSET);	//Coast - don't output anything
	TIMSK0 = 0x0;	//Disable PWM
	OCR0A = 0x00;	//Minimum duty cycle
	DCmotorRUNNING = 0;
}

ISR(TIMER0_COMPA_vect){
	//I just don't want an error to restart the main
}
ISR(TIMER0_OVF_vect){
	//has to exist
}