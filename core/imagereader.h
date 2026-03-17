#ifndef IMAGEREADER_H
#define IMAGEREADER_H

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

struct archive;

namespace flasher {

/**
 * Streaming image reader with automatic decompression via libarchive.
 * Supports: raw .img, .iso, .gz, .bz2, .xz, .zst, .zip, .lz4
 */
class ImageReader {
public:
  ImageReader();
  ~ImageReader();

  /**
   * Open an image file. Auto-detects compression.
   * Returns false on error (call errorString() for details).
   */
  bool open(const std::string &path);

  /**
   * Open an image from a URL via libcurl + UNIX pipe.
   */
  bool openUrl(const std::string &url, const std::string &username = "",
               const std::string &password = "");

  /**
   * Read next chunk of decompressed data.
   * Returns number of bytes read, 0 on EOF, -1 on error.
   */
  int read(void *buffer, int bufferSize);

  /** Close the image and free resources. */
  void close();

  /** Total compressed file size in bytes. */
  uint64_t compressedSize() const { return m_compressedSize; }

  /** Estimated decompressed size (0 if unknown). */
  uint64_t estimatedSize() const { return m_estimatedSize; }

  /** Current position in the decompressed stream. */
  uint64_t bytesRead() const { return m_bytesRead; }

  /** Last error message. */
  const std::string &errorString() const { return m_error; }

  /** Original file path. */
  const std::string &filePath() const { return m_path; }

private:
  struct archive *m_archive;
  int m_fd;
  int m_pipefd[2];
  std::thread *m_curlThread;
  std::atomic<bool> m_curlAborted;

  std::string m_path;
  std::string m_error;
  volatile uint64_t m_compressedSize;
  volatile uint64_t m_estimatedSize;
  uint64_t m_bytesRead;
  bool m_opened;
  bool m_isRaw; // true if file is not an archive (raw img/iso)

  static size_t curlWriteCb(void *ptr, size_t size, size_t nmemb,
                            void *userdata);
  static int curlProgressCb(void *clientp, double dltotal, double dlnow,
                            double ultotal, double ulnow);
};

} // namespace flasher

#endif
