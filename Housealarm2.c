/*
 * home_security.c
 *
 * Created: 02-Mar-13 8:00:26 PM
 *  Author: Priyanka
 */ 

/*
Hardware connections
--------------------
KEYPAD connections

COL1 ->  PB6
COL2 ->  PB5
COL3 ->  PB4

ROW1 ->  PB3
ROW2 -> PB2
ROW3 ->  PB1
ROW4 -> PB0

7 Seg Display Connections

A	->	PC0
B	->	PC1
C	->	PC2
D	->	PC3
E	->	PC4
F	->	PC5
G	->	PC6


//PA0 (INPUT) from PLL sensor....
//PA1 (OUTPUT) to MOTOR to OPEN DOOR
//PA2 (OUTPUT) to Siren
*/

#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
//---------------------------------------
//Setbit And Clearbit
#define sbi(b,x) b |= (1<<x)
#define cbi(b,x) b &= ~(1<<x)
//Toggle and check bit
#define tog(b,x) b = b ^ (1<<x)
#define check(b,x) ( (b && (1<<x)) >> x)
//---------------------------------------

//*******************************
//KEYPAD IS ATTACHED ON PORTB
#define KEYPAD B  
#define KEYPAD_PORT PORTB
#define KEYPAD_DDR   DDRB
#define KEYPAD_PIN   PINB
//*******************************

// 7 Seg Display (CONNECTED TO PORT C)
#define seg7Display	C
#define Seg7_PORT	PORTC
#define Seg7_DDR	DDRC

#define togg	~	
#define one	togg(0x06)
#define two	togg(0x5B)
#define thr	togg(0x4F)
#define fou	togg(0x66)
#define fiv	togg(0x6D)
#define six	togg(0x7D)
#define sev	togg(0x07)
#define eig	togg(0x7F)
#define nin togg(0x6F)
#define zer togg(0x3F)

#define codeA	togg(0x77)		// Used to display Alert
#define codeC	togg(0x39)		// Used to display If Clear to pass
#define codeP	togg(0x73)		// Used to display before Entering password
#define codeU	togg(0x3E)		// Used to display If door is Unlocked
#define codeL	togg(0x38)		// Used to display If door is Locked
#define codeH	togg(0x76)		// Not Assigned (N/A) (Used here in Initialization state)
#define codeE	togg(0x79)		// Used to display an Error in Password entry

#define Clear	togg(0x00)		// No display
//*******************************

// Sensor, Motor and Siren Definitions
#define Sensor	check(PORTA,0)

#define Motor_open	sbi(PORTA,1);cbi(PORTA,2)
#define Motor_close cbi(PORTA,1);sbi(PORTA,2)
#define Motor_lock	sbi(PORTA,1);sbi(PORTA,2)
#define Motor_free	cbi(PORTA,1);cbi(PORTA,2)

#define Siren_on	sbi(PORTA,3)
#define Siren_off	cbi(PORTA,3)

// Password related definitions
#define Pass_length 4
#define Retry_length 3


//Global variables declarations
int password[Pass_length];
int retry_no = 0;
bool lock_state = false;		// Door Lock State
bool pass_status = false;
int key = 0xFF;
bool key_flag = false;			// Used for reading key only when we want to...
int code_temp=0;				// Code (Entered password) related Temporary variable
bool cancel_state = false;

//Function Declarations
void set_password(unsigned long int);
bool Check_password(int* );
void init_homeSecurity(void);
void Seg_display(uint8_t);
void Seg_blink(uint8_t, int);
uint8_t GetKeyPressed(void);
void GC_Code (void);
void door_unlock(void);
void door_lock(void);


//--------------------------------
int main(void)
{
	
	int temp = 0;	
	
	
	
	init_homeSecurity();
	set_password(1234);				// change the argument to change password...
	
    while(1)
    {
        if( (GetKeyPressed() == 9) && (lock_state == false) && (Sensor == 1) )		// Close Door and press Left Soft Key
		{
			door_lock();
		}	
		
		else if( (lock_state == true) && (Sensor == 0) )			// Door Locked but Sensor stopped => Unauthorized access -> Siren ON
		{
			Siren_on;
			Seg_blink(codeA,3);		
		}			
		
		else if( (lock_state == true) && (Sensor == 1) )			// Door Locked and Safe
		{
			Siren_off;
			if( GetKeyPressed() == 9)			//Enter Unlocking mode (ASK for PASSWORD)
			{
				key_flag = true;
				Seg_blink(codeP,1);			// Display P in 7 Seg display (Enter password Mode)
											// Wait for 2 secs before entering password
				key_flag = false;
				key = GetKeyPressed();
				while( key == 0xFF)		// Wait till key is pressed
				{
					key = GetKeyPressed();
				}
				
				GC_Code();
				if(pass_status == true && cancel_state == false) 
				{
					lock_state = false;
				}
				else if(pass_status == false && cancel_state == true)
				{
					cancel_state == false;
					Seg_display(codeL);
				}
				else if(pass_status == false && cancel_state == false)
				{
					Siren_on;
					Seg_blink(codeA,3);			// Alert Mode
					do
					{
					key = GetKeyPressed();
					while( key == 0xFF)		// Wait till key is pressed
					{
						key = GetKeyPressed();
					}
					
					GC_Code();
					}while(pass_status == false && cancel_state == false)	;
					if(pass_status == true && cancel_state == false)
					{
						Siren_off;
						lock_state = false;
					}				
				}					
									
			}
		}
		
		else if ((lock_state == false) && (pass_status == true))
		{
			door_unlock();
		}					
    }
}


