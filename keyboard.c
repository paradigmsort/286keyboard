#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "usb_keyboard_debug.h"
#include "print.h"

#define CPU_PRESCALE(n)	(CLKPR = 0x80, CLKPR = (n))

#define NUM_COLUMNS 8
#define NUM_ROWS 13

#define BIT(iNDEX) (1<<(iNDEX))
#define BIT_IS_SET(rEG, iNDEX) ((rEG) & BIT(iNDEX))
#define BIT_IS_CLEAR(rEG, iNDEX) (! BIT_IS_SET(rEG, iNDEX))

#define KEY_NONE 0

#define MAX_NAME_LENGTH 16

#define KEYS(oP) \
	{oP(F8),		oP(F7),		oP(F6),		oP(HOME),	oP(SCROLL_LOCK),	oP(NUM_LOCK),	oP(F10),	oP(F9)}, \
	{oP(CAPS_LOCK),	oP(SPACE),	oP(NONE),	oP(F5), 	oP(F4), 			oP(F3),			oP(F2),		oP(F1)},\
	{oP(9), 		oP(8),		oP(7),		oP(TAB),	oP(BACKSPACE),		oP(EQUAL),		oP(MINUS),	oP(0)},\
	{oP(NONE), 		oP(ESC),	oP(NONE),	oP(6),		oP(5),				oP(4),			oP(3),		oP(2)},\
	{oP(E), 		oP(W),		oP(Q),		oP(I),		oP(U),				oP(Y),			oP(T),		oP(R)},\
	{oP(LEFT_BRACE),oP(P),		oP(O),		oP(S),		oP(A),				oP(NONE),		oP(ENTER),	oP(RIGHT_BRACE)},\
	{oP(NONE), 		oP(NONE),	oP(NONE),	oP(NONE),	oP(NONE),			oP(CTRL),		oP(NONE),	oP(NONE)},\
	{oP(NONE), 		oP(NONE),	oP(ALT),	oP(NONE),	oP(NONE),			oP(NONE),		oP(NONE),	oP(NONE)},\
	{oP(H), 		oP(F10),	oP(NUM_LOCK),	oP(SCROLL_LOCK),	oP(HOME),	oP(F6),		oP(F7),		oP(F8)},\
	{oP(COMMA), 	oP(F10),	oP(NUM_LOCK),	oP(SCROLL_LOCK),	oP(HOME),	oP(F6),		oP(F7),		oP(F8)},\
	{oP(BACKSLASH), oP(F10),	oP(NUM_LOCK),	oP(SCROLL_LOCK),	oP(HOME),	oP(F6),		oP(F7),		oP(F8)},\
	{oP(PRINTSCREEN),oP(PAGE_UP),oP(UP),	oP(END),	oP(NONE),			oP(RIGHT),		oP(NONE),	oP(LEFT)},\
	{oP(INSERT), 	oP(PAGE_DOWN),oP(DOWN),	oP(NONE),	oP(NONE),			oP(NONE),		oP(NONE),	oP(DELETE)}
#define CODE(k) KEY_##k
#define NAME(k) #k


static char name_matrix[NUM_ROWS][NUM_COLUMNS][MAX_NAME_LENGTH] PROGMEM = { KEYS(NAME) };

// input columns are on pins D
void init_columns(void)
{
	DDRD = 0x00; // configure as input (0)
	PORTD = 0xFF; // configure as pull-up (1)
	DDRB = 0x00;
	PORTB = 0xFF;
}

uint8_t read_columns(void)
{
	uint8_t columns = ((~PIND) & 0xBF); // all columns except 6 are on D
	columns |= (PINB & 1) ? 0x00 : 0x40; // column 6 is B0
	return columns;
}

void select_row(uint8_t n)
{
	if (n < 8) {
		DDRC |= (1<<n); 	// output
		PORTC &= ~(1<<n); 	// low
	} else {
		n -= 8;
		DDRF |= (1<<n);
		PORTF &= ~(1<<n);
	}
	_delay_us(30);
}

void unselect_rows(void)
{
	// switch to high-impedence ie floating input
	DDRC = 0x00;	//input
	PORTC = 0x00;	// floating
	DDRF &= 0x00; 	//input
	PORTF &= 0x00; 	// floating
	_delay_us(30);
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
	print("keydown ");
	print_P(name_matrix[detect_row][col]);
	print(" ");
	print_row_col(detect_row, col);
	print("\n");
}

void on_keyup(uint8_t col)
{
	print("keyup   ");
	print_P(name_matrix[detect_row][col]);
	print(" ");
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

	DDRD |= (1<<6); // led is output
	PORTD &= ~(1<<6); // led is off

	// Initialize the USB, and then wait for the host to set configuration.
	// If the Teensy is powered without a PC connected to the USB port,
	// this will wait forever.
	usb_init();
	while (!usb_configured()) /* wait */ ;

	// Wait an extra second for the PC's operating system to load drivers
	// and do whatever it does to actually be ready for input
	_delay_ms(1000);

	uint8_t indices[NUM_ROWS] = {7, 6, 5, 4, 3, 2 ,1, 0, 15, 14, 13, 12, 11}; 
	uint8_t prev_cols[NUM_ROWS];
	uint8_t cols[NUM_ROWS];

	uint8_t i;
	for(i=0; i < NUM_ROWS; i++) {
		cols[i] = 0;
		prev_cols[i] = 0;
	}
	while (1) {
		for (i=0; i< NUM_ROWS; i++) {
			select_row(i);
			cols[i] = read_columns();
			unselect_rows();
			set_detect_row(indices[i]);
			detect_changes(cols[i], prev_cols[i]);
			prev_cols[i] = cols[i];
		}
	}
}