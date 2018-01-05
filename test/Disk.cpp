#include <gtest/gtest.h>
#include <experimental/filesystem>
#include "credb/defines.h"
#include "../src/server/Disk.h"

namespace fs = std::experimental::filesystem;

TEST(DiskTest, persistency)
{
    const auto content_size = 4096;
    const auto temp_dir_name = "credb-unit-test-temp-" + credb::random_object_key(8);
    const auto root = fs::temp_directory_path() / temp_dir_name;
    fs::create_directories(root);

    auto disk = new Disk;
    disk->set_disk_path(root.string());
    std::unordered_map<std::string, std::string> data;
    for (int i = 0; i < 10000; ++i)
    {
        auto filename = credb::random_object_key(32);
        auto content = credb::random_object_key(content_size);
        ASSERT_TRUE(disk->write(filename, reinterpret_cast<const uint8_t*>(content.c_str()), content.size()));
        data.emplace(filename, content);
    }
    delete disk;

    disk = new Disk;
    disk->set_disk_path(root.string());
    uint8_t buf[content_size];
    for (const auto &[filename, content] : data)
    {
        ASSERT_TRUE(disk->read(filename, buf, content_size));
        ASSERT_TRUE(memcmp(buf, content.c_str(), content_size) == 0);
    }
    delete disk;

    fs::remove_all(root);
}
