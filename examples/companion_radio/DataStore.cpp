#include <Arduino.h>
#include "DataStore.h"

#if defined(EXTRAFS) || defined(QSPIFLASH)
  #define MAX_BLOBRECS 100
#else
  #define MAX_BLOBRECS 20
#endif

DataStore::DataStore(FILESYSTEM& fs, mesh::RTCClock& clock) : _fs(&fs), _fsExtra(nullptr), _clock(&clock),
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
    identity_store(fs, "")
#elif defined(RP2040_PLATFORM)
    identity_store(fs, "/identity")
#else
    identity_store(fs, "/identity")
#endif
{
}

#if defined(EXTRAFS) || defined(QSPIFLASH)
DataStore::DataStore(FILESYSTEM& fs, FILESYSTEM& fsExtra, mesh::RTCClock& clock) : _fs(&fs), _fsExtra(&fsExtra), _clock(&clock),
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
    identity_store(fs, "")
#elif defined(RP2040_PLATFORM)
    identity_store(fs, "/identity")
#else
    identity_store(fs, "/identity")
#endif
{
}
#endif

static File openWrite(FILESYSTEM* fs, const char* filename) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  fs->remove(filename);
  return fs->open(filename, FILE_O_WRITE);
#elif defined(RP2040_PLATFORM)
  return fs->open(filename, "w");
#else
  return fs->open(filename, "w", true);
#endif
}

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  static uint32_t _ContactsChannelsTotalBlocks = 0;
#endif

void DataStore::begin() {
#if defined(RP2040_PLATFORM)
  identity_store.begin();
#endif

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  _ContactsChannelsTotalBlocks = _getContactsChannelsFS()->_getFS()->cfg->block_count;
  checkAdvBlobFile();
  #if defined(EXTRAFS) || defined(QSPIFLASH)
  migrateToSecondaryFS();
  #endif
#else
  // init 'blob store' support
  _fs->mkdir("/bl");
#endif
}

#if defined(ESP32)
  #include <SPIFFS.h>
  #include <nvs_flash.h>
#elif defined(RP2040_PLATFORM)
  #include <LittleFS.h>
#elif defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  #if defined(QSPIFLASH)
    #include <CustomLFS_QSPIFlash.h>
  #elif defined(EXTRAFS)
    #include <CustomLFS.h>
  #else 
    #include <InternalFileSystem.h>
  #endif
#endif

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
int _countLfsBlock(void *p, lfs_block_t block){
      if (block > _ContactsChannelsTotalBlocks) {
        MESH_DEBUG_PRINTLN("ERROR: Block %d exceeds filesystem bounds - CORRUPTION DETECTED!", block);
        return LFS_ERR_CORRUPT;  // return error to abort lfs_traverse() gracefully
    }
  lfs_size_t *size = (lfs_size_t*) p;
  *size += 1;
    return 0;
}

lfs_ssize_t _getLfsUsedBlockCount(FILESYSTEM* fs) {
  lfs_size_t size = 0;
  int err = lfs_traverse(fs->_getFS(), _countLfsBlock, &size);
  if (err) {
    MESH_DEBUG_PRINTLN("ERROR: lfs_traverse() error: %d", err);
    return 0;
  }
  return size;
}
#endif

uint32_t DataStore::getStorageUsedKb() const {
#if defined(ESP32)
  return SPIFFS.usedBytes() / 1024;
#elif defined(RP2040_PLATFORM)
  FSInfo info;
  info.usedBytes = 0;
  _fs->info(info);
  return info.usedBytes / 1024;
#elif defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  const lfs_config* config = _getContactsChannelsFS()->_getFS()->cfg;
  int usedBlockCount = _getLfsUsedBlockCount(_getContactsChannelsFS());
  int usedBytes = config->block_size * usedBlockCount;
  return usedBytes / 1024;
#else
  return 0;
#endif
}

