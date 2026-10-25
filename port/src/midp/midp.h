// midp: the MIDP 2.0 subset the game uses (lcdui, media, midlet, rms), on top of jrt + platform.
// Rendering is a software rasteriser into 0xAARRGGBB buffers with exact MIDP clip/anchor rules.
#pragma once

#include "jrt/jrt.h"

namespace j {

struct Graphics;

// ---------------------------------------------------------------------------------------------
// lcdui

struct Image final : virtual Object {
  jint width, height;
  bool mutable_;
  std::unique_ptr<uint32_t[]> px;  // 0xAARRGGBB

  Image(jint w, jint h, bool isMutable);
  static Ref<Image> createImage(jint w, jint h);
  static Ref<Image> createImage(Ref<String> resName);
  static Ref<Image> createRGBImage(jarr<jint> rgb, jint w, jint h, jboolean processAlpha);
  Ref<Graphics> getGraphics();
  void getRGB(jarr<jint> rgb, jint offset, jint scanlength, jint x, jint y, jint w, jint h);
  jint getWidth() { return width; }
  jint getHeight() { return height; }
};

struct Graphics final : virtual Object {
  enum : jint {
    HCENTER = 1, VCENTER = 2, LEFT = 4, RIGHT = 8, TOP = 16, BOTTOM = 32, BASELINE = 64,
  };
  enum : jint {  // javax.microedition.lcdui.game.Sprite transforms
    TRANS_NONE = 0, TRANS_MIRROR_ROT180 = 1, TRANS_MIRROR = 2, TRANS_ROT180 = 3,
    TRANS_MIRROR_ROT270 = 4, TRANS_ROT90 = 5, TRANS_ROT270 = 6, TRANS_MIRROR_ROT90 = 7,
  };

  Graphics(uint32_t* target, jint w, jint h);

  void translate(jint x, jint y) { tx += x; ty += y; }
  jint getTranslateX() { return tx; }
  jint getTranslateY() { return ty; }

  void setColor(jint rgb) { color = uint32_t(rgb) | 0xFF000000u; }
  void setColor(jint r, jint g, jint b);
  jint getColor() { return jint(color & 0xFFFFFF); }

  void setClip(jint x, jint y, jint w, jint h);
  void clipRect(jint x, jint y, jint w, jint h);
  jint getClipX() { return rx0 - tx; }
  jint getClipY() { return ry0 - ty; }
  jint getClipWidth() { return rx1 - rx0; }
  jint getClipHeight() { return ry1 - ry0; }

  void fillRect(jint x, jint y, jint w, jint h);
  void drawRect(jint x, jint y, jint w, jint h);
  void drawLine(jint x1, jint y1, jint x2, jint y2);
  void drawRoundRect(jint x, jint y, jint w, jint h, jint arcW, jint arcH);
  void fillRoundRect(jint x, jint y, jint w, jint h, jint arcW, jint arcH);
  void drawImage(Ref<Image> img, jint x, jint y, jint anchor);
  void drawRegion(Ref<Image> src, jint xSrc, jint ySrc, jint w, jint h, jint transform,
                  jint xDst, jint yDst, jint anchor);
  void drawRGB(jarr<jint> rgb, jint offset, jint scanlength, jint x, jint y, jint w, jint h,
               jboolean processAlpha);

  // target surface
  uint32_t* buf;
  jint bw, bh;
  Ref<Image> owner;  // keeps a mutable image alive while its Graphics is in use

 private:
  void clip(jint& x0, jint& y0, jint& x1, jint& y1) const;  // effective clip (∩ surface)
  void hspan(jint x0, jint x1, jint y);  // absolute coords, clipped, [x0, x1)
  void plot(jint x, jint y);
  void blit(const uint32_t* src, jint srcStride, jint sx, jint sy, jint w, jint h, jint transform,
            jint dx, jint dy, bool alpha);

