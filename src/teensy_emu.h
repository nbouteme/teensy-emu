#pragma once

#include <functional>
#include <fcntl.h>

int teensy_emu_init(const char *);

void set_teensy_serial_print(std::function<void(const void *data, size_t len)>);
void set_teensy_lcd_print(std::function<void(const void *data, size_t len)>);
void set_teensy_sd_root(const char *);
void teensy_remote_send(int);

void teensy_observe_pin(int n, std::function<void(int)>);

// Les définitions des structures si dessous doivent correspondrent avec ce avec quoi
// les "modules" sont compilés

struct LiquidCrystal {
	LiquidCrystal(int, int, int, int, int, int) {}

	void begin(int, int);

	void setCursor(int, int);
	void cursor();
	void blink();
	void noCursor();
	void noBlink();
	void clear();

	void print(const char *s);
	void printf(const char *f, ...);
};

#define FILE_WRITE O_RDWR

struct File {
	int fd;
	void read(void *b, int n);
	void write(void *b, int n);
	operator bool() const {return fd >= 0;}
	const char *name();
	bool isDirectory();
	File openNextFile();
	void flush();
};

struct SDS {
	bool begin(int);
	int exists(const char *);
	File open(const char *, int mode = FILE_WRITE);
};

struct AudioSource {
};

struct AudioSink {
};

struct AudioPlaySdWav : AudioSource {
	void play(const char *fn);
	void stop();
	int isPlaying();
	int isStopped();
};

struct AudioConnection {
	AudioConnection(AudioSource&, int, AudioSink&, int);
};

struct tmElements_t {
	unsigned char Second, Minute, Hour, Wday, Day, Month, Year;
};

struct DS1307RTC {
	void read(tmElements_t&);
	void write(tmElements_t&);
};


struct IntervalTimer {
	bool begin(void (*f)(void), int);
	void end();
};

struct SerialS {
	void begin(int);
	void print(const char *s);
	void println(const char *s);
	void printf(const char *f, ...);
};

void delay(int ms);
void attachInterrupt(int, void (*fun)(void), int);
void pinMode(int, int);
void digitalWrite(int p, int v);


struct decode_results {
	int value;
};

struct IRrecv {
	IRrecv(int) {}
	void enableIRIn(){}
	void resume();
	int decode(decode_results*);
};

void triggerInt(int pin, int mode);
