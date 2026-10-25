// jrt: the slice of java.lang / java.io / java.util the game uses, with Java semantics.
//
// Generated code (port/src/game/gen) is written against this header. Rules it relies on:
//  - Every Java object derives (virtually) from j::Object and lives behind Ref<T>,
//    an intrusive ref-counting pointer. Dereferencing a null Ref throws NullPointerException.
//  - Arrays are Ref<Array<T>> (alias jarr<T>), bounds-checked: ArrayIndexOutOfBoundsException.
//  - Java exceptions are thrown as j::Thrown (wrapping a Ref<Throwable>) so the game's
//    `catch (Exception e)` blocks work exactly like on the phone.
//  - Integer semantics: built with -fwrapv; shifts/div/rem go through the helpers below.
//  - One Java thread runs at a time (global lock released in sleep/yield), like a single-core
//    phone VM, so refcounts and game state need no atomics.
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

namespace j {

using jint = int32_t;
using jlong = int64_t;
using jbyte = int8_t;
using jshort = int16_t;
using jchar = char16_t;
using jboolean = bool;

// ---------------------------------------------------------------------------------------------
// Object + Ref

struct Class;
struct String;
template <class T> class Ref;

struct Object {
  mutable int32_t refcount_ = 1;  // 1 while constructing; make<T>() adopts that reference
  Object() = default;
  Object(const Object&) = delete;
  Object& operator=(const Object&) = delete;
  virtual ~Object() = default;

  virtual jint hashCode();
  virtual jboolean equals(Ref<Object> other);
  virtual Ref<String> toString();
  Ref<Class> getClass();
};

inline void retain(const Object* o) { if (o) ++o->refcount_; }
inline void release(const Object* o) { if (o && --o->refcount_ == 0) delete o; }

[[noreturn]] void throwNPE();
[[noreturn]] void throwAIOOBE(jint index, jint length);
[[noreturn]] void throwClassCast(const char* from = nullptr, const char* to = nullptr);
[[noreturn]] void throwArithmetic();

struct AdoptTag {};

template <class T>
class Ref {
 public:
  Ref() = default;
  Ref(std::nullptr_t) {}
  Ref(T* p) : p_(p) { retain(obj()); }   // from raw `this`
  Ref(T* p, AdoptTag) : p_(p) {}           // takes over the construction reference
  Ref(const Ref& o) : p_(o.p_) { retain(obj()); }
  Ref(Ref&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
  template <class U, class = std::enable_if_t<std::is_convertible_v<U*, T*>>>
  Ref(const Ref<U>& o) : p_(o.get()) { retain(obj()); }
  ~Ref() { release(obj()); }

  Ref& operator=(const Ref& o) { Ref(o).swap(*this); return *this; }
  Ref& operator=(Ref&& o) noexcept { Ref(std::move(o)).swap(*this); return *this; }
  Ref& operator=(std::nullptr_t) { Ref().swap(*this); return *this; }
  void swap(Ref& o) noexcept { std::swap(p_, o.p_); }

  T* get() const { return p_; }
  T* operator->() const { if (!p_) throwNPE(); return p_; }
  T& operator*() const { if (!p_) throwNPE(); return *p_; }
  explicit operator bool() const { return p_ != nullptr; }

  // Array element access straight through the Ref: a[i]
  template <class I, class U = T>
  auto operator[](I i) const -> decltype(std::declval<U&>().at(jint(i))) { return (**this).at(jint(i)); }

 private:
  const Object* obj() const { return p_; }  // requires complete T at instantiation
  T* p_ = nullptr;
};

template <class A, class B>
inline bool operator==(const Ref<A>& a, const Ref<B>& b) {
  return static_cast<const Object*>(a.get()) == static_cast<const Object*>(b.get());
}
template <class A, class B>
inline bool operator!=(const Ref<A>& a, const Ref<B>& b) { return !(a == b); }
template <class A> inline bool operator==(const Ref<A>& a, std::nullptr_t) { return !a.get(); }
template <class A> inline bool operator!=(const Ref<A>& a, std::nullptr_t) { return a.get(); }
template <class A> inline bool operator==(std::nullptr_t, const Ref<A>& a) { return !a.get(); }
template <class A> inline bool operator!=(std::nullptr_t, const Ref<A>& a) { return a.get(); }

// new T(args...)
template <class T, class... A>
inline Ref<T> make(A&&... a) { return Ref<T>(new T(std::forward<A>(a)...), AdoptTag{}); }

// `this` as a Ref (for passing/storing `this`)
template <class T> inline Ref<T> self(T* t) { return Ref<T>(t); }

// (T) expr for references
template <class T, class U>
inline Ref<T> cast(const Ref<U>& u) {
  if (!u) return nullptr;
  T* t = dynamic_cast<T*>(static_cast<Object*>(u.get()));
  if (!t) throwClassCast(typeid(*u.get()).name(), typeid(T).name());
  return Ref<T>(t);
}
template <class T> inline Ref<T> cast(std::nullptr_t) { return nullptr; }

template <class T, class U>
inline bool instanceof(const Ref<U>& u) { return u && dynamic_cast<T*>(static_cast<Object*>(u.get())); }

// ---------------------------------------------------------------------------------------------
// Arrays

struct ArrayBase : virtual Object {
  const jint length;
  explicit ArrayBase(jint n) : length(n) {}
  // System.arraycopy support: element-wise copy between arrays of the same element type.
  virtual void copyTo(jint srcPos, ArrayBase& dst, jint dstPos, jint len) = 0;
};

// Arrays of references share this base, giving Java array covariance (class_8[] is an Object[]).
struct RefArrayBase : ArrayBase {
  using ArrayBase::ArrayBase;
  virtual Ref<Object> getObj(jint i) = 0;
  virtual void setObj(jint i, const Ref<Object>& v) = 0;  // ArrayStoreException check
};

template <class T> struct IsRef : std::false_type {};
template <class T> struct IsRef<Ref<T>> : std::true_type { using type = T; };

template <class T>
struct Array final : std::conditional_t<IsRef<T>::value, RefArrayBase, ArrayBase> {
  using Base = std::conditional_t<IsRef<T>::value, RefArrayBase, ArrayBase>;
  using ArrayBase::length;
  std::unique_ptr<T[]> data;  // not std::vector: vector<bool> has no T&
  explicit Array(jint n) : Base(n), data(new T[size_t(n)]()) {}
  Array(std::initializer_list<T> init) : Base(jint(init.size())), data(new T[init.size()]()) {
    std::copy(init.begin(), init.end(), data.get());
  }

