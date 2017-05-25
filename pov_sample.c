#define F_CPU 16000000UL
#include <util/delay.h>
#include <avr/io.h>
#include <string.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>

#include <stdlib.h>

#define DATA	0
#define CLOCK	1
#define LATCH	2
#define ENABLE	3
#define CLEAR	4

volatile unsigned char str[10];
volatile unsigned int strcomplete = 0;
volatile unsigned int idx = 0;

volatile unsigned int hours = 0;
volatile unsigned int minutes = 0;
volatile unsigned int seconds = 0;

void data_in(unsigned int v, unsigned int r);

/* Char map */
uint8_t digit[11][5] = {
	{0x3E, 0x41, 0x41, 0x41, 0x3E},	/* 0 */
	{0x00, 0x00, 0x10, 0x20, 0x7f},	/* 1 */
	{0x27, 0x49, 0x49, 0x49, 0x31},	/* 2 */
	{0x22, 0x41, 0x49, 0x49, 0x36},	/* 3 */
	{0x0C, 0x14, 0x24, 0x7F, 0x04},	/* 4 */
	{0x07A, 0x49, 0x49, 0x49, 0x46,	/* 5 */
	{0x3E, 0x49, 0x49, 0x49, 0x26},	/* 6 */
	{0x40, 0x47, 0x48, 0x50, 0x60},	/* 7 */
	{0x36, 0x49, 0x49, 0x49, 0x36},	/* 8 */
	{0x32, 0x49, 0x49, 0x49, 0x3E},	/* 9 */
	{0x00, 0x00, 0x42, 0x00, 0x00}	/*   */
};

/* Counter ISR */
ISR(TIMER1_COMPA_vect)
{
	if (++seconds >= 60) {
		seconds = 0;
		if (++minutes >= 60) {
			minutes = 0;
			if (++hours >= 24)
				hours = 0;
		}
	}

}

/* Refresh ISR */
ISR(PCINT3_vect)
{
	/* Only on falling edge */
	if (!(PIND & (1 << PD0)))
		refresh();
}


void show_char(uint8_t pos, uint8_t row)
{
	unsigned char *current_char = &(digit[0][0]);
	unsigned int i = 0;

	for (i = 0; i < 5; i++) {
		/* turn on the 1st LED (green strip) */
		data_in(0x01, 1);
		/* turn on the last LED (green strip) */
		data_in(0x80, 8);
		/*
		 * retirve the data byte for the character and send it to the
		 * shift register
		 */
		data_in(*(current_char + i + pos * 5), row);
		_delay_us(100);
	}

}

void show_nothing(uint8_t row)
{
	/* turn on the 1st LED (green strip) */
	data_in(0x01, 1);
	/* turn on the last LED (green strip) */
	data_in(0x80, 8);
	/* clear the LEDs */
	data_in(0x00, row);
	_delay_us(100);
}

void one_sec_int()
{
	/* Set prescale */
	TCCR1B = (1 << CS12) | (1 << WGM12);
	TCCR1A = 0;
	OCR1A = 0xF424;
	TIMSK1 |= (1 << OCIE1A);
}

void extInt()
{
	/* Enable interrupts on PD0  - PCIE3 */
	PCICR = (1 << PCIE3);
	PCMSK3 = (1 << PCINT24);

}

void refresh()
{
	uint8_t row;
	uint8_t i;

	/* Changing the row  where to display the clock each 30sec */
	if (seconds > 0)
		row = 2;
	if (seconds > 30)
		row = 4;

	/* Clear all */
	data_in(0x00, 8);

	/* Green strip animation */
	for (i = 0; i < (59 - seconds); i++)
		_delay_us(50);

	data_in(0x80, 8);

	for (i = 0; i < seconds; i++)
		_delay_us(50);
	data_in(0x80, 8);

	/* show 1st hour digit */
	show_char(hours / 10, row);
	/* a delay as a spacer */
	show_nothing(row);
	/* show 2nd hour digit */
	show_char(hours % 10, row);

	/* : */
	show_char(10, row);

	show_char(minutes / 10, row);
	show_nothing(row);
	show_char(minutes % 10, row);

	/* : */
	show_char(10, row);

	show_char(seconds / 10, row);
	show_nothing(row);
	show_char(seconds % 10, row);

	data_in(0x80, 8);
	for (i = 0; i < seconds; i++)
		_delay_us(50);
	data_in(0x00, 8);
}

/* Alternate clock pin for SR */
void clock_in(void)
{
	PORTB ^= (1 << CLOCK);
	PORTB ^= (1 << CLOCK);
}

/* Alternate latch pin for SR */
void latch_in(void)
{
	PORTB ^= (1 << LATCH);
	PORTB ^= (1 << LATCH);
}

/*
 * data_in - send serial data to SR
 * @v - column value
 * @r - row value
 */
void data_in(unsigned int v, unsigned int r)
{
	unsigned int i;

	for (i = 0; i < 8; i++) {
		if (!(r & (128 >> i)))
			PORTB |= (1 << DATA);
		else
			PORTB &= ~(1 << DATA);
		/* Send clock pulse */
		clock_in();
	}

	/* Same way for the column byte */
	for (i = 0; i < 8; i++) {
		if ((v & (128 >> i)))
			PORTB |= (1 << DATA);
		else
			PORTB &= ~(1 << DATA);
		/* Send clock pulse */
		clock_in();
	}

	/* Send latch pulse - apply the sended DATA to the actual LEDs */
	latch_in();
}

int main(void)
{
	DDRB |=
	    (1 << CLOCK) | (1 << LATCH) | (1 << CLEAR) | (1 << DATA) |
	    (1 << ENABLE);
	PORTB &=
	    ~(1 << CLOCK) | ~(1 << LATCH) | ~(1 << CLEAR) | ~(1 << DATA) |
	    ~(1 << ENABLE);


	PORTB |= (1 << CLEAR);

	DDRD &= ~(1 << PD0);

	data_in(0x00, 0);

	one_sec_int();
	extInt();
	sei();

	while (1) {
		if (strcomplete > 0) {
			if (str[0] == 'h')
				hours = (str[1] - '0') * 10 + str[2] - '0';
			if (str[0] == 'm')
				minutes = (str[1] - '0') * 10 + str[2] - '0';
			if (str[0] == 's')
				seconds = (str[1] - '0') * 10 + str[2] - '0';

			strcomplete = 0;
		}
	}

	return 0;
}
