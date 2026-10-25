#include "platform/platform.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>

#ifdef __vita__
#include <psp2/io/stat.h>
#include <psp2/power.h>
#endif

namespace plat {

namespace {

constexpr int DISPLAY_W = 960, DISPLAY_H = 544;  // desktop window matches the Vita screen

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* screenTex;   // w x h, streaming, nearest
SDL_Texture* upTex;       // integer-upscaled render target for sharp-bilinear
int texW, texH, upK;
Scale curScale = Scale::Fit;
SDL_Joystick* joy;
bool audioOk;
std::map<uint32_t, Mix_Chunk*> clips;
// Per clip: its MIDI playing time (snd/lengths.txt). A phone's MIDI player stops at the file's
// last event; the pre-rendered clip rings on for seconds after that.
std::map<uint32_t, int> clipMs;
constexpr int MAX_VOICES = 8;
constexpr int TAIL_FADE_MS = 120;  // how quickly a clip's synth tail fades once its MIDI has ended
struct Voice { uint32_t clip = 0; uint64_t start = 0; int ms = -1; bool fading = false; };
Voice voices[MAX_VOICES];

#ifdef __vita__
const char* const RES_ROOT = "app0:res/";
const char* const SAVE_ROOT = "ux0:data/diamondrush/";
#else
std::string resRoot, saveRoot;
#endif

// ------------------------------------------------------------------------------------------
// Input

// Vita buttons as seen by SDL's joystick API.
enum VitaButton {
  VB_TRIANGLE, VB_CIRCLE, VB_CROSS, VB_SQUARE, VB_L, VB_R,
  VB_DOWN, VB_LEFT, VB_UP, VB_RIGHT, VB_SELECT, VB_START,
};

// Vita controls:
//   D-pad / left stick : move          Cross    : fire / OK (5)
//   Circle             : back (right soft key)    Triangle : left soft key
//   Square / L         : *  (special action)      R        : #
//   START              : pause (right soft key)
// (Scaling and widescreen are set in the game's Options menu.)
int mapJoyButton(int b) {
  switch (b) {
    case VB_UP: return KEY_UP;
    case VB_DOWN: return KEY_DOWN;
    case VB_LEFT: return KEY_LEFT;
    case VB_RIGHT: return KEY_RIGHT;
    case VB_CROSS: return KEY_FIRE;
    case VB_CIRCLE: return KEY_SOFT_RIGHT;
    case VB_TRIANGLE: return KEY_SOFT_LEFT;
    case VB_START: return KEY_SOFT_RIGHT;
    case VB_SQUARE: return KEY_STAR;
    case VB_L: return KEY_STAR;
    case VB_R: return KEY_POUND;
  }
  return 0;
}

// Desktop keyboard: arrows, Z/Enter = fire, X/Esc/Backspace = back, A = left soft,
// S = *, D = #, digits/numpad = phone keys, Tab = cycle scaling, F11 = toggle widescreen.
int mapKeyboard(SDL_Keycode k) {
  switch (k) {
    case SDLK_UP: return KEY_UP;
    case SDLK_DOWN: return KEY_DOWN;
    case SDLK_LEFT: return KEY_LEFT;
    case SDLK_RIGHT: return KEY_RIGHT;
    case SDLK_RETURN: case SDLK_z: case SDLK_SPACE: return KEY_FIRE;
    case SDLK_a: case SDLK_F1: return KEY_SOFT_LEFT;
    case SDLK_x: case SDLK_ESCAPE: case SDLK_BACKSPACE: return KEY_SOFT_RIGHT;
    case SDLK_s: case SDLK_KP_MULTIPLY: return KEY_STAR;
    case SDLK_d: case SDLK_KP_DIVIDE: return KEY_POUND;
  }
  if (k >= SDLK_0 && k <= SDLK_9) return KEY_NUM0 + (k - SDLK_0);
  if (k >= SDLK_KP_1 && k <= SDLK_KP_9) return KEY_NUM1 + (k - SDLK_KP_1);
  if (k == SDLK_KP_0) return KEY_NUM0;
  return 0;
}

// Directions can come from the D-pad and the stick at once; report a direction as held
// while either source holds it.
struct DirState { bool pad = false, stick = false; };
DirState dirs[4];  // up, down, left, right

int dirIndex(int key) { return key == KEY_UP ? 0 : key == KEY_DOWN ? 1 : key == KEY_LEFT ? 2 : 3; }
constexpr int DIR_KEYS[4] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT};

void setDir(int i, bool stick, bool on, std::vector<KeyEvent>& out) {
  DirState& d = dirs[i];
  bool before = d.pad || d.stick;
  (stick ? d.stick : d.pad) = on;
  bool after = d.pad || d.stick;
  if (before != after) out.push_back({DIR_KEYS[i], after});
}

void stickAxis(int axis, int value, std::vector<KeyEvent>& out) {
  // Hysteresis: engage past 60% deflection, release under 40%.
  constexpr int ON = 19660, OFF = 13107;
  int neg = axis == 0 ? 2 : 0, pos = axis == 0 ? 3 : 1;
  if (value < -ON) setDir(neg, true, true, out);
  else if (value > -OFF) setDir(neg, true, false, out);
  if (value > ON) setDir(pos, true, true, out);
  else if (value < OFF) setDir(pos, true, false, out);
}

void handleButton(int code, bool pressed, std::vector<KeyEvent>& out) {
  if (code == 0) return;
  if (code <= KEY_UP && code >= KEY_RIGHT) {
    setDir(dirIndex(code), false, pressed, out);
    return;
  }
  out.push_back({code, pressed});
}

// ------------------------------------------------------------------------------------------
// Display

void ensureTextures(int w, int h) {
  if (screenTex && w == texW && h == texH) return;
  if (screenTex) SDL_DestroyTexture(screenTex);
  if (upTex) SDL_DestroyTexture(upTex);
  texW = w; texH = h;
  screenTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
  SDL_SetTextureScaleMode(screenTex, SDL_ScaleModeNearest);
  // Sharp bilinear: nearest-upscale to the next integer factor, then linear-downscale to fit.
  float fit = std::min(float(DISPLAY_W) / w, float(DISPLAY_H) / h);
  upK = int(std::ceil(fit));
  upTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w * upK, h * upK);
  if (upTex) SDL_SetTextureScaleMode(upTex, SDL_ScaleModeLinear);
}

}  // namespace

