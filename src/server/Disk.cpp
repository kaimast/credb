/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include <fstream>
#include <cstring>

#include "Disk.h"
#include "EnclaveHandle.h"

#ifdef FAKE_ENCLAVE
#include "FakeEnclave.h"
#else
#include "Enclave_u.h"
#endif

#include <glog/logging.h>

/// needed for C enclave bindings
Disk *g_disk = nullptr;

Disk::Disk(std::string disk_path)
    : m_disk_path(std::move(disk_path))
{
    if(!m_disk_path.empty() && m_disk_path.back() != '/')
    {
        m_disk_path += '/';
    }

    g_disk = this;
}

Disk::~Disk()
{
    for(auto &shard : m_shards)
    {
        shard.flush(m_disk_path, false);

        for(auto it : shard.files)
        {
            delete it.second;
        }
    }
}

void Disk::remove(const std::string &filename)
{
    auto &shard = to_shard(filename);

    shard.lock();
    auto it = shard.files.find(filename);

    if(it != shard.files.end())
    {
        auto f = it->second;
        shard.files.erase(it);
        delete f;

        shard.flush(m_disk_path, false);
        auto fullpath = m_disk_path + filename;

        if(std::remove(fullpath.c_str()) == 0)
        {
            LOG(FATAL) << "Failed to remove file: " << fullpath;
        }
    }

    shard.unlock();
}

bool Disk::write(const std::string &filename, uint8_t const* data, uint32_t length)
{
    if(filename.find('/') != std::string::npos)
    {
        throw std::runtime_error("Enclave is not allowed to create folders");
    }

    auto new_data = new uint8_t[length];
    memcpy(new_data, data, length);

    auto &shard = to_shard(filename);

    shard.lock();
    file_t *file = nullptr;

    auto it = shard.files.find(filename);

    if(it == shard.files.end())
    {
        file = new file_t;
        shard.files[filename] = file;
        m_num_files++;

#ifndef IS_TEST
        if(num_files() % 1000 == 0)
        {
            LOG(INFO) << num_files() << " files so far";
        }
#endif
    }
    else
    {
        // DLOG(INFO) << "Updating file " << filename;
        file = it->second;

        m_byte_size -= file->size;
        file->clear();
    }

    file->data = new_data;
    file->size = length;

    m_byte_size += length;

    shard.flush_pending_list.insert(filename);
    shard.flush_pending_size += length;
    shard.flush(m_disk_path,true);

    shard.unlock();

    return true;
}

int32_t Disk::get_size(const std::string &filename)
{
    if(filename.find('/') != std::string::npos)
    {
        LOG(FATAL) << "Enclave is not allowed to create folders";
    }

    int32_t result = -1;

    auto &shard = to_shard(filename);

    shard.lock();
    
    auto f = shard.get_file(m_disk_path, filename);

    if(f != nullptr)
    {
        result = f->size;
    }

    shard.unlock();
    return result;
}

bool Disk::read(const std::string &filename, uint8_t *data, uint32_t buffer_size)
{
    if(filename.find('/') != std::string::npos)
    {
        LOG(FATAL) << "Enclave is not allowed to create folders";
    }

    auto &shard = to_shard(filename);

    shard.lock();
    auto file = shard.get_file(m_disk_path, filename);

    if(!file)
    {
        LOG(FATAL) << "No such file: " << filename;
    }

    if(file->size != buffer_size)
    {
        LOG(FATAL) << "File sizes don't match. filename: " << filename
                   << " file->size: " << file->size << " buffer_size: " << buffer_size;
    }

    memcpy(data, file->data, file->size);

    auto num_loads = ++m_num_loads;

#ifndef IS_TEST
    if(num_loads % 10000 == 0)
    {
        LOG(INFO) << num_loads << " loads so far.";
    }
#else
    (void)num_loads;
#endif

    shard.unlock();
    return true;
}

bool Disk::read_undecrypted_from_disk(const std::string &full_name, bitstream &bstream)
{
    auto &shard = to_shard(full_name);
    shard.lock();

    file_t *file = nullptr;
    auto it = shard.files.find(full_name);
    if(it != shard.files.end())
    {
        file = it->second;
    }
    m_num_loads += 1;
#ifndef IS_TEST
    if(m_num_loads % 10000 == 0)
    {
        LOG(INFO) << m_num_loads << " loads so far.";
    }
#endif
    
    if(file == nullptr)
    {
        LOG(ERROR) << "No such file: " << full_name;
        return false;
    }

    bstream.resize(file->size);
    memcpy(bstream.data(), file->data, file->size);

    shard.unlock();
    return true;
}

#define ENSURE_IO(x)                          \
    do                                        \
    {                                         \
        if(!static_cast<bool>(x))             \
        {                                     \
            LOG(ERROR) << "bad fstream: " #x; \
            return 0;                         \
        }                                     \
    } while(0)