uint32_t DataStore::getStorageTotalKb() const {
#if defined(ESP32)
  return SPIFFS.totalBytes() / 1024;
#elif defined(RP2040_PLATFORM)
  FSInfo info;
  info.totalBytes = 0;
  _fs->info(info);
  return info.totalBytes / 1024;
#elif defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  const lfs_config* config = _getContactsChannelsFS()->_getFS()->cfg;
  int totalBytes = config->block_size * config->block_count;
  return totalBytes / 1024;
#else
  return 0;
#endif
}

File DataStore::openRead(const char* filename) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  return _fs->open(filename, FILE_O_READ);
#elif defined(RP2040_PLATFORM)
  return _fs->open(filename, "r");
#else
  return _fs->open(filename, "r", false);
#endif
}

File DataStore::openRead(FILESYSTEM* fs, const char* filename) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  return fs->open(filename, FILE_O_READ);
#elif defined(RP2040_PLATFORM)
  return fs->open(filename, "r");
#else
  return fs->open(filename, "r", false);
#endif
}

bool DataStore::removeFile(const char* filename) {
  return _fs->remove(filename);
}

bool DataStore::removeFile(FILESYSTEM* fs, const char* filename) {
  return fs->remove(filename);
}

bool DataStore::formatFileSystem() {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  if (_fsExtra == nullptr) {
    return _fs->format();
  } else {
    return _fs->format() && _fsExtra->format();
  }
#elif defined(RP2040_PLATFORM)
  return LittleFS.format();
#elif defined(ESP32)
  bool fs_success = ((fs::SPIFFSFS *)_fs)->format();
  esp_err_t nvs_err = nvs_flash_erase(); // no need to reinit, will be done by reboot
  return fs_success && (nvs_err == ESP_OK);
#else
  #error "need to implement format()"
#endif
}

bool DataStore::loadMainIdentity(mesh::LocalIdentity &identity) {
  return identity_store.load("_main", identity);
}

bool DataStore::saveMainIdentity(const mesh::LocalIdentity &identity) {
  return identity_store.save("_main", identity);
}

void DataStore::loadPrefs(NodePrefs& prefs) {
  if (_fs->exists("/prefs.json")) {
    File file = openRead(_fs, "/prefs.json");
    if (file) {
      prefs.loadSerial(file);   // new Serial prefs
      file.close();
    }
  } else if (_fs->exists("/new_prefs")) {
    loadPrefsInt("/new_prefs", prefs);
    if (savePrefs(prefs) ) {                // save to new format
      //_fs->remove("/new_prefs"); // remove old
    }
  }
}

