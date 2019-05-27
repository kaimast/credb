/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#pragma once

#include "util/Mutex.h"
#include <bitstream.h>
#include <string>
#include <array>
#include <atomic>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

#ifdef IS_TEST
#include <gtest/gtest_prod.h>
#endif

#include <glog/logging.h>

class Disk
{
private:
    static constexpr size_t NUM_SHARDS = 64;

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
    };

    struct shard_t : public credb::Mutex
    {
        file_t *get_file(const std::string &disk_path, const std::string &filename);
        
        void flush(const std::string &disk_path, bool batch);

        size_t flush_pending_size = 0;
        std::unordered_set<std::string> flush_pending_list;

        std::unordered_map<std::string, file_t*> files;
    };

    shard_t& to_shard(const std::string &filename)
    {
        auto sid = std::hash<std::string>()(filename) % NUM_SHARDS;
        return m_shards[sid];
    }

public:
    Disk(const std::string &disk_path = "");
    ~Disk();

    bool write(const std::string &filename, uint8_t const* data, uint32_t length);

    int32_t get_size(const std::string &filename);

    size_t get_total_size() { return m_byte_size; }

    void remove(const std::string &filename);

    bool read(const std::string &filename, uint8_t *data, uint32_t buffer_size);

    bool read_undecrypted_from_disk(const std::string &full_name, bitstream &bstream);

    bool dump_everything(const std::string &filename);
    bool load_everything(const std::string &filename);

    void clear()
    {
        for(auto &shard : m_shards)
        {
            shard.lock();
            for(auto &it : shard.files)
            {
                delete it.second;
            }

            m_num_files -= shard.files.size();
            shard.files.clear();
            shard.unlock();
        }
    }

    size_t num_files() const { return m_num_files.load(); }

private:
#ifdef IS_TEST
    FRIEND_TEST(StringIndexTest, string_index_staleness_attack_children);
    FRIEND_TEST(StringIndexTest, string_index_staleness_attack_object);

    FRIEND_TEST(HashMapTest, staleness_attack_children);
    FRIEND_TEST(HashMapTest, staleness_attack_object);
#endif

    std::atomic<size_t> m_num_loads = 0;
    std::atomic<size_t> m_byte_size = 0;
    std::atomic<size_t> m_num_files = 0;

    /// Path to store data in 
    /// Should only be modified byconstructor
    std::string m_disk_path;

    std::array<shard_t, NUM_SHARDS> m_shards;
};

inline Disk::file_t* Disk::shard_t::get_file(const std::string &disk_path, const std::string &filename)
{
    auto it = this->files.find(filename);
    if(it != this->files.end())
    {
        return it->second;
    }

    if(disk_path.empty())
    {
        return nullptr;
    }

    auto fullpath = disk_path + filename;
    std::ifstream fin(fullpath, std::ios::binary | std::ios::ate);

    if(!fin.is_open())
    {
        LOG(FATAL) << "Failed to open file: " << fullpath;
    }

    auto file = new file_t;
    file->size = fin.tellg();
    if(!fin.seekg(0, std::ios::beg))
    {
        LOG(FATAL) << "Failed to seekg: " << fullpath;
    }

    file->data = new uint8_t[file->size];
    if(!fin.read(reinterpret_cast<char *>(file->data), file->size))
    {
        LOG(FATAL) << "Failed to read: " << fullpath;
    }

    files[filename] = file;
    return file;
}
