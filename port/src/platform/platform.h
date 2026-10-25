// Platform layer: display, input, resource files, save dir, audio, timing.
// Everything above this (jrt, midp, the translated game) is platform independent.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace plat {

// Phone screen height; width is 128 (original) or wider in widescreen mode.
constexpr int PHONE_W = 128;
constexpr int PHONE_H = 160;

// Nokia / MIDP raw key codes, as delivered to Canvas.keyPressed().
enum Key : int {
  KEY_UP = -1, KEY_DOWN = -2, KEY_LEFT = -3, KEY_RIGHT = -4, KEY_FIRE = -5,
  KEY_SOFT_LEFT = -6, KEY_SOFT_RIGHT = -7,
  KEY_NUM0 = '0', KEY_NUM1, KEY_NUM2, KEY_NUM3, KEY_NUM4,
  KEY_NUM5, KEY_NUM6, KEY_NUM7, KEY_NUM8, KEY_NUM9,
  KEY_STAR = '*', KEY_POUND = '#',
};

// Port-level actions (never sent to the game).
enum SysAction : int {
  SYS_NONE = 0,
  SYS_QUIT = 1000,         // window closed
};

struct KeyEvent {
  int code;      // Key or SysAction
  bool pressed;
};

// Pixel: integer nearest. Fit: sharp-bilinear (nearest to the next integer, then linear) to fill.
enum class Scale { Pixel, Fit };

bool init();
void shutdown();

// Pumps OS events; returns false when the app should quit. Appends key/system events.
bool poll(std::vector<KeyEvent>& out);

// Presents a w x h 0xAARRGGBB framebuffer, scaled to the display.
void present(const uint32_t* fb, int w, int h);
void setScale(Scale s);
Scale scale();

// Reads a resource ("/icon.png" or "snd/x.ogg"). Empty vector if missing.
std::vector<uint8_t> readResource(const std::string& name);

// Directory for save data (RMS + settings), with trailing slash.
std::string saveDir();

// Audio: one-shot clips by id (pre-rendered snd/<id>.ogg). Voice handles are >= 0.
int audioPlay(uint32_t clipId, int volume);  // volume 0..100; returns voice or -1
bool audioPlaying(int voice, uint32_t clipId);
void audioStop(int voice);
void audioSetVolume(int voice, int volume);

uint64_t millis();
void sleepMs(int ms);

// Threads and locks via SDL (std::thread is unreliable on the Vita's pthread layer).
void startThread(const char* name, void (*fn)(void*), void* arg);
class Mutex {
 public:
  Mutex();
  ~Mutex();
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;
  void lock();
  void unlock();

 private:
  void* m_;
};
struct LockGuard {
  explicit LockGuard(Mutex& m) : m_(m) { m_.lock(); }
  ~LockGuard() { m_.unlock(); }
  Mutex& m_;
};

}  // namespace plat