void DataStore::loadPrefsInt(const char *filename, NodePrefs& _prefs) {
  File file = openRead(_fs, filename);
  if (file) {
    uint8_t pad[8];

    file.read((uint8_t *)&_prefs.airtime_factor, sizeof(float));                           // 0
    file.read((uint8_t *)_prefs.node_name, sizeof(_prefs.node_name));                      // 4
    file.read(pad, 4);                                                                     // 36
    file.read((uint8_t *)&_prefs.node_lat, sizeof(_prefs.node_lat));                       // 40
    file.read((uint8_t *)&_prefs.node_lon, sizeof(_prefs.node_lon));                       // 48
    file.read((uint8_t *)&_prefs.freq, sizeof(_prefs.freq));                               // 56
    file.read((uint8_t *)&_prefs.sf, sizeof(_prefs.sf));                                   // 60
    file.read((uint8_t *)&_prefs.cr, sizeof(_prefs.cr));                                   // 61
    file.read((uint8_t *)&_prefs._client_repeat, sizeof(_prefs._client_repeat));             // 62
    file.read((uint8_t *)&_prefs.manual_add_contacts, sizeof(_prefs.manual_add_contacts)); // 63
    file.read((uint8_t *)&_prefs.bw, sizeof(_prefs.bw));                                   // 64
    file.read((uint8_t *)&_prefs.tx_power_dbm, sizeof(_prefs.tx_power_dbm));               // 68
    file.read((uint8_t *)&_prefs.telemetry_mode_base, sizeof(_prefs.telemetry_mode_base)); // 69
    file.read((uint8_t *)&_prefs.telemetry_mode_loc, sizeof(_prefs.telemetry_mode_loc));   // 70
    file.read((uint8_t *)&_prefs.telemetry_mode_env, sizeof(_prefs.telemetry_mode_env));   // 71
    file.read((uint8_t *)&_prefs.rx_delay_base, sizeof(_prefs.rx_delay_base));             // 72
    file.read((uint8_t *)&_prefs.advert_loc_policy, sizeof(_prefs.advert_loc_policy));     // 76
    file.read((uint8_t *)&_prefs.multi_acks, sizeof(_prefs.multi_acks));                   // 77
    file.read((uint8_t *)&_prefs.path_hash_mode, sizeof(_prefs.path_hash_mode));           // 78
    file.read(pad, 1);                                                                     // 79
    file.read((uint8_t *)&_prefs.ble_pin, sizeof(_prefs.ble_pin));                         // 80
    file.read((uint8_t *)&_prefs.buzzer_quiet, sizeof(_prefs.buzzer_quiet));               // 84
    file.read((uint8_t *)&_prefs.gps_enabled, sizeof(_prefs.gps_enabled));                 // 85
    file.read((uint8_t *)&_prefs.gps_interval, sizeof(_prefs.gps_interval));               // 86
    file.read((uint8_t *)&_prefs.autoadd_config, sizeof(_prefs.autoadd_config));           // 87
    file.read((uint8_t *)&_prefs.autoadd_max_hops, sizeof(_prefs.autoadd_max_hops));       // 88
    file.read((uint8_t *)&_prefs.rx_boosted_gain, sizeof(_prefs.rx_boosted_gain));         // 89
    file.read((uint8_t *)_prefs.default_scope_name, sizeof(_prefs.default_scope_name));    // 90
    file.read((uint8_t *)_prefs.default_scope_key, sizeof(_prefs.default_scope_key));     // 121

    // migrate old fields
    _prefs.setRepeatEn(_prefs._client_repeat != 0);

    file.close();
  }
}

bool DataStore::savePrefs(NodePrefs& _prefs) {
  File file = openWrite(_fs, "/prefs.json");
  if (file) {
    bool success = _prefs.saveSerial(file);
    file.close();
    return success;
  }
  return false;
}

namespace {
constexpr uint32_t CONTACT_RECORD_SIZE = 152;
constexpr uint32_t CONTACT_META_MAGIC = 0x43335631; // "C3V1"
constexpr uint16_t CONTACT_META_VERSION = 1;

struct __attribute__((packed)) ContactStoreMeta {
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  uint32_t generation;
  uint32_t count;
  uint32_t byte_count;
  uint32_t crc32;
};

struct ContactSlot {
  const char* data;
  const char* meta;
  const char* temp;
};

constexpr ContactSlot CONTACT_SLOTS[] = {
  {"/contacts3.a", "/contacts3.a.meta", "/contacts3.a.tmp"},
  {"/contacts3.b", "/contacts3.b.meta", "/contacts3.b.tmp"},
};

uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t length) {
  crc = ~crc;
  while (length--) {
    crc ^= *data++;
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc >> 1) ^ (0xEDB88320UL & (-(int32_t)(crc & 1)));
    }
  }
  return ~crc;
}

bool readContactMeta(FILESYSTEM* fs, const char* path, ContactStoreMeta& meta) {
  File file = fs->open(path, FILE_O_READ);
  if (!file || file.size() != sizeof(ContactStoreMeta)) {
    if (file) file.close();
    return false;
  }
  const bool success = file.read((uint8_t*)&meta, sizeof(ContactStoreMeta)) == sizeof(ContactStoreMeta);
  file.close();
  return success && meta.magic == CONTACT_META_MAGIC && meta.version == CONTACT_META_VERSION &&
         meta.byte_count == meta.count * CONTACT_RECORD_SIZE;
}

