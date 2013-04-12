#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "usb_keyboard_debug.h"
#include "print.h"

#define SLOW //1s between column scans

#define CPU_PRESCALE(n)	(CLKPR = 0x80, CLKPR = (n))

#define BIT(iNDEX) (1<<(iNDEX))
#define BIT_IS_SET(rEG, iNDEX) ((rEG) & BIT(iNDEX))
#define BIT_IS_CLEAR(rEG, iNDEX) (! BIT_IS_SET(rEG, iNDEX))

#define LOG_ROWS_PER_BLOCK 3
#define ROWS_PER_BLOCK (1<<LOG_ROWS_PER_BLOCK) 
#define BLOCK(rOW) (rOW>>3)
#define ROW_MASK (ROWS_PER_BLOCK-1)
#define ROW(rOW) (rOW & ROW_MASK)
#define ROW_BIT(rOW) BIT(7 - ROW(rOW))
typedef enum {
	ROW_0,
	ROW_1,
	ROW_2,
	NUM_ROWS
} Row;

typedef enum {
	BLOCK_C,
	NUM_BLOCKS
} Block;

#define NUM_COLUMNS 8

#define SET_HIGH(rEG, bITS) ((rEG) |= (bITS))
#define SET_LOW(rEG, bITS) ((rEG) &= ~(bITS))
#define SET_OUTPUT SET_HIGH
#define SET_INPUT SET_LOW
#define SET_FLOATING SET_LOW
#define SET_PULLUP SET_HIGH

#define ALL_ROWS ((1<<ROWS_PER_BLOCK)-1)
#define ALL_COLUMNS ((1<<NUM_COLUMNS)-1)
#define SET_ROW_OUTPUT(ddrx, rOW) SET_OUTPUT(ddrx, ROW_BIT(rOW))
#define SET_ROW_LOW(portx, rOW) SET_LOW(portx, ROW_BIT(rOW))

// input columns are on pins D
void init_columns(void)
{
	SET_INPUT(DDRD,ALL_COLUMNS);
	SET_PULLUP(DDRD,ALL_COLUMNS);
}

uint8_t read_columns(void)
{
	return ~PIND; // active-low
}

void select_row(Row row)
{
	switch (BLOCK(row)) {
	case BLOCK_C:
#ifdef SLOW
		print("select row DDRC ");
		phex(ROW_BIT(row));
		print("\n");
#endif
		SET_ROW_OUTPUT(DDRC,row);
		SET_ROW_LOW(PORTC,row);
		break;
	default:
		break;
	}
#ifdef SLOW
	_delay_ms(30);
#else
	_delay_us(30);
#endif
}

void unselect_rows(void)
{
	// switch to high-impedence ie floating input
#ifdef SLOW
	print("unselect row DDRC ");
	phex(ALL_ROWS);
	print("\n");
#endif
	SET_INPUT(DDRC, ALL_ROWS);
	SET_FLOATING(DDRC, ALL_ROWS);
#ifdef SLOW
	_delay_ms(30);
#else
	_delay_us(30);
#endif
}

void print_row_col(uint8_t row, uint8_t col)
{
	print("row:");
	phex(row);
	print(" col:");
	phex(col);
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
#ifdef SLOW
		print("read cols:");
		phex(cols);
		print(" prev:");
		phex(prev_cols);
		print("\n");
#endif
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

	uint8_t prev_cols[NUM_ROWS];
	uint8_t cols[NUM_ROWS];
	uint8_t i;
	for(i=0; i< NUM_COLUMNS; i++) {
		SET_HIGH(cols[i],ALL_COLUMNS);
		SET_HIGH(prev_cols[i],ALL_COLUMNS);
	}

	while (1) {
		for(i=0; i< NUM_ROWS; i++) {
			select_row(i);
			cols[i] = read_columns();
			unselect_rows();
			set_detect_row(i);
			detect_changes(cols[i], prev_cols[i]);
			prev_cols[i] = cols[i];
		}
#ifdef SLOW
			_delay_ms(4000);
#endif
		print("\n");
	}
}