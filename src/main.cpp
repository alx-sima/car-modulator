#include <FMTX.h>
#include <LiquidCrystal_I2C.h>

#include "HardwareSerial.h"

#define FREQ_SEL_BTN PC2 // INT0

#define FREQUENCY_PIN     PC3
#define MIN_FREQ_ADC_READ 3
#define MAX_FREQ_ADC_READ (340 - MIN_FREQ_ADC_READ)
#define MIN_FREQ          875
#define MAX_FREQ          1080

LiquidCrystal_I2C lcd(0x27, 16, 2);

bool selecting = false;

float fm_set_freq = 90; // Here set the default FM frequency
float fm_sel_freq = 90; // Here set the default FM frequency

void print_freq_banner()
{
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("Playing on");
	lcd.setCursor(0, 1);
	lcd.print(fm_set_freq);
	lcd.print(" MHz");
}

static inline void setup_adc(void)
{
	ADMUX |= (0b01 << REFS0); // AVcc with external capacitor at AREF pin

	ADMUX |= (ADC3D << MUX0); // ADC3

	ADCSRA |= (1 << ADATE); // auto trigger enable
	ADCSRA |= (1 << ADIE);  // enable ADC interrupt
	ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // prescaler 128

	ADCSRB &= ~(0b111 << ADTS0); // free running mode
}

void setup(void)
{
	DDRC &= ~(1 << FREQUENCY_PIN);
	DDRD &= ~(1 << FREQ_SEL_BTN);

	/* button pull-ups */
	PORTD |= (1 << FREQ_SEL_BTN);

	EICRA |= (1 << ISC01); // interrupt on falling edge
	EIMSK |= (1 << INT0);  // enable INT0

	setup_adc();

	Serial.begin(9600);

	lcd.init();
	lcd.backlight();
	print_freq_banner();

	fmtx_init(fm_set_freq, EUROPE);
}

int samples[64];
int collected = 0;

#define ADC_TOL 2

volatile int adc_val = 0;

ISR(ADC_vect)
{
	int new_adc_val = ADC;
	if (abs(new_adc_val - adc_val) > ADC_TOL) {
		adc_val = new_adc_val;
		Serial.print("ADC: ");
		Serial.print(ADC);
		Serial.println();
	}
}

ISR(INT0_vect)
{
	Serial.println("INT0");
	Serial.println();
}

void loop(void)
{
	if (!(PINC & (1 << FREQ_SEL_BTN))) {
		if (selecting) {
			ADCSRA &= ~(1 << ADEN); // disable ADC
		} else {
			ADCSRA |= (1 << ADEN) | (1 << ADSC); // enable & start ADC
		}
		selecting = !selecting;
	}

	return;
	if (selecting) {
		lcd.setCursor(0, 1);
		lcd.print(fm_set_freq);
	}
	Serial.println(PINC);
	if (!(PINC & (1 << FREQ_SEL_BTN))) {
		delay(1000);
		selecting = !selecting;

		if (selecting) {
			lcd.clear();
			lcd.setCursor(0, 0);
			lcd.print("Select Frequency");
		} else {
			fmtx_set_freq(fm_set_freq);
			print_freq_banner();
		}

		return;
	}
	if (selecting) {
		int value = analogRead(FREQUENCY_PIN);
		if (value < MIN_FREQ_ADC_READ) {
			value = MIN_FREQ;
		} else if (value > MAX_FREQ_ADC_READ) {
			value = MAX_FREQ;
		} else {
			value = map(value, MIN_FREQ_ADC_READ, MAX_FREQ_ADC_READ, MIN_FREQ,
						MAX_FREQ);
		}

		samples[collected++] = value;
		if (collected * sizeof(*samples) > sizeof(samples)) {
			long sum = 0;
			for (int i = 0; i < sizeof(samples) / sizeof(*samples); i++) {
				sum += samples[i];
			}
			value = (float)sum / (sizeof(samples) / sizeof(*samples));
			collected = 0;
		}

		fm_set_freq = value / 10.0; // Convert to MHz
	}
}