bool checksumFile(FILESYSTEM* fs, const char* path, uint32_t expectedBytes, uint32_t& crc) {
  File file = fs->open(path, FILE_O_READ);
  if (!file || file.size() != expectedBytes) {
    if (file) file.close();
    return false;
  }
  uint8_t buffer[128];
  uint32_t readBytes = 0;
  crc = 0;
  while (readBytes < expectedBytes) {
    const size_t wanted = min((uint32_t)sizeof(buffer), expectedBytes - readBytes);
    const int count = file.read(buffer, wanted);
    if (count <= 0) {
      file.close();
      return false;
    }
    crc = crc32Update(crc, buffer, count);
    readBytes += count;
  }
  file.close();
  return readBytes == expectedBytes;
}

bool isValidContactSlot(FILESYSTEM* fs, const ContactSlot& slot, ContactStoreMeta& meta) {
  if (!readContactMeta(fs, slot.meta, meta)) return false;
  uint32_t crc = 0;
  return checksumFile(fs, slot.data, meta.byte_count, crc) && crc == meta.crc32;
}

int newestValidContactSlot(FILESYSTEM* fs, ContactStoreMeta& meta) {
  ContactStoreMeta candidate;
  int newest = -1;
  for (uint8_t i = 0; i < sizeof(CONTACT_SLOTS) / sizeof(CONTACT_SLOTS[0]); i++) {
    if (isValidContactSlot(fs, CONTACT_SLOTS[i], candidate) &&
        (newest < 0 || candidate.generation > meta.generation)) {
      newest = i;
      meta = candidate;
    }
  }
  return newest;
}

bool isValidLegacyContacts(FILESYSTEM* fs, const char* path) {
  File file = fs->open(path, FILE_O_READ);
  if (!file) return false;
  const bool valid = file.size() > 0 && (file.size() % CONTACT_RECORD_SIZE) == 0;
  file.close();
  return valid;
}

bool copyFile(FILESYSTEM* fs, const char* source, const char* destination) {
  File input = fs->open(source, FILE_O_READ);
  if (!input) return false;
  const uint32_t expected = input.size();
  File output = openWrite(fs, destination);
  if (!output) {
    input.close();
    return false;
  }
  uint8_t buffer[128];
  uint32_t copied = 0;
  while (input.available()) {
    int count = input.read(buffer, sizeof(buffer));
    if (count <= 0 || output.write(buffer, count) != (size_t)count) break;
    copied += count;
  }
  input.close();
  output.close();
  return copied == expected;
}

bool writeContactMeta(FILESYSTEM* fs, const char* path, const ContactStoreMeta& meta) {
  File file = openWrite(fs, path);
  if (!file) return false;
  const bool success = file.write((const uint8_t*)&meta, sizeof(ContactStoreMeta)) == sizeof(ContactStoreMeta);
  file.close();
  ContactStoreMeta check;
  return success && readContactMeta(fs, path, check) &&
         check.generation == meta.generation && check.crc32 == meta.crc32;
}
}

