#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "usb_keyboard_debug.h"
#include "print.h"

#define CPU_PRESCALE(n)	(CLKPR = 0x80, CLKPR = (n))

#define NUM_COLUMNS 8
#define NUM_MODIFIER_KEYS 4

#define ROWS(oP)  \
	oP(7) oP(6) oP(5) oP(4) oP(3) oP(2) oP(1) oP(0) oP(15) oP(14) oP(13) oP(12) oP(11)
#define INDICES(row) row,
#define ZERO(row) 0,
#define NUM(row) 1 +
#define NUM_ROWS (ROWS(NUM) 0)

#define BIT(iNDEX) (1<<(iNDEX))
#define BIT_IS_SET(rEG, iNDEX) ((rEG) & BIT(iNDEX))
#define BIT_IS_CLEAR(rEG, iNDEX) (! BIT_IS_SET(rEG, iNDEX))

#define KEY_NONE 0
#define KEY_SYS_REQ KEY_PAUSE

#define MAX_NAME_LENGTH 16

#define KEYS(oP) \
	{oP(F8),			oP(F7),		oP(F6),		oP(HOME),	oP(SCROLL_LOCK),	oP(NUM_LOCK),	oP(F10),	oP(F9)}, \
	{oP(CAPS_LOCK),		oP(SPACE),	oP(NONE),	oP(F5), 	oP(F4), 			oP(F3),			oP(F2),		oP(F1)},\
	{oP(9), 			oP(8),		oP(7),		oP(TAB),	oP(BACKSPACE),		oP(EQUAL),		oP(MINUS),	oP(0)},\
	{oP(1), 			oP(ESC),	oP(NONE),	oP(6),		oP(5),				oP(4),			oP(3),		oP(2)},\
	{oP(E), 			oP(W),		oP(Q),		oP(I),		oP(U),				oP(Y),			oP(T),		oP(R)},\
	{oP(LEFT_BRACE),	oP(P),		oP(O),		oP(S),		oP(A),				oP(NONE),		oP(ENTER),	oP(RIGHT_BRACE)},\
	{oP(NONE), 			oP(NONE),	oP(NONE),	oP(NONE),	oP(NONE),			oP(MOD_CTRL),	oP(NONE),	oP(NONE)},\
	{oP(NONE), 			oP(NONE),	oP(MOD_ALT),oP(NONE),	oP(NONE),			oP(NONE),		oP(NONE),	oP(NONE)},\
	{oP(G), 			oP(F),		oP(D),		oP(SEMICOLON),oP(L),			oP(K),			oP(J),		oP(H)},\
	{oP(M), 			oP(N),		oP(B),		oP(SYS_REQ),oP(MOD_RIGHT_SHIFT),oP(SLASH),		oP(PERIOD),	oP(COMMA)},\
	{oP(MOD_LEFT_SHIFT),oP(TILDE),	oP(QUOTE),	oP(V),		oP(C),				oP(X),			oP(Z),		oP(BACKSLASH)},\
	{oP(PRINTSCREEN),	oP(PAGE_UP),oP(UP),		oP(END),	oP(NONE),			oP(RIGHT),		oP(NONE),	oP(LEFT)},\
	{oP(INSERT), 		oP(PAGE_DOWN),oP(DOWN),	oP(NONE),	oP(NONE),			oP(NONE),		oP(NONE),	oP(DELETE)}
#define CODE(k) KEY_##k
#define NAME(k) #k

#define KEY_MODIFIER_BIT 	(1<<7)
#define KEY_MODIFIER_INDEX_MASK (KEY_MODIFIER_BIT - 1)
#define KEY_MOD_LEFT_SHIFT  (KEY_MODIFIER_BIT | 0)
#define KEY_MOD_RIGHT_SHIFT (KEY_MODIFIER_BIT | 1)
#define KEY_MOD_CTRL 		(KEY_MODIFIER_BIT | 2)
#define KEY_MOD_ALT 		(KEY_MODIFIER_BIT | 3)

static char name_matrix[NUM_ROWS][NUM_COLUMNS][MAX_NAME_LENGTH] PROGMEM = { KEYS(NAME) };
uint8_t code_matrix[NUM_ROWS][NUM_COLUMNS] = { KEYS(CODE) };
uint8_t modifier_codes[NUM_MODIFIER_KEYS] = { KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT, KEY_CTRL, KEY_ALT };

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
	print_P(name_matrix[row][col]);
	print(" ");
	print("row:");
	phex(row);
	print(" col:");
	phex(col);
}

#define MAX_KEYS 6
uint8_t num_keys_down = 0;
void add_key(uint8_t code)
{
	uint8_t i;
	for (i=0; i<MAX_KEYS; i++) {
		if (keyboard_keys[i] == 0) {
			keyboard_keys[i] = code;
			num_keys_down++;
			break;
		}
	}
}

void remove_key(uint8_t code)
{
	uint8_t i;
	for (i=0; i<MAX_KEYS; i++) {
		if (keyboard_keys[i] == code) {
			keyboard_keys[i] = 0;
			num_keys_down--;
			break;
		}
	}
}

uint8_t detect_row;
void set_detect_row(uint8_t row)
{
	detect_row = row;
}

void on_keydown(uint8_t col)
{
	print("keydown ");
	print_row_col(detect_row, col);
	print("\n");
	uint8_t code = code_matrix[detect_row][col];
	if (code & KEY_MODIFIER_BIT) {
		keyboard_modifier_keys |= modifier_codes[code & KEY_MODIFIER_INDEX_MASK];
	} else {
		add_key(code);
	}
	usb_keyboard_send();
}

void on_keyup(uint8_t col)
{
	print("keyup   ");
	print_row_col(detect_row, col);
	print("\n");
	uint8_t code = code_matrix[detect_row][col];
	if (code & KEY_MODIFIER_BIT) {
		keyboard_modifier_keys &= ~ modifier_codes[code & KEY_MODIFIER_INDEX_MASK];
	} else {
		remove_key(code);
	}
	usb_keyboard_send();
}

void detect_changes(uint8_t cols, uint8_t prev_cols)
{
	uint8_t i;
	for (i=0; i< NUM_COLUMNS; i++) {
		if (BIT_IS_CLEAR(cols,i) && BIT_IS_SET(prev_cols,i)) {
			on_keyup(i);
		}
	}
	if (num_keys_down >= 2) return; /* anti-ghosting */
	for (i=0; i< NUM_COLUMNS; i++) {
		if (BIT_IS_SET(cols,i) && BIT_IS_CLEAR(prev_cols,i)) {
			on_keydown(i);
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

	uint8_t indices[NUM_ROWS] 	= { ROWS(INDICES) }; 
	uint8_t prev_cols[NUM_ROWS] = { ROWS(ZERO) };

	uint8_t i;
	while (1) {
		for (i=0; i< NUM_ROWS; i++) {
			select_row(indices[i]);
			uint8_t cols = read_columns();
			unselect_rows();
			set_detect_row(i);
			detect_changes(cols, prev_cols[i]);
			prev_cols[i] = cols;
		}
	}
}