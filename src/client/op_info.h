#pragma once

#include "util/OperationType.h"

namespace credb
{

struct operation_info_t
{
public:
    OperationType type;
    virtual void write_to_req(bitstream &req) = 0;
    virtual ~operation_info_t() {}

protected:
    operation_info_t(OperationType t) : type(t) {}
};

struct has_obj_info_t : public operation_info_t
{
    const std::string collection;
    const std::string key;
    const bool result;

    has_obj_info_t(const std::string &collection_, const std::string &key_, const bool &result_)
    : operation_info_t(OperationType::HasObject), collection(collection_), key(key_), result(result_)
    {
    }

    void write_to_req(bitstream &req) override { req << collection << key << result; }
};

struct check_obj_info_t : public operation_info_t
{
    const std::string collection;
    const std::string key;
    const json::Document predicates;
    const bool result;
    
    check_obj_info_t(const std::string &collection_, const std::string &key_, const json::Document &predicates_, const bool &result_)
    : operation_info_t(OperationType::HasObject), collection(collection_), key(key_), predicates(predicates_.duplicate()), result(result_)
    {
    }

    void write_to_req(bitstream &req) override { req << collection << key << predicates << result; }
};

struct get_info_t : public operation_info_t
{
    const std::string collection;
    const std::string key;
    const event_id_t eid;

    get_info_t(const std::string &collection_, const std::string &key_, const event_id_t &eid_)
    : operation_info_t(OperationType::GetObject), collection(collection_), key(key_), eid(eid_)
    {
    }

    void write_to_req(bitstream &req) override { req << collection << key << eid; }
};

struct put_info_t : public operation_info_t
{
    const std::string collection;
    const std::string key;

    json::Document doc;
    put_info_t(const std::string &collection_, const std::string &key_, const json::Document &doc_)
    : operation_info_t(OperationType::PutObject), collection(collection_), key(key_),
      doc(doc_.duplicate(true))
    {
    }

    void write_to_req(bitstream &req) override { req << collection << key << doc; }
};

struct add_info_t : public operation_info_t
{
    const std::string collection;
    const std::string key;

    json::Document doc;
    add_info_t(const std::string &collection_, const std::string &key_, const json::Document &doc_)
    : operation_info_t(OperationType::AddToObject), collection(collection_), key(key_), doc(doc_.duplicate(true))
    {
    }

    void write_to_req(bitstream &req) override { req << collection << key << doc; }
};

struct remove_info_t : public operation_info_t
{
    std::string collection;
    std::string key;

    remove_info_t(const std::string &collection_, const std::string &key_)
    : operation_info_t(OperationType::RemoveObject), collection(collection_), key(key_)
    {
    }

    void write_to_req(bitstream &req) override { req << collection << key; }
};

struct find_info_t : public operation_info_t
{
    std::string collection;
    json::Document predicates;
    std::vector<std::string> projection;
    int32_t limit;
    std::vector<std::pair<std::string, event_id_t>> res;
    find_info_t(const std::string &collection_,
                const json::Document &predicates_,
                const std::vector<std::string> &projection_,
                int32_t limit_,
                std::vector<std::pair<std::string, event_id_t>> &&res_)
    : operation_info_t(OperationType::FindObjects), collection(collection_),
      predicates(predicates_.duplicate(true)), projection(projection_), limit(limit_), res(std::move(res_))
    {
    }

    void write_to_req(bitstream &req) override
    {
        req << collection << predicates << projection << limit;
        req << static_cast<uint32_t>(res.size());
        for(auto &it : res)
            req << it.first << it.second;
    }
};

}