void DataStore::loadContacts(DataStoreHost* host) {
    FILESYSTEM* fs = _getContactsChannelsFS();
    ContactStoreMeta meta;
    const int slot = newestValidContactSlot(fs, meta);
    const char* source = nullptr;
    if (slot >= 0) {
      source = CONTACT_SLOTS[slot].data;
    } else if (isValidLegacyContacts(fs, "/contacts3")) {
      source = "/contacts3";
      MESH_DEBUG_PRINTLN("Contacts loaded from legacy store; will migrate on next save");
    } else if (isValidLegacyContacts(fs, "/contacts3.bak")) {
      source = "/contacts3.bak";
      MESH_DEBUG_PRINTLN("Contacts primary invalid; recovered legacy backup");
    } else {
      MESH_DEBUG_PRINTLN("No complete contact store found");
      return;
    }

    File file = openRead(fs, source);
    if (!file) return;
    bool full = false;
    uint32_t loaded = 0;
    while (!full) {
      ContactInfo c;
      uint8_t pub_key[32];
      uint8_t unused;
      bool success = (file.read(pub_key, 32) == 32);
      success = success && (file.read((uint8_t *)&c.name, 32) == 32);
      success = success && (file.read(&c.type, 1) == 1);
      success = success && (file.read(&c.flags, 1) == 1);
      success = success && (file.read(&unused, 1) == 1);
      success = success && (file.read((uint8_t *)&c.sync_since, 4) == 4);
      success = success && (file.read((uint8_t *)&c.out_path_len, 1) == 1);
      success = success && (file.read((uint8_t *)&c.last_advert_timestamp, 4) == 4);
      success = success && (file.read(c.out_path, 64) == 64);
      success = success && (file.read((uint8_t *)&c.lastmod, 4) == 4);
      success = success && (file.read((uint8_t *)&c.gps_lat, 4) == 4);
      success = success && (file.read((uint8_t *)&c.gps_lon, 4) == 4);
      if (!success) break;
      c.id = mesh::Identity(pub_key);
      if (!host->onContactLoaded(c)) full = true;
      else loaded++;
    }
    file.close();
    MESH_DEBUG_PRINTLN("Loaded %u contacts from %s", loaded, source);
}

void DataStore::saveContacts(DataStoreHost* host, bool (*filter)(const ContactInfo& c)) {
  FILESYSTEM* fs = _getContactsChannelsFS();
  ContactStoreMeta activeMeta;
  const int activeSlot = newestValidContactSlot(fs, activeMeta);
  const uint8_t targetSlot = activeSlot == 0 ? 1 : 0;
  const ContactSlot& target = CONTACT_SLOTS[targetSlot];
  const uint32_t generation = activeSlot >= 0 ? activeMeta.generation + 1 : 1;

  File file = openWrite(fs, target.temp);
  if (file) {
    uint32_t idx = 0;
    uint32_t saved = 0;
    ContactInfo c;
    uint8_t unused = 0;
    bool writeFailed = false;

    while (host->getContactForSave(idx, c)) {
      if (filter && !filter(c)) {
        idx++;  // advance to next contact
        continue;
      }
      bool success = (file.write(c.id.pub_key, 32) == 32);
      success = success && (file.write((uint8_t *)&c.name, 32) == 32);
      success = success && (file.write(&c.type, 1) == 1);
      success = success && (file.write(&c.flags, 1) == 1);
      success = success && (file.write(&unused, 1) == 1);
      success = success && (file.write((uint8_t *)&c.sync_since, 4) == 4);
      success = success && (file.write((uint8_t *)&c.out_path_len, 1) == 1);
      success = success && (file.write((uint8_t *)&c.last_advert_timestamp, 4) == 4);
      success = success && (file.write(c.out_path, 64) == 64);
      success = success && (file.write((uint8_t *)&c.lastmod, 4) == 4);
      success = success && (file.write((uint8_t *)&c.gps_lat, 4) == 4);
      success = success && (file.write((uint8_t *)&c.gps_lon, 4) == 4);

      if (!success) {
        writeFailed = true;
        break;
      }

      idx++;  // advance to next contact
      saved++;
    }
    file.close();

    uint32_t crc = 0;
    const bool complete = !writeFailed && checksumFile(fs, target.temp, saved * CONTACT_RECORD_SIZE, crc);
    if (!complete) {
      fs->remove(target.temp);
      MESH_DEBUG_PRINTLN("Contacts save aborted: temporary file incomplete");
      return;
    }

    // The active valid slot is never modified. A power loss can only leave this
    // inactive slot incomplete; boot will continue using the previous generation.
    if (!copyFile(fs, target.temp, target.data)) {
      fs->remove(target.temp);
      MESH_DEBUG_PRINTLN("Contacts save aborted: inactive slot write failed");
      return;
    }

    ContactStoreMeta newMeta = { CONTACT_META_MAGIC, CONTACT_META_VERSION, 0,
                                 generation, saved, saved * CONTACT_RECORD_SIZE, crc };
    if (!writeContactMeta(fs, target.meta, newMeta)) {
      fs->remove(target.temp);
      MESH_DEBUG_PRINTLN("Contacts save aborted: inactive slot verification failed");
      return;
    }
    fs->remove(target.temp);
    MESH_DEBUG_PRINTLN("Saved %u contacts (generation %u)", saved, generation);
  }
}

