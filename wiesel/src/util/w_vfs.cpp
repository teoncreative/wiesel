//
// Created by Metehan Gezer on 12.02.2026.
//

#include "util/w_vfs.hpp"

namespace Wiesel {

VfsFile::VfsFile(std::vector<uint8_t> data, std::string path)
    : data_(std::move(data)), path_(std::move(path)) {}

size_t VfsFile::Read(void* buffer, size_t size) {
    size_t available = data_.size() - position_;
    size_t to_read = std::min(size, available);
    std::memcpy(buffer, data_.data() + position_, to_read);
    position_ += to_read;
    return to_read;
}

std::vector<uint8_t> VfsFile::ReadBytes(size_t count) {
    size_t available = data_.size() - position_;
    size_t to_read = std::min(count, available);
    std::vector<uint8_t> result(data_.begin() + position_, data_.begin() + position_ + to_read);
    position_ += to_read;
    return result;
}

void VfsFile::Seek(size_t position) {
    position_ = std::min(position, data_.size());
}

void VfsFile::SeekRelative(int64_t offset) {
    int64_t new_pos = static_cast<int64_t>(position_) + offset;
    if (new_pos < 0) new_pos = 0;
    position_ = std::min(static_cast<size_t>(new_pos), data_.size());
}

size_t VfsFile::Tell() const {
    return position_;
}

size_t VfsFile::Size() const {
    return data_.size();
}

const std::string& VfsFile::Path() const {
    return path_;
}

bool VfsFile::IsEof() const {
    return position_ >= data_.size();
}

const uint8_t* VfsFile::Data() const {
    return data_.data();
}

std::istream& VfsFile::Stream() {
    if (!stream_) {
        membuf_ = std::make_unique<MemBuf>(data_.data(), data_.size());
        stream_ = std::make_unique<std::istream>(membuf_.get());
    }
    return *stream_;
}

std::vector<char> VfsFile::AsChars() const {
    return std::vector<char>(data_.begin(), data_.end());
}

std::vector<uint32_t> VfsFile::AsUint32() const {
    size_t count = data_.size() / sizeof(uint32_t);
    std::vector<uint32_t> result(count);
    std::memcpy(result.data(), data_.data(), count * sizeof(uint32_t));
    return result;
}

void VirtualFileSystem::Mount(const std::string& mount_point, const std::string& physical_path, int priority) {
    MountPoint mp;
    mp.mount_point = NormalizePath(mount_point);
    mp.physical_path = std::filesystem::absolute(physical_path);
    mp.priority = priority;
    mp.is_archive = false;

    if (!std::filesystem::exists(mp.physical_path)) {
        throw std::runtime_error("Physical path does not exist: " + physical_path);
    }

    mount_points_.push_back(mp);
    std::sort(mount_points_.begin(), mount_points_.end());
}

void VirtualFileSystem::MountArchive(const std::string& mount_point, const std::string& archive_path) {
    std::filesystem::path abs_archive_path = std::filesystem::absolute(archive_path);

    if (!std::filesystem::exists(abs_archive_path)) {
        throw std::runtime_error("Archive does not exist: " + archive_path);
    }

    Archive archive;
    archive.path = abs_archive_path;

    if (!LoadArchive(abs_archive_path, archive)) {
        throw std::runtime_error("Failed to load archive: " + archive_path);
    }

    std::string normalized_mount = NormalizePath(mount_point);
    archives_[normalized_mount] = archive;

    MountPoint mp;
    mp.mount_point = normalized_mount;
    mp.physical_path = abs_archive_path;
    mp.priority = 0;
    mp.is_archive = true;

    mount_points_.push_back(mp);
    std::sort(mount_points_.begin(), mount_points_.end());
}

VfsFile VirtualFileSystem::Open(const std::string& virtual_path) {
    std::string normalized = NormalizePath(virtual_path);

    for (const auto& mp : mount_points_) {
        if (normalized.find(mp.mount_point) != 0) {
            continue;
        }

        std::string relative_path = normalized.substr(mp.mount_point.length());
        if (!relative_path.empty() && relative_path[0] == '/') {
            relative_path = relative_path.substr(1);
        }

        if (mp.is_archive) {
            auto archive_it = archives_.find(mp.mount_point);
            if (archive_it != archives_.end()) {
                const Archive& archive = archive_it->second;
                auto entry_it = archive.entries.find(relative_path);

                if (entry_it != archive.entries.end()) {
                    auto data = ReadFromArchive(archive, entry_it->second);
                    return VfsFile(std::move(data), virtual_path);
                }
            }
        } else {
            std::filesystem::path full_path = mp.physical_path / relative_path;

            if (std::filesystem::exists(full_path) && std::filesystem::is_regular_file(full_path)) {
                std::ifstream file(full_path, std::ios::binary | std::ios::ate);
                if (!file.is_open()) {
                    continue;
                }

                size_t size = file.tellg();
                file.seekg(0, std::ios::beg);

                std::vector<uint8_t> buffer(size);
                file.read(reinterpret_cast<char*>(buffer.data()), size);

                return VfsFile(std::move(buffer), virtual_path);
            }
        }
    }

    throw std::runtime_error("File not found: " + virtual_path);
}

bool VirtualFileSystem::FileExists(const std::string& virtual_path) {
    std::string normalized = NormalizePath(virtual_path);

    for (const auto& mp : mount_points_) {
        if (normalized.find(mp.mount_point) != 0) {
            continue;
        }

        std::string relative_path = normalized.substr(mp.mount_point.length());
        if (!relative_path.empty() && relative_path[0] == '/') {
            relative_path = relative_path.substr(1);
        }

        if (mp.is_archive) {
            auto archive_it = archives_.find(mp.mount_point);
            if (archive_it != archives_.end()) {
                const Archive& archive = archive_it->second;
                if (archive.entries.find(relative_path) != archive.entries.end()) {
                    return true;
                }
            }
        } else {
            std::filesystem::path full_path = mp.physical_path / relative_path;
            if (std::filesystem::exists(full_path)) {
                return true;
            }
        }
    }

    return false;
}

std::vector<std::string> VirtualFileSystem::ListFiles(const std::string& virtual_dir, bool recursive) {
    std::string normalized = NormalizePath(virtual_dir);
    std::vector<std::string> results;

    for (const auto& mp : mount_points_) {
        if (normalized.find(mp.mount_point) != 0) {
            continue;
        }

        std::string relative_path = normalized.substr(mp.mount_point.length());
        if (!relative_path.empty() && relative_path[0] == '/') {
            relative_path = relative_path.substr(1);
        }

        if (mp.is_archive) {
            auto archive_it = archives_.find(mp.mount_point);
            if (archive_it != archives_.end()) {
                const Archive& archive = archive_it->second;

                for (const auto& [name, entry] : archive.entries) {
                    if (relative_path.empty() || name.find(relative_path) == 0) {
                        std::string full_virtual_path = mp.mount_point + "/" + name;
                        results.push_back(full_virtual_path);
                    }
                }
            }
        } else {
            std::filesystem::path full_path = mp.physical_path / relative_path;

            if (std::filesystem::exists(full_path) && std::filesystem::is_directory(full_path)) {
                if (recursive) {
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(full_path)) {
                        if (entry.is_regular_file()) {
                            std::filesystem::path rel = std::filesystem::relative(entry.path(), mp.physical_path);
                            std::string virtual_path = mp.mount_point + "/" + rel.generic_string();
                            results.push_back(virtual_path);
                        }
                    }
                } else {
                    for (const auto& entry : std::filesystem::directory_iterator(full_path)) {
                        if (entry.is_regular_file()) {
                            std::filesystem::path rel = std::filesystem::relative(entry.path(), mp.physical_path);
                            std::string virtual_path = mp.mount_point + "/" + rel.generic_string();
                            results.push_back(virtual_path);
                        }
                    }
                }
            }
        }
    }

