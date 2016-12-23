// PIF_TOOL_CHAIN_OPTION: UPLOAD_OPTIONS := -c "raw,cr"
// PIF_TOOL_CHAIN_OPTION: EXTRA_LIBS := ArduinoTools

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
	#define DATA 2
	#define CLOCK 3
	#define BEEP PWM_1_B // = 4
	#define BEEP_PWM 1
	#define BEEP_PWM_MODE_ON WGM_1_FAST_OCR1C | SET_PWM_1_B
	#define BEEP_PWM_MODE_OFF SET_PWM_1_NONE
	// doesn't work with COMPARE_OUTPUT_MODE_1_SET_NONE : why ???
	#define BEEP_PWM_OUTPUT_ON COMPARE_OUTPUT_MODE_1_CLEAR_NONE
	#define BEEP_PWM_OUTPUT_OFF COMPARE_OUTPUT_MODE_1_NONE_NONE

	#ifdef DEMUX_2_TO_3
		#define DIGIT_L 0
		#define DIGIT_H 1
	#else
		#define DIGIT_1 ?
		#define DIGIT_2 ?
		#define DIGIT_3 ?
	#endif
#else
	#define DATA 2
	#define CLOCK 3
	#define BEEP_PWM 1
	#define BEEP PWM_1_B // = 10
	#define BEEP_PWM_MODE_ON  WGM_1_FAST_ICR
	#define BEEP_PWM_MODE_OFF WGM_1_FAST_ICR
	#define BEEP_PWM_OUTPUT_ON  COMPARE_OUTPUT_MODE_TOGGLE
	#define BEEP_PWM_OUTPUT_OFF COMPARE_OUTPUT_MODE_NONE

	#ifdef DEMUX_2_TO_3
		#define DIGIT_L 8
		#define DIGIT_H 9
	#else
		#define DIGIT_1 8
		#define DIGIT_2 9
		#define DIGIT_3 10
	#endif
//	#define DEBUG
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
#define MSG_TEMPO  (mapDisplays +  0)
#define MSG_TIMES  (mapDisplays +  3)
#define MSG_SUBS   (mapDisplays +  6)
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
#define TEMPO_MIN 30
#define TEMPO_MAX 180
// times per measure : 1 - 4
byte times =  4;
// current time
byte timeNb;
// subdivisions per time : 1 - 4
byte subs  =  2;
// number of ticks needed to handle everything (FLASH * times * subs)
byte nbTicks;
// current tick
byte tick;

byte digit = 0;
byte buttons = 0;
byte oldButtons;
#define DEBOUNCE_DELAY 10
byte debounce = DEBOUNCE_DELAY;
#define BUTTON_MINUS 0x04
#define BUTTON_PLUS  0x02
#define BUTTON_MODE  0x01

enum {
	enteringTempo, inTempo,
	enteringTimes, inTimes,
	enteringSubs,  inSubs,
	running
} menuState = running;

word top, prescale; // initialized once in setup
setIntervalTimer tacTimer = -1;
void stopTac(void *, long, int) {
	setPWM(BEEP_PWM, 0,
			BEEP_PWM_OUTPUT_OFF, 0,
			BEEP_PWM_OUTPUT_OFF ,0,
			BEEP_PWM_MODE_OFF, prescale);
//	GTCCR = 0;
//	TCCR1 = 0;
	changeInterval(tacTimer, SET_INTERVAL_PAUSED);
}
void tac(unsigned long delay) {
//	TCNT1 = 0;
//	OCR1A = top / 2;
//	OCR1C = top;
//	TCCR1 = 0b10000111;
//	GTCCR = 0b01110000;
	setPWM(BEEP_PWM, top,
			BEEP_PWM_OUTPUT_OFF, 0,
			BEEP_PWM_OUTPUT_ON, top / 2,
			BEEP_PWM_MODE_ON, prescale);
	changeInterval(tacTimer, delay);
}

setIntervalTimer tickTimer;
void computeTicks() {
	nbTicks = times * subs * FLASH;
	long interval = 60000L / (tempo * subs * FLASH);
	tick = 0;
	changeInterval(tickTimer, interval);
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
		if(menuState == running) {
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
		}
		tac(10);
	} else if (tick % (FLASH * subs) == 1) {
		if(menuState == running) {
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
		}
	} else if (tick % FLASH == 0) {
		if(menuState == running) {
			// add half
			addChars(MSG_HALF);
		}
		tac(5);
	} else if (tick % FLASH == 1) {
		if(menuState == running) {
			// remove half
			subChars(MSG_HALF);
		}
	}
}