void DataStore::loadChannels(DataStoreHost* host) {
    File file = openRead(_getContactsChannelsFS(), "/channels2");
    if (file) {
      bool full = false;
      uint8_t channel_idx = 0;
      while (!full) {
        ChannelDetails ch;
        uint8_t unused[4];

        bool success = (file.read(unused, 4) == 4);
        success = success && (file.read((uint8_t *)ch.name, 32) == 32);
        success = success && (file.read((uint8_t *)ch.channel.secret, 32) == 32);

        if (!success) break; // EOF

        if (host->onChannelLoaded(channel_idx, ch)) {
          channel_idx++;
        } else {
          full = true;
        }
      }
      file.close();
    }
}

void DataStore::saveChannels(DataStoreHost* host) {
  File file = openWrite(_getContactsChannelsFS(), "/channels2");
  if (file) {
    uint8_t channel_idx = 0;
    ChannelDetails ch;
    uint8_t unused[4];
    memset(unused, 0, 4);

    while (host->getChannelForSave(channel_idx, ch)) {
      bool success = (file.write(unused, 4) == 4);
      success = success && (file.write((uint8_t *)ch.name, 32) == 32);
      success = success && (file.write((uint8_t *)ch.channel.secret, 32) == 32);

      if (!success) break; // write failed
      channel_idx++;
    }
    file.close();
  }
}

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)

#define MAX_ADVERT_PKT_LEN   (2 + 32 + PUB_KEY_SIZE + 4 + SIGNATURE_SIZE + MAX_ADVERT_DATA_SIZE)

struct BlobRec {
  uint32_t timestamp;
  uint8_t  key[7];
  uint8_t  len;
  uint8_t  data[MAX_ADVERT_PKT_LEN];
};

void DataStore::checkAdvBlobFile() {
  if (!_getContactsChannelsFS()->exists("/adv_blobs")) {
    File file = openWrite(_getContactsChannelsFS(), "/adv_blobs");
    if (file) {
      BlobRec zeroes;
      memset(&zeroes, 0, sizeof(zeroes));
      for (int i = 0; i < MAX_BLOBRECS; i++) {     // pre-allocate to fixed size
        file.write((uint8_t *) &zeroes, sizeof(zeroes));
      }
      file.close();
    }
  }
}

