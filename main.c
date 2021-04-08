/*
 Milestone 4
 Program 1
 Project: ObjectSorter
 Group: 6
 Name 1: Connor Wiebe V00872959
 Name 2: Dai Minh Tran V00928014
 Desc: DC and Stepper motor drivers
 */

 //Port A	-	Stepper motor
 //Port B	-	DC motor (Control & PWM)
 //Port C	-	LED bank
 //Port D	-	Button input
 //Port E	-	???
 //Port F	-	ADC input

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include "dcDriver.h"
#include "stepperDriver.h"
#include "myqueue.h"


	int metal_av, albedo_av, hall_av;
	int metal_top, albedo_top;

	char t3A, t3B, t3C;
	int *t3A_ptr, *t3B_ptr, *t3C_ptr;

	void debounce(int* flag_var, int ms);

	

int main(void)
{


/*			Lots of initialiizations I bet			*/



//Main Flags	-	Use their bits as flags of different types
	int Metal_Detector_latch=0, Reflectometer_latch=0, Hall_Sensor_latch=0, on_deck_latch=0;
	int Metal_Detector_pulse=0, Reflectometer_pulse=0, Hall_Sensor_pulse=0, on_deck_pulse=0;
	
	int dump_flag = 0;
	
	Q Item_Q;
	Item_Q = Q_new();
	material tray_state = ERROR;
	material dump_check = ERROR;
	material next_item = ERROR;		//the material of the next item to be added to the queue
	material next_metal = ERROR;	//the metal type of the next item to be added to the buffer
	char read_next = 0;

	char dump_early = 0;
	char dest_bin = 0;
	char next_bin = 0;

	//ADC is 10-bit, but 10 bit precision is unneccesary, so the 8 MSB will be evaluated
	const unsigned char ALUMINUM_THRESH = 100;
	const unsigned char STEEL_THRESH = 200;
	const unsigned char ALBEDO_THRESH_METAL = 200;
	const unsigned char ALBEDO_THRESH_HIGH = 150;
	const unsigned char ALBEDO_THRESH_LOW = 60;
	const unsigned char HALL_THRESH = 120;

	
	

	//wait for the start button
	while((PORTD & 0x01) == 0x01);

    while (1)	//***************	MAIN LOOP	******************
    {

	/*	*	*	*	*	INPUTS	:	SENSORS,	BUTTONS		*	*	*	*	*/

		//reset pulse flags
		Metal_Detector_pulse = 0;
		Reflectometer_pulse = 0;
		on_deck_pulse = 0;

		/*		Poll inputs		*/
		
		//Device on deck laser sensor
		on_deck_latch = ((PORTE & 0x02)==0x02);
		//TODO - edit debounce to accept a rising edge option (set after x ms) then use it here... maybe


		//METAL DETECTOR (analog)
		if ((metal_av > ALUMINUM_THRESH) && !(Metal_Detector_latch)){	//Above the metallic threshold:
			Metal_Detector_latch = 1;								//Latch the input
		}else{												//Below the metallic threshold:
			debounce(&Metal_Detector_latch, 25);				//Unlatch the input
			Metal_Detector_pulse = 1;						//set a one scan pulse
		}


		//ALBEDO SENSOR (analog)
		//Above or below the thresholds  i.e. something is there
		if (((albedo_av > ALBEDO_THRESH_HIGH) || (albedo_av < ALBEDO_THRESH_LOW)) && !(Reflectometer_latch)){
			Reflectometer_latch = 1;							//Latch the input

		}else{												//In the ambient range:
			debounce(&Reflectometer_latch, 25);				//Unlatch the input
			Reflectometer_pulse = 1;							//set a one scan pulse
		}
		

		//HALL SENSOR (analog probably)	-	 I don't know what it's output will be like so I won't waste time guessing
		Hall_Sensor_latch = (hall_av > HALL_THRESH) ? 1 : 0;	//tells you if the hall effect sensor senses a thing
			// (Logical Condition) ? if True : if False;
			//This is the Ternary Operator
	



	/*		CONDITIONAL LOGIC - STATE MACHINES		*/

	//Metal Detector State Machine
	if (Metal_Detector_latch){					//if an item is in front of the sensor
			if (metal_av > metal_top){
				metal_top = metal_av;			//find maximum... capacitance? in item
												//Select ALUMINUM or STEEL
				next_metal = (metal_top > STEEL_THRESH) ? STEEL : ALUMINUM;
			}
	}

	if (Metal_Detector_pulse){					//After the item passes the sensor
		write_metal_buffer(&Item_Q,next_metal);	//Add the item to the buffer
		metal_top = 0;							//Reset max metal detector-icity
	}



	//Reflectometer State Machine
	if (Reflectometer_latch){					//if an item is in front of the sensor

		if (albedo_av > ALBEDO_THRESH_HIGH){	//Above the shiny threshold:
			if (albedo_av > albedo_top){
				albedo_top = albedo_av;			//find maximum albedo in item
												//Select WHITE or METAL
				next_item = (albedo_top > ALBEDO_THRESH_METAL) ? METAL : WHITE;
			}	
		}

		if(albedo_av < ALBEDO_THRESH_LOW){		//Below the dark threshold:
			next_item = BLACK;
		}
	}

	if (Reflectometer_pulse){				//After the item passes the sensor
		Q_add(&Item_Q, next_item);					//Add the item to the queue
		albedo_top = 0;						//Reset max albedo
	}


//Conveyor belt state machine

	//The belt moves if it is dumping an item, or if there is no item there to dump
	if (dump_flag || !(on_deck_latch)){
		setDCspeed(32000);	//half speed. just a guess
    }else{
		setDCspeed(0);
	}

	//Falling edge - after a dump reset dump flag
	if (on_deck_pulse) debounce(&dump_flag, 100);


	//Dispense only one object at a time, and decide for each new object only as they arrive
	if ((on_deck_latch) && !(dump_flag)){
		if (dump_check == tray_state) {	//Wait for the tray
		dump_flag = 1;
		read_next = 1;
		}
	}
	if (read_next && Item_Q.size>0){
		dump_check = Q_read(&Item_Q);		//Remove the old item, get the next item's type
		read_next = 0;
	}


//Tray state machine

	//If the last tray move has completed,
	//		and the tray is not in position:	move the tray
	if ((!stepperRUNNINGflag && (dump_check != tray_state)) || dump_early){
		dest_bin = dump_check;				//update bin destination
		next_bin = Item_Q.head->Material;	//check following bin (not yet used)



		/*		Move the bin to match the object on deck
			4 quadrants
		STEEL - BLACK			00 - 01 | 0 - 1		This is how it
		WHITE - ALUMINUM		10 - 11 | 2 - 3		is in the enum
		*/
		char rot = dest_bin | tray_state<<2;
		switch (rot){
			case 0b0001: case 0b0111: case 0b1110: case 0b1000: turnStepper(90); 
			case 0b0010: case 0b1011: case 0b1101: case 0b0100: turnStepper(-90);
			case 0b0011: case 0b0110: case 0b1100: case 0b1001: turnStepper(180);
		}
		tray_state = dest_bin;
	}




}
}




