/*
 * Voltmeter.c
 *
 * Created: 5/10/2022 8:40:33 PM
 * Author : Orhan Ozbasaran
 */ 

#include <avr/io.h>

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <stdio.h>

#define XTAL_FRQ 8000000lu

#define SET_BIT(p,i) ((p) |=  (1 << (i)))
#define CLR_BIT(p,i) ((p) &= ~(1 << (i)))
#define GET_BIT(p,i) ((p) &   (1 << (i)))

#define WDR() asm volatile("wdr"::)
#define NOP() asm volatile("nop"::)
#define RST() for(;;);

#define DDR    DDRB
#define PORT   PORTB
#define RS_PIN 0
#define RW_PIN 1
#define EN_PIN 2

void avr_init(void)
{
	WDTCR = 15;
}

void avr_wait(unsigned short msec)
{
	TCCR0 = 3;
	while (msec--) {
		TCNT0 = (unsigned char)(256 - (XTAL_FRQ / 64) * 0.001);
		SET_BIT(TIFR, TOV0);
		WDR();
		while (!GET_BIT(TIFR, TOV0));
	}
	TCCR0 = 0;
}

static inline void
set_data(unsigned char x)
{
	PORTD = x;
	DDRD = 0xff;
}

static inline unsigned char
get_data(void)
{
	DDRD = 0x00;
	return PIND;
}

static inline void
sleep_700ns(void)
{
	NOP();
	NOP();
	NOP();
}

static unsigned char
input(unsigned char rs)
{
	unsigned char d;
	if (rs) SET_BIT(PORT, RS_PIN); else CLR_BIT(PORT, RS_PIN);
	SET_BIT(PORT, RW_PIN);
	get_data();
	SET_BIT(PORT, EN_PIN);
	sleep_700ns();
	d = get_data();
	CLR_BIT(PORT, EN_PIN);
	return d;
}

static void
output(unsigned char d, unsigned char rs)
{
	if (rs) SET_BIT(PORT, RS_PIN); else CLR_BIT(PORT, RS_PIN);
	CLR_BIT(PORT, RW_PIN);
	set_data(d);
	SET_BIT(PORT, EN_PIN);
	sleep_700ns();
	CLR_BIT(PORT, EN_PIN);
}

static void
write(unsigned char c, unsigned char rs)
{
	while (input(0) & 0x80);
	output(c, rs);
}

void
lcd_init(void)
{
	SET_BIT(DDR, RS_PIN);
	SET_BIT(DDR, RW_PIN);
	SET_BIT(DDR, EN_PIN);
	avr_wait(16);
	output(0x30, 0);
	avr_wait(5);
	output(0x30, 0);
	avr_wait(1);
	write(0x3c, 0);
	write(0x0c, 0);
	write(0x06, 0);
	write(0x01, 0);
}

void
lcd_clr(void)
{
	write(0x01, 0);
}

void
lcd_pos(unsigned char r, unsigned char c)
{
	unsigned char n = r * 40 + c;
	write(0x02, 0);
	while (n--) {
		write(0x14, 0);
	}
}

void
lcd_put(char c)
{
	write(c, 1);
}

void
lcd_puts1(const char *s)
{
	char c;
	while ((c = pgm_read_byte(s++)) != 0) {
		write(c, 1);
	}
}

void
lcd_puts2(const char *s)
{
	char c;
	while ((c = *(s++)) != 0) {
		write(c, 1);
	}
}

int is_pressed(int r, int c) {
	// Set all 8 GPIOs to N/C
	DDRC = 0;
	PORTC = 0;
	SET_BIT(DDRC, 4 + r);
	SET_BIT(PORTC, c);
	avr_wait(1);
	if (!GET_BIT(PINC, c)) {
		return 1;
	}
	return 0;
}

int get_key() {
	int i, j;
	for (i=0; i < 4; i++) {
		for (j=0; j < 4; j++) {
			if (is_pressed(i, j)) {
				return 4 * i + j + 1;
			}
		}
	}
	return 0;
}

int get_sample() {
	ADMUX = 0b01000000;
	ADCSRA = 0b11000000;
	while (GET_BIT(ADCSRA, 6));
	return ADC;
}

int main () {
	char buf[20];
	int new_sample, reset = 0, max = 0, min = 1023, count = 0, k = 0;
	unsigned long long sum = 0;
	avr_init();
	lcd_init();
	while (1) {
		k = get_key();
		avr_wait(1);
		switch(k){
			case 1:
				if (reset == 0){
					reset = 1;
				}
				else{
					reset = 0;
				}
				max = 0;
				min = 1023;
				count = 0;
				sum = 0;	
				break;
		}
		new_sample = get_sample();
		sum += new_sample;
		if (new_sample > max) {
			max = new_sample;
		}
		if (new_sample < min) {
			min = new_sample;
		}
		if (reset == 0){
			sprintf(buf, "%4.2f : %4.2f", (new_sample / 1023.0) * 5, (max / 1023.0) * 5);
			lcd_clr();
			lcd_pos(0, 0);
			lcd_puts2(buf);
			sprintf(buf, "%4.2f : %4.2f", (min / 1023.0) * 5, (sum / ++count / 1023.0) * 5);
			lcd_pos(1, 0);
			lcd_puts2(buf);
		}
		else{
			sprintf(buf, "%4.2f : %s", (new_sample / 1023.0) * 5, "----");
			lcd_clr();
			lcd_pos(0, 0);
			lcd_puts2(buf);
			sprintf(buf, "%s : %s", "----", "----");
			lcd_pos(1, 0);
			lcd_puts2(buf);		
		}
		
		avr_wait(500);
	}
}

