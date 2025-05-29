#include <FMTX.h>
#include <LiquidCrystal_I2C.h>
#include <SD.h>
#include <TMRpcm.h>

#include "button.hpp"
#include "pinout.hpp"

LiquidCrystal_I2C lcd(0x27, 16, 2);
TMRpcm sd_player;

static bool playing_bluetooth = true;
static bool selecting_freq = false;
static bool sd_card_error = false;

static float fm_set_freq = 90;

volatile float fm_sel_freq;
volatile bool freq_changed = false;

void print_freq_banner(void)
{
	lcd.clear();
	lcd.setCursor(0, 0);
	if (sd_card_error) {
		lcd.print("[SD] Read error");
		return;
	}

	if (playing_bluetooth)
		lcd.print("[BT] Playing on");
	else
		lcd.print("[SD] Playing on");

	lcd.setCursor(0, 1);
	lcd.print(fm_set_freq);
	lcd.print(" MHz");
}

void print_freq_select_banner(void)
{
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("Selecting freq:");
	lcd.setCursor(0, 1);
	lcd.print(fm_sel_freq);
	lcd.print(" MHz");
}

static inline void adc_setup(void)
{
	ADMUX |= (0b01 << REFS0); // AVcc with external capacitor at AREF pin

	ADMUX |= (ADC3D << MUX0); // ADC3

	ADCSRA |= (1 << ADATE); // auto trigger enable
	ADCSRA |= (1 << ADIE);  // enable ADC interrupt
	ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // prescaler 128

	ADCSRB &= ~(0b111 << ADTS0); // free running mode
}

ISR(ADC_vect)
{
	float read_freq =
		map(ADC, MIN_FREQ_ADC_READ, MAX_FREQ_ADC_READ, MIN_FREQ, MAX_FREQ);
	read_freq /= 10.0;

	if (abs(read_freq - fm_sel_freq) >= 0.05) {
		fm_sel_freq = read_freq;
		freq_changed = true;
	}
}

#define MAX_FILES 5
String audio_files[MAX_FILES];
int file_count = 0;
int current_file = 0;

void collect_wav_files(File dir)
{
	File file = dir.openNextFile();
	while (file) {
		if (!file.isDirectory() && file.name()[0] != '.') {
			String filename = String(file.name());
			if (filename.endsWith(".WAV") && file_count < MAX_FILES) {
				audio_files[file_count++] = filename;
			}
		}

		file.close();
		file = dir.openNextFile();
	}

	if (file_count > 0) {
		sd_player.quality(true);
		sd_player.volume(5);
		sd_player.play(audio_files[0].c_str());
	} else
		sd_card_error = true;
}

void switch_play_mode(void)
{
	playing_bluetooth = !playing_bluetooth;
	sd_card_error = false;
	selecting_freq = false;

	if (playing_bluetooth) {
		PORTD |= (1 << BTH_ENABLE); // enable bluetooth
		print_freq_banner();
		sd_player.disable();
		SD.end();
		return;
	}

	if (SD.begin())
		collect_wav_files(SD.open("/"));
	else
		sd_card_error = true;
	print_freq_banner();
}

void toggle_freq_sel(void)
{
	selecting_freq = !selecting_freq;
	if (selecting_freq) {
		ADCSRA |= (1 << ADEN) | (1 << ADSC); // enable & start ADC
		print_freq_select_banner();
		return;
	}

	ADCSRA &= ~(1 << ADEN); // disable ADC
	fm_set_freq = fm_sel_freq;
	fmtx_set_freq(fm_set_freq);
	print_freq_banner();
}

void setup(void)
{
	DDRC &= ~(1 << FREQ_SEL_KNOB);
	DDRD &= ~(1 << FREQ_SEL_BTN);
	DDRD |= (1 << BTH_ENABLE);

	PORTD |= (1 << FREQ_SEL_BTN); // button pull-up
	PORTD |= (1 << BTH_ENABLE);   // enable bluetooth

	EICRA |= (1 << ISC00);  // INT0 edge trigger
	EICRA &= ~(1 << ISC01); //
	EIMSK |= (1 << INT0);   // enable INT0

	adc_setup();

	sei();

	lcd.init();
	lcd.backlight();
	print_freq_banner();

	sd_player.speakerPin = SD_PLAYER_SINK;

	fmtx_init(fm_set_freq, EUROPE);
}

void skip_song(void)
{
	current_file = (current_file + 1) % file_count;
	sd_player.play(audio_files[current_file].c_str());
}

void loop(void)
{
	// Update the displayed selected frequency
	if (selecting_freq && freq_changed) {
		freq_changed = false;
		lcd.setCursor(0, 1);
		lcd.print(fm_sel_freq);
		lcd.print(" MHz");
	}

	enum button_state state = handle_button_input();
	if (state == PRESS) {
		if (playing_bluetooth)
			toggle_freq_sel();
		else
			skip_song();
	} else if (state == HOLD) {
		switch_play_mode();
	}
}
