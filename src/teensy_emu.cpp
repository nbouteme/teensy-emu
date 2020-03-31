#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "teensy_emu.h"
#include <dlfcn.h>
#include <unistd.h>
#include <cstring>
#include <map>
#include <thread>
#include <stdarg.h>
#include <dirent.h>
#include <sys/stat.h>
#include <condition_variable>
#include <mutex>
#include <signal.h>


typedef void (*entry)(...);

struct Serial {
    void write(const void *data, size_t len);
};

struct LCD {
    void write(const void *data, size_t len);
};

char *root_sddir = 0;
int current_code;
int playing = 0;

std::function<void(const void *data, size_t len)> wcb;
std::map<int, void (*)(void)> ints;

void Serial::write(const void *data, size_t len) {
    if (wcb)
        wcb(data, len);
}

std::function<void(const void *data, size_t len)> lcb;
void LCD::write(const void *data, size_t len) {
    if (lcb)
        lcb(data, len);
}

std::map<int, std::function<void(int)>> observers;
void teensy_observe_pin(int n, std::function<void(int)> f) {
    observers[n] = f;
}


/*
   Contrairement au teensy où les interruptions des timers suspendent l'execution
   de l'unique thread, ici les timers sont implémentés avec des threads, donc
   peuvent être une source de différence de comportement entre "l'émulateur" et
   une version physique
   Pour éviter de mauvaises surprises: le code des interruption doit être simple, tel que recommandé de base
*/

struct interruptible_sleep {
    std::condition_variable c;
    std::mutex m;
    bool stop = false;
    bool active = true; // false si thread est dormant

    void sleep(unsigned ms) {
        std::unique_lock<std::mutex> l(m);
        c.wait_for(l, std::chrono::microseconds(ms), [this]() { return stop; });
    }

    void interrupt() {
        stop = true;
        c.notify_one();
        // permet à un thread de mettre fin à
        // son timer de lui même si interrupt
        // est appelé depuis son callback interne
        if (active)
            return;
    	std::unique_lock<std::mutex> l(m);
    	c.wait(l, [this](){return !stop;});
    }
};
struct interval_state {
    std::thread t;
    std::thread::native_handle_type handle;
    interruptible_sleep is;
};

std::map<IntervalTimer*, interval_state> itmap;

int teensy_emu_init(const char *modp) {
    // reset l'état:
	static volatile int running;
	static std::thread t;
	static std::thread::native_handle_type handle;

    for (auto &p: itmap) {
        p.first->end();
        // provoque une fuite évidente mais inévitable a cause du fait qu'on
        // fait executer aux threads du code arbitraire et qu'on peut pas
        // traquer leurs ressources
        pthread_cancel(p.second.handle);
    }
    itmap.clear();

    if (running) { // un thread d'émulation déjà en cours
        pthread_cancel(handle);
    }

    auto mod = dlopen(modp, RTLD_NOW | RTLD_GLOBAL);
	if (!mod)
		return printf("An error has occurrd: %s\n", dlerror()), 0;
	entry setup = (entry)dlsym(mod, "_Z5setupv");
	entry loop = (entry)dlsym(mod, "_Z4loopv");
	if (t.joinable()) {
	    running = 0;
	    t.join();
	}
	running = 1;
	t = std::thread([=](){
    	setup();
    	while (running) {
        	loop();
            // on a pas de vrai interruptions,
        	// donc cette boucle infini est couteuse sans sommeil
        	usleep(16666);
    	}
	});
	handle = t.native_handle();
	t.detach();
    return 1;
}

void set_teensy_serial_print(std::function<void(const void *data, size_t len)> _wcb) {
    wcb = _wcb;
}

void set_teensy_lcd_print(std::function<void(const void *data, size_t len)> _wcb) {
    lcb = _wcb;
}

void set_teensy_sd_root(const char *path) {
    free(root_sddir);
    root_sddir = strdup(path);
}

void teensy_remote_send(int code) {
    current_code = code;
}

/*
    Le sous ensemble implémenté ici n'est que ce qui est nécessaire pour simuler
    mon projet de réveil
*/


char screen[2][16];
int x, y;
void LiquidCrystal::begin(int, int){
}

void LiquidCrystal::setCursor(int a, int b) {
    x = a;
    y = b;
}

void LiquidCrystal::cursor() {}
void LiquidCrystal::blink() {}
void LiquidCrystal::noCursor() {}
void LiquidCrystal::noBlink() {}
void LiquidCrystal::clear() {
    memset(screen, 32, sizeof screen);
    print("");
}

void LiquidCrystal::print(const char *s) {
    int m = 16 - x;
    int l = strlen(s);
    memcpy(screen[y] + x, s, l < m ? l : m);
    lcb(screen, 32);
}

void LiquidCrystal::printf(const char *f, ...) {
	va_list args;
	va_start(args, f);
	char *line;
    vasprintf(&line, f, args);
    print(line);
    free(line);
    va_end(args);
}

void File::read(void *b, int n) {
    ::read(fd, b, n);
}

void File::write(void *b, int n) {
    ::write(fd, b, n);
}

const char *File::name() {
    static char filePath[PATH_MAX];
    char filepath[PATH_MAX];
    sprintf(filepath, "/proc/self/fd/%d", fd);
    memset(filePath, 0, sizeof(filePath));
    readlink(filepath, filePath, PATH_MAX);
    return strrchr(filePath, '/') + 1;
}

