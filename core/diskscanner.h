#ifndef DISKSCANNER_H
#define DISKSCANNER_H

#include <cstdint>
#include <string>
#include <vector>

namespace flasher {

struct Mountpoint {
  std::string path;
  std::string label;
};

struct DriveInfo {
  std::string device;      // e.g. "/dev/sdb"
  std::string model;       // e.g. "Kingston DataTraveler"
  std::string description; // human-friendly label
  std::string busType;     // USB, SATA, NVME, UNKNOWN...
  uint64_t size;           // bytes
  int blockSize;           // physical sector size
  bool removable;
  bool readOnly;
  bool isSystem; // system disk (not removable, not virtual)
  bool isUSB;
  bool isVirtual; // subsystem == "block" only
  std::vector<Mountpoint> mountpoints;
};

/**
 * Scan drives using lsblk (same method as Etcher).
 * Returns ALL valid drives (excludes loop, sr, ram, zram).
 */
std::vector<DriveInfo> scanDrives();

/**
 * Check if drive is large enough for the given image size.
 */
bool isDriveLargeEnough(const DriveInfo &drive, uint64_t imageSize);

/**
 * Unmount all partitions of a device.
 * Returns true on success.
 */
bool unmountDrive(const std::string &device);

} // namespace flasher

#endif
