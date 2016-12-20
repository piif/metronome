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

#define DEMUX_2_TO_3

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

// single char mappings
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

// 3 digits mapping
byte mapDisplays[] = {
	// tempo : "tmp"
	0b00001110 , 0b00101010 , 0b11001110 ,
	// times : "tim"
	0b00001110 , 0b00001000 , 0b00101010 ,
	// subdivisions : "sub"
	0b10110110 , 0b00111000 , 0b00111110 ,
	// time down : "___"
	0b00010000 , 0b00010000 , 0b00010000 ,
	// time down : " _ "
	0b00000000 , 0b00010000 , 0b00000000 ,
	// time left :"|  "
	0b00001100 , 0b00000000 , 0b00000000 ,
	// time left :"'  "
	0b00000100 , 0b00000000 , 0b00000000 ,
	// time right: "  |"
	0b00000000 , 0b00000000 , 0b01100000 ,
	// time right: "  '"
	0b00000000 , 0b00000000 , 0b01000000 ,
	// time up   : "^^^"
	0b10000000 , 0b10000000 , 0b10000000 ,
	// time up   : " ^ "
	0b00000000 , 0b10000000 , 0b00000000 ,
	// half : " - "
	0b00000000 , 0b00000010 , 0b00000000 ,
	// empty: "   "
	0b00000000 , 0b00000000 , 0b00000000 ,
};
#define MSG_TMP    (mapDisplays +  0)
#define MSG_TIM    (mapDisplays +  3)
#define MSG_SUB    (mapDisplays +  6)
#define MSG_DOWN   (mapDisplays +  9)
#define MSG_DOWN2  (mapDisplays + 12)
#define MSG_LEFT   (mapDisplays + 15)
#define MSG_LEFT2  (mapDisplays + 18)
#define MSG_RIGHT  (mapDisplays + 21)
#define MSG_RIGHT2 (mapDisplays + 24)
#define MSG_TOP    (mapDisplays + 27)
#define MSG_TOP2   (mapDisplays + 30)
#define MSG_HALF   (mapDisplays + 33)
#define MSG_NONE   (mapDisplays + 36)

byte currentDisplay[3];
void displayChars(byte *map) {
	currentDisplay[0] = map[0];
	currentDisplay[1] = map[1];
	currentDisplay[2] = map[2];
}
void addChars(byte *map) {
	currentDisplay[0] |= map[0];
	currentDisplay[1] |= map[1];
	currentDisplay[2] |= map[2];
}
void subChars(byte *map) {
	currentDisplay[0] &= ~(map[0]);
	currentDisplay[1] &= ~(map[1]);
	currentDisplay[2] &= ~(map[2]);
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

// times are marked during 1/FLASH time duration
#define FLASH 5
// tempo times per minute
byte tempo = 60;
// times per measure
byte times =  4;
// current time
byte timeNb;
// subdivisions per time
byte subs  =  2;
// number of ticks needed to handle everything (FLASH * times * subs)
byte nbTicks;
// current tick
byte tick;

byte digit = 0;
byte oldButtons = 0;
byte buttons = 0;
#define BUTTON_MINUS 0x04
#define BUTTON_PLUS  0x02
#define BUTTON_MODE  0x01

enum {
	enteringTempo, inTempo,
	enteringTimes, inTimes,
	enteringSubs,  inSubs,
	running
} menuState = inTempo;

void handleButtons(byte buttons, byte oldButtons) {
	/**
	 * press mode -> entering suivant + aff msg associé
	 * release mode -> in* + aff valeur courante
	 * press +/- -> setTimer pour démarrer en autorepeat
	 *   si timer écouler, changer son délai pour dérouler vite
	 * release +/- -> unset timer
	 *
	 * + chaque action nécessite un computeTicks
	 * + gérer les cas tordus ?
	 *  press alors qu'on est déjà en entering => ignorer ?
	 *  plusieurs boutons ?
	 *  + et - = mise à 0 ? bascule en mode tuner dans une v2.1 ?
	 */
}

void updateDisplay(void *, long, int) {
	// read buttons state
	pinMode(DATA, INPUT_PULLUP);
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
	if (oldButtons != buttons) {
		handleButtons(buttons, oldButtons);
		oldButtons = buttons;
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

setIntervalTimer ticker;
void computeTicks() {
	nbTicks = times * subs * FLASH;
	long interval = 60000L / (tempo * subs * FLASH);
	changeInterval(ticker, interval);
}

void nextTick(void *, long, int) {
	tick++;
	if (tick == nbTicks) {
		tick = 0;
	}
	if (tick % (FLASH * subs) == 0) {
		// new time : find which one, depending on time number and nb times
		// 2/4 => down,top
		// 3/4 => down,right,top
		// 4/4 => down,left,right,top
		// thus : t0 = down, last = top, previous = right, else = left.
		timeNb = tick / (FLASH * subs);
		byte *msg;
		if (timeNb == 0) {
			msg = MSG_DOWN;
		} else if (timeNb == times - 1) {
			msg = MSG_TOP;
		} else if (timeNb == times - 2) {
			msg = MSG_RIGHT;
		} else {
			msg = MSG_LEFT;
		}
		displayChars(msg);
		tac(10);
	} else if (tick % (FLASH * subs) == 1) {
		// reduce current time
		byte *msg;
		if (timeNb == 0) {
			msg = MSG_DOWN2;
		} else if (timeNb == times - 1) {
			msg = MSG_TOP2;
		} else if (timeNb == times - 2) {
			msg = MSG_RIGHT2;
		} else {
			msg = MSG_LEFT2;
		}
		displayChars(msg);
	} else if (tick % FLASH == 0) {
		// add half
		addChars(MSG_HALF);
		tac(5);
	} else if (tick % FLASH == 1) {
		// remove half
		subChars(MSG_HALF);
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

	ticker = setInterval(10000, nextTick, NULL);
	computeTicks();
	setInterval(3, updateDisplay, NULL);
}

void loop() {
	setIntervalStep();
}
