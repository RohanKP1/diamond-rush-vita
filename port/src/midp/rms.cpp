// javax.microedition.rms.RecordStore: one file per store in plat::saveDir().
// File format: "RMS1", u32 record count, then per record u32 length + bytes (little endian).
#include <cstdio>
#include <cstring>
#include <map>

#include "midp/midp.h"
#include "platform/platform.h"

namespace j {

namespace {
std::map<std::string, Ref<RecordStore>> openStores;  // stores stay cached once opened

struct RecordStoreNotFoundException : RecordStoreException {
  const char* name() const override { return "javax.microedition.rms.RecordStoreNotFoundException"; }
};
struct InvalidRecordIDException : RecordStoreException {
  const char* name() const override { return "javax.microedition.rms.InvalidRecordIDException"; }
};

template <class E> [[noreturn]] void throwRms() {
  Ref<E> e = make<E>();
  throw Thrown{Ref<Throwable>(e)};
}

bool readU32(FILE* f, uint32_t& v) {
  uint8_t b[4];
  if (std::fread(b, 1, 4, f) != 4) return false;
  v = b[0] | b[1] << 8 | b[2] << 16 | uint32_t(b[3]) << 24;
  return true;
}

void writeU32(FILE* f, uint32_t v) {
  uint8_t b[4] = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)};
  std::fwrite(b, 1, 4, f);
}
}  // namespace

Ref<RecordStore> RecordStore::openRecordStore(Ref<String> name, jboolean createIfNecessary) {
  std::string key = name->utf8();
  auto it = openStores.find(key);
  if (it != openStores.end()) return it->second;

  auto rs = make<RecordStore>();
  rs->path = plat::saveDir() + key + ".rms";
  FILE* f = std::fopen(rs->path.c_str(), "rb");
  if (f) {
    char magic[4];
    uint32_t n;
    if (std::fread(magic, 1, 4, f) == 4 && std::memcmp(magic, "RMS1", 4) == 0 && readU32(f, n)) {
      for (uint32_t i = 0; i < n; ++i) {
        uint32_t len;
        if (!readU32(f, len) || len > (16u << 20)) break;
        std::vector<int8_t> rec(len);
        if (std::fread(rec.data(), 1, len, f) != len) break;
        rs->records.push_back(std::move(rec));
      }
    }
    std::fclose(f);
  } else if (!createIfNecessary) {
    throwRms<RecordStoreNotFoundException>();
  } else {
    rs->save();
  }
  openStores[key] = rs;
  return rs;
}

jarr<jbyte> RecordStore::getRecord(jint id) {
  if (id < 1 || id > jint(records.size())) throwRms<InvalidRecordIDException>();
  const std::vector<int8_t>& r = records[size_t(id - 1)];
  if (r.empty()) return nullptr;
  auto a = newArray<jbyte>(jint(r.size()));
  std::memcpy(a->raw(), r.data(), r.size());
  return a;
}

jint RecordStore::addRecord(jarr<jbyte> data, jint off, jint len) {
  std::vector<int8_t> rec;
  if (data) {
    if (off < 0 || len < 0 || off > data->length - len) throwAIOOBE(off + len, data->length);
    rec.assign(data->raw() + off, data->raw() + off + len);
  }
  records.push_back(std::move(rec));
  save();
  return jint(records.size());
}

void RecordStore::setRecord(jint id, jarr<jbyte> data, jint off, jint len) {
  if (id < 1 || id > jint(records.size())) throwRms<InvalidRecordIDException>();
  std::vector<int8_t> rec;
  if (data) {
    if (off < 0 || len < 0 || off > data->length - len) throwAIOOBE(off + len, data->length);
    rec.assign(data->raw() + off, data->raw() + off + len);
  }
  records[size_t(id - 1)] = std::move(rec);
  save();
}

void RecordStore::save() {
  // Write to a temp file and rename, so a crash or power-off mid-save can't corrupt progress.
  std::string tmp = path + ".tmp";
  FILE* f = std::fopen(tmp.c_str(), "wb");
  if (!f) {
    std::fprintf(stderr, "rms: cannot write %s\n", tmp.c_str());
    return;
  }
  std::fwrite("RMS1", 1, 4, f);
  writeU32(f, uint32_t(records.size()));
  for (const auto& r : records) {
    writeU32(f, uint32_t(r.size()));
    std::fwrite(r.data(), 1, r.size(), f);
  }
  std::fclose(f);
  std::remove(path.c_str());
  std::rename(tmp.c_str(), path.c_str());
}

}  // namespace j