//*******************************************************
//Password functions
void set_password(unsigned long int code)
{
	int i;
	for(i = 0;i<Pass_length; i++)
	{
		password[i] = code % 10 ; 
		code = code/10;
	}
	return;	
}

bool Check_password(int* codeAddr)
{
	int i=0;
	for(i = 0; (i<Pass_length) && (*(codeAddr + i) == password[Pass_length-i-1]); i++)
	{
		;
	}
	if (i == Pass_length) return true;
	else return false;
}
//-----------------------------------------

// Home Security Initialization function
// Sets PORT i/o directions and switch OFF Siren and Motor(In free motion)
void init_homeSecurity(void)
{
	Seg7_DDR = 0xFF;		// PORT C connected to 7 Segment display as OUTPUT
	Seg7_PORT = codeH;		// Initially Display H on 7 seg. display
	
	//PA0 (INPUT) from PLL sensor....
	//PA1&PA2 (OUTPUT) to MOTOR to OPEN DOOR
	//PA3 (OUTPUT) to Siren	
	DDRA = 0b00001110;
	Motor_free;
	Siren_off;
	
	key_flag = false;
	return;	
}


//7 Seg display Function
void Seg_display(uint8_t data)
{
	Seg7_PORT = data;
	return;
}

void Seg_blink(uint8_t data, int time)
{
	int temp;
	for(temp = 0;temp < time; temp ++)
	{
		Seg_display(data);				// Display A in 7 Seg display (Alert Mode Display)
		_delay_ms(1000);
		Seg_display(Clear);
		_delay_ms(1000);
	}
	Seg_display(data);
	return;
}


//door LOCK and UNLOCK functions
void door_unlock(void)
{
	Siren_off;
	Motor_open;
	Seg_display(codeU);
	_delay_ms(1000);
	Motor_free;
	pass_status = false;
}

void door_lock(void)
{
	Motor_lock;
	Siren_off;
	Seg_blink(codeL,3);				// Display L in 7 Seg display (Lock Mode display)
	lock_state = true;
	pass_status = false;
}

//Keyboard Input....
/*Function return the key code of key pressed
on the Keypad. Keys are numbered as follows

[00] [01] [02]
[03] [04] [05]
[06] [07] [08]
[09] [10] [11]
*/
uint8_t GetKeyPressed(void)
{
	uint8_t r,c;
	if(key_flag == false){
	KEYPAD_PORT|= 0X0F;
	for(c=0;c<3;c++)
	{		
		KEYPAD_DDR&=~(0X7F);
		KEYPAD_DDR|=(0X40>>c);
		for(r=0;r<4;r++)
		{
			if(!(KEYPAD_PIN & (0X08>>r)))
			{
			return (r*3+c);
			}
		}
	}
	}	
	
	return 0XFF;//Indicate No key pressed
}


// Function for getting the code from the key pad...
void GC_Code (void)
{
	int code[Pass_length];
	int prev_key = key;
	int temp;
	int retry_temp = 0;
	do 
	{
	
	if(retry_no < Retry_length)
	{
	
	if (key == 9)
	{
		Seg_display(codeL);
		cancel_state = true;
		return;		// Exit password entry mode
	}
	
	else
	{
		for(code_temp = 0; code_temp < Pass_length; code_temp++)
		{
			if(code_temp < Pass_length)
			{
			if(key == 9) {;}
			else if(key == 10) {code[code_temp] = 0;}
			else {code[code_temp] = (key+1);}
			key = GetKeyPressed();
			while(prev_key == key)
			{
				key = GetKeyPressed();
				temp = key;
			}
			while(key == 0xFF)
			{
				key = GetKeyPressed();
			}
			if(prev_key == 9 && key == 9) 
			{
				Seg_display(codeL);
				cancel_state = true;
				return;		// Exit password entry mode
			}
			else if (prev_key == 9)
			{
				if(key == 10) {code[code_temp] = 0;}
				else {code[code_temp] = (key+1);}
				prev_key = key;
				while(prev_key == key)
				{
					key = GetKeyPressed();
				}
				while(key == 0xFF)
				{
					key = GetKeyPressed();
				}
				prev_key = key;
			}
			prev_key = key;
			if (key == 9)				// Clear Password
			{
				code_temp = (-1);
			}
			}			
			
			else if(code_temp == Pass_length)
			{
				
				if(key == 0xFF )
				{
					if (temp == 9)
					{
						code_temp = (-1);							
					}
					else {
					while (temp != 11)
					{
						key = GetKeyPressed();
						
						while(key == 0xFF)
						{
							key = GetKeyPressed();
						}
						prev_key == key;
						temp = key;
						while(prev_key == key)
						{
							key = GetKeyPressed();
						}
						
					}
					}					
					
				}
				
			}
		}
	}		
		
				
		if (Check_password(code) == true)
		{
			//door_unlock();
			cancel_state = false;
			pass_status = true;
			retry_temp = 0;
		}
		else
		{
			cancel_state = false;
			pass_status = false;
			retry_no ++;
			retry_temp = 1;
		}
	}
	
	else 
	{
		cancel_state = false;
		pass_status = false;
		retry_temp = 0;	
	}
	
	}while(retry_temp)	;
	return;
}
