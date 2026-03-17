#ifndef FLASHER_H
#define FLASHER_H

#include <cstdint>
#include <functional>
#include <string>

namespace flasher {

/**
 * Flash operation states.
 */
enum FlashState {
  STATE_IDLE,
  STATE_WRITING,
  STATE_VERIFYING,
  STATE_DONE,
  STATE_ERROR
};

/**
 * Progress information reported during flashing.
 */
struct FlashProgress {
  FlashState state;
  uint64_t bytesProcessed;
  uint64_t totalBytes;
  double speed;    // bytes per second
  int percentDone; // 0-100
  std::string errorMsg;
};

/**
 * Callback for flash progress updates.
 */
using FlashProgressCallback =
    std::function<void(const FlashProgress &progress)>;

/**
 * Orchestrate the full flash pipeline:
 *   open image → decompress → write to device → verify
 *
 * This function is BLOCKING — run it from a worker thread.
 *
 * @param imagePath   Path to the image file (may be compressed)
 * @param devicePath  Block device to write to (e.g. /dev/sdb)
 * @param verify      Whether to verify after writing
 * @param progressCb  Callback for progress updates (called from this thread)
 * @return true on success, false on error (check progress.errorMsg)
 */
bool flashImage(const std::string &imagePath, const std::string &devicePath,
                bool verify, volatile bool *cancelled,
                FlashProgressCallback progressCb,
                const std::string &urlUsername = "",
                const std::string &urlPassword = "");

} // namespace flasher

#endif
