#pragma once

#include "util/Mutex.h"
#include <bitstream.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

#ifdef TEST
#include <gtest/gtest_prod.h>
#endif

class Disk
{
public:
    Disk() = default;
    ~Disk();

    void set_disk_path(const std::string &path);

    bool write(const std::string &filename, uint8_t const* data, uint32_t length);

    int32_t get_size(const std::string &filename);

    void remove(const std::string &filename);

    bool read(const std::string &filename, uint8_t *data, uint32_t buffer_size);

    bool read_undecrypted_from_disk(const std::string &full_name, bitstream &bstream);

    bool dump_everything(const std::string &filename);
    bool load_everything(const std::string &filename);

    void clear()
    {
        for(auto &it : m_files)
            delete it.second;
        m_files.clear();
    }

    uint32_t num_files() const { return m_files.size(); }

private:
    struct file_t
    {
        ~file_t() { clear(); }

        void clear()
        {
            delete[] data;
            data = nullptr;
        }

        uint8_t *data = nullptr;
        size_t size = 0;

        credb::Mutex mutex;
    };

#ifdef TEST
    FRIEND_TEST(StringIndexTest, string_index_staleness_attack_children);
    FRIEND_TEST(StringIndexTest, string_index_staleness_attack_object);
#endif

    void flush(bool batch);
    file_t *get_file(const std::string &filename);

    std::unordered_map<std::string, file_t *> m_files;
    size_t m_flush_pending_size = 0;
    std::unordered_set<std::string> m_flush_pending_list;
    std::string m_disk_path;
    credb::Mutex m_map_lock;

    size_t m_num_loads = 0;
    size_t m_byte_size = 0;
};

extern Disk g_disk;
