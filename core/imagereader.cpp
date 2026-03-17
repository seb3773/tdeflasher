#include "imagereader.h"
#include <archive.h>
#include <archive_entry.h>
#include <cstring>
#include <curl/curl.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace flasher {

ImageReader::ImageReader()
    : m_archive(nullptr), m_fd(-1), m_curlThread(nullptr), m_curlAborted(false),
      m_compressedSize(0), m_estimatedSize(0), m_bytesRead(0), m_opened(false),
      m_isRaw(false) {
  m_pipefd[0] = -1;
  m_pipefd[1] = -1;
  signal(SIGPIPE, SIG_IGN);
}

ImageReader::~ImageReader() { close(); }

bool ImageReader::open(const std::string &path) {
  close();
  m_path = path;
  m_bytesRead = 0;

  // Get size and check if block device
  struct stat st;
  if (stat(path.c_str(), &st) == 0) {
    if (S_ISBLK(st.st_mode)) {
      m_fd = ::open(path.c_str(), O_RDONLY);
      if (m_fd >= 0) {
        // Find accurate size of block device
        uint64_t bytes = 0;
        if (ioctl(m_fd, BLKGETSIZE64, &bytes) == 0) {
          m_compressedSize = bytes;
          m_estimatedSize = bytes;
        } else {
          // Fallback to BLKGETSIZE
          unsigned long blocks = 0;
          if (ioctl(m_fd, BLKGETSIZE, &blocks) == 0) {
            m_compressedSize = blocks * 512;
            m_estimatedSize = blocks * 512;
          }
        }
        m_isRaw = true;
        m_opened = true;
        return true;
      }
    } else {
      m_compressedSize = st.st_size;
    }
  }

  // Determine if it's a "raw" image that we should read directly
  // (bypass libarchive for reliability on large ISO/IMG files)
  bool endsWithIso =
      (path.size() > 4 && path.substr(path.size() - 4) == ".iso");
  bool endsWithImg =
      (path.size() > 4 && path.substr(path.size() - 4) == ".img");

  if (endsWithIso || endsWithImg) {
    m_fd = ::open(path.c_str(), O_RDONLY);
    if (m_fd >= 0) {
      m_isRaw = true;
      m_estimatedSize = m_compressedSize;
      m_opened = true;
      return true;
    }
  }

  // Fallback to archive/compressed file via libarchive
  m_archive = archive_read_new();
  if (!m_archive) {
    m_error = "Failed to create archive reader";
    return false;
  }

  archive_read_support_filter_all(m_archive);
  archive_read_support_format_raw(m_archive);
  archive_read_support_format_all(m_archive);

  if (archive_read_open_filename(m_archive, path.c_str(), 1024 * 1024) !=
      ARCHIVE_OK) {
    m_error = "Failed to open: " + std::string(archive_error_string(m_archive));
    archive_read_free(m_archive);
    m_archive = nullptr;
    return false;
  }

  struct archive_entry *entry;
  if (archive_read_next_header(m_archive, &entry) != ARCHIVE_OK) {
    m_error = "Failed to read header: " +
              std::string(archive_error_string(m_archive));
    archive_read_free(m_archive);
    m_archive = nullptr;
    return false;
  }

  if (archive_entry_size_is_set(entry)) {
    m_estimatedSize = archive_entry_size(entry);
  }

  const char *fmt = archive_format_name(m_archive);
  m_isRaw = (fmt && strstr(fmt, "raw") != nullptr);
  if (archive_filter_count(m_archive) <= 1 && m_isRaw) {
    m_estimatedSize = m_compressedSize;
  }

  m_opened = true;
  return true;
}