bool Disk::dump_everything(const std::string &filename)
{
    for(auto &shard: m_shards)
    {
        std::lock_guard<credb::Mutex> lock(shard);

        std::ofstream fout(filename, std::ios::binary);
        if(!fout.is_open())
        {
            LOG(INFO) << "Failed to open file: " << filename;
            return false;
        }

        uint32_t num_files = shard.files.size();
        ENSURE_IO(fout.write(reinterpret_cast<const char *>(&num_files), sizeof(num_files)));

        size_t cum_size = 0, last_report_size = 0, cnt_files = 0;
        for(auto it : shard.files)
        {
            uint32_t len_filename = it.first.size();
            ENSURE_IO(fout.write(reinterpret_cast<const char *>(&len_filename), sizeof(len_filename)));
            ENSURE_IO(fout.write(it.first.c_str(), len_filename));
            ENSURE_IO(fout.write(reinterpret_cast<const char *>(&it.second->size), sizeof(it.second->size)));
            ENSURE_IO(fout.write(reinterpret_cast<const char *>(it.second->data), it.second->size));

            cum_size += it.second->size;
            ++cnt_files;
            if(cum_size - last_report_size > (100 << 20) || cnt_files == num_files)
            {
                LOG(INFO) << "Dumped " << cnt_files << "/" << num_files << " files "
                          << (cum_size / 1024.0 / 1024.0) << "MBytes";
                last_report_size = cum_size;
            }
        }

        LOG(INFO) << "Every file is dumped";

        shard.unlock();
    }

    return true;
}

bool Disk::load_everything(const std::string &filename)
{
    for(auto &s: m_shards)
    {
        s.lock();
    
        if(!s.files.empty())
        {
            LOG(INFO) << "There are already files in the disk cache. Use at your own risk!";
        }
    }

    bool success = false;
    
    std::ifstream fin(filename, std::ios::binary);
    if(!fin.is_open())
    {
        LOG(INFO) << "Failed to open file: " << filename;
    }
    else
    {
        uint32_t num_files = 0;
        ENSURE_IO(fin.read(reinterpret_cast<char *>(&num_files), sizeof(num_files)));

        std::vector<char> buf_filename;
        size_t cum_size = 0, last_report_size = 0;
        for(size_t cnt_files = 1; cnt_files <= num_files; ++cnt_files)
        {
            uint32_t len_filename = 0;
            ENSURE_IO(fin.read(reinterpret_cast<char *>(&len_filename), sizeof(len_filename)));

            buf_filename.clear();
            buf_filename.resize(len_filename);
            ENSURE_IO(fin.read(buf_filename.data(), len_filename));
            std::string filename(buf_filename.data(), len_filename);

            auto &shard = to_shard(filename);

            auto it = shard.files.find(filename);
            if(it != shard.files.end())
            {
                LOG(INFO) << "Replacing file: " << filename;
                delete it->second;
                shard.files.erase(it);
            }

            auto file = new file_t();
            m_num_files++;

            ENSURE_IO(fin.read(reinterpret_cast<char *>(&file->size), sizeof(file->size)));
            file->data = new uint8_t[file->size];
            ENSURE_IO(fin.read(reinterpret_cast<char *>(file->data), file->size));
            shard.files[filename] = file;

            cum_size += file->size;
            if(cum_size - last_report_size > (100 << 20) || cnt_files == num_files)
            {
                LOG(INFO) << "Loaded " << cnt_files << "/" << num_files << " files "
                          << (cum_size / 1024.0 / 1024.0) << "MBytes";
                last_report_size = cum_size;
            }
        }

        LOG(INFO) << "All files has been loaded";
        success = true;
    }

    for(auto &s: m_shards)
    {
        s.unlock();
    }

    return success;
}

void Disk::shard_t::flush(const std::string &disk_path, bool batch)
{
    constexpr size_t FLUSH_BATCH = 1 << 20;

    if(batch && this->flush_pending_size < FLUSH_BATCH)
    {
        return;
    }

    if(!disk_path.empty())
    {
        for(auto &filename : this->flush_pending_list)
        {
            auto file = this->files[filename];
            auto fullpath = disk_path + filename;

            std::ofstream fout(fullpath, std::ios::binary);
            if(!fout.is_open())
            {
                LOG(FATAL) << "Failed to open file: " << fullpath;
            }

            if(!fout.write(reinterpret_cast<const char *>(file->data), file->size))
            {
                LOG(FATAL) << "Failed to write to file: " << fullpath;
            }
        }
    }

    this->flush_pending_size = 0;
    this->flush_pending_list.clear();
}
void remove_from_disk(const char *filename) { g_disk->remove(filename); }

bool write_to_disk(const char *filename, const uint8_t *data, uint32_t length)
{
    return g_disk->write(filename, data, length);
}

size_t get_total_file_size()
{
   return g_disk->get_total_size();
}

size_t get_num_files()
{
    return g_disk->num_files();
}

int32_t get_file_size(const char *filename) { return g_disk->get_size(filename); }

bool read_from_disk(const char *filename, uint8_t *data, uint32_t length)
{
    return g_disk->read(filename, data, length);
}

bool dump_everything(const char *filename, uint8_t const* disk_key, size_t length)
{
    if(!g_disk->write("___disk_key", disk_key, length))
    {
        return false;
    }
    
    if(!g_disk->dump_everything(filename))
    {
        return false;
    }

    g_disk->remove("___disk_key");
    return true;
}

bool load_everything(const char *filename, uint8_t* disk_key, size_t length)
{
#ifdef IS_TEST
    (void)disk_key;
    (void)length;
    (void)filename;

    memset(disk_key, 0, length); //clang-tidy
#else
    if(length != sizeof(sgx_aes_gcm_128bit_key_t))
    {
        LOG(ERROR) << "len != sizeof(sgx_aes_gcm_128bit_key_t)";
        return false;
    }

    if(!g_disk->load_everything(filename))
    {
        return false;
    }

    if(!g_disk->read("___disk_key", disk_key, length))
    {
        return false;
    }

    g_disk->remove("___disk_key");
#endif
    return true;
}