  T& at(jint i) {
    if (uint32_t(i) >= uint32_t(length)) throwAIOOBE(i, length);
    return data[size_t(i)];
  }
  T* raw() { return data.get(); }

  void copyTo(jint srcPos, ArrayBase& dstBase, jint dstPos, jint len) override {
    if (len < 0 || srcPos < 0 || dstPos < 0 || srcPos > length - len || dstPos > dstBase.length - len)
      throwAIOOBE(srcPos + len, length);
    auto* dst = dynamic_cast<Array<T>*>(&dstBase);
    if (!dst) {
      // Different reference array types (e.g. class_8[] -> Object[]): copy element-wise with
      // store checks. Primitive arrays of different types are an ArrayStoreException in Java.
      auto* src = dynamic_cast<RefArrayBase*>(this);
      auto* d = dynamic_cast<RefArrayBase*>(&dstBase);
      if (!src || !d) throwClassCast(typeid(*this).name(), typeid(dstBase).name());
      bool backwards = static_cast<ArrayBase*>(d) == this && srcPos < dstPos;
      for (jint k = 0; k < len; ++k) {
        jint i = backwards ? len - 1 - k : k;
        d->setObj(dstPos + i, src->getObj(srcPos + i));
      }
      return;
    }
    if (dst == this && srcPos < dstPos) {
      for (jint i = len - 1; i >= 0; --i) dst->data[size_t(dstPos + i)] = data[size_t(srcPos + i)];
    } else {
      for (jint i = 0; i < len; ++i) dst->data[size_t(dstPos + i)] = data[size_t(srcPos + i)];
    }
  }

  // RefArrayBase overrides (meaningful for reference element types only)
  Ref<Object> getObj(jint i) { return getObjDispatch(i, IsRef<T>{}); }
  void setObj(jint i, const Ref<Object>& v) { setObjDispatch(i, v, IsRef<T>{}); }

