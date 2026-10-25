// MIDlet host: Canvas/Display/MIDlet glue, the main (platform) loop, key delivery, frame handoff.
//
// Threads: the main thread owns SDL (events, presenting). The game's own Thread runs the
// MIDlet logic and painting, holding the jrt VM lock except while it sleeps/yields. Key events
// are queued by the main thread and delivered on the game thread at those yield points, so the
// game sees them exactly where a phone VM's event thread could interleave.
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <string>

#include "midp/midp.h"
#include "platform/platform.h"

namespace j {

namespace midp {
jint screenW = plat::PHONE_W, screenH = plat::PHONE_H;

namespace {
std::map<std::string, std::string> appProps;
Ref<Display> display;
std::vector<uint32_t> backBuf;  // game thread paints here

plat::Mutex* frameMx;  // guards frontBuf/frontW/frameReady
std::vector<uint32_t> frontBuf;
int frontW = plat::PHONE_W;
bool frameReady = false;

// Port settings (saveDir()/settings.cfg): widescreen on/off, display scaling.
constexpr int WIDE_W = 282;  // 160 * 960/544, rounded to even: fills the Vita's 16:9 screen
bool widescreen = true;

void loadSettings() {
  std::string path = plat::saveDir() + "settings.cfg";
  if (FILE* f = std::fopen(path.c_str(), "r")) {
    char line[128];
    while (std::fgets(line, sizeof line, f)) {
      int v;
      if (std::sscanf(line, "widescreen=%d", &v) == 1) widescreen = v != 0;
      if (std::sscanf(line, "scale=%d", &v) == 1 && v >= 0 && v <= 1) plat::setScale(plat::Scale(v));
    }
    std::fclose(f);
  }
}

void saveSettings() {
  std::string path = plat::saveDir() + "settings.cfg";
  if (FILE* f = std::fopen(path.c_str(), "w")) {
    std::fprintf(f, "widescreen=%d\nscale=%d\n", widescreen ? 1 : 0, int(plat::scale()));
    std::fclose(f);
  }
}

// Called with the VM lock held: the game picks the new width up at its next paint().
void applyScreenWidth() {
  screenW = widescreen ? WIDE_W : plat::PHONE_W;
  backBuf.assign(size_t(screenW) * screenH, 0xFF000000u);
}

plat::Mutex* keyMx;  // guards keyQueue
std::deque<plat::KeyEvent> keyQueue;
std::atomic<bool> quitRequested{false};

// Game-thread key state. The game ORs keys into a bitmask on press and clears it on release,
// polling once per 62 ms tick, so a quick tap could be released before any tick sees it.
// Releases are therefore held back until the key has been down for at least MIN_HOLD_MS.
constexpr uint64_t MIN_HOLD_MS = 90;
std::map<int, uint64_t> downSince;
std::map<int, uint64_t> pendingRelease;  // key -> earliest delivery time

Ref<Canvas> currentCanvas() {
  if (!display || !display->current) return nullptr;
  auto* c = dynamic_cast<Canvas*>(display->current.get());
  return c ? Ref<Canvas>(c) : nullptr;
}

void safely(const char* what, void (*fn)(Canvas*, int), Canvas* c, int arg) {
  try {
    fn(c, arg);
  } catch (Thrown& t) {
    std::fprintf(stderr, "exception in %s: ", what);
    t.ex->printStackTrace();
  }
}

void deliverKeys() {
  std::deque<plat::KeyEvent> evs;
  {
    plat::LockGuard g(*keyMx);
    evs.swap(keyQueue);
  }
  Ref<Canvas> c = currentCanvas();
  if (!c) return;
  uint64_t now = plat::millis();
  auto release = [&](int code) { safely("keyReleased", [](Canvas* cv, int k) { cv->keyReleased(k); }, c.get(), code); };
  auto press = [&](int code) { safely("keyPressed", [](Canvas* cv, int k) { cv->keyPressed(k); }, c.get(), code); };

  for (auto it = pendingRelease.begin(); it != pendingRelease.end();) {
    if (now >= it->second) {
      release(it->first);
      it = pendingRelease.erase(it);
    } else {
      ++it;
    }
  }
  for (const plat::KeyEvent& e : evs) {
    if (e.pressed) {
      if (pendingRelease.erase(e.code)) release(e.code);  // finish the previous tap first
      downSince[e.code] = now;
      press(e.code);
    } else {
      uint64_t since = downSince.count(e.code) ? downSince[e.code] : 0;
      if (now - since < MIN_HOLD_MS) pendingRelease[e.code] = since + MIN_HOLD_MS;
      else release(e.code);
    }
  }
}

void publishFrame() {
  plat::LockGuard g(*frameMx);
  frontBuf = backBuf;
  frontW = screenW;
  frameReady = true;
}

// Scripted test driver (desktop): DR_SCRIPT="W7 N5 W2 S DOWN*2 FIRE S Q", DR_SHOTS=<dir>.
// Same vocabulary as tools/j2me-probe.ps1, so the port and the original can be compared:
//   W<sec> wait, S screenshot (<dir>/NN.ppm), Q quit, otherwise a key held 150 ms (*n repeats).
struct Script {
  std::vector<std::string> steps;
  size_t pos = 0;
  uint64_t waitUntil = 0;
  int shot = 0;
  std::string shotDir;

