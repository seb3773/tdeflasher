#include "diskscanner.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mntent.h>
#include <sstream>
#include <sys/mount.h>

namespace flasher {

// ---- Minimal JSON parser for lsblk output ----
// We only need to extract specific fields from lsblk --json --paths

// Skip whitespace
static const char *skipWs(const char *p) {
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
    p++;
  return p;
}

// Parse a JSON string value (after opening ")
static std::string parseString(const char *&p) {
  std::string result;
  p++; // skip opening "
  while (*p && *p != '"') {
    if (*p == '\\' && *(p + 1)) {
      p++;
      if (*p == '"')
        result += '"';
      else if (*p == '\\')
        result += '\\';
      else if (*p == 'n')
        result += '\n';
      else
        result += *p;
    } else {
      result += *p;
    }
    p++;
  }
  if (*p == '"')
    p++;
  return result;
}

// Parse a JSON value, return as string representation
static std::string parseValue(const char *&p) {
  p = skipWs(p);
  if (*p == '"') {
    return parseString(p);
  } else if (*p == 'n' && strncmp(p, "null", 4) == 0) {
    p += 4;
    return "";
  } else if (*p == 't' && strncmp(p, "true", 4) == 0) {
    p += 4;
    return "1";
  } else if (*p == 'f' && strncmp(p, "false", 5) == 0) {
    p += 5;
    return "0";
  } else if (*p == '[') {
    // Skip arrays (we handle children separately)
    int depth = 1;
    p++;
    std::string arr = "[";
    while (*p && depth > 0) {
      if (*p == '[')
        depth++;
      else if (*p == ']')
        depth--;
      if (depth > 0)
        arr += *p;
      p++;
    }
    return arr;
  } else if (*p == '{') {
    // Skip objects
    int depth = 1;
    p++;
    while (*p && depth > 0) {
      if (*p == '{')
        depth++;
      else if (*p == '}')
        depth--;
      p++;
    }
    return "{}";
  } else {
    // Number
    std::string num;
    while (*p && *p != ',' && *p != '}' && *p != ']' && *p != ' ' &&
           *p != '\n') {
      num += *p;
      p++;
    }
    return num;
  }
}

struct LsblkDevice {
  std::string name;
  std::string kname;
  std::string model;
  std::string vendor;
  std::string label;
  std::string tran;
  std::string subsystems;
  std::string mountpoint;
  std::string type; // "disk", "part", etc.
  uint64_t size;
  int phySec;
  bool rm;
  bool ro;
  bool hotplug;
  std::vector<LsblkDevice> children;
};

// Parse a single device object from JSON
static LsblkDevice parseDevice(const char *&p) {
  LsblkDevice dev;
  dev.size = 0;
  dev.phySec = 512;
  dev.rm = false;
  dev.ro = false;
  dev.hotplug = false;

  p = skipWs(p);
  if (*p != '{')
    return dev;
  p++;

  while (*p && *p != '}') {
    p = skipWs(p);
    if (*p == '}')
      break;
    if (*p == ',') {
      p++;
      continue;
    }

    // Parse key
    std::string key = parseString(p);
    p = skipWs(p);
    if (*p == ':')
      p++;
    p = skipWs(p);

    // Parse children specially
    if (key == "children" && *p == '[') {
      p++; // skip [
      while (*p && *p != ']') {
        p = skipWs(p);
        if (*p == ',') {
          p++;
          continue;
        }
        if (*p == ']')
          break;
        if (*p == '{') {
          dev.children.push_back(parseDevice(p));
        } else {
          p++;
        }
      }
      if (*p == ']')
        p++;
      continue;
    }

    std::string val = parseValue(p);

    if (key == "name")
      dev.name = val;
    else if (key == "kname")
      dev.kname = val;
    else if (key == "model")
      dev.model = val;
    else if (key == "vendor")
      dev.vendor = val;
    else if (key == "label")
      dev.label = val;
    else if (key == "tran")
      dev.tran = val;
    else if (key == "subsystems")
      dev.subsystems = val;
    else if (key == "mountpoint")
      dev.mountpoint = val;
    else if (key == "type")
      dev.type = val;
    else if (key == "size")
      dev.size = strtoull(val.c_str(), nullptr, 10);
    else if (key == "phy-sec")
      dev.phySec = atoi(val.c_str());
    else if (key == "rm")
      dev.rm = (val == "1" || val == "true");
    else if (key == "ro")
      dev.ro = (val == "1" || val == "true");
    else if (key == "hotplug")
      dev.hotplug = (val == "1" || val == "true");
  }
  if (*p == '}')
    p++;
  return dev;
}

static std::vector<LsblkDevice> parseLsblkJson(const std::string &json) {
  std::vector<LsblkDevice> devices;
  const char *p = json.c_str();

  // Find "blockdevices" array
  const char *found = strstr(p, "\"blockdevices\"");
  if (!found)
    return devices;
  p = found;
  // Skip to [
  while (*p && *p != '[')
    p++;
  if (*p != '[')
    return devices;
  p++;

  while (*p) {
    p = skipWs(p);
    if (*p == ']')
      break;
    if (*p == ',') {
      p++;
      continue;
    }
    if (*p == '{') {
      devices.push_back(parseDevice(p));
    } else {
      p++;
    }
  }
  return devices;
}

// ---- Main scan function ----

std::vector<DriveInfo> scanDrives() {
  std::vector<DriveInfo> drives;

  // Run lsblk with specific columns to ensure we get physical block SIZE and
  // not FSSIZE
  FILE *fp = popen("lsblk --bytes --all --json --paths --output "
                   "NAME,KNAME,MODEL,VENDOR,TRAN,SUBSYSTEMS,MOUNTPOINT,TYPE,"
                   "SIZE,PHY-SEC,RM,RO,HOTPLUG,LABEL "
                   "2>/dev/null",
                   "r");
  if (!fp)
    return drives;

  std::string output;
  char buf[4096];
  while (fgets(buf, sizeof(buf), fp)) {
    output += buf;
  }
  pclose(fp);

  std::vector<LsblkDevice> devices = parseLsblkJson(output);

  for (const auto &dev : devices) {
    // Filter (same rules as Etcher)
    if (dev.name.find("/dev/loop") == 0)
      continue;
    if (dev.name.find("/dev/sr") == 0)
      continue;
    if (dev.name.find("/dev/ram") == 0)
      continue;
    if (dev.name.find("/dev/zram") == 0)
      continue;
    if (dev.size == 0)
      continue;

    // Only consider whole disks, not partitions
    if (dev.type == "part")
      continue;

    DriveInfo info;
    info.device = dev.name;
    info.size = dev.size;
    info.blockSize = dev.phySec;
    info.readOnly = dev.ro;

    // Model / description
    info.model = dev.model;
    // Trim trailing spaces from model
    while (!info.model.empty() && info.model.back() == ' ')
      info.model.pop_back();

    // Build description (like Etcher)
    std::string desc;
    if (!dev.vendor.empty()) {
      desc += dev.vendor;
      desc += " ";
    }
    if (!info.model.empty())
      desc += info.model;
    // Add child labels/mountpoints
    for (const auto &child : dev.children) {
      if (!child.label.empty()) {
        desc += " (" + child.label + ")";
        break;
      }
      if (!child.mountpoint.empty()) {
        desc += " (" + child.mountpoint + ")";
        break;
      }
    }
    // Trim
    while (!desc.empty() && desc[0] == ' ')
      desc.erase(0, 1);
    if (desc.empty())
      desc = dev.name;
    info.description = desc;

    // Bus type
    if (!dev.tran.empty()) {
      info.busType = dev.tran;
      // Uppercase
      for (auto &c : info.busType)
        c = toupper(c);
    } else {
      info.busType = "UNKNOWN";
    }

    // USB detection
    info.isUSB = (dev.tran == "usb");

    // Virtual detection (subsystems == "block" only)
    info.isVirtual = (dev.subsystems == "block");

    // Removable (same logic as Etcher)
    info.removable = dev.rm || dev.hotplug || info.isVirtual;

    // System = not removable and not virtual
    info.isSystem = !info.removable && !info.isVirtual;

    // Mountpoints from children
    for (const auto &child : dev.children) {
      if (!child.mountpoint.empty()) {
        Mountpoint mp;
        mp.path = child.mountpoint;
        mp.label = child.label;
        info.mountpoints.push_back(mp);
      }
    }
    // Also check device itself
    if (!dev.mountpoint.empty()) {
      Mountpoint mp;
      mp.path = dev.mountpoint;
      mp.label = dev.label;
      info.mountpoints.push_back(mp);
    }

    drives.push_back(info);
  }

  return drives;
}

bool isDriveLargeEnough(const DriveInfo &drive, uint64_t imageSize) {
  return drive.size >= imageSize;
}

bool unmountDrive(const std::string &device) {
  FILE *mtab = setmntent("/proc/mounts", "r");
  if (!mtab)
    return false;

  std::vector<std::string> toUnmount;
  struct mntent *entry;
  while ((entry = getmntent(mtab)) != nullptr) {
    std::string mntDev = entry->mnt_fsname;
    if (mntDev.find(device) == 0) {
      toUnmount.push_back(entry->mnt_dir);
    }
  }
  endmntent(mtab);

  for (const auto &mp : toUnmount) {
    if (umount2(mp.c_str(), MNT_DETACH) != 0) {
      return false;
    }
  }
  return true;
}

} // namespace flasher
