#include <p18f452.h>
#include "definitions.h"
#pragma config WDT = OFF

#pragma interrupt interruptFunction
void interruptFunction(void)
{
	if(INTCONbits.INT0IF)
	{
		INTCONbits.INT0IF = 0;
		if(!choose)	
		{
			PIE1bits.SSPIE = 0;
			SLAVE_SELECT = 1;
			INTCON3bits.INT1IE = 1;
			INTCON3bits.INT1IF = 0;
			choose = 0x01;
			clearHome();	
			displayParameter();
		}
		else
		{
			changePosition = ~changePosition;
			changeArrowPosition();
		}
	}
	else if(INTCON3bits.INT1IF)
	{
		INTCON3bits.INT1IF = 0;
		INTCON3bits.INT1IE = 0;		
		PIE1bits.SSPIE = 1;
		clearHome();
		choose = 0;
		SLAVE_SELECT = 0;
		SSPBUF = CLOCK_BURST_READ;
	}	
	else if(PIE1bits.SSPIE)
	{
		if(PIR1bits.SSPIF)	
		{
			PIR1bits.SSPIF = 0;
			if(spiState == WRITE)
			{
				if(i < 8)
					SSPBUF = bytes[i++];
				else
				{
					SLAVE_SELECT = 1;
					spiState = READ;
					reg = DUMMY;
					SLAVE_SELECT = 0;
					i = 0;
					SSPBUF = CLOCK_BURST_READ;
				}	
			}
			else
			{
				if(reg == DUMMY)				// SEND DUMMY DATA
				{
					SSPBUF = DUMMY_DATA;
					reg = SAVE;
				}
				else						// STORE THE RECEIVED DATA FROM MISO LINE
				{
					reg = DUMMY;
					bytes[i++] = SSPBUF;	
					if(i < 8)
						PIR1bits.SSPIF = 1;		// TRIGGER A SOFTWARE SPI INTERRUPT 
					else
					{
						reg = DUMMY;
						SLAVE_SELECT = 1;		// DISCONNECTED THE SLAVE
						displayResult();		// DISPLAY DATA INTO LCD
						SLAVE_SELECT = 0;
						SSPBUF = CLOCK_BURST_READ;	// INITIATE A BURST MODE READING 
						i = 0;
					}
				}
			}
		}
	}
}

#pragma code vector = 0x00008
void vector(void)
{
	_asm
		GOTO interruptFunction
	_endasm
}
#pragma code