  static int keyCode(const std::string& k) {
    static const std::map<std::string, int> names = {
        {"UP", plat::KEY_UP}, {"DOWN", plat::KEY_DOWN}, {"LEFT", plat::KEY_LEFT},
        {"RIGHT", plat::KEY_RIGHT}, {"FIRE", plat::KEY_FIRE}, {"LS", plat::KEY_SOFT_LEFT},
        {"RS", plat::KEY_SOFT_RIGHT}, {"STAR", plat::KEY_STAR}, {"POUND", plat::KEY_POUND}};
    auto it = names.find(k);
    if (it != names.end()) return it->second;
    if (k.size() == 2 && k[0] == 'N' && k[1] >= '0' && k[1] <= '9') return k[1];
    return 0;
  }

  void load() {
    const char* s = std::getenv("DR_SCRIPT");
    const char* d = std::getenv("DR_SHOTS");
    if (!s) return;
    shotDir = d ? d : ".";
    std::string cur;
    for (const char* p = s;; ++p) {
      if (*p == ' ' || *p == ',' || *p == 0) {
        if (!cur.empty()) steps.push_back(cur);
        cur.clear();
        if (!*p) break;
      } else {
        cur += *p;
      }
    }
  }

  void saveShot(const std::vector<uint32_t>& px, int w, int h) {
    char path[512];
    std::snprintf(path, sizeof path, "%s/%02d.ppm", shotDir.c_str(), shot++);
    FILE* f = std::fopen(path, "wb");
    if (!f) return;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (uint32_t p : px) {
      unsigned char rgb[3] = {uint8_t(p >> 16), uint8_t(p >> 8), uint8_t(p)};
      std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
    std::printf("saved %s\n", path);
  }

  // Returns false when the script asks to quit.
  bool step(std::vector<plat::KeyEvent>& out, const std::vector<uint32_t>& shown, int w, int h) {
    uint64_t now = plat::millis();
    if (now < waitUntil) return true;
    if (pos >= steps.size()) return true;
    const std::string& s = steps[pos++];
    if (s == "Q") return false;
    if (s == "S") { saveShot(shown, w, h); return true; }
    if (s[0] == 'W' && s.size() > 1 && (std::isdigit(uint8_t(s[1])) || s[1] == '.')) {
      waitUntil = now + uint64_t(std::atof(s.c_str() + 1) * 1000);
      return true;
    }
    std::string name = s;
    int reps = 1;
    size_t star = s.find('*');
    if (star != std::string::npos) { name = s.substr(0, star); reps = std::atoi(s.c_str() + star + 1); }
    int code = keyCode(name);
    if (!code) { std::fprintf(stderr, "script: unknown step %s\n", s.c_str()); return true; }
    // Queue reps x (press, hold, release, gap) as timed events.
    timeline.clear();
    for (int i = 0; i < reps; ++i) {
      timeline.push_back({now + uint64_t(i) * 300, code, true});
      timeline.push_back({now + uint64_t(i) * 300 + 150, code, false});
    }
    waitUntil = now + uint64_t(reps) * 300;
    return true;
  }

  struct Timed { uint64_t at; int code; bool pressed; };
  std::vector<Timed> timeline;
  void flushTimeline(std::vector<plat::KeyEvent>& out) {
    uint64_t now = plat::millis();
    for (auto it = timeline.begin(); it != timeline.end();) {
      if (now >= it->at) { out.push_back({it->code, it->pressed}); it = timeline.erase(it); }
      else ++it;
    }
  }
} script;
}  // namespace

void setAppProperty(const char* key, const char* value) { appProps[key] = value; }

int run(Ref<MIDlet> (*createMidlet)()) {
  if (!plat::init()) return 1;
  frameMx = new plat::Mutex();
  keyMx = new plat::Mutex();
  rt::init();
  rt::onYield = deliverKeys;
  loadSettings();
  if (const char* w = std::getenv("DR_WIDESCREEN")) widescreen = std::atoi(w) != 0;  // tests
  applyScreenWidth();

  rt::lock();
  Ref<MIDlet> midlet;
  try {
    midlet = createMidlet();
    midlet->startApp();
  } catch (Thrown& t) {
    std::fprintf(stderr, "exception starting MIDlet: ");
    t.ex->printStackTrace();
  }
  rt::unlock();

  std::vector<plat::KeyEvent> events;
  std::vector<uint32_t> shown(backBuf.size(), 0xFF000000u);
  int shownW = screenW, shownH = screenH;
  script.load();
  for (;;) {
    events.clear();
    bool alive = plat::poll(events);
    script.flushTimeline(events);
    if (!script.step(events, shown, shownW, shownH)) alive = false;
    {
      plat::LockGuard g(*keyMx);
      for (const plat::KeyEvent& e : events)
        if (e.code < plat::SYS_QUIT) keyQueue.push_back(e);
    }
    if (!alive || quitRequested) break;

    {
      plat::LockGuard g(*frameMx);
      if (frameReady) {
        shown.swap(frontBuf);
        shownW = frontW;
        shownH = screenH;
        frameReady = false;
      }
    }
    plat::present(shown.data(), shownW, shownH);  // vsync paces this loop
  }
  plat::shutdown();
  // The game thread may still be inside the VM; exit without unwinding it.
  std::_Exit(0);
}

}  // namespace midp

// ---------------------------------------------------------------------------------------------
// Canvas / Display / MIDlet

Canvas::Canvas() = default;

void Canvas::repaint() { repaintPending_ = true; }

void Canvas::serviceRepaints() {
  if (!repaintPending_) return;
  repaintPending_ = false;
  if (midp::display == nullptr || midp::display->current.get() != static_cast<Displayable*>(this)) return;
  auto g = make<Graphics>(midp::backBuf.data(), midp::screenW, midp::screenH);
  try {
    paint(g);
  } catch (Thrown& t) {
    std::fprintf(stderr, "exception in paint: ");
    t.ex->printStackTrace();
  }
  midp::publishFrame();
}

jint Canvas::getWidth() { return midp::screenW; }
jint Canvas::getHeight() { return midp::screenH; }

Ref<Display> Display::getDisplay(Ref<MIDlet>) {
  if (!midp::display) midp::display = make<Display>();
  return midp::display;
}

void Display::setCurrent(Ref<Displayable> d) {
  if (current == d) return;
  Ref<Displayable> old = current;
  current = d;
  if (old) old->hideNotify();
  if (d) d->showNotify();
}

jboolean Display::vibrate(jint) { return true; }

Ref<String> MIDlet::getAppProperty(Ref<String> key) {
  std::string k = key->utf8();
  if (k == "Port-Widescreen") return String::fromUtf8(midp::widescreen ? "1" : "0");
  if (k == "Port-Scale") return String::fromUtf8(plat::scale() == plat::Scale::Pixel ? "pixel" : "fit");
  // Test hook: DR_UNLOCK=1 opens every stage (the game's own cheat code; see class_9).
  if (k == "Port-Debug-Unlock") return String::fromUtf8(std::getenv("DR_UNLOCK") ? "1" : "0");
  auto it = midp::appProps.find(k);
  return it == midp::appProps.end() ? nullptr : String::fromUtf8(it->second);
}

void MIDlet::notifyDestroyed() { midp::quitRequested = true; }

jboolean MIDlet::platformRequest(Ref<String> url) {
  // Called on the game thread (VM lock held) from the game's Options: DISPLAY and SCALE.
  std::string u = url ? url->utf8() : std::string();
  if (u == "port:toggle-widescreen") {
    midp::widescreen = !midp::widescreen;
    midp::applyScreenWidth();
    midp::saveSettings();
  } else if (u == "port:toggle-scale") {
    plat::setScale(plat::scale() == plat::Scale::Pixel ? plat::Scale::Fit : plat::Scale::Pixel);
    midp::saveSettings();
  }
  return false;
}

}  // namespace j
