// javax.microedition.media: MIDI players backed by pre-rendered OGG clips.
// The MIDI bytes handed to Manager.createPlayer() are hashed (FNV-1a 32); the clip is
// res/snd/<hash>.ogg, produced at build time by tools/extract_sounds.py from the same bytes.
#include "midp/midp.h"
#include "platform/platform.h"

namespace j {

Ref<Player> Manager::createPlayer(Ref<InputStream> in, Ref<String>) {
  uint32_t h = 0x811C9DC5u;
  for (jint c; (c = in->read()) >= 0;) h = (h ^ uint32_t(c)) * 0x01000193u;
  return make<Player>(h);
}

Player::Player(uint32_t hash) : clipHash(hash) {}
Player::~Player() { plat::audioStop(voice); }

void Player::realize() {
  if (state == UNREALIZED) state = REALIZED;
}

void Player::start() {
  if (state == CLOSED) return;
  plat::audioStop(voice);
  voice = plat::audioPlay(clipHash, volume);
  state = STARTED;
}

void Player::stop() {
  if (state != STARTED) return;
  plat::audioStop(voice);
  voice = -1;
  state = PREFETCHED;
}

void Player::deallocate() {
  stop();
  if (state != CLOSED) state = REALIZED;
}

void Player::close() {
  plat::audioStop(voice);
  voice = -1;
  state = CLOSED;
}

jint Player::getState() {
  // A clip that finished playing returns to PREFETCHED, as on the phone.
  if (state == STARTED && !plat::audioPlaying(voice, clipHash)) {
    voice = -1;
    state = PREFETCHED;
  }
  return state;
}

Ref<Control> Player::getControl(Ref<String> type) {
  if (type && type->utf8().find("VolumeControl") != std::string::npos) return make<VolumeControl>(this);
  return nullptr;
}

jint VolumeControl::setLevel(jint level) {
  level = level < 0 ? 0 : level > 100 ? 100 : level;
  player->volume = level;
  plat::audioSetVolume(player->voice, level);
  return level;
}

}  // namespace j
