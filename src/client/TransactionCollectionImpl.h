/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information

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
    bool check(const std::string &key, const json::Document &predicates) override;

    event_id_t put(const std::string &key, const json::Document &doc) override;
    event_id_t add(const std::string &key, const json::Document &doc) override;
    std::pair<json::Document, event_id_t> get_with_eid(const std::string &key) override;
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