// ------------------------------------------------------------------------------------------

bool init() {
#ifdef __vita__
  // Run the CPU/GPU at full clocks: the game is light, but software rendering + mixing at
  // low clocks can drop frames.
  scePowerSetArmClockFrequency(444);
  scePowerSetGpuClockFrequency(222);
#endif
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_AUDIO) != 0) {
    std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return false;
  }
  window = SDL_CreateWindow("Diamond Rush", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            DISPLAY_W, DISPLAY_H, 0);
  if (!window) return false;
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC |
                                                SDL_RENDERER_TARGETTEXTURE);
  if (!renderer) renderer = SDL_CreateRenderer(window, -1, 0);
  if (!renderer) return false;
  if (SDL_NumJoysticks() > 0) joy = SDL_JoystickOpen(0);

  audioOk = Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) == 0;
  if (audioOk) Mix_AllocateChannels(MAX_VOICES);
  else std::fprintf(stderr, "Mix_OpenAudio: %s\n", Mix_GetError());

#ifdef __vita__
  sceIoMkdir(SAVE_ROOT, 0777);
#else
  char* base = SDL_GetBasePath();
  resRoot = std::string(base ? base : "./") + "res/";
  SDL_free(base);
  char* pref = SDL_GetPrefPath("diamondrush", "diamondrush");
  saveRoot = pref ? pref : "./";
  SDL_free(pref);
#endif
  return true;
}

