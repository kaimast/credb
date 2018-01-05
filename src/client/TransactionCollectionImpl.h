#pragma once

#include "CollectionImpl.h"

namespace credb
{

class TransactionImpl;

class TransactionCollectionImpl : public CollectionImpl
{
public:
    TransactionCollectionImpl(TransactionImpl &transaction, ClientImpl &client, const std::string &name)
    : CollectionImpl(client, name), m_transaction(transaction)
    {
    }

    bool has_object(const std::string &key) override;
    event_id_t put(const std::string &key, const json::Document &doc) override;
    event_id_t add(const std::string &key, const json::Document &doc) override;
    json::Document get(const std::string &key, event_id_t &event_id) override;
    std::tuple<std::string, json::Document> find_one(const json::Document &predicates = json::Document(""),
                                                     const std::vector<std::string> &projection = {}) override;
    std::vector<std::tuple<std::string, json::Document>>
    find(const json::Document &predicates = json::Document(""),
         const std::vector<std::string> &projection = {},
         int32_t limit = -1) override;
    event_id_t remove(const std::string &key) override;

private:
    TransactionImpl &m_transaction;
};

} // namespace credb
