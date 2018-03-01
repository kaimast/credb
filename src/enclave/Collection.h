#pragma once

#include <unordered_map>
#include <unordered_set>
#include <mutex>

#include <bitstream.h>
#include "util/defines.h"
#include "HashMap.h"

namespace credb
{
namespace trusted
{

class Ledger;
class Enclave;
class Index;
class RemoteParties;
class StringIndex;
class BufferManager;

class Collection
{
public:
    Collection(BufferManager &buffer_manager, const std::string &name);

    Collection(Collection &&other) noexcept;

    ~Collection();

    void load_metadata(bitstream &input);
    void unload_everything();
    void dump_metadata(bitstream &output);

    bool create_index(const std::string &name, const std::vector<std::string> &paths, Enclave &enclave, Ledger &ledger);

    bool drop_index(const std::string &name);

#ifndef TEST
    void notify_triggers(RemoteParties &parties);
#endif

    void set_trigger(remote_party_id identifier);

    void unset_trigger(remote_party_id identifier);

    HashMap &primary_index() { return *m_primary_index; }

    std::unordered_map<std::string, Index *> secondary_indexes() { return m_secondary_indexes; }

private:
    BufferManager &m_buffer_manager;
    const std::string m_name;
//    StringIndex *m_primary_index;
    HashMap *m_primary_index;
    std::unordered_map<std::string, Index *> m_secondary_indexes;
    std::unordered_set<remote_party_id> m_triggers;
    std::mutex m_mutex;
};

} // namespace trusted
} // namespace credb
