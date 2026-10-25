#include "jrt/jrt.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>

#include "platform/platform.h"

namespace j {

// ---------------------------------------------------------------------------------------------
// Object

jint Object::hashCode() { return jint(reinterpret_cast<uintptr_t>(this) >> 3); }
jboolean Object::equals(Ref<Object> other) { return other.get() == this; }
Ref<String> Object::toString() { return concat(JSTR(u"Object@"), hashCode()); }

Ref<Class> Object::getClass() {
  static Ref<Class> cls = make<Class>();  // the game only uses getClass().getResourceAsStream()
  return cls;
}

template <class E>
[[noreturn]] static void throwNew(const char16_t* msg = nullptr) {
  Ref<E> e = make<E>();
  if (msg) e->message = String::lit(msg);
  throw Thrown{Ref<Throwable>(e)};
}

void throwNPE() { throwNew<NullPointerException>(); }
void throwAIOOBE(jint index, jint) {
  Ref<ArrayIndexOutOfBoundsException> e = make<ArrayIndexOutOfBoundsException>(String::valueOf(index));
  throw Thrown{Ref<Throwable>(e)};
}
void throwClassCast(const char* from, const char* to) {
  if (from && to && std::getenv("DR_TRACE")) std::fprintf(stderr, "ClassCastException: %s -> %s\n", from, to);
  throwNew<ClassCastException>();
}
void throwArithmetic() { throwNew<ArithmeticException>(u"/ by zero"); }
void throwNegativeArraySize() { throwNew<NegativeArraySizeException>(); }

// ---------------------------------------------------------------------------------------------
// String

Ref<String> String::lit(const char16_t* text) { return make<String>(std::u16string(text)); }

Ref<String> String::fromUtf8(const std::string& u8) {
  std::u16string out;
  for (size_t i = 0; i < u8.size();) {
    uint8_t c = uint8_t(u8[i]);
    uint32_t cp;
    int n;
    if (c < 0x80) { cp = c; n = 1; }
    else if ((c >> 5) == 6) { cp = c & 0x1F; n = 2; }
    else if ((c >> 4) == 14) { cp = c & 0x0F; n = 3; }
    else { cp = c & 0x07; n = 4; }
    for (int k = 1; k < n && i + k < u8.size(); ++k) cp = (cp << 6) | (uint8_t(u8[i + k]) & 0x3F);
    i += n;
    if (cp >= 0x10000) {
      cp -= 0x10000;
      out += char16_t(0xD800 + (cp >> 10));
      out += char16_t(0xDC00 + (cp & 0x3FF));
    } else {
      out += char16_t(cp);
    }
  }
  return make<String>(std::move(out));
}

std::string String::utf8() const {
  std::string out;
  for (char16_t c : s) {
    if (c < 0x80) out += char(c);
    else if (c < 0x800) { out += char(0xC0 | (c >> 6)); out += char(0x80 | (c & 0x3F)); }
    else { out += char(0xE0 | (c >> 12)); out += char(0x80 | ((c >> 6) & 0x3F)); out += char(0x80 | (c & 0x3F)); }
  }
  return out;
}

String::String(jarr<jbyte> bytes, jint off, jint len, Ref<String>) {
  // The game only decodes ISO-8859-1: one byte per char.
  if (off < 0 || len < 0 || off > bytes->length - len) throwAIOOBE(off + len, bytes->length);
  s.resize(size_t(len));
  for (jint i = 0; i < len; ++i) s[size_t(i)] = char16_t(uint8_t(bytes->raw()[off + i]));
}

jchar String::charAt(jint i) {
  if (uint32_t(i) >= s.size()) throwAIOOBE(i, jint(s.size()));
  return s[size_t(i)];
}

jint String::compareTo(Ref<String> o) {
  const std::u16string& b = o->s;
  size_t n = std::min(s.size(), b.size());
  for (size_t i = 0; i < n; ++i)
    if (s[i] != b[i]) return jint(s[i]) - jint(b[i]);
  return jint(s.size()) - jint(b.size());
}

jboolean String::endsWith(Ref<String> o) {
  const std::u16string& b = o->s;
  return b.size() <= s.size() && s.compare(s.size() - b.size(), b.size(), b) == 0;
}

jarr<jbyte> String::getBytes() {  // platform default encoding: ISO-8859-1 on the phone
  auto a = newArray<jbyte>(jint(s.size()));
  for (size_t i = 0; i < s.size(); ++i) a->raw()[i] = jbyte(s[i] < 256 ? s[i] : '?');
  return a;
}

jint String::indexOf(jint ch, jint from) {
  for (size_t i = size_t(std::max(from, 0)); i < s.size(); ++i)
    if (jint(s[i]) == ch) return jint(i);
  return -1;
}

jint String::indexOf(Ref<String> str, jint from) {
  size_t i = s.find(str->s, size_t(std::max(from, 0)));
  return i == std::u16string::npos ? -1 : jint(i);
}

Ref<String> String::substring(jint begin, jint end) {
  if (begin < 0 || end > jint(s.size()) || begin > end) throwAIOOBE(begin, jint(s.size()));
  return make<String>(s.substr(size_t(begin), size_t(end - begin)));
}

Ref<String> String::toLowerCase() {
  std::u16string r = s;
  for (char16_t& c : r) {
    if ((c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7)) c = char16_t(c + 32);
  }
  return make<String>(std::move(r));
}

Ref<String> String::valueOf(jint v) {
  std::u16string out;
  appendTo(out, v);
  return make<String>(std::move(out));
}

jint String::hashCode() {
  jint h = 0;
  for (char16_t c : s) h = 31 * h + jint(c);
  return h;
}

jboolean String::equals(Ref<Object> other) {
  auto* o = dynamic_cast<String*>(other.get());
  return o && o->s == s;
}

void appendTo(std::u16string& out, jint v) { appendTo(out, jlong(v)); }
void appendTo(std::u16string& out, jlong v) {
  char buf[24];
  std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(v));
  for (char* p = buf; *p; ++p) out += char16_t(*p);
}

