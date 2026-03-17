#include "flasher.h"
#include "blockwriter.h"
#include "diskscanner.h"
#include "imagereader.h"
#include "verifier.h"
#include <cstdlib>
#include <cstring>
#include <sys/time.h>

namespace flasher {

static const int FLASH_BUF_SIZE = 1024 * 1024; // 1 MB chunks

static double getTime() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

bool flashImage(const std::string &imagePath, const std::string &devicePath,
                bool verify, volatile bool *cancelled,
                FlashProgressCallback progressCb,
                const std::string &urlUsername,
                const std::string &urlPassword) {

  FlashProgress progress;
  progress.state = STATE_IDLE;
  progress.bytesProcessed = 0;
  progress.totalBytes = 0;
  progress.speed = 0;
  progress.percentDone = 0;

  auto reportError = [&](const std::string &msg) {
    progress.state = STATE_ERROR;
    progress.errorMsg = msg;
    if (progressCb)
      progressCb(progress);
    return false;
  };

  // 1. Unmount the target device
  if (!unmountDrive(devicePath)) {
    // Non-fatal: device might not be mounted
  }

  // 2. Open the image
  ImageReader reader;
  bool isUrl = (imagePath.rfind("http://", 0) == 0 ||
                imagePath.rfind("https://", 0) == 0);

  bool openSuccess = false;
  if (isUrl) {
    openSuccess = reader.openUrl(imagePath, urlUsername, urlPassword);
  } else {
    openSuccess = reader.open(imagePath);
  }

  if (!openSuccess) {
    return reportError("Cannot open image: " + reader.errorString());
  }
  progress.totalBytes = reader.estimatedSize();

  // 3. Open the block device for writing
  BlockWriter writer;
  if (!writer.open(devicePath)) {
    return reportError("Cannot open device: " + writer.errorString());
  }

  // 4. Initialize the verifier (hash source data during write)
  Verifier sourceHash;
  if (verify) {
    sourceHash.begin();
  }

  // 5. Write loop
  progress.state = STATE_WRITING;
  if (progressCb)
    progressCb(progress);

  void *buf = nullptr;
  if (posix_memalign(&buf, 512, FLASH_BUF_SIZE) != 0)
    return reportError("Out of memory");

  double startTime = getTime();
  double lastReportTime = startTime;

  bool firstRead = true;
  while (true) {
    int n = reader.read(buf, FLASH_BUF_SIZE);
    if (n == 0) {
      if (firstRead && progress.totalBytes > 0) {
        free(buf);
        return reportError(
            "Read error: Image appears to be empty or unreadable.");
      }
      break; // EOF
    }
    firstRead = false;
    if (n < 0) {
      free(buf);
      return reportError("Read error: " + reader.errorString());
    }

    // Write to device
    int written = writer.write(buf, n);
    if (written < 0) {
      free(buf);
      return reportError("Write error: " + writer.errorString());
    }

    if (cancelled && *cancelled) {
      free(buf);
      return reportError("Cancelled by user");
    }

    // Update source hash
    if (verify) {
      sourceHash.update(buf, n);
    }

    // Report progress (throttle to ~10 updates/sec)
    double now = getTime();
    if (now - lastReportTime >= 0.1) {
      progress.bytesProcessed = writer.bytesWritten();
      // Dynamically update totalBytes from reader (URL downloads
      // learn the size from HTTP Content-Length after headers arrive)
      uint64_t currentEstimate = reader.estimatedSize();
      if (currentEstimate > 0)
        progress.totalBytes = currentEstimate;
      double elapsed = now - startTime;
      progress.speed =
          (elapsed > 0) ? (double)progress.bytesProcessed / elapsed : 0;
      if (progress.totalBytes > 0) {
        progress.percentDone =
            (int)(progress.bytesProcessed * 100 / progress.totalBytes);
      }
      if (progressCb)
        progressCb(progress);
      lastReportTime = now;
    }
  }

  free(buf);

  // Sync
  writer.sync();

  uint64_t totalWritten = writer.bytesWritten();
  writer.close();
  reader.close();

  // Final write progress
  progress.bytesProcessed = totalWritten;
  progress.percentDone = 100;
  double elapsed = getTime() - startTime;
  progress.speed = (elapsed > 0) ? (double)totalWritten / elapsed : 0;
  if (progressCb)
    progressCb(progress);

  // 6. Verification pass
  if (verify) {
    std::string expectedHash = sourceHash.finalize();

    progress.state = STATE_VERIFYING;
    progress.bytesProcessed = 0;
    progress.percentDone = 0;
    progress.totalBytes = totalWritten;
    if (progressCb)
      progressCb(progress);

    Verifier deviceVerifier;
    double verifyStart = getTime();

    bool ok = deviceVerifier.verifyDevice(
        devicePath, totalWritten, expectedHash, cancelled,
        [&](uint64_t bytesVerified, uint64_t total) {
          if (cancelled && *cancelled)
            return; // Verifier doesn't have a direct cancel, but we skip
                    // updates
          double now = getTime();
          if (now - lastReportTime >= 0.1) {
            progress.bytesProcessed = bytesVerified;
            double vel = now - verifyStart;
            progress.speed = (vel > 0) ? (double)bytesVerified / vel : 0;
            progress.percentDone =
                (total > 0) ? (int)(bytesVerified * 100 / total) : 0;
            if (progressCb)
              progressCb(progress);
            lastReportTime = now;
          }
        });

    if (!ok) {
      return reportError("Verification failed: " +
                         deviceVerifier.errorString());
    }
  }

  // 7. Done!
  progress.state = STATE_DONE;
  progress.percentDone = 100;
  progress.bytesProcessed = totalWritten;
  if (progressCb)
    progressCb(progress);

  return true;
}

} // namespace flasher