void DataStore::migrateToSecondaryFS() {
  // migrate old adv_blobs, contacts3 and channels2 files to secondary FS if they don't already exist
  if (!_fsExtra->exists("/adv_blobs")) {
    if (_fs->exists("/adv_blobs")) {
    File oldAdvBlobs = openRead(_fs, "/adv_blobs");
    File newAdvBlobs = openWrite(_fsExtra, "/adv_blobs");

    if (oldAdvBlobs && newAdvBlobs) {
      BlobRec rec;
      size_t count = 0;

      // Copy 20 BlobRecs from old to new
      while (count < 20 && oldAdvBlobs.read((uint8_t *)&rec, sizeof(rec)) == sizeof(rec)) {
        newAdvBlobs.seek(count * sizeof(BlobRec));
        newAdvBlobs.write((uint8_t *)&rec, sizeof(rec));
        count++;
      }
    }
    if (oldAdvBlobs) oldAdvBlobs.close();
    if (newAdvBlobs) newAdvBlobs.close();
    _fs->remove("/adv_blobs");
    }
  }
  if (!_fsExtra->exists("/contacts3")) {
    if (_fs->exists("/contacts3")) {
      File oldFile = openRead(_fs, "/contacts3");
      const uint32_t expectedSize = oldFile ? oldFile.size() : 0;
      const bool sourceComplete = oldFile && (expectedSize == 0 || (expectedSize % CONTACT_RECORD_SIZE) == 0);
      if (sourceComplete) {
        File newFile = openWrite(_fsExtra, "/contacts3.migrate.tmp");
        if (newFile) {
        uint8_t buf[64];
        uint32_t copied = 0;
        bool copyComplete = true;
        while (copied < expectedSize) {
          const size_t wanted = min((uint32_t)sizeof(buf), expectedSize - copied);
          int n = oldFile.read(buf, wanted);
          if (n <= 0 || newFile.write(buf, n) != (size_t)n) {
            copyComplete = false;
            break;
          }
          copied += n;
        }
        newFile.close();
        File check = openRead(_fsExtra, "/contacts3.migrate.tmp");
        copyComplete = copyComplete && copied == expectedSize && check && check.size() == expectedSize;
        if (check) check.close();
        if (copyComplete && copyFile(_fsExtra, "/contacts3.migrate.tmp", "/contacts3")) {
          File finalCheck = openRead(_fsExtra, "/contacts3");
          copyComplete = finalCheck && finalCheck.size() == expectedSize;
          if (finalCheck) finalCheck.close();
          if (copyComplete) _fs->remove("/contacts3");
        }
        _fsExtra->remove("/contacts3.migrate.tmp");
        if (!copyComplete) MESH_DEBUG_PRINTLN("Contact migration failed; original retained");
        }
      }
      if (oldFile) oldFile.close();
    }
  }
  if (!_fsExtra->exists("/channels2")) {
    if (_fs->exists("/channels2")) {
      File oldFile = openRead(_fs, "/channels2");
      File newFile = openWrite(_fsExtra, "/channels2");

      if (oldFile && newFile) {
        uint8_t buf[64];
        int n;
        while ((n = oldFile.read(buf, sizeof(buf))) > 0) {
          newFile.write(buf, n);
        }
      }
      if (oldFile) oldFile.close();
      if (newFile) newFile.close();
      _fs->remove("/channels2");
    }
  }
  // cleanup nodes which have been testing the extra fs, copy _main.id and new_prefs back to primary
  if (_fsExtra->exists("/_main.id")) {
      if (_fs->exists("/_main.id")) {_fs->remove("/_main.id");}
      File oldFile = openRead(_fsExtra, "/_main.id");
      File newFile = openWrite(_fs, "/_main.id");

      if (oldFile && newFile) {
        uint8_t buf[64];
        int n;
        while ((n = oldFile.read(buf, sizeof(buf))) > 0) {
          newFile.write(buf, n);
        }
      }
      if (oldFile) oldFile.close();
      if (newFile) newFile.close();
      _fsExtra->remove("/_main.id");
  }
  if (_fsExtra->exists("/new_prefs")) {
    if (_fs->exists("/new_prefs")) {_fs->remove("/new_prefs");}
      File oldFile = openRead(_fsExtra, "/new_prefs");
      File newFile = openWrite(_fs, "/new_prefs");

      if (oldFile && newFile) {
        uint8_t buf[64];
        int n;
        while ((n = oldFile.read(buf, sizeof(buf))) > 0) {
          newFile.write(buf, n);
        }
      }
      if (oldFile) oldFile.close();
      if (newFile) newFile.close();
      _fsExtra->remove("/new_prefs");
  }
  // remove files from where they should not be anymore
  if (_fs->exists("/adv_blobs")) {
    _fs->remove("/adv_blobs");
  }
  if (_fs->exists("/contacts3")) {
    _fs->remove("/contacts3");
  }
  if (_fs->exists("/channels2")) {
    _fs->remove("/channels2");
  }
  if (_fsExtra->exists("/_main.id")) {
    _fsExtra->remove("/_main.id");
  }
  if (_fsExtra->exists("/new_prefs")) {
    _fsExtra->remove("/new_prefs");
  }
}

