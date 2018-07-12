#pragma once

#include "EncryptedIO.h"

namespace credb
{
namespace trusted
{

class LocalEncryptedIO : public EncryptedIO
{
public:
    size_t num_files() override;
    
    size_t total_file_size() override;

    bool is_remote() const override
    {
        return false;
    }
    
    [[nodiscard]] bool read_from_disk(const std::string &filename, bitstream &data) override;
    
    [[nodiscard]] bool write_to_disk(const std::string &filename, const bitstream &data) override;
};

} // namespace trusted
} // namespace credb
