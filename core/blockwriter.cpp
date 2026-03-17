#include "blockwriter.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace flasher {

static const int BLOCK_SIZE = 1024 * 1024; // 1 MB write chunks
static const int ALIGNMENT = 512;          // O_DIRECT alignment

BlockWriter::BlockWriter()
    : m_fd(-1), m_bytesWritten(0), m_alignedBuf(nullptr), m_alignedBufSize(0) {}

BlockWriter::~BlockWriter() { close(); }

bool BlockWriter::open(const std::string &devicePath) {
  close();
  m_device = devicePath;
  m_bytesWritten = 0;

  // Open with O_DIRECT for unbuffered I/O, O_SYNC for safety, O_EXCL for
  // exclusive lock
  m_fd = ::open(devicePath.c_str(), O_WRONLY | O_DIRECT | O_SYNC | O_EXCL);
  if (m_fd < 0) {
    // Fallback without O_DIRECT if the device doesn't support it
    m_fd = ::open(devicePath.c_str(), O_WRONLY | O_SYNC | O_EXCL);
    if (m_fd < 0) {
      m_error = "Failed to open " + devicePath + ": " + strerror(errno);
      return false;
    }
  }

  // Allocate aligned buffer for O_DIRECT
  m_alignedBufSize = BLOCK_SIZE;
  if (posix_memalign(&m_alignedBuf, ALIGNMENT, m_alignedBufSize) != 0) {
    m_error = "Failed to allocate aligned buffer";
    ::close(m_fd);
    m_fd = -1;
    return false;
  }

  return true;
}

int BlockWriter::write(const void *data, int size) {
  if (m_fd < 0) {
    m_error = "Device not open";
    return -1;
  }

  // Copy to aligned buffer and write
  int totalWritten = 0;
  const char *src = (const char *)data;

  while (totalWritten < size) {
    int chunk = size - totalWritten;
    if (chunk > m_alignedBufSize)
      chunk = m_alignedBufSize;

    // For O_DIRECT, size must be aligned to 512 bytes
    // Pad the last chunk if necessary
    int writeSize = chunk;
    if (writeSize % ALIGNMENT != 0) {
      writeSize = ((writeSize / ALIGNMENT) + 1) * ALIGNMENT;
    }

    memcpy(m_alignedBuf, src + totalWritten, chunk);
    // Zero-fill padding
    if (writeSize > chunk) {
      memset((char *)m_alignedBuf + chunk, 0, writeSize - chunk);
    }

    ssize_t n = ::write(m_fd, m_alignedBuf, writeSize);
    if (n < 0) {
      m_error = "Write error: " + std::string(strerror(errno));
      return -1;
    }

    totalWritten += chunk;
    m_bytesWritten += chunk;
  }

  return totalWritten;
}

void BlockWriter::sync() {
  if (m_fd >= 0) {
    fdatasync(m_fd);
  }
}

void BlockWriter::close() {
  if (m_fd >= 0) {
    fdatasync(m_fd);
    ::close(m_fd);
    m_fd = -1;
  }
  if (m_alignedBuf) {
    free(m_alignedBuf);
    m_alignedBuf = nullptr;
  }
}

} // namespace flasher