  jint tx = 0, ty = 0;
  jint rx0, ry0, rx1, ry1;  // requested clip in absolute coords, [rx0, rx1) x [ry0, ry1)
  uint32_t color = 0xFF000000u;
};

struct Displayable : virtual Object {
  virtual void showNotify() {}
  virtual void hideNotify() {}
};

struct Canvas : Displayable {
  Canvas();
  void repaint();
  void serviceRepaints();
  void setFullScreenMode(jboolean) {}
  jint getWidth();
  jint getHeight();

  virtual void paint(Ref<Graphics> g) = 0;
  virtual void keyPressed(jint) {}
  virtual void keyReleased(jint) {}
  virtual void keyRepeated(jint) {}

 private:
  bool repaintPending_ = false;
};

struct MIDlet;

struct Display final : virtual Object {
  static Ref<Display> getDisplay(Ref<MIDlet> m);
  void setCurrent(Ref<Displayable> d);
  Ref<Displayable> getCurrent() { return current; }
  jboolean vibrate(jint ms);

  Ref<Displayable> current;
};

// ---------------------------------------------------------------------------------------------
// midlet

struct MIDletStateChangeException : Exception {
  const char* name() const override { return "javax.microedition.midlet.MIDletStateChangeException"; }
};

struct MIDlet : virtual Object {
  virtual void startApp() = 0;
  virtual void pauseApp() = 0;
  virtual void destroyApp(jboolean unconditional) = 0;

  Ref<String> getAppProperty(Ref<String> key);
  void notifyDestroyed();
  // Port hook: "port:toggle-widescreen" switches the display mode (see runner.cpp).
  jboolean platformRequest(Ref<String> url);
};

// ---------------------------------------------------------------------------------------------
// media

struct Control : virtual Object {};

struct VolumeControl final : Control {
  struct Player* player;
  explicit VolumeControl(struct Player* p) : player(p) {}
  jint setLevel(jint level);
};

struct Controllable : virtual Object {
  virtual Ref<Control> getControl(Ref<String> type) = 0;
};

struct Player final : Controllable {
  enum : jint { CLOSED = 0, UNREALIZED = 100, REALIZED = 200, PREFETCHED = 300, STARTED = 400 };

  explicit Player(uint32_t clipHash);
  ~Player() override;
  void realize();
  void prefetch() { realize(); }
  void start();
  void stop();
  void deallocate();
  void close();
  jint getState();
  Ref<Control> getControl(Ref<String> type) override;

  uint32_t clipHash;  // FNV-1a of the MIDI bytes; selects the pre-rendered clip
  jint state = UNREALIZED;
  jint volume = 100;
  int voice = -1;     // platform audio handle while playing
};

struct Manager {
  static Ref<Player> createPlayer(Ref<InputStream> in, Ref<String> type);
};

// ---------------------------------------------------------------------------------------------
// rms

struct RecordStoreException : Exception {
  using Exception::Exception;
  const char* name() const override { return "javax.microedition.rms.RecordStoreException"; }
};

struct RecordStore final : virtual Object {
  static Ref<RecordStore> openRecordStore(Ref<String> name, jboolean createIfNecessary);
  void closeRecordStore() {}
  jint getNumRecords() { return jint(records.size()); }
  jarr<jbyte> getRecord(jint id);
  jint addRecord(jarr<jbyte> data, jint off, jint len);
  void setRecord(jint id, jarr<jbyte> data, jint off, jint len);

  std::string path;
  std::vector<std::vector<int8_t>> records;  // record id = index + 1

 private:
  void save();
};

// ---------------------------------------------------------------------------------------------
// Runner (port/src/midp/runner.cpp): boots a MIDlet and pumps the platform.

namespace midp {
// Virtual screen size. Height is the phone's 160; width is 128 or wider (widescreen).
extern jint screenW, screenH;
// JAD/manifest attributes returned by MIDlet.getAppProperty().
void setAppProperty(const char* key, const char* value);
// Boots the MIDlet (createMidlet runs with the VM lock held) and runs until it exits.
int run(Ref<MIDlet> (*createMidlet)());
}  // namespace midp

}  // namespace j