void shutdown() {
  if (audioOk) {
    Mix_HaltChannel(-1);
    for (auto& c : clips) Mix_FreeChunk(c.second);
    Mix_CloseAudio();
  }
  if (joy) SDL_JoystickClose(joy);
  if (upTex) SDL_DestroyTexture(upTex);
  if (screenTex) SDL_DestroyTexture(screenTex);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

bool poll(std::vector<KeyEvent>& out) {
  // fade out clips whose MIDI has ended (see clipMs)
  if (audioOk) {
    uint64_t now = SDL_GetTicks64();
    for (int ch = 0; ch < MAX_VOICES; ++ch) {
      Voice& v = voices[ch];
      if (v.ms >= 0 && !v.fading && now - v.start >= uint64_t(v.ms) && Mix_Playing(ch)) {
        Mix_FadeOutChannel(ch, TAIL_FADE_MS);
        v.fading = true;
      }
    }
  }
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    switch (e.type) {
      case SDL_QUIT:
        return false;
      case SDL_KEYDOWN:
      case SDL_KEYUP:
        if (e.key.repeat) break;
        handleButton(mapKeyboard(e.key.keysym.sym), e.type == SDL_KEYDOWN, out);
        break;
      case SDL_JOYBUTTONDOWN:
      case SDL_JOYBUTTONUP: {
        bool down = e.type == SDL_JOYBUTTONDOWN;
        handleButton(mapJoyButton(e.jbutton.button), down, out);
        break;
      }
      case SDL_JOYAXISMOTION:
        if (e.jaxis.axis <= 1) stickAxis(e.jaxis.axis, e.jaxis.value, out);
        break;
    }
  }
  return true;
}

void setScale(Scale s) { curScale = s; }
Scale scale() { return curScale; }

void present(const uint32_t* fb, int w, int h) {
  ensureTextures(w, h);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);

  SDL_Rect dst;
  SDL_UpdateTexture(screenTex, nullptr, fb, w * 4);
  if (curScale == Scale::Pixel || !upTex) {
    int k = std::max(1, std::min(DISPLAY_W / w, DISPLAY_H / h));
    if (curScale == Scale::Fit) {  // no render targets: plain nearest fit
      float f = std::min(float(DISPLAY_W) / w, float(DISPLAY_H) / h);
      dst = {int(DISPLAY_W - w * f) / 2, int(DISPLAY_H - h * f) / 2, int(w * f), int(h * f)};
    } else {
      dst = {(DISPLAY_W - w * k) / 2, (DISPLAY_H - h * k) / 2, w * k, h * k};
    }
    SDL_RenderCopy(renderer, screenTex, nullptr, &dst);
  } else {
    SDL_SetRenderTarget(renderer, upTex);
    SDL_RenderCopy(renderer, screenTex, nullptr, nullptr);
    SDL_SetRenderTarget(renderer, nullptr);
    float f = std::min(float(DISPLAY_W) / w, float(DISPLAY_H) / h);
    int dw = int(std::lround(w * f)), dh = int(std::lround(h * f));
    dst = {(DISPLAY_W - dw) / 2, (DISPLAY_H - dh) / 2, dw, dh};
    SDL_RenderCopy(renderer, upTex, nullptr, &dst);
  }
  SDL_RenderPresent(renderer);
}

std::vector<uint8_t> readResource(const std::string& name) {
  std::string rel = (!name.empty() && name[0] == '/') ? name.substr(1) : name;
#ifdef __vita__
  std::string path = RES_ROOT + rel;
#else
  std::string path = resRoot + rel;
#endif
  std::vector<uint8_t> data;
  SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb");
  if (!rw) return data;
  Sint64 size = SDL_RWsize(rw);
  if (size > 0) {
    data.resize(size_t(size));
    if (SDL_RWread(rw, data.data(), 1, data.size()) != data.size()) data.clear();
  }
  SDL_RWclose(rw);
  return data;
}

std::string saveDir() {
#ifdef __vita__
  return SAVE_ROOT;
#else
  return saveRoot;
#endif
}

// ------------------------------------------------------------------------------------------
// Audio