// ---------------------------------------------------------------------------------------------
// StringBuffer, Integer, Throwable, PrintStream

jchar StringBuffer::charAt(jint i) {
  if (uint32_t(i) >= s.size()) throwAIOOBE(i, jint(s.size()));
  return s[size_t(i)];
}

Ref<StringBuffer> StringBuffer::deleteRange(jint start, jint end) {
  jint n = jint(s.size());
  if (end > n) end = n;
  if (start < 0 || start > end) throwAIOOBE(start, n);
  s.erase(size_t(start), size_t(end - start));
  return Ref<StringBuffer>(this);
}

void StringBuffer::setCharAt(jint i, jchar c) {
  if (uint32_t(i) >= s.size()) throwAIOOBE(i, jint(s.size()));
  s[size_t(i)] = c;
}

jboolean Integer::equals(Ref<Object> o) {
  auto* i = dynamic_cast<Integer*>(o.get());
  return i && i->value == value;
}

void Throwable::printStackTrace() {
  std::fprintf(stderr, "%s%s%s\n", name(), message ? ": " : "", message ? message->utf8().c_str() : "");
}

void PrintStream::println(Ref<String> s) {
  std::printf("%s\n", s ? s->utf8().c_str() : "null");
  std::fflush(stdout);  // the host exits with _Exit, which would drop buffered output
}

// ---------------------------------------------------------------------------------------------
// System, Thread, global lock

namespace {
plat::Mutex* vmLock;  // created in rt::init (after SDL is up)
const auto startSteady = std::chrono::steady_clock::now();
const int64_t startEpochMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::system_clock::now().time_since_epoch()).count();
}  // namespace

namespace rt {
void (*onYield)() = nullptr;
static bool tracing = false;
void init() {
  vmLock = new plat::Mutex();
  System::out = make<PrintStream>();
  tracing = std::getenv("DR_TRACE") != nullptr;
}
void traceCatch(const Thrown& t, const char* where) {
  if (!tracing) return;
  Throwable* e = t.ex.get();
  std::fprintf(stderr, "[catch %s] %s%s%s\n", where, e->name(), e->message ? ": " : "",
               e->message ? e->message->utf8().c_str() : "");
}
void lock() { vmLock->lock(); }
void unlock() { vmLock->unlock(); }
}  // namespace rt