void main(void)
{
	TRISD = 0x00;				// ALL PINS ARE OUTPUT
	TRISC = 0x90;				// MAKE SERIAL PINS AS OUTPUT + LCD CONTROL PINS OUTPUT
	SSPSTAT = 0xC0;				// CONFIGURATION OF SPI PROTOCOL
	SSPCON1 = 0x20;	
	initialization();			// INITIALIZATION OF LCD(CLEAR HOME, CURSOR OFF, 5*7 PIXEL SIZE)
	INTCONbits.GIE = 1;			// GLOBAL INTERRUPT ENABLE
	INTCONbits.PEIE = 1;		// PERIPHERAL INTERRUPT ENABLE		
	INTCONbits.INT0IE = 1;
	INTCONbits.INT0IF = 0;
	PIE1bits.SSPIE = 1;			// SPI INTERRUPT BIT
	PIR1bits.SSPIF = 0;			// INTERRUPT FLAG  
	SLAVE_SELECT = 0;
	SSPBUF = CLOCK_BURST_WRITE;
	while(TRUE);					// KEEP LOOPING
}
void changeArrowPosition(void)
{
	if(changePosition)
	{
		home(ARROW_SECOND_LINE);
		displaySpace();
		home(ARROW_THIRD_LINE);
		displayArrow();
	}
	else
	{
		home(ARROW_THIRD_LINE);
		displaySpace();
		home(ARROW_SECOND_LINE);
		displayArrow();
	}
}
void displaySpace(void)
{
	LATD = 0x20;
	data();
}
void displayArrow(void)
{
	LATD = '>';
	data();
}
void displayParameter(void)
{
	unsigned char strings[3][16] = {"     LANGUAGE :", "      ENGLISH", "      FRENSH"}, i = 0, j = 0;
	while(i < 3)
	{
		j = 0;
		while(strings[i][j] != '\0')
		{
			LATD = strings[i][j];
			data();
			++j;
		}
		if(i == 0)
			home(0xC0);
		else if(i == 1)
			home(0x94);
		++i;
	}
	home(0xC4);
	displayArrow();
}
void clearHome(void)
{
	LATD = 0x01;
	command();
	T0CON = 0x80;
	TMR0H = 0xF9;
	TMR0L = 0x8E;	
	timerZero();
}
void home(unsigned char value)		// THIS FUNCTION IS USED TO CHANGE THE POSITION OF THE CURSOR(SECOND LINE, FIRST LINE)
{
	LATD = value;
	command();
	busyFlag();
}
void displayTime(void)				// THIS IS USED FOR DISPLAYING TIME 
{
	signed char m = 2;
	home(0xC6);
	while(m >= 0)
	{
		LATD = 0x30 + ((bytes[m] & 0xF0) >> 4);		// EXTRACT THE HIGH BYTE TO DISPLAY IT
		data();
		LATD = 0x30 + (bytes[m] & 0x0F);			// EXTRACT THE LOW BYTE TO DISPLAY IT
		data();
		if(m != 0)									// DISPLAY TIME IN THIS FORMAT : x:x:x
		{
			LATD = ':';			
			data();
		}
		--m;
	}
}
void displayDate(void)			// THIS FUNCTION DISPLAYS DATE : x/x/20x
{		
	unsigned char m = (bytes[5] - 1), n = 0, date = bytes[3], month = bytes[4], year = bytes[6];
	home(0x94);
	if(m > 7)
		m = 0;
	if(!changePosition)
	{
		for(; daysInEnglish[m][n] != '\0'; ++n)
		{
			LATD = daysInEnglish[m][n];
			data();
		}	
	}
	else
	{
		for(; daysInFrensh[m][n] != '\0'; ++n)
		{
			LATD = daysInFrensh[m][n];
			data();
		}
	}
	LATD = 0x30 + ((date & 0xF0) >> 4);
	data();
	LATD = 0x30 + (date & 0x0F);
	data();
	LATD = '/';
	data();
	LATD = 0x30 + ((month & 0xF0) >> 4);
	data();
	LATD = 0x30 + (month & 0x0F);
	data();
	LATD = '/';
	data();
	LATD = 0x32;			
	data();	
	LATD = 0x30;
	data();
	LATD = 0x30 + ((year & 0xF0) >> 4);
	data();
	LATD = 0x30 + (year & 0x0F);
	data();
	displaySpace();
	displaySpace();
	displaySpace();
}
void displayInformation(void)
{
	unsigned char inforEnglish[] = "DATE & TIME", i = 0, inforFrensh[] = "TEMPS & DATE";
	home(0x84);
	if(!changePosition)
	{
		while(inforEnglish[i] != '\0')
		{
			LATD = inforEnglish[i++];
			data();	
		}
	}
	else
	{
		while(inforFrensh[i] != '\0')
		{
			LATD = inforFrensh[i++];
			data();	
		}
	}
}
void displayResult(void)
{
	displayInformation();
	displayTime();
	displayDate();
}
void initialization(void)		
{
	LATD = 0x38;
	command();
	delay250ms();
	LATD = 0x01;
	command();
	delay250ms();
	LATD = 0x0C;
	command();
	delay250ms();
}
void delay250ms(void)
{
	T0CON = 0x01;
	TMR0H = 0x0B;
	TMR0L = 0xBC;
	timerZero();		
}
void delay3us(void)				
{
	T0CON = 0x48;
	TMR0L = 253;
	timerZero();
}
void command(void)
{
	RS_PIN = 0;
	RW_PIN = 0;
	EN_PIN = 1;
	delay3us();
	EN_PIN = 0;
}
void data(void)
{
	RS_PIN = 1;
	RW_PIN = 0;
	EN_PIN = 1;
	delay3us();
	EN_PIN = 0;
	busyFlag();
}
void busyFlag(void)
{
	RS_PIN = 0;
	RW_PIN = 1;	
	TRISDbits.TRISD7 = 1;
	do
	{
		EN_PIN = 0;
		delay3us();
		EN_PIN = 1;
	}while(PORTDbits.RD7 == 1);
	EN_PIN = 0;
	TRISDbits.TRISD7 = 0;
}
void timerZero(void)
{
	INTCONbits.TMR0IF = 0;
	T0CONbits.TMR0ON = 1;
	while(INTCONbits.TMR0IF == 0);
	INTCONbits.TMR0IF = 0;
	T0CONbits.TMR0ON = 0;	
}