// Diamond Rush for PS Vita: boots the translated MIDlet (port/src/game/gen) on the MIDP host.
#include "game/gen/game.h"

#ifdef __vita__
extern "C" {
// The MIDlet constructor runs on the main thread; give it the same headroom as Java threads.
unsigned int sceUserMainThreadStackSize = 2 * 1024 * 1024;
int _newlib_heap_size_user = 128 * 1024 * 1024;
}
#endif

int main(int, char**) {
  // From the jar's META-INF/MANIFEST.MF
  j::midp::setAppProperty("MIDlet-Name", "Diamond Rush");
  j::midp::setAppProperty("MIDlet-Vendor", "Gameloft SA");
  j::midp::setAppProperty("MIDlet-Version", "1.0.4");

  return j::midp::run([] {
    j::game_clinit();
    return j::game_createMidlet();
  });
}