 private:
  Ref<Object> getObjDispatch(jint i, std::true_type) { return Ref<Object>(at(i)); }
  Ref<Object> getObjDispatch(jint, std::false_type) { throwClassCast(); }
  void setObjDispatch(jint i, const Ref<Object>& v, std::true_type) {
    T& slot = at(i);
    if (!v) { slot = nullptr; return; }
    auto* p = dynamic_cast<typename IsRef<T>::type*>(v.get());
    if (!p) throwClassCast(typeid(*v.get()).name(), typeid(T).name());  // ArrayStoreException
    slot = T(p);
  }
  void setObjDispatch(jint, const Ref<Object>&, std::false_type) { throwClassCast(); }
};

// ((Object[]) expr)[i] for an array of any reference type, read or written through Object.
class ObjArrayView {
 public:
  explicit ObjArrayView(const Ref<Object>& o) : keep_(o) {
    a_ = o ? dynamic_cast<RefArrayBase*>(o.get()) : nullptr;
    if (!o) throwNPE();
    if (!a_) throwClassCast(typeid(*o.get()).name(), "Object[]");
  }
  struct Elem {
    RefArrayBase* a;
    jint i;
    operator Ref<Object>() const { return a->getObj(i); }
    template <class U> Elem& operator=(const Ref<U>& v) { a->setObj(i, Ref<Object>(v)); return *this; }
    Elem& operator=(std::nullptr_t) { a->setObj(i, nullptr); return *this; }
  };
  Elem operator[](jint i) const { return {a_, i}; }
  ArrayBase* operator->() const { return a_; }  // .length

 private:
  Ref<Object> keep_;
  RefArrayBase* a_;
};
template <class U> inline ObjArrayView objArray(const Ref<U>& o) { return ObjArrayView(Ref<Object>(o)); }

template <class T> using jarr = Ref<Array<T>>;

[[noreturn]] void throwNegativeArraySize();

template <class T> struct ArrayElem;
template <class T> struct ArrayElem<Ref<Array<T>>> { using type = T; };

// new T[n] / new T[n][m]... ; E is the element type of the outermost array.
template <class E>
inline jarr<E> newArray(jint n) {
  if (n < 0) throwNegativeArraySize();
  return make<Array<E>>(n);
}
template <class E, class... Rest>
inline jarr<E> newArray(jint n, jint m, Rest... rest) {
  auto a = newArray<E>(n);
  for (jint i = 0; i < n; ++i) a->data[size_t(i)] = newArray<typename ArrayElem<E>::type>(m, rest...);
  return a;
}
// new T[]{...}
template <class E>
inline jarr<E> arrayOf(std::initializer_list<E> init) { return make<Array<E>>(init); }

// ---------------------------------------------------------------------------------------------
// Java integer semantics

inline jint shl(jint a, jlong n) { return jint(uint32_t(a) << (n & 31)); }
inline jint shr(jint a, jlong n) { return a >> (n & 31); }
inline jint ushr(jint a, jlong n) { return jint(uint32_t(a) >> (n & 31)); }
inline jlong shl(jlong a, jlong n) { return jlong(uint64_t(a) << (n & 63)); }
inline jlong shr(jlong a, jlong n) { return a >> (n & 63); }
inline jlong ushr(jlong a, jlong n) { return jlong(uint64_t(a) >> (n & 63)); }

inline jint div(jint a, jint b) {
  if (b == 0) throwArithmetic();
  return b == -1 ? jint(0u - uint32_t(a)) : a / b;
}
inline jint rem(jint a, jint b) {
  if (b == 0) throwArithmetic();
  return b == -1 ? 0 : a % b;
}
inline jlong div(jlong a, jlong b) {
  if (b == 0) throwArithmetic();
  return b == -1 ? jlong(0ull - uint64_t(a)) : a / b;
}
inline jlong rem(jlong a, jlong b) {
  if (b == 0) throwArithmetic();
  return b == -1 ? 0 : a % b;
}

// Compound assignments that need the helpers (lvalue evaluated once, result narrowed like Java).
template <class T> using Promoted = std::conditional_t<std::is_same_v<T, jlong>, jlong, jint>;
template <class T, class N> inline T& shl_assign(T& x, N n) { return x = T(shl(Promoted<T>(x), jlong(n))); }
template <class T, class N> inline T& shr_assign(T& x, N n) { return x = T(shr(Promoted<T>(x), jlong(n))); }
template <class T, class N> inline T& ushr_assign(T& x, N n) { return x = T(ushr(Promoted<T>(x), jlong(n))); }
template <class T, class N> inline T& div_assign(T& x, N n) {
  using P = std::conditional_t<std::is_same_v<T, jlong> || std::is_same_v<N, jlong>, jlong, jint>;
  return x = T(div(P(x), P(n)));
}
template <class T, class N> inline T& rem_assign(T& x, N n) {
  using P = std::conditional_t<std::is_same_v<T, jlong> || std::is_same_v<N, jlong>, jlong, jint>;
  return x = T(rem(P(x), P(n)));
}

// ---------------------------------------------------------------------------------------------
// java.lang

struct Class : virtual Object {
  Ref<struct InputStream> getResourceAsStream(Ref<String> name);
};

struct String final : virtual Object {
  std::u16string s;
  explicit String(std::u16string v) : s(std::move(v)) {}
  String() = default;
  // new String(byte[], off, len, enc): Latin-1 / UTF-8
  String(jarr<jbyte> bytes, jint off, jint len, Ref<String> enc);

