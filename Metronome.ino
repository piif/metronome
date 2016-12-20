// PIF_TOOL_CHAIN_OPTION: UPLOAD_OPTIONS := -c "raw,cr"
// PIF_TOOL_CHAIN_OPTION: EXTRA_LIBS := ArduinoLibs ArduinoTools

#include <Arduino.h>

/* tests to handle a 7 segments 3 digits display with one 74LS164
 */

#ifdef PIF_TOOL_CHAIN
	#include "setInterval/setInterval.h"
	#include "pwm/pwm.h"
#else
	#include "setInterval.h"
	#include "pwm.h"
#endif

#ifndef DEFAULT_BAUDRATE
	#ifdef __AVR_ATmega644P__
		#define DEFAULT_BAUDRATE 19200
	#else
		#define DEFAULT_BAUDRATE 115200
	#endif
#endif


#if defined __AVR_ATtinyX5__
	#define DATA ?
	#define CLOCK ?
	#define BEEP PWM_1_A // = 1

	#ifdef DEMUX_2_TO_3
		#define DIGIT_L ?
		#define DIGIT_H ?
	#else
		#define DIGIT_1 ?
		#define DIGIT_2 ?
		#define DIGIT_3 ?
	#endif
#else
	#define DATA 2
	#define CLOCK 3
	#define BEEP PWM_2_A // = 11

	#ifdef DEMUX_2_TO_3
		#define DIGIT_L 8
		#define DIGIT_H 9
	#else
		#define DIGIT_1 8
		#define DIGIT_2 9
		#define DIGIT_3 10
	#endif
#endif

byte mapSegments[] = {
	//tRrblLmp
	0b11111100, // 0
	0b01100000, // 1
	0b11011010, // 2
	0b11110010, // 3
	0b01100110, // 4
	0b10110110, // 5
	0b10111110, // 6
	0b11100000, // 7
	0b11111110, // 8
	0b11110110, // 9
	0b00000001, // .
	0b00000000, // ' '
};

byte currentDisplay[3];
void displayChars(byte *map) {
	currentDisplay[0] = map[0];
	currentDisplay[1] = map[1];
	currentDisplay[2] = map[2];
}

void displayNumber(int value) {
	currentDisplay[0] = mapSegments[(value / 100) % 10];
	currentDisplay[1] = mapSegments[(value / 10) % 10];
	currentDisplay[2] = mapSegments[value % 10];
}

void outputChar(byte map) {
	for(byte mask = 0x01; mask; mask <<= 1) {
		digitalWrite(DATA, mask & map ? LOW : HIGH);
		digitalWrite(CLOCK, HIGH);
		digitalWrite(CLOCK, LOW);
	}
}

byte digit = 0;
byte buttons = 0;

void updateDisplay(void *, long, int) {
	// read buttons state
	pinMode(DATA, INPUT_PULLUP);
//	_NOP();
	if (digitalRead(DATA)) {
		buttons &= ~(1 << digit);
	} else {
		buttons |= 1 << digit;
	}
	pinMode(DATA, OUTPUT);

	// set 'dot' segment like button states
	if (buttons & 0x04) {
		currentDisplay[0] |= 0x01;
	} else {
		currentDisplay[0] &= 0xFE;
	}
	if (buttons & 0x02) {
		currentDisplay[1] |= 0x01;
	} else {
		currentDisplay[1] &= 0xFE;
	}
	if (buttons & 0x01) {
		currentDisplay[2] |= 0x01;
	} else {
		currentDisplay[2] &= 0xFE;
	}

#ifdef DEMUX_2_TO_3
	// display one digit
	PORTB |= 0x03; // set bits 0, 1 and 2
	outputChar(currentDisplay[digit]); // prepare output
	PORTB &= 0xFC | digit; // set it to current digit
#else
	// display one digit
	PORTB |= 0x07; // set bits 0, 1 and 2
	outputChar(currentDisplay[digit]); // prepare output
	PORTB &= ~(1 << digit); // set it to current digit
#endif

	if (digit == 2) {
		digit = 0;
	} else {
		digit++;
	}
}

int count = 1;

word top, prescale;
setIntervalTimer tacTimer = -1;
void stopTac(void *, long, int) {
	setPWM(2, top,
			COMPARE_OUTPUT_MODE_NONE, top,
			COMPARE_OUTPUT_MODE_NONE, 0,
			WGM_2_FAST_OCRA, prescale);
	//digitalWrite(BEEP, LOW);
	changeInterval(tacTimer, 0);
	tacTimer = -1;
}
void tac(unsigned long delay) {
	setPWM(2, top,
			COMPARE_OUTPUT_MODE_TOGGLE, top / 2,
			COMPARE_OUTPUT_MODE_NONE, 0,
			WGM_2_FAST_OCRA, prescale);
	//tone(BEEP, 294 * 2, 10);
	//noTone(BEEP);//digitalWrite(BEEP, HIGH);
	tacTimer = setInterval(delay, stopTac, NULL);
}

void updateCounter(void *, long, int) {
	count++;
	displayNumber(count);
	if (count % 10 == 0) {
		tac(10);
	} else if (count % 5 == 0) {
		tac(2);
	}
}

void setup() {
	pinMode(DATA, OUTPUT);
	pinMode(CLOCK, OUTPUT);
	pinMode(BEEP, OUTPUT);

#ifdef DEMUX_2_TO_3
	pinMode(DIGIT_L, OUTPUT);
	pinMode(DIGIT_H, OUTPUT);
#else
	pinMode(DIGIT_1, OUTPUT);
	pinMode(DIGIT_2, OUTPUT);
	pinMode(DIGIT_3, OUTPUT);
#endif

	digitalWrite(DATA, LOW);
	digitalWrite(CLOCK, LOW);

#ifdef DEMUX_2_TO_3
	digitalWrite(DIGIT_L, HIGH);
	digitalWrite(DIGIT_H, HIGH);
#else
	digitalWrite(DIGIT_1, HIGH);
	digitalWrite(DIGIT_2, HIGH);
	digitalWrite(DIGIT_3, HIGH);
#endif

	unsigned long frequency = 294 * 2; // D, octave 1
	computePWM(2, frequency, prescale, top);

	setInterval(100, updateCounter, NULL);
	setInterval(3, updateDisplay, NULL);
}

void loop() {
	setIntervalStep();
}