static void loadClipLengths() {
  std::vector<uint8_t> data = readResource("snd/lengths.txt");
  std::string text(data.begin(), data.end());
  size_t pos = 0;
  while (pos < text.size()) {
    size_t eol = text.find('\n', pos);
    if (eol == std::string::npos) eol = text.size();
    unsigned id;
    int ms;
    if (std::sscanf(text.c_str() + pos, "%x %d", &id, &ms) == 2) clipMs[id] = ms;
    pos = eol + 1;
  }
  if (clipMs.empty()) {
    std::fprintf(stderr, "audio: no snd/lengths.txt; clips play to their end\n");
    clipMs[0] = 0;  // don't look again
  }
}

static Mix_Chunk* clip(uint32_t id) {
  if (clipMs.empty()) loadClipLengths();
  auto it = clips.find(id);
  if (it != clips.end()) return it->second;
  char name[32];
  std::snprintf(name, sizeof name, "snd/%08x.ogg", id);
  Mix_Chunk* c = nullptr;
  std::vector<uint8_t> data = readResource(name);
  if (!data.empty()) c = Mix_LoadWAV_RW(SDL_RWFromConstMem(data.data(), int(data.size())), 1);
  if (!c) std::fprintf(stderr, "audio: missing %s\n", name);
  else if (std::getenv("DR_TRACE")) std::fprintf(stderr, "audio: loaded %s\n", name);
  clips[id] = c;  // cache misses too
  return c;
}

int audioPlay(uint32_t clipId, int volume) {
  if (!audioOk) return -1;
  Mix_Chunk* c = clip(clipId);
  if (!c) return -1;
  int ch = Mix_PlayChannel(-1, c, 0);
  if (std::getenv("DR_TRACE")) std::fprintf(stderr, "audio: play %08x on %d\n", clipId, ch);
  if (ch >= 0) {
    Mix_Volume(ch, volume * MIX_MAX_VOLUME / 100);
    if (ch < MAX_VOICES) {
      auto len = clipMs.find(clipId);
      voices[ch] = {clipId, SDL_GetTicks64(), len == clipMs.end() ? -1 : len->second, false};
    }
  }
  return ch;
}

bool audioPlaying(int voice, uint32_t clipId) {
  if (!audioOk || voice < 0) return false;
  auto it = clips.find(clipId);
  if (it == clips.end() || !Mix_Playing(voice) || Mix_GetChunk(voice) != it->second) return false;
  // as on the phone: the clip has ended once its MIDI has, even while its synth tail fades
  if (voice < MAX_VOICES) {
    const Voice& v = voices[voice];
    if (v.clip == clipId && v.ms >= 0 && SDL_GetTicks64() - v.start >= uint64_t(v.ms)) return false;
  }
  return true;
}

void audioStop(int voice) {
  if (audioOk && voice >= 0) Mix_HaltChannel(voice);
}

void audioSetVolume(int voice, int volume) {
  if (audioOk && voice >= 0) Mix_Volume(voice, volume * MIX_MAX_VOLUME / 100);
}

uint64_t millis() { return SDL_GetTicks64(); }
void sleepMs(int ms) { SDL_Delay(ms); }

void startThread(const char* name, void (*fn)(void*), void* arg) {
  struct Start { void (*fn)(void*); void* arg; };
  auto* s = new Start{fn, arg};
  // Generous stack: the translated game has very large methods (hoisted locals).
  SDL_Thread* t = SDL_CreateThreadWithStackSize(
      [](void* p) -> int {
        auto* st = static_cast<Start*>(p);
        st->fn(st->arg);
        delete st;
        return 0;
      },
      name, 2 * 1024 * 1024, s);
  if (!t) {
    std::fprintf(stderr, "SDL_CreateThread: %s\n", SDL_GetError());
    delete s;
    return;
  }
  SDL_DetachThread(t);
}

Mutex::Mutex() : m_(SDL_CreateMutex()) {}
Mutex::~Mutex() { SDL_DestroyMutex(static_cast<SDL_mutex*>(m_)); }
void Mutex::lock() { SDL_LockMutex(static_cast<SDL_mutex*>(m_)); }
void Mutex::unlock() { SDL_UnlockMutex(static_cast<SDL_mutex*>(m_)); }

}  // namespace plat
