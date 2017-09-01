#include "io.c"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>

//For Timer:

volatile unsigned char TimerFlag = 0; // TimerISR() sets this to 1. C programmer should clear to 0.
unsigned long _avr_timer_M = 1; // Start count from here, down to 0. Default 1 ms.
unsigned long _avr_timer_cntcurr = 0; // Current internal count of 1ms ticks


void TimerOn() {
	// AVR timer/counter controller register TCCR1
	TCCR1B = 0x0B;// bit3 = 0: CTC mode (clear timer on compare)
	// bit2bit1bit0=011: pre-scaler /64
	// 00001011: 0x0B
	// SO, 8 MHz clock or 8,000,000 /64 = 125,000 ticks/s
	// Thus, TCNT1 register will count at 125,000 ticks/s

	// AVR output compare register OCR1A.
	OCR1A = 125;    // Timer interrupt will be generated when TCNT1==OCR1A
	// We want a 1 ms tick. 0.001 s * 125,000 ticks/s = 125
	// So when TCNT1 register equals 125,
	// 1 ms has passed. Thus, we compare to 125.
	// AVR timer interrupt mask register
	TIMSK1 = 0x02; // bit1: OCIE1A -- enables compare match interrupt

	//Initialize avr counter
	TCNT1=0;

	_avr_timer_cntcurr = _avr_timer_M;
	// TimerISR will be called every _avr_timer_cntcurr milliseconds

	//Enable global interrupts
	SREG |= 0x80; // 0x80: 1000000
}

void TimerOff() {
	TCCR1B = 0x00; // bit3bit1bit0=000: timer off
}

void TimerISR() {
	TimerFlag = 1;
}

// In our approach, the C programmer does not touch this ISR, but rather TimerISR()
ISR(TIMER1_COMPA_vect) {
	// CPU automatically calls when TCNT1 == OCR1 (every 1 ms per TimerOn settings)
	_avr_timer_cntcurr--; // Count down to 0 rather than up to TOP
	if (_avr_timer_cntcurr == 0) { // results in a more efficient compare
		TimerISR(); // Call the ISR that the user uses
		_avr_timer_cntcurr = _avr_timer_M;
	}
}

// Set TimerISR() to tick every M ms
void TimerSet(unsigned long M) {
	_avr_timer_M = M;
	_avr_timer_cntcurr = _avr_timer_M;
}

//Global Variables:

unsigned char sequence[9];
unsigned char index;

unsigned char wrong;
unsigned char curRound;

unsigned char startButton;
unsigned char input;

const unsigned char MAX_ROUND = 9;

//Simon Tick Function

enum simon_tick_states {SM_init,SM_welcome,SM_blink_on,SM_blink_off,SM_waitForInput,SM_checkInput,SM_lose,SM_win,SM_reset} simon_tick_state;

void Simon_Tick()
{
	switch (simon_tick_state)
	{
		case SM_init:
			startButton = 0;
			input = 0;
			index = 0;
			curRound = 1;
			wrong = 0;
			LCD_DisplayString(1,"Welcome ");
			simon_tick_state = SM_welcome;
			break;
		case SM_welcome:
			if (startButton)
			{
				unsigned char i = 0;
			    for (i = 0; i < MAX_ROUND; i++)
					sequence[i] = rand() % 4;
				simon_tick_state = SM_blink_on;
			}
			else
				simon_tick_state = SM_welcome;
			break;
		case SM_blink_on:
			simon_tick_state = SM_blink_off;
			break;
		case SM_blink_off:
			if (index < curRound)
				simon_tick_state = SM_blink_on;
			else
				simon_tick_state = SM_waitForInput;
		case SM_waitForInput:
			if (input != 0 && index < curRound)
				simon_tick_state = SM_checkInput;
			if (index >= curRound && curRound >= MAX_ROUND)
			{
				simon_tick_state = SM_win;
				LCD_DisplayString(1,"YOU WIN");
			}
			else
				simon_tick_state = SM_waitForInput;
			break;
		case SM_checkInput:
			if (input == 0 && !wrong)
			{
				simon_tick_state = SM_waitForInput;
				index++;
			}
			else if (wrong && input == 0)
			{
				LCD_DisplayString(1,"YOU LOSE");
				simon_tick_state = SM_lose;
			}
			else if (input != 0)
				simon_tick_state = SM_checkInput;
			break;
		case SM_lose:
			if (startButton)
				simon_tick_state = SM_reset;
			else
				simon_tick_state = SM_lose;
			break;
		case SM_win:
			if (startButton)
				simon_tick_state = SM_reset;
			else
				simon_tick_state = SM_win;
			break;
		case SM_reset:
			simon_tick_state = SM_welcome;
			break;
	}

	switch (simon_tick_state)
	{
		case SM_init:
			break;
		case SM_welcome:
			startButton = (~PINA & 0x10) >> 4;
			break;
		case SM_blink_on:
			if (sequence[index] == 0)
				PORTA = 0x01;
			else if (sequence[index] == 1)
				PORTA = 0x02;
			else if (sequence[index] == 2)
				PORTA = 0x04;
			else if (sequence[index] == 3)
				PORTA = 0x08;
			break;
			index++;
		case SM_blink_off:
			PORTA = 0x00;
			break;
		case SM_waitForInput:
			input = ~PINB & 0x0F;
			PORTA = input;
			break;
		case SM_checkInput:
			PORTA = input;
			if ( (1 << (sequence[index])) != input)
				wrong = 1;
		case SM_win:
			break;
		case SM_lose:
			break;
		case SM_reset:
			wrong = 0;
			index = 0;
			curRound = 1;
			startButton = 0;
			break;
	}
}

int main(void)
{
	srand(1);

	TimerSet(200);
	TimerOn();

	DDRA = 0xFF; PORTA = 0x00;
	DDRB = 0x00; PORTB = 0xFF;
	DDRC = 0xFF; PORTC = 0x00;
	DDRD = 0xFF; PORTD = 0x00;

	LCD_init();

	simon_tick_state = SM_init;

    while (1)
    {
		Simon_Tick();
		while(!TimerFlag);
		TimerFlag = 0;
    }
}

