#ifndef VERIFIER_H
#define VERIFIER_H

#include <cstdint>
#include <functional>
#include <string>

namespace flasher {

/**
 * Verifies written data by re-reading the device and comparing
 * a SHA-256 hash against the source hash computed during writing.
 */
class Verifier {
public:
  Verifier();
  ~Verifier();

  /** Initialize the hasher. Call before feeding data. */
  void begin();

  /** Feed data to the hasher (call during write to hash the source). */
  void update(const void *data, int size);

  /** Finalize and return the hex hash string. */
  std::string finalize();

  /**
   * Verify a device's contents against an expected hash.
   * Reads `bytesToVerify` bytes from `devicePath` and computes SHA-256.
   * Calls progressCb with (bytesVerified, totalBytes) during verification.
   * Returns true if hashes match.
   */
  bool
  verifyDevice(const std::string &devicePath, uint64_t bytesToVerify,
               const std::string &expectedHash, volatile bool *cancelled,
               std::function<void(uint64_t, uint64_t)> progressCb = nullptr);

  /** Last error message. */
  const std::string &errorString() const { return m_error; }

private:
  void *m_ctx; // gcrypt hash context (opaque)
  std::string m_error;
  bool m_active;
};

} // namespace flasher

#endif
