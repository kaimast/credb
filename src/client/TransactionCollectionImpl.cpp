/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information

#include "TransactionCollectionImpl.h"
#include "TransactionImpl.h"

#include "op_info.h"

namespace credb
{

event_id_t TransactionCollectionImpl::put(const std::string &key, const json::Document &doc)
{
    m_transaction.assert_not_committed();
    m_transaction.queue_op(new put_info_t(name(), key, doc));
    return INVALID_EVENT;
}

event_id_t TransactionCollectionImpl::add(const std::string &key, const json::Document &doc)
{
    m_transaction.assert_not_committed();
    m_transaction.queue_op(new add_info_t(name(), key, doc));
    return INVALID_EVENT;
}

event_id_t TransactionCollectionImpl::remove(const std::string &key)
{
    m_transaction.assert_not_committed();
    m_transaction.queue_op(new remove_info_t(name(), key));
    return INVALID_EVENT;
}

bool TransactionCollectionImpl::check(const std::string &key, const json::Document &predicates)
{
    m_transaction.assert_not_committed();
    auto res = CollectionImpl::check(key, predicates);
    m_transaction.queue_op(new check_obj_info_t(name(), key, predicates, res));
    return res;
}

bool TransactionCollectionImpl::has_object(const std::string &key)
{
    m_transaction.assert_not_committed();
    auto res = CollectionImpl::has_object(key);
    m_transaction.queue_op(new has_obj_info_t(name(), key, res));
    return res;
}

std::pair<json::Document, event_id_t> TransactionCollectionImpl::get_with_eid(const std::string &key)
{
    m_transaction.assert_not_committed();

    auto [doc, event_id] = CollectionImpl::get_with_eid(key);

    m_transaction.queue_op(new get_info_t(name(), key, event_id));

    return std::pair<json::Document, event_id_t>
            {std::move(doc), event_id};
}

std::vector<std::tuple<std::string, json::Document>>
TransactionCollectionImpl::find(const json::Document &predicates, const std::vector<std::string> &projection, int32_t limit)
{
    m_transaction.assert_not_committed();

    auto res = CollectionImpl::internal_find(predicates, projection, limit);

    std::vector<std::tuple<std::string, json::Document>> r;
    std::vector<std::pair<std::string, event_id_t>> v;

    for(auto & [key, eid, doc] : res)
    {
        r.emplace_back(key, std::move(doc));
        v.emplace_back(key, eid);
    }

    m_transaction.queue_op(
    new find_info_t(name(), predicates, projection, limit, std::move(v)));
    return r;
}

std::tuple<std::string, json::Document>
TransactionCollectionImpl::find_one(const json::Document &predicates, const std::vector<std::string> &projection)
{
    auto res = find(predicates, projection, 1);
    if(res.empty())
    {
        throw std::runtime_error("didn't find anything!");
    }

    return std::move(res[0]);
}


} // namespace credb
