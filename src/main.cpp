#include <LiquidCrystal_I2C.h>

#include <FMTX.h>

#define FREQ_SEL_BTN PC2

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

void setup(void)
{
	DDRC &= ~(1 << FREQUENCY_PIN);
	DDRC &= ~(1 << FREQ_SEL_BTN);

	/* Button pull-ups */
	PORTC |= (1 << FREQ_SEL_BTN);

	Serial.begin(9600);
	Serial.print("FM-TX Demo\r\n");

	Serial.print(PORTC);
	Serial.print(" ");
	Serial.print(DDRC);

	lcd.init();
	lcd.backlight();
	print_freq_banner();

	fmtx_init(fm_set_freq, EUROPE);
}

int samples[64];
int collected = 0;

void loop(void)
{
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
	return;

	if (Serial.available()) {
		switch (Serial.read()) {
		case '&':
			u8 i, buf[4];
			float ch;
			i = 0;
			delay(30);
			while (Serial.available() && i < 4) {
				buf[i] = Serial.read();
				if (buf[i] <= '9' && buf[i] >= '0') {
					i++;
				} else {
					i = 0;
					break;
				}
			}
			if (i == 4) {
				ch = (buf[0] - '0') * 100 + (buf[1] - '0') * 10 +
					 (buf[2] - '0') * 1 + 0.1 * (buf[3] - '0');
				if (ch >= 70 && ch <= 108) {
					Serial.print("New Channel:");
					Serial.print(ch, 1);
					Serial.println("MHz");
					fmtx_set_freq(ch);
				} else {
					Serial.println(
						"ERROR:Channel must be range from 70Mhz to 108Mhz.");
				}
			} else {
				Serial.println("ERROR:Input Format Error.");
			}

			while (Serial.available()) {
				Serial.read();
			}
			break;
		}
	}
}