void initADC(){
	cli();	//disable global interrupts
	
	//REFS(0,1) = 01 -> +reference AVCC with ext. cap on AREF pin
	/*ADLAR: Left adjusts the 10-bit ADC result		ADCH	 ADCL
								Right Adjusted:	  000000A9 87654321
								Left Adjusted:	  A9876543 21000000*/

	//MUX4:0 all zero selects ADC0 (PINF0) single-ended input
	ADMUX = (0x1<<REFS0)|(0x1<<ADLAR);
	
	ADCSRA |= 0x1<<ADEN;	//enable ADC
	ADCSRA |= 0x1<<ADIE;	//enable interrupt
	
	sei();	//re-enable global interrupts

	ADCSRA |= 0x1<<ADSC;	//Start conversion
}


unsigned char sensor = 0;	//global for which sensor to read ADC from
ISR(ADC_vect){

	//get value into appropriate variable - weighted rolling average
	switch(sensor){
	case 0: metal_av = (4*metal_av + ADCH)/5; break;
	case 1: albedo_av = (4*albedo_av + ADCH)/5; break;
	case 2: hall_av = (4*hall_av + ADCH)/5; break;
	}

	//cycle through the three sources
	if (++sensor > 2) sensor = 0;	
	ADMUX = (ADMUX & 0xF0) | sensor;	//reset MUX and set to sensor

	//Start a new conversion
	ADCSRA |= 1<<ADSC;

}


void init_debounce(){
//16 bit timer
	TCCR3A = 0x00;	//normal compare mode
	TCCR3B = 1<<WGM32 | 1<<WGM33 | 1<<CS32 | 1<<CS30;	//prescaler /1024
	TIMSK3 = 0x0;
}

void debounce (int* flag_var, int ms){
//debounce claims a channel on timer 3, and resets the flag passed to it after 25ms 

	//Timer 3 is in normal mode and constantly running

	if (!t3A){	//channel A is not in use
		t3A = 1;	//claim channel A
		
		//set the flag variable
		*flag_var = 1;

		//point channel pointer at the flag
		t3A_ptr = flag_var;

		//~wait x ms, or up to 30ms extra to avoid missing via overflow
		OCR3A = (TCNT3 < (0xFFFF-ms-30)) ? TCNT3+ms : ms;


		//enable Channel A interrupt
		TIMSK3 |= 1<<OCIE3A;
		return;
	}
	if (!t3B){	//channel B is not in use
		t3B = 1;	//claim channel B
		
		//set the flag variable
		*flag_var = 1;

		//point channel pointer at the flag
		t3B_ptr = flag_var;

		//~wait x ms, or up to 30ms extra to avoid missing via overflow
		OCR3B = (TCNT3 < (0xFFFF-ms-30)) ? TCNT3+ms : ms;


		//enable Channel B interrupt
		TIMSK3 |= 1<<OCIE3B;
		return;
	}
	if (!t3C){	//channel C is not in use
		t3C = 1;	//claim channel C
		
		//set the flag variable
		*flag_var = 1;

		//point channel pointer at the flag
		t3C_ptr = flag_var;

		//~wait x ms, or up to 30ms extra to avoid missing via overflow
		OCR3C = (TCNT3 < (0xFFFF-ms-30)) ? TCNT3+ms : ms;


		//enable Channel A interrupt
		TIMSK3 |= 1<<OCIE3C;
		return;
	}
}

ISR(TIMER3_COMPA_vect){
	t3A = 0;				//release channel
	TIMSK3 &= ~(1<<OCIE3A);	//disable channel interrupt
	*t3A_ptr = 0;			//reset flag
}
ISR(TIMER3_COMPB_vect){
	t3B = 0;				//release channel
	TIMSK3 &= ~(1<<OCIE3B);	//disbale channel interrupt
	*t3B_ptr = 0;			//reset flag
}
ISR(TIMER3_COMPC_vect){
	t3C = 0;				//release channel
	TIMSK3 &= ~(1<<OCIE3C);	//disbale channel interrupt
	*t3C_ptr = 0;			//reset flag
}



//Timer0 DC motor PWM
//Timer1 mtimer?	I don't use it
//Timer2 Stepper motor interrupts
//Timer3 debounce timer, 3 channels