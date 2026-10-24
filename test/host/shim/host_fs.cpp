#include <SD.h>
#include <SPI.h>

#include <algorithm>
#include <cstdarg>
#include <cstring>

File::File(std::string path, int mode) : path_(std::move(path)), mode_(mode) {
  open_ = hostfs::files.count(path_) > 0 || mode != FILE_READ;
  if (mode == FILE_WRITE) {
    hostfs::files[path_].clear();
    position_ = 0;
  }
  if (mode == FILE_APPEND && hostfs::files.count(path_) > 0) {
    position_ = hostfs::files[path_].size();
  }
}

bool File::available() {
  if (!open_) {
    return false;
  }
  return position_ < hostfs::files[path_].size();
}

int File::read() {
  if (!open_) {
    return -1;
  }
  std::string& data = hostfs::files[path_];
  if (position_ >= data.size()) {
    return -1;
  }
  return static_cast<unsigned char>(data[position_++]);
}

String File::readStringUntil(char delimiter) {
  if (!open_) {
    return String();
  }
  std::string& data = hostfs::files[path_];
  std::string result;
  while (position_ < data.size()) {
    char character = data[position_++];
    if (character == delimiter) {
      break;
    }
    result.push_back(character);
  }
  return String(result);
}

size_t File::size() const {
  auto it = hostfs::files.find(path_);
  return it == hostfs::files.end() ? 0 : it->second.size();
}

size_t File::writeBytes(const std::string& data) {
  if (!open_) {
    return 0;
  }
  std::string& target = hostfs::files[path_];
  size_t allowed = data.size();
  if (hostfs::tornWriteLimit != static_cast<size_t>(-1)) {
    allowed = std::min(allowed, hostfs::tornWriteLimit);
    hostfs::tornWriteLimit -= allowed;
  }
  if (target.size() < position_) {
    target.resize(position_, '\0');
  }
  target.replace(position_, allowed, data.substr(0, allowed));
  position_ += allowed;
  return allowed;
}

size_t File::write(const uint8_t* data, size_t length) {
  return writeBytes(std::string(reinterpret_cast<const char*>(data), length));
}

size_t File::print(const char* text) { return writeBytes(text); }

size_t File::println(const char* text) {
  return writeBytes(std::string(text) + "\n");
}

size_t File::println(const String& text) {
  return writeBytes(std::string(text.c_str()) + "\n");
}

size_t File::printf(const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  int written = std::vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  if (written <= 0) {
    return 0;
  }
  return writeBytes(std::string(buffer, static_cast<size_t>(written)));
}

void File::close() { open_ = false; }

HostSerial Serial;

namespace hostfs {
std::map<std::string, std::string> files;
bool failNextRename = false;
std::string failRenameTo;
size_t tornWriteLimit = static_cast<size_t>(-1);

void reset() {
  files.clear();
  failNextRename = false;
  failRenameTo.clear();
  tornWriteLimit = static_cast<size_t>(-1);
}

void setFile(const std::string& path, const std::string& content) {
  files[path] = content;
}

std::string content(const std::string& path) {
  auto it = files.find(path);
  return it == files.end() ? std::string() : it->second;
}

bool exists(const std::string& path) { return files.count(path) > 0; }
}  // namespace hostfs

HostSDClass SD;

bool HostSDClass::begin(uint8_t cs, SPIClass& spi, uint32_t speed) {
  (void)cs;
  (void)spi;
  (void)speed;
  return mounted_;
}

File HostSDClass::open(const char* path, int mode) {
  if (mode == FILE_READ && hostfs::files.count(path) == 0) {
    return File();
  }
  return File(path, mode);
}

bool HostSDClass::remove(const char* path) {
  return hostfs::files.erase(path) > 0;
}

bool HostSDClass::rename(const char* from, const char* to) {
  if (!hostfs::failRenameTo.empty() && hostfs::failRenameTo == to) {
    // Сбой однократный: как отдельный сбойный вызов, а не «сломанный» rename.
    hostfs::failRenameTo.clear();
    return false;
  }
  if (hostfs::failNextRename) {
    hostfs::failNextRename = false;
    return false;
  }
  auto it = hostfs::files.find(from);
  if (it == hostfs::files.end()) {
    return false;
  }
  hostfs::files[to] = it->second;
  hostfs::files.erase(it);
  return true;
}

// --- RTC-память: обнуление имитирует холодный старт после потери питания ---
extern "C" {
extern char __start_rtc_data[] __attribute__((weak));
extern char __stop_rtc_data[] __attribute__((weak));
}

void hostResetRtcMemory() {
  if (__start_rtc_data && __stop_rtc_data) {
    std::memset(__start_rtc_data, 0,
                static_cast<size_t>(__stop_rtc_data - __start_rtc_data));
  }
}
