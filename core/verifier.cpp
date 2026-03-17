#include "verifier.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <gcrypt.h>
#include <iomanip>
#include <sstream>
#include <unistd.h>

namespace flasher {

static const int VERIFY_BUF_SIZE = 1024 * 1024; // 1 MB

Verifier::Verifier() : m_ctx(nullptr), m_active(false) {}

Verifier::~Verifier() {
  if (m_ctx) {
    gcry_md_close((gcry_md_hd_t)m_ctx);
    m_ctx = nullptr;
  }
}

void Verifier::begin() {
  if (m_ctx) {
    gcry_md_close((gcry_md_hd_t)m_ctx);
    m_ctx = nullptr;
  }

  // Initialize libgcrypt if not already done
  static bool gcryptInitialized = false;
  if (!gcryptInitialized) {
    if (!gcry_check_version(GCRYPT_VERSION)) {
      m_error = "libgcrypt version mismatch";
      return;
    }
    gcry_control(GCRYCTL_DISABLE_SECMEM, 0);
    gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
    gcryptInitialized = true;
  }

  gcry_md_hd_t hd;
  gcry_error_t err = gcry_md_open(&hd, GCRY_MD_SHA256, 0);
  if (err) {
    m_error = "Failed to init SHA-256";
    return;
  }
  m_ctx = (void *)hd;
  m_active = true;
}

void Verifier::update(const void *data, int size) {
  if (!m_active || !m_ctx)
    return;
  gcry_md_write((gcry_md_hd_t)m_ctx, data, size);
}

std::string Verifier::finalize() {
  if (!m_active || !m_ctx)
    return "";

  gcry_md_final((gcry_md_hd_t)m_ctx);
  unsigned char *digest = gcry_md_read((gcry_md_hd_t)m_ctx, GCRY_MD_SHA256);
  int digestLen = gcry_md_get_algo_dlen(GCRY_MD_SHA256);

  std::ostringstream hex;
  for (int i = 0; i < digestLen; i++) {
    hex << std::setw(2) << std::setfill('0') << std::hex << (int)digest[i];
  }

  m_active = false;
  return hex.str();
}

bool Verifier::verifyDevice(
    const std::string &devicePath, uint64_t bytesToVerify,
    const std::string &expectedHash, volatile bool *cancelled,
    std::function<void(uint64_t, uint64_t)> progressCb) {
  int fd = ::open(devicePath.c_str(), O_RDONLY);
  if (fd < 0) {
    m_error =
        "Cannot open " + devicePath + " for verification: " + strerror(errno);
    return false;
  }

  // Initialize a fresh hash context for verification
  Verifier vhash;
  vhash.begin();

  void *buf = malloc(VERIFY_BUF_SIZE);
  if (!buf) {
    m_error = "Out of memory";
    ::close(fd);
    return false;
  }

  uint64_t totalRead = 0;
  while (totalRead < bytesToVerify) {
    if (cancelled && *cancelled) {
      m_error = "Verification cancelled by user";
      free(buf);
      ::close(fd);
      return false;
    }

    uint64_t remaining = bytesToVerify - totalRead;
    int toRead =
        (remaining > VERIFY_BUF_SIZE) ? VERIFY_BUF_SIZE : (int)remaining;

    ssize_t n = ::read(fd, buf, toRead);
    if (n <= 0) {
      if (n < 0)
        m_error =
            "Read error during verification: " + std::string(strerror(errno));
      else
        m_error = "Unexpected EOF during verification";
      free(buf);
      ::close(fd);
      return false;
    }

    vhash.update(buf, (int)n);
    totalRead += n;

    if (progressCb)
      progressCb(totalRead, bytesToVerify);
  }

  free(buf);
  ::close(fd);

  std::string deviceHash = vhash.finalize();
  if (deviceHash != expectedHash) {
    m_error = "Verification failed: hash mismatch";
    return false;
  }

  return true;
}

} // namespace flasher