bool File::isDirectory() {
    struct stat sb;
    fstat(fd, &sb);
    return S_ISDIR(sb.st_mode);
}

File File::openNextFile() {
    static std::map<int, DIR*> fd_dir;
    if (!isDirectory())
        return {-1};
    if (!fd_dir[fd]) {
        puts("No existing folder inst, creating...");
        fd_dir[fd] = fdopendir(fd);
    }
    DIR *ent = fd_dir[fd];
    printf("ent: %p\n", ent);
    struct dirent *dent = readdir(ent);
    printf("dent: %p\n", dent);
    if (!dent) {
        closedir(ent);
        return File{-1};
    }
    return File{openat(fd, dent->d_name, O_RDWR)};
}

void File::flush() {
    fsync(fd);
}

bool SDS::begin(int) {
    printf("%p (%s)\n", root_sddir, root_sddir);
    return !!root_sddir;
}

int SDS::exists(const char *path) {
    char *fullpath;
    asprintf(&fullpath, "%s/%s", root_sddir, path);
    int r = access(fullpath, F_OK) == 0;
    free(fullpath);
    return r;
}

File SDS::open(const char *path, int mode) {
    char *fullpath;
    asprintf(&fullpath, "%s%s", root_sddir, path);
    struct stat sb;

    File f;
    stat(fullpath, &sb);
    if(S_ISDIR(sb.st_mode)) {
        f = {::open(fullpath, O_DIRECTORY | O_RDONLY)};
    } else {
        f = {::open(fullpath, O_RDWR | O_CREAT | mode, 0644)};
    }
    printf("Opened %s @ %d\n", fullpath, f.fd);
    free(fullpath);
    return f;
}

void AudioPlaySdWav::play(const char *fn) {
    playing = 1;
}

void AudioPlaySdWav::stop() {
    playing = 0;
}

int AudioPlaySdWav::isPlaying() {
    return playing;
}

int AudioPlaySdWav::isStopped() {
    return !isPlaying();
}

AudioConnection::AudioConnection(AudioSource &a, int b, AudioSink &c, int d) {}


//struct tmElements_t {
//	unsigned char Second, Minute, Hour, Wday, Day, Month, Year;
//};

void DS1307RTC::read(tmElements_t &v) {
    time_t s;
    time(&s);
    struct tm t = *localtime(&s);
    v = tmElements_t{
        t.tm_sec,
        t.tm_min,
        t.tm_hour,
        t.tm_wday,
        t.tm_mday,
        t.tm_mon + 1,
        t.tm_year - 70
    };
}

void DS1307RTC::write(tmElements_t &v) {
}


bool IntervalTimer::begin(void (*f)(void), int inter) {
    // me suis rendu compte que y'a plusieurs instances d'interval timer
    // flemme de recompiler/corriger les headers donc je sépare les instances en
    // fonction du pointeur this
    // cela pose aussi quelques problèmes avec l'ordre de deconstruction à la
    // fermeture du programme, mais c'est pas grave parce que c'est plus un jouet
    // qu'un truc sérieux
    auto &state = itmap[this];
    state.t = std::thread([=, &state](){
        state.is.stop = false; // peut etre rendre stop atomic?
        while (!state.is.stop) {
            state.is.active = false;
            state.is.sleep(inter);
            if (state.is.stop)
                break;
            state.is.active = true;
            f();
        }
        state.is.stop = false;
        if (!state.is.active) // si c'est un auto-arrêt, pas besoin de signaler
            state.is.c.notify_one();
    });
    state.handle = state.t.native_handle();
    state.t.detach();
    return true;
}

void IntervalTimer::end() {
    if (itmap.find(this) == itmap.end())
        return;
    auto &state = itmap[this];
    state.is.interrupt();
}

void SerialS::begin(int){}
void SerialS::print(const char *s) {
    wcb(s, strlen(s));
}
void SerialS::println(const char *s) {
    char *l;
    asprintf(&l, "%s\n", s);
    print(l);
    free(l);
}

void SerialS::printf(const char *f, ...) {
	va_list args;
	va_start(args, f);
	char *line;
    vasprintf(&line, f, args);
    print(line);
    free(line);
    va_end(args);
}

void delay(int ms) {usleep(ms * 1000);}

// les interruptions ne sont pas simulées au même niveau que celles du cortex-m7,
// seulement au degré de l'API teensy, parce que le code de gestion de plus bas niveau
// est dans la bibliothèque core, et n'est que difficilement simulable
void attachInterrupt(int pin, void (*fun)(void), int mode /*s'attendre toujours à FALLING*/) {
    ints[pin] = fun;
}

void triggerInt(int pin, int mode) {
    // assume mode == FALLING
    if (ints[pin])
        ints[pin]();
}

void pinMode(int, int) {}

void digitalWrite(int p, int v) {
    if (observers[p])
        observers[p](v);
}


//struct decode_results {
//	int value;
//};

int IRrecv::decode(decode_results *dr) {
    if (current_code)
        dr->value = current_code;
    return !!current_code;
}

void IRrecv::resume() {
    current_code = 0;
}


// Je stub les classes avec des int, c'est pas grave car elle seront
// utilisées comme thisptr, et comme ce sont des singletons aucune mémoire
// n'est effectivement utilisée (i.e: aucun code ne référencera this)
int SD, SYST_CSR, SYST_CVR, RTC, SPI, LCD, Serial, SCB_VTOR, SCB_ICSR;
int _VectorsRam[170+16];