    // Remove duplicates (same file from different mount points)
    std::sort(results.begin(), results.end());
    results.erase(std::unique(results.begin(), results.end()), results.end());

    return results;
}

std::optional<std::filesystem::path> VirtualFileSystem::GetPhysicalPath(const std::string& virtual_path) {
    std::string normalized = NormalizePath(virtual_path);

    for (const auto& mp : mount_points_) {
        if (normalized.find(mp.mount_point) != 0) {
            continue;
        }

        std::string relative_path = normalized.substr(mp.mount_point.length());
        if (!relative_path.empty() && relative_path[0] == '/') {
            relative_path = relative_path.substr(1);
        }

        if (mp.is_archive) {
            continue;
        }

        std::filesystem::path full_path = mp.physical_path / relative_path;
        if (std::filesystem::exists(full_path)) {
            return full_path;
        }
    }

    return std::nullopt;
}

void VirtualFileSystem::Unmount(const std::string& mount_point) {
    std::string normalized = NormalizePath(mount_point);

    mount_points_.erase(
        std::remove_if(mount_points_.begin(), mount_points_.end(),
            [&normalized](const MountPoint& mp) {
                return mp.mount_point == normalized;
            }),
        mount_points_.end()
    );

    archives_.erase(normalized);
}

void VirtualFileSystem::Clear() {
    mount_points_.clear();
    archives_.clear();
}