  static Ref<String> lit(const char16_t* text);
  static Ref<String> fromUtf8(const std::string& u8);
  std::string utf8() const;

  jint length() { return jint(s.size()); }
  jchar charAt(jint i);
  jint compareTo(Ref<String> o);
  jboolean endsWith(Ref<String> o);
  jarr<jbyte> getBytes();
  jint indexOf(jint ch) { return indexOf(ch, 0); }
  jint indexOf(jint ch, jint from);
  jint indexOf(Ref<String> str, jint from);
  Ref<String> substring(jint begin, jint end);
  Ref<String> toLowerCase();
  static Ref<String> valueOf(jint v);

  jint hashCode() override;
  jboolean equals(Ref<Object> other) override;
  Ref<String> toString() override { return Ref<String>(this); }
};

// String literal with a per-site cached instance.
#define JSTR(text) ([]() -> const ::j::Ref<::j::String>& { static const ::j::Ref<::j::String> s_ = ::j::String::lit(text); return s_; }())

// String concatenation: "a" + 1 + 'c' + obj
inline void appendTo(std::u16string& out, const Ref<String>& v) { out += v ? v->s : u"null"; }
void appendTo(std::u16string& out, jint v);
void appendTo(std::u16string& out, jlong v);
inline void appendTo(std::u16string& out, jchar v) { out += v; }
inline void appendTo(std::u16string& out, jbyte v) { appendTo(out, jint(v)); }
inline void appendTo(std::u16string& out, jshort v) { appendTo(out, jint(v)); }
inline void appendTo(std::u16string& out, jboolean v) { out += v ? u"true" : u"false"; }
template <class T, class = std::enable_if_t<!std::is_same_v<T, String>>>
inline void appendTo(std::u16string& out, const Ref<T>& v) { appendTo(out, v ? Ref<Object>(v)->toString() : Ref<String>()); }

template <class... A>
inline Ref<String> concat(const A&... a) {
  std::u16string out;
  (appendTo(out, a), ...);
  return make<String>(std::move(out));
}
template <class T>
inline Ref<String>& concat_assign(Ref<String>& s, const T& v) { return s = concat(s, v); }

struct StringBuffer final : virtual Object {
  std::u16string s;
  StringBuffer() = default;
  explicit StringBuffer(Ref<String> init) : s(init->s) {}
  template <class T>
  Ref<StringBuffer> append(const T& v) { appendTo(s, v); return Ref<StringBuffer>(this); }
  jchar charAt(jint i);
  Ref<StringBuffer> deleteRange(jint start, jint end);  // Java: delete(int,int)
  jint length() { return jint(s.size()); }
  void setCharAt(jint i, jchar c);
  Ref<String> toString() override { return make<String>(s); }
};

struct Integer final : virtual Object {
  const jint value;
  explicit Integer(jint v) : value(v) {}
  jint intValue() { return value; }
  jbyte byteValue() { return jbyte(value); }
  jint hashCode() override { return value; }
  jboolean equals(Ref<Object> o) override;
  Ref<String> toString() override { return String::valueOf(value); }
};

struct Throwable : virtual Object {
  Ref<String> message;
  Throwable() = default;
  explicit Throwable(Ref<String> m) : message(std::move(m)) {}
  virtual const char* name() const { return "java.lang.Throwable"; }
  void printStackTrace();
};
#define JRT_EXCEPTION(Name, Base, Full)                          \
  struct Name : Base {                                           \
    Name() = default;                                            \
    explicit Name(Ref<String> m) : Base(std::move(m)) {}         \
    const char* name() const override { return Full; }           \
  };
JRT_EXCEPTION(Exception, Throwable, "java.lang.Exception")
JRT_EXCEPTION(RuntimeException, Exception, "java.lang.RuntimeException")
JRT_EXCEPTION(NullPointerException, RuntimeException, "java.lang.NullPointerException")
JRT_EXCEPTION(ArrayIndexOutOfBoundsException, RuntimeException, "java.lang.ArrayIndexOutOfBoundsException")
JRT_EXCEPTION(ArithmeticException, RuntimeException, "java.lang.ArithmeticException")
JRT_EXCEPTION(ClassCastException, RuntimeException, "java.lang.ClassCastException")
JRT_EXCEPTION(NegativeArraySizeException, RuntimeException, "java.lang.NegativeArraySizeException")
JRT_EXCEPTION(IllegalArgumentException, RuntimeException, "java.lang.IllegalArgumentException")
JRT_EXCEPTION(IllegalStateException, RuntimeException, "java.lang.IllegalStateException")
JRT_EXCEPTION(InterruptedException, Exception, "java.lang.InterruptedException")
JRT_EXCEPTION(IOException, Exception, "java.io.IOException")
#undef JRT_EXCEPTION

// The C++ exception carrying a Java throwable.
struct Thrown {
  Ref<Throwable> ex;
};
template <class T> [[noreturn]] inline void throwJ(const Ref<T>& t) {
  if (!t) throwNPE();
  throw Thrown{Ref<Throwable>(t)};
}

struct Math {
  static jint abs(jint v) { return v < 0 ? jint(0u - uint32_t(v)) : v; }
  static jlong abs(jlong v) { return v < 0 ? jlong(0ull - uint64_t(v)) : v; }
  static jint min(jint a, jint b) { return a < b ? a : b; }
  static jint max(jint a, jint b) { return a > b ? a : b; }
};

struct PrintStream final : virtual Object {
  void println(Ref<String> s);
};

struct System {
  static inline Ref<PrintStream> out;
  static jlong currentTimeMillis();
  static void gc() {}
  template <class A, class B>
  static void arraycopy(const Ref<A>& src, jint sp, const Ref<B>& dst, jint dp, jint len) {
    if (!src || !dst) throwNPE();
    auto* s = dynamic_cast<ArrayBase*>(static_cast<Object*>(src.get()));
    auto* d = dynamic_cast<ArrayBase*>(static_cast<Object*>(dst.get()));
    if (!s || !d) throwClassCast();
    s->copyTo(sp, *d, dp, len);
  }
};

struct Runnable : virtual Object {
  virtual void run() = 0;
};

struct Thread final : virtual Object {
  Ref<Runnable> target;
  explicit Thread(Ref<Runnable> r) : target(std::move(r)) {}
  void start();
  void setPriority(jint) {}
  static void sleep(jlong ms);
  static void yield();
};

// ---------------------------------------------------------------------------------------------
// java.io

struct InputStream : virtual Object {
  virtual jint read() = 0;
  virtual jint read(jarr<jbyte> b, jint off, jint len);
  jint read(jarr<jbyte> b) { return read(b, 0, b->length); }
  virtual jlong skip(jlong n);
  virtual void close() {}
};

struct ByteArrayInputStream : InputStream {
  jarr<jbyte> buf;
  jint pos = 0, count = 0;
  explicit ByteArrayInputStream(jarr<jbyte> b) : buf(std::move(b)), count(buf->length) {}
  using InputStream::read;
  jint read() override;
  jint read(jarr<jbyte> b, jint off, jint len) override;
  jlong skip(jlong n) override;
};

// ---------------------------------------------------------------------------------------------
// java.util

struct Random final : virtual Object {
  jlong seed;
  explicit Random(jlong s) { setSeed(s); }
  void setSeed(jlong s) { seed = (s ^ 0x5DEECE66DLL) & ((1LL << 48) - 1); }
  jint next(int bits);
  jint nextInt() { return next(32); }
};

struct Hashtable final : virtual Object {
  std::vector<std::pair<Ref<Object>, Ref<Object>>> entries;  // small tables; linear probe is fine
  Ref<Object> get(Ref<Object> key);
  Ref<Object> put(Ref<Object> key, Ref<Object> value);
};

// ---------------------------------------------------------------------------------------------
// Runtime services

namespace rt {
void init();
// Global VM lock: exactly one Java thread runs at a time.
void lock();
void unlock();
// Called with the lock held, whenever a Java thread yields (sleep/yield).
// The MIDP layer uses it to deliver key events on the game thread.
extern void (*onYield)();
// Every translated catch block calls this; prints the exception when DR_TRACE is set.
void traceCatch(const struct Thrown& t, const char* where);
}  // namespace rt

}  // namespace j