uint8_t DataStore::getBlobByKey(const uint8_t key[], int key_len, uint8_t dest_buf[]) {
  File file = openRead(_getContactsChannelsFS(), "/adv_blobs");
  uint8_t len = 0;  // 0 = not found
  if (file) {
    BlobRec tmp;
    while (file.read((uint8_t *) &tmp, sizeof(tmp)) == sizeof(tmp)) {
      if (memcmp(key, tmp.key, sizeof(tmp.key)) == 0) {  // only match by 7 byte prefix
        len = tmp.len;
        memcpy(dest_buf, tmp.data, len);
        break;
      }
    }
    file.close();
  }
  return len;
}

bool DataStore::putBlobByKey(const uint8_t key[], int key_len, const uint8_t src_buf[], uint8_t len) {
  if (len < PUB_KEY_SIZE+4+SIGNATURE_SIZE || len > MAX_ADVERT_PKT_LEN) return false;
  checkAdvBlobFile();
  File file = _getContactsChannelsFS()->open("/adv_blobs", FILE_O_WRITE);
  if (file) {
    uint32_t pos = 0, found_pos = 0;
    uint32_t min_timestamp = 0xFFFFFFFF;

    // search for matching key OR evict by oldest timestamp
    BlobRec tmp;
    file.seek(0);
    while (file.read((uint8_t *) &tmp, sizeof(tmp)) == sizeof(tmp)) {
      if (memcmp(key, tmp.key, sizeof(tmp.key)) == 0) {  // only match by 7 byte prefix
        found_pos = pos;
        break;
      }
      if (tmp.timestamp < min_timestamp) {
        min_timestamp = tmp.timestamp;
        found_pos = pos;
      }

      pos += sizeof(tmp);
    }

    memcpy(tmp.key, key, sizeof(tmp.key));  // just record 7 byte prefix of key
    memcpy(tmp.data, src_buf, len);
    tmp.len = len;
    tmp.timestamp = _clock->getCurrentTime();

    file.seek(found_pos);
    file.write((uint8_t *) &tmp, sizeof(tmp));

    file.close();
    return true;
  }
  return false; // error
}
bool DataStore::deleteBlobByKey(const uint8_t key[], int key_len) {
  return true; // this is just a stub on NRF52/STM32 platforms
}
#else
inline void makeBlobPath(const uint8_t key[], int key_len, char* path, size_t path_size) {
  char fname[18];
  if (key_len > 8) key_len = 8; // just use first 8 bytes (prefix)
  mesh::Utils::toHex(fname, key, key_len);
  sprintf(path, "/bl/%s", fname);
}

uint8_t DataStore::getBlobByKey(const uint8_t key[], int key_len, uint8_t dest_buf[]) {
  char path[64];
  makeBlobPath(key, key_len, path, sizeof(path));

  if (_fs->exists(path)) {
    File f = openRead(_fs, path);
    if (f) {
      int len = f.read(dest_buf, 255); // currently MAX 255 byte blob len supported!!
      f.close();
      return len;
    }
  }
  return 0; // not found
}

bool DataStore::putBlobByKey(const uint8_t key[], int key_len, const uint8_t src_buf[], uint8_t len) {
  char path[64];
  makeBlobPath(key, key_len, path, sizeof(path));

  File f = openWrite(_fs, path);
  if (f) {
    int n = f.write(src_buf, len);
    f.close();
    if (n == len) return true; // success!

    _fs->remove(path); // blob was only partially written!
  }
  return false; // error
}

bool DataStore::deleteBlobByKey(const uint8_t key[], int key_len) {
  char path[64];
  makeBlobPath(key, key_len, path, sizeof(path));

  _fs->remove(path);
  
  return true; // return true even if file did not exist
}
#endif