void doPlus() {
#ifdef DEBUG
	Serial.print('+');
#endif
	switch(menuState) {
	case inTempo:
		tempo += 2;
		if (tempo > TEMPO_MAX) {
			tempo = TEMPO_MIN;
		}
		displayNumber(tempo);
		break;
	case inTimes:
		if (times < 4) {
			times++;
			displayNumber(times);
		}
		break;
	case inSubs:
		if (subs < 4) {
			subs++;
			displayNumber(subs);
		}
		break;
	}
	computeTicks();
}
void doMinus() {
#ifdef DEBUG
	Serial.print('-');
#endif
	switch(menuState) {
	case inTempo:
		tempo -= 2;
		if (tempo < TEMPO_MIN) {
			tempo = TEMPO_MAX;
		}
		displayNumber(tempo);
		break;
	case inTimes:
		if (times > 1) {
			times--;
			displayNumber(times);
		}
		break;
	case inSubs:
		if (subs > 1) {
			subs--;
			displayNumber(subs);
		}
		break;
	}
	computeTicks();
}

#define AUTOREPEAT_DELAY 1000
#define AUTOREPEAT_SPEED 100
setIntervalTimer autorepeatTimer;

void autorepeat(void *, long, int) {
	changeInterval(autorepeatTimer, AUTOREPEAT_SPEED);
	if (buttons & BUTTON_PLUS) {
		doPlus();
	} else {
		doMinus();
	}
}

#define PRESS(which) (!(oldButtons & which) && (buttons & which))
#define RELEASE(which) ((oldButtons & which) && !(buttons & which))
void handleButtons(byte buttons, byte oldButtons) {
#ifdef DEBUG
	Serial.print('b');
#endif
	// press "mode" button -> enter next menu item
	if (PRESS(BUTTON_MODE)) {
		switch(menuState) {
		case running:
			menuState = enteringTempo;
			displayChars(MSG_TEMPO);
			return;
		case inTempo:
			menuState = enteringTimes;
			displayChars(MSG_TIMES);
			return;
		case inTimes:
			menuState = enteringSubs;
			displayChars(MSG_SUBS);
			return;
		case inSubs:
			menuState = running;
			displayChars(MSG_NONE);
			return;
		default:
			return;
		}
	}
	// release "mode" button -> display current value
	if (RELEASE(BUTTON_MODE)) {
		switch(menuState) {
		case enteringTempo:
			menuState = inTempo;
			displayNumber(tempo);
			return;
		case enteringTimes:
			menuState = inTimes;
			displayNumber(times);
			return;
		case enteringSubs:
			menuState = inSubs;
			displayNumber(subs);
			return;
		default:
			return;
		}
	}
	if (PRESS(BUTTON_PLUS)) {
		changeInterval(autorepeatTimer, AUTOREPEAT_DELAY);
		doPlus();
	} else if (RELEASE(BUTTON_PLUS)) {
		changeInterval(autorepeatTimer, SET_INTERVAL_PAUSED);
	} else if (PRESS(BUTTON_MINUS)) {
		changeInterval(autorepeatTimer, AUTOREPEAT_DELAY);
		doMinus();
	} else if (RELEASE(BUTTON_MINUS)) {
		changeInterval(autorepeatTimer, SET_INTERVAL_PAUSED);
	}
}

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
	if (buttons & BUTTON_MODE) {
		currentDisplay[0] |= 0x01;
	} else {
		currentDisplay[0] &= 0xFE;
	}
	if (buttons & BUTTON_MINUS) {
		currentDisplay[1] |= 0x01;
	} else {
		currentDisplay[1] &= 0xFE;
	}
	if (buttons & BUTTON_PLUS) {
		currentDisplay[2] |= 0x01;
	} else {
		currentDisplay[2] &= 0xFE;
	}

	// handle button change
	if (oldButtons != buttons) {
		if (debounce == 0) {
			handleButtons(buttons, oldButtons);
			oldButtons = buttons;
			debounce = DEBOUNCE_DELAY;
		} else {
			debounce--;
		}
	} else {
		debounce = DEBOUNCE_DELAY;
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

void setup() {
#ifdef __AVR_ATtinyX5__
	// set clock frequency prescaler to 1 => 8MHz
	CLKPR = 0x80; // set high bit to enabled prescale write
	CLKPR = 0x00; // set prescale to 0 = /1
#endif

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
	computePWM(BEEP_PWM, frequency, prescale, top);
	// usefull to debug on ATtiny
	// tempo=frequency;
	// times=prescale;
	// subs=top;

	// reserve timers, but stop them
	autorepeatTimer = setInterval(SET_INTERVAL_PAUSED, autorepeat, NULL);
	tacTimer = setInterval(SET_INTERVAL_PAUSED, stopTac, NULL);
	tickTimer = setInterval(SET_INTERVAL_PAUSED, nextTick, NULL);
	computeTicks();
	setIntervalTimer updateDisplayTimer = setInterval(3, updateDisplay, NULL);

#ifdef DEBUG
	Serial.begin(DEFAULT_BAUDRATE);
	Serial.print("tickTimer = ");Serial.println(tickTimer);
	Serial.print("updateDisplayTimer = ");Serial.println(updateDisplayTimer);
	Serial.print("autorepeatTimer = ");Serial.println(autorepeatTimer);
	Serial.print("tacTimer = ");Serial.println(tacTimer);
	Serial.println("Ready ..");
#endif

}

void loop() {
	setIntervalStep();
}
