#pragma once

#include "credb/Collection.h"

namespace credb
{

class ClientImpl;

class CollectionImpl : public Collection
{
public:
    CollectionImpl(ClientImpl &client, const std::string &name) : m_client(client), m_name(name) {}

    virtual ~CollectionImpl() {}

    virtual cow::ValuePtr call(const std::string &program_name, const std::vector<std::string> &args) override;

    virtual event_id_t put_code(const std::string &key, const std::string &code) override;
    virtual event_id_t put_code_from_file(const std::string &key, const std::string &filename) override;

    virtual event_id_t put_from_file(const std::string &key, const std::string &filename) override;

    virtual bool set_trigger(std::function<void()> lambda) override;
    virtual bool unset_trigger() override;

    virtual bool clear() override;

    virtual event_id_t add(const std::string &key, const json::Document &value) override;
    virtual event_id_t put(const std::string &key, const json::Document &document) override;
    virtual event_id_t remove(const std::string &key) override;

    virtual std::tuple<std::string, event_id_t> put(const json::Document &document) override;
 
    virtual bool create_index(const std::string &index_name, const std::vector<std::string> &paths) override;

    bool drop_index(const std::string &index_name) override;

    virtual bool has_object(const std::string &key) override;
    virtual bool check(const std::string &key, const json::Document &predicate) override;

    virtual json::Document get(const std::string &key, event_id_t &event_id) override;
    virtual json::Document get_with_witness(const std::string &key, event_id_t &event_id, Witness &witness) override;

    virtual std::vector<json::Document> get_history(const std::string &key) override;

    virtual uint32_t count(const json::Document &predicates) override;

    std::tuple<std::string, event_id_t, json::Document>
    internal_find_one(const json::Document &predicates, const std::vector<std::string> &projection);
    std::vector<std::tuple<std::string, event_id_t, json::Document>>
    internal_find(const json::Document &predicates, const std::vector<std::string> &projection, int32_t limit = -1);

    virtual std::tuple<std::string, json::Document>
    find_one(const json::Document &predicates, const std::vector<std::string> &projection) override;
    virtual std::vector<std::tuple<std::string, json::Document>>
    find(const json::Document &predicates, const std::vector<std::string> &projection, int32_t limit = -1) override;

    virtual std::vector<json::Document>
    diff(const std::string &key, version_number_t version1, version_number_t version2) override;

    virtual const std::string &name() override { return m_name; }

private:
    ClientImpl &m_client;
    const std::string m_name;
};

} // namespace credb