jlong System::currentTimeMillis() {
  // Monotonic, but anchored to the wall clock so Random(currentTimeMillis()) still varies per run.
  auto d = std::chrono::steady_clock::now() - startSteady;
  return startEpochMs + std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
}

void Thread::start() {
  plat::startThread("java", [](void* p) {
    Ref<Runnable> r(static_cast<Runnable*>(p), AdoptTag{});  // takes the reference retained below
    rt::lock();
    try {
      r->run();
    } catch (Thrown& t) {
      std::fprintf(stderr, "Uncaught exception in thread: ");
      t.ex->printStackTrace();
    }
    r = nullptr;
    rt::unlock();
  }, (retain(target.get()), target.get()));
}

void Thread::sleep(jlong ms) {
  if (rt::onYield) rt::onYield();
  rt::unlock();
  plat::sleepMs(int(ms > 0 ? (ms < 0x7FFFFFFF ? ms : 0x7FFFFFFF) : 0));
  rt::lock();
}

void Thread::yield() {
  // A real sleep instead of sched_yield: the game's sound thread spins on yield(), and a
  // 1 ms nap keeps it from burning a Vita core (and from starving the game thread).
  sleep(1);
}

// ---------------------------------------------------------------------------------------------
// java.io

Ref<InputStream> Class::getResourceAsStream(Ref<String> name) {
  std::vector<uint8_t> data = plat::readResource(name->utf8());
  if (data.empty()) {
    std::fprintf(stderr, "resource not found: %s\n", name->utf8().c_str());
    return nullptr;
  }
  auto bytes = newArray<jbyte>(jint(data.size()));
  std::memcpy(bytes->raw(), data.data(), data.size());
  return make<ByteArrayInputStream>(bytes);
}

jint InputStream::read(jarr<jbyte> b, jint off, jint len) {
  if (off < 0 || len < 0 || off > b->length - len) throwAIOOBE(off + len, b->length);
  jint n = 0;
  for (; n < len; ++n) {
    jint c = read();
    if (c < 0) return n == 0 ? -1 : n;
    b->raw()[off + n] = jbyte(c);
  }
  return n;
}

jlong InputStream::skip(jlong n) {
  jlong k = 0;
  while (k < n && read() >= 0) ++k;
  return k;
}

jint ByteArrayInputStream::read() { return pos < count ? uint8_t(buf->raw()[pos++]) : -1; }

jint ByteArrayInputStream::read(jarr<jbyte> b, jint off, jint len) {
  if (off < 0 || len < 0 || off > b->length - len) throwAIOOBE(off + len, b->length);
  if (pos >= count) return -1;
  jint n = std::min(len, count - pos);
  std::memcpy(b->raw() + off, buf->raw() + pos, size_t(n));
  pos += n;
  return n;
}

jlong ByteArrayInputStream::skip(jlong n) {
  jlong k = std::min<jlong>(n, count - pos);
  if (k < 0) k = 0;
  pos += jint(k);
  return k;
}

// ---------------------------------------------------------------------------------------------
// java.util

jint Random::next(int bits) {
  seed = (seed * 0x5DEECE66DLL + 0xBLL) & ((1LL << 48) - 1);
  return jint(uint64_t(seed) >> (48 - bits));
}

Ref<Object> Hashtable::get(Ref<Object> key) {
  for (auto& e : entries)
    if (e.first->equals(key)) return e.second;
  return nullptr;
}

Ref<Object> Hashtable::put(Ref<Object> key, Ref<Object> value) {
  if (!key || !value) throwNPE();
  for (auto& e : entries) {
    if (e.first->equals(key)) {
      Ref<Object> old = e.second;
      e.second = value;
      return old;
    }
  }
  entries.emplace_back(key, value);
  return nullptr;
}

}  // namespace j
