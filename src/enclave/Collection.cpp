/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "Collection.h"
#include "BufferManager.h"
#include "Enclave.h"
#include "Index.h"
#include "HashMap.h"
#include "Ledger.h"
#include "RemoteParties.h"
#include "logging.h"

namespace credb::trusted
{

Collection::Collection(BufferManager &buffer_manager, std::string name)
: m_buffer_manager(buffer_manager), m_name(std::move(name))
{
    m_primary_index = new HashMap(m_buffer_manager, m_name + "_primary_index");
}

Collection::Collection(Collection &&other) noexcept
: m_buffer_manager(other.m_buffer_manager), m_name(other.m_name),
  m_primary_index(other.m_primary_index), m_secondary_indexes(std::move(other.m_secondary_indexes))
{
    other.m_primary_index = nullptr;
}

Collection::~Collection()
{
    delete m_primary_index;
    m_primary_index = nullptr;

    for(auto it : m_secondary_indexes)
    {
        delete it.second;
    }

    m_secondary_indexes.clear();
}

bool Collection::drop_index(const std::string &name)
{
    auto it = m_secondary_indexes.find(name);

    if(it == m_secondary_indexes.end())
    {
        return false;
    }

    // FIXME not thread safe!
    it->second->clear();
    delete it->second;
    m_secondary_indexes.erase(it);
    return true;
}

bool Collection::create_index(const std::string &name, const std::vector<std::string> &paths, Enclave &enclave, Ledger &ledger)
{
    if(m_secondary_indexes.find(name) != m_secondary_indexes.end())
    {
        log_debug("index " + name + " already exists. ignore");
        return false;
    }

    Index *index = new HashIndex(m_buffer_manager, name, paths);
    m_secondary_indexes.insert({ name, index });

    std::unique_ptr<ObjectKeyProvider> key_provider(new HashMap::LinearScanKeyProvider(*m_primary_index));

    OpContext context(enclave.identity());

    ObjectListIterator oit(context, m_name, json::Document(""), ledger, nullptr, std::move(key_provider));

    std::string key;
    ObjectEventHandle event;
    while(oit.next(key, event))
    {
        json::Document view = event.value();
        index->insert(view, key);
    }

    return true;
}

void Collection::update_index(const std::string &name, bitstream &changes)
{
    if(name.empty())
    {
        primary_index().apply_changes(changes);
    }
    else
    {
        //TODO
    }
}

void Collection::set_trigger(remote_party_id identifier)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_triggers.insert(identifier);
}

void Collection::unset_trigger(remote_party_id identifier)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_triggers.find(identifier);

    if(it != m_triggers.end())
    {
        m_triggers.erase(it);
    }
}

#ifndef IS_TEST
void Collection::notify_triggers(RemoteParties &parties)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for(auto &t : m_triggers)
    {
        bitstream outstream;
        outstream << MessageType::NotifyTrigger;
        outstream << m_name;

        auto p = parties.find<RemoteParty>(t);
        
        if(p)
        {
            p->lock();
            p->send(outstream);
            p->unlock();
        }
    }
}
#endif

void Collection::load_metadata(bitstream &input)
{
    //m_primary_index->load_metadata(input);

    size_t num_s_indexes;
    input >> num_s_indexes;

    for(size_t j = 0; j < num_s_indexes; ++j)
    {
        std::string name;
        input >> name;

        HashIndex *index = HashIndex::new_from_metadata(m_buffer_manager, input);

        auto it = m_secondary_indexes.find(name);
        if(it != m_secondary_indexes.end())
        {
            log_debug("Replacing index " + name);
            delete it->second;
            m_secondary_indexes.erase(it);
        }

        m_secondary_indexes[name] = index;
    }
}

void Collection::dump_metadata(bitstream &output)
{
    //m_primary_index->dump_metadata(output);

    output << m_secondary_indexes.size();

    for(auto it : m_secondary_indexes)
    {
        output << it.first;
        it.second->dump_metadata(output);
    }
}

void Collection::unload_everything()
{
    //m_primary_index->unload_everything();

    for(auto it_ : m_secondary_indexes)
    {
        delete it_.second;
    }

    m_secondary_indexes.clear();
}

} // namespace credb::trusted
