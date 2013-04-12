#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "usb_keyboard_debug.h"
#include "print.h"

#define CPU_PRESCALE(n)	(CLKPR = 0x80, CLKPR = (n))

#define BIT(iNDEX) (1<<(iNDEX))
#define BIT_IS_SET(rEG, iNDEX) ((rEG) & BIT(iNDEX))
#define BIT_IS_CLEAR(rEG, iNDEX) (! BIT_IS_SET(rEG, iNDEX))

// input columns are on pins D
void init_columns(void)
{
	DDRD = 0x00; // configure as input (0)
	PORTD = 0xFF; // configure as pull-up (1)
}

uint8_t read_columns(void)
{
	return ~PIND; // active-low
}

// for now, only one row
void select_row(void)
{
	DDRC |= (1<<7); 	// output
	PORTC &= ~(1<<7); 	// low
	_delay_us(30);
}

void unselect_rows(void)
{
	// switch to high-impedence ie floating input
	DDRC &= ~(1<<7); 	//input
	PORTC &= ~(1<<7); 	// floating
	_delay_us(30);
}

uint8_t detect_row;
void set_detect_row(uint8_t row)
{
	detect_row = row;
}

void on_keydown(uint8_t col)
{
	print("keydown");
	print_row_col(detect_row, col);
	print("\n");
}

void on_keyup(uint8_t col)
{
	print("keyup  ");
	print_row_col(detect_row, col);
	print("\n");
}

void detect_changes(uint8_t cols, uint8_t prev_cols)
{
	uint8_t i;
	for (i=0; i< NUM_COLUMNS; i++) {
		if (BIT_IS_SET(cols,i) && BIT_IS_CLEAR(prev_cols,i)) {
			on_keydown(i);
		}
		if (BIT_IS_CLEAR(cols,i) && BIT_IS_SET(prev_cols,i)) {
			on_keyup(i);
		}
	}
}

int main(void)
{
	// set for 16 MHz clock
	CPU_PRESCALE(0);

	// set columns for input and rows as off
	init_columns();
	unselect_rows();

	// Initialize the USB, and then wait for the host to set configuration.
	// If the Teensy is powered without a PC connected to the USB port,
	// this will wait forever.
	usb_init();
	while (!usb_configured()) /* wait */ ;

	// Wait an extra second for the PC's operating system to load drivers
	// and do whatever it does to actually be ready for input
	_delay_ms(1000);

	uint8_t prev_cols = 0;
	uint8_t cols = 0;
	while (1) {
		select_row();
		cols = read_columns();
		unselect_rows();

		if ((cols & (1<<7)) && !(prev_cols&(1<<7))) {
			print("F9 down\n");
		}
		if (!(cols & (1<<7)) && (prev_cols&(1<<7))) {
			print("F9 up\n");
		}
		prev_cols = cols;
	}
}