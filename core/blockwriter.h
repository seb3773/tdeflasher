#ifndef BLOCKWRITER_H
#define BLOCKWRITER_H

#include <cstdint>
#include <functional>
#include <string>

namespace flasher {

/**
 * Progress callback type.
 * Parameters: bytesWritten, totalBytes (0 if unknown), bytesPerSecond
 */
using ProgressCallback = std::function<void(uint64_t bytesWritten,
                                            uint64_t totalBytes, double speed)>;

/**
 * Writes raw data to a block device using O_DIRECT for safe, unbuffered I/O.
 */
class BlockWriter {
public:
  BlockWriter();
  ~BlockWriter();

  /**
   * Open a block device for writing.
   * Requires root privileges.
   */
  bool open(const std::string &devicePath);

  /**
   * Write a buffer to the device.
   * Returns bytes written, or -1 on error.
   */
  int write(const void *data, int size);

  /** Sync all pending writes to disk. */
  void sync();

  /** Close the device. */
  void close();

  /** Total bytes written so far. */
  uint64_t bytesWritten() const { return m_bytesWritten; }

  /** Last error message. */
  const std::string &errorString() const { return m_error; }

private:
  int m_fd;
  uint64_t m_bytesWritten;
  std::string m_device;
  std::string m_error;

  // Aligned buffer for O_DIRECT
  void *m_alignedBuf;
  int m_alignedBufSize;
};

} // namespace flasher

#endif
