#include "os/filesystem/virtual_fs.h"
#include "core/log.h"
#include <algorithm>

namespace pas::os {

VirtualFilesystem::VirtualFilesystem(std::string root) : root_(std::move(root)) {}

void VirtualFilesystem::RegisterDevicePath(const std::string& guest_path) {
    device_paths_.push_back(guest_path);
}

bool VirtualFilesystem::IsDevicePath(const std::string& guest_path) const {
    return std::find(device_paths_.begin(), device_paths_.end(), guest_path)
           != device_paths_.end();
}

std::string VirtualFilesystem::ResolveRealPath(const std::string& guest_path) const {
    if (IsDevicePath(guest_path)) {
        PAS_LOG_WARN("VirtualFilesystem",
                     "ResolveRealPath llamado sobre una ruta de dispositivo (%s) -- "
                     "deberia resolverse en arcade/devices/, no aqui", guest_path.c_str());
    }
    return root_ + guest_path;
}

} // namespace pas::os