std::string VirtualFileSystem::NormalizePath(const std::string& path) {
    std::string normalized = path;

    // Replace backslashes with forward slashes
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    // Remove trailing slash
    if (!normalized.empty() && normalized.back() == '/') {
        normalized.pop_back();
    }

    // Ensure it starts with /
    if (normalized.empty() || normalized[0] != '/') {
        normalized = "/" + normalized;
    }

    return normalized;
}

bool VirtualFileSystem::LoadArchive(const std::filesystem::path& archive_path, Archive& archive) {
    std::ifstream file(archive_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Read header
    char magic[4];
    file.read(magic, 4);

    if (std::strncmp(magic, "WPAK", 4) != 0) {
        return false;
    }

    uint32_t version;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));

    if (version != 1) {
        return false;
    }

    // Read number of entries
    uint32_t entry_count;
    file.read(reinterpret_cast<char*>(&entry_count), sizeof(entry_count));

    // Read entries
    for (uint32_t i = 0; i < entry_count; i++) {
        ArchiveEntry entry;

        // Read name length
        uint32_t name_length;
        file.read(reinterpret_cast<char*>(&name_length), sizeof(name_length));

        // Read name
        std::string name(name_length, '\0');
        file.read(&name[0], name_length);
        entry.name = name;

        // Read metadata
        file.read(reinterpret_cast<char*>(&entry.offset), sizeof(entry.offset));
        file.read(reinterpret_cast<char*>(&entry.size), sizeof(entry.size));
        file.read(reinterpret_cast<char*>(&entry.compressed_size), sizeof(entry.compressed_size));

        uint8_t compressed;
        file.read(reinterpret_cast<char*>(&compressed), sizeof(compressed));
        entry.compressed = (compressed != 0);

        archive.entries[entry.name] = entry;
    }

    return true;
}

std::vector<uint8_t> VirtualFileSystem::ReadFromArchive(const Archive& archive, const ArchiveEntry& entry) {
    std::ifstream file(archive.path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open archive: " + archive.path.string());
    }

    file.seekg(entry.offset);

    if (entry.compressed) {
        std::vector<uint8_t> compressed_data(entry.compressed_size);
        file.read(reinterpret_cast<char*>(compressed_data.data()), entry.compressed_size);

        // TODO: Decompress
        throw std::runtime_error("Compressed archives not yet implemented");
    } else {
        std::vector<uint8_t> data(entry.size);
        file.read(reinterpret_cast<char*>(data.data()), entry.size);
        return data;
    }
}

} // namespace Wiesel