size_t ImageReader::curlWriteCb(void *ptr, size_t size, size_t nmemb,
                                void *userdata) {
  ImageReader *self = static_cast<ImageReader *>(userdata);
  if (self->m_curlAborted)
    return 0;

  size_t total = size * nmemb;
  size_t remaining = total;
  const char *src = static_cast<const char *>(ptr);

  while (remaining > 0) {
    if (self->m_curlAborted)
      return 0;
    ssize_t written = ::write(self->m_pipefd[1], src, remaining);
    if (written < 0) {
      if (errno == EINTR)
        continue; // Interrupted, retry
      return 0;   // Real error — abort
    }
    src += written;
    remaining -= written;
  }
  return total;
}

int ImageReader::curlProgressCb(void *clientp, double dltotal, double dlnow,
                                double ultotal, double ulnow) {
  (void)ultotal;
  (void)ulnow;
  ImageReader *self = static_cast<ImageReader *>(clientp);
  if (dltotal > 0) {
    uint64_t total = static_cast<uint64_t>(dltotal);
    if (self->m_compressedSize == 0)
      self->m_compressedSize = total;
    if (self->m_estimatedSize == 0)
      self->m_estimatedSize = total;
  }
  return self->m_curlAborted ? 1 : 0;
}

// Helper: check if URL path ends with a given suffix (case-insensitive)
static bool urlEndsWith(const std::string &url, const char *suffix) {
  // Strip query string / fragment
  std::string path = url;
  size_t q = path.find('?');
  if (q != std::string::npos)
    path = path.substr(0, q);
  q = path.find('#');
  if (q != std::string::npos)
    path = path.substr(0, q);

  size_t slen = strlen(suffix);
  if (path.size() < slen)
    return false;
  std::string tail = path.substr(path.size() - slen);
  for (size_t i = 0; i < tail.size(); i++)
    tail[i] = tolower(tail[i]);
  return tail == suffix;
}

bool ImageReader::openUrl(const std::string &url, const std::string &username,
                          const std::string &password) {
  close();
  m_path = url;
  m_bytesRead = 0;
  m_compressedSize = 0;
  m_estimatedSize = 0;
  m_isRaw = false;

  // Detect if the URL points to a raw image (ISO/IMG) that should NOT go
  // through libarchive (libarchive's format_raw misinterprets ISO9660 headers)
  bool rawUrl = urlEndsWith(url, ".iso") || urlEndsWith(url, ".img");

  // Do a quick HEAD request to get Content-Length before downloading
  {
    CURL *head = curl_easy_init();
    if (head) {
      curl_easy_setopt(head, CURLOPT_URL, url.c_str());
      curl_easy_setopt(head, CURLOPT_NOBODY, 1L);
      curl_easy_setopt(head, CURLOPT_FOLLOWLOCATION, 1L);
      curl_easy_setopt(head, CURLOPT_CONNECTTIMEOUT, 15L);
      curl_easy_setopt(head, CURLOPT_LOW_SPEED_LIMIT,
                       100L); // drop if below 100 bytes/sec
      curl_easy_setopt(head, CURLOPT_LOW_SPEED_TIME, 30L); // for 30 seconds
      if (!username.empty() || !password.empty()) {
        std::string auth = username + ":" + password;
        curl_easy_setopt(head, CURLOPT_USERPWD, auth.c_str());
      }
      if (curl_easy_perform(head) == CURLE_OK) {
        double cl = 0;
        curl_easy_getinfo(head, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &cl);
        if (cl > 0) {
          m_compressedSize = static_cast<uint64_t>(cl);
          m_estimatedSize = m_compressedSize;
        }
      }
      curl_easy_cleanup(head);
    }
  }

  if (pipe(m_pipefd) != 0) {
    m_error = "Failed to create UNIX pipe";
    return false;
  }

  m_curlAborted = false;
  m_curlThread = new std::thread([this, url, username, password]() {
    CURL *curl = curl_easy_init();
    if (curl) {
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      if (!username.empty() || !password.empty()) {
        std::string auth = username + ":" + password;
        curl_easy_setopt(curl, CURLOPT_USERPWD, auth.c_str());
      }
      curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
      curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
      curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT,
                       100L); // drop if below 100 bytes/sec
      curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L); // for 30 seconds
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCb);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
      curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
      curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, curlProgressCb);
      curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, this);
      curl_easy_perform(curl);
      curl_easy_cleanup(curl);
    }
    ::close(m_pipefd[1]);
    m_pipefd[1] = -1;
  });

  if (rawUrl) {
    // For raw ISO/IMG: read directly from the pipe, no libarchive
    m_isRaw = true;
    m_fd = m_pipefd[0]; // read() will use m_fd path
    m_pipefd[0] = -1;   // prevent close() from double-closing
    m_opened = true;
    return true;
  }

  // For compressed streams: use libarchive on the pipe
  m_archive = archive_read_new();
  if (!m_archive) {
    m_error = "Failed to create archive reader";
    return false;
  }
  archive_read_support_filter_all(m_archive);
  archive_read_support_format_raw(m_archive);
  archive_read_support_format_all(m_archive);

  if (archive_read_open_fd(m_archive, m_pipefd[0], 16384) != ARCHIVE_OK) {
    m_error = "Failed to open stream: " +
              std::string(archive_error_string(m_archive));
    archive_read_free(m_archive);
    m_archive = nullptr;
    return false;
  }

  struct archive_entry *entry;
  if (archive_read_next_header(m_archive, &entry) != ARCHIVE_OK) {
    m_error = "Failed to read header from stream: " +
              std::string(archive_error_string(m_archive));
    archive_read_free(m_archive);
    m_archive = nullptr;
    return false;
  }

  if (archive_entry_size_is_set(entry)) {
    m_estimatedSize = archive_entry_size(entry);
  }
  const char *fmt = archive_format_name(m_archive);
  m_isRaw = (fmt && strstr(fmt, "raw") != nullptr);

  m_opened = true;
  return true;
}

int ImageReader::read(void *buffer, int bufferSize) {
  if (!m_opened)
    return -1;

  if (m_fd >= 0) {
    int totalRead = 0;
    char *dest = static_cast<char *>(buffer);
    while (totalRead < bufferSize) {
      ssize_t n = ::read(m_fd, dest + totalRead, bufferSize - totalRead);
      if (n < 0) {
        if (errno == EINTR)
          continue; // Interrupted, try again
        m_error = "Read error: " + std::string(strerror(errno));
        return -1;
      }
      if (n == 0) {
        // EOF
        break;
      }
      totalRead += n;
    }
    m_bytesRead += totalRead;
    return totalRead;
  }

  if (m_archive) {
    ssize_t n = archive_read_data(m_archive, buffer, bufferSize);
    if (n > 0) {
      m_bytesRead += n;
      return (int)n;
    } else if (n == 0) {
      struct archive_entry *entry;
      if (archive_read_next_header(m_archive, &entry) == ARCHIVE_OK) {
        n = archive_read_data(m_archive, buffer, bufferSize);
        if (n > 0) {
          m_bytesRead += n;
          return (int)n;
        }
      }
      return 0; // EOF
    } else {
      m_error = "Read error: " + std::string(archive_error_string(m_archive));
      return -1;
    }
  }

  return -1;
}

void ImageReader::close() {
  m_curlAborted = true;

  if (m_fd >= 0) {
    ::close(m_fd);
    m_fd = -1;
  }
  if (m_archive) {
    archive_read_close(m_archive);
    archive_read_free(m_archive);
    m_archive = nullptr;
  }
  if (m_pipefd[0] >= 0) {
    ::close(m_pipefd[0]);
    m_pipefd[0] = -1;
  }

  if (m_curlThread) {
    if (m_curlThread->joinable()) {
      m_curlThread->join();
    }
    delete m_curlThread;
    m_curlThread = nullptr;
  }

  m_opened = false;
}

} // namespace flasher
