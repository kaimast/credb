#include "CollectionImpl.h"
#include "ClientImpl.h"
#include "DocParser.h"

#include "PendingPutWithoutKeyResponse.h"
#include "PendingCallResponse.h"
#include "PendingBitstreamResponse.h"
#include "PendingBooleanResponse.h"
#include "PendingDocumentResponse.h"
#include "PendingEventIdResponse.h"
#include "PendingFindResponse.h"
#include "PendingListResponse.h"
#include "PendingSizeResponse.h"

#include <cowlang/cow.h>
#include <fstream>
#include <json/Document.h>

namespace credb
{

std::tuple<std::string, event_id_t> CollectionImpl::put(const json::Document &document)
{
    auto op_id = m_client.get_next_operation_id();
    auto req = m_client.generate_op_request(op_id, OperationType::PutObjectWithoutKey);
    req << m_name << document;

    m_client.send_encrypted(req);

    PendingPutWithoutKeyResponse resp(op_id, m_client);
    resp.wait();

    if(!resp.success())
    {
        throw std::runtime_error("Put failed!");
    }

    return {resp.key(), resp.event_id()};
} 

event_id_t CollectionImpl::remove(const std::string &key)
{
    auto op_id = m_client.get_next_operation_id();
    auto req = m_client.generate_op_request(op_id, OperationType::RemoveObject);
    req << m_name << key;

    m_client.send_encrypted(req);

    PendingEventIdResponse resp(op_id, m_client);
    resp.wait();

    return resp.event_id();
}

bool CollectionImpl::clear()
{
    auto op_id = m_client.get_next_operation_id();
    auto req = m_client.generate_op_request(op_id, OperationType::Clear);

    req << m_name;

    m_client.send_encrypted(req);

    PendingBooleanResponse resp(op_id, m_client);
    resp.wait();
    return resp.success();
}

bool CollectionImpl::set_trigger(std::function<void()> lambda)
{
    m_client.set_trigger(m_name, lambda);

    auto op_id = m_client.get_next_operation_id();
    auto req = m_client.generate_op_request(op_id, OperationType::SetTrigger);

    req << m_name;

    m_client.send_encrypted(req);

    PendingBooleanResponse resp(op_id, m_client);
    resp.wait();

    return resp.success();
}

bool CollectionImpl::unset_trigger()
{
    m_client.unset_trigger(m_name);

    auto op_id = m_client.get_next_operation_id();
    auto req = m_client.generate_op_request(op_id, OperationType::UnsetTrigger);

    req << m_name;

    m_client.send_encrypted(req);

    PendingBooleanResponse resp(op_id, m_client);
    resp.wait();

    return resp.success();
}

bool CollectionImpl::has_object(const std::string &key)
{
    auto op_id = m_client.get_next_operation_id();
    auto req = m_client.generate_op_request(op_id, OperationType::HasObject);
    req << m_name << key;

    m_client.send_encrypted(req);

    PendingBooleanResponse resp(op_id, m_client);
    resp.wait();

    return resp.success();
}

bool CollectionImpl::check(const std::string &key, const json::Document &predicate)
{
    auto op_id = m_client.get_next_operation_id();
    auto req = m_client.generate_op_request(op_id, OperationType::CheckObject);
    req << m_name << key << predicate;

    m_client.send_encrypted(req);

    PendingBooleanResponse resp(op_id, m_client);
    resp.wait();

    return resp.success();
}



json::Document CollectionImpl::get(const std::string &key, event_id_t &event_id)
{
    auto op_id = m_client.get_next_operation_id();
    auto req = m_client.generate_op_request(op_id, OperationType::GetObject);
    req << m_name << key;

    m_client.send_encrypted(req);

    PendingDocumentResponse resp(op_id, m_client);
    resp.wait();

    if(!resp.success())
    {
        throw std::runtime_error("Failed to get " + key);
    }

    event_id = resp.event_id();
    return resp.document();
}

json::Document CollectionImpl::get_with_witness(const std::string &key, event_id_t &event_id, Witness &witness)
{
    auto op_id = m_client.get_next_operation_id();
    auto req = m_client.generate_op_request(op_id, OperationType::GetObjectWithWitness);
    req << m_name << key;

    m_client.send_encrypted(req);

    bitstream bstream;
    PendingBitstreamResponse resp(op_id, m_client, bstream);
    resp.wait();

    event_id = INVALID_EVENT;
    bstream >> event_id;
    if(!event_id)
    {
        throw std::runtime_error("Failed to get " + key);
    }

    json::Document doc("");
    bstream >> doc;

    bool ok = false;
    bstream >> ok;
    if(ok)
    {
        bstream >> witness;
    }
    else
    {
        throw std::runtime_error("Failed to generate witness");
    }

    return doc;
}

std::vector<json::Document> CollectionImpl::get_history(const std::string &key)
{
    auto op_id = m_client.get_next_operation_id();
    auto req = m_client.generate_op_request(op_id, OperationType::GetObjectHistory);
    req << m_name << key;

    m_client.send_encrypted(req);

    PendingListResponse<json::Document> resp(op_id, m_client);
    resp.wait();

    if(!resp.success())
    {
        throw std::runtime_error("Failed to get ohistory of " + key);
    }

    return resp.result();
}

bool CollectionImpl::create_index(const std::string &index_name, const std::vector<std::string> &paths)
{
    auto op_id = m_client.get_next_operation_id();
    auto req = m_client.generate_op_request(op_id, OperationType::CreateIndex);
    req << m_name << index_name << paths;

    m_client.send_encrypted(req);

    PendingBooleanResponse resp(op_id, m_client);
    resp.wait();
    return resp.success();
}

cow::ValuePtr CollectionImpl::call(const std::string &program_name, const std::vector<std::string> &args)
{
    auto op_id = m_client.get_next_operation_id();
    auto req = m_client.generate_op_request(op_id, OperationType::CallProgram);
    req << m_name << program_name << args << false;

    m_client.send_encrypted(req);

    PendingCallResponse resp(op_id, m_client, m_client.memory_manager());
    resp.wait();

    if(resp.success())
    {
        return resp.return_value();
    }
    else
    {
        throw std::runtime_error("CollectionImpl Call failed: [" + resp.error() + "]");
    }
}

event_id_t CollectionImpl::put_code(const std::string &key, const std::string &code)
{
    bitstream bstream = cow::compile_code(code);
    json::Binary bin(bstream);
    return put(key, bin);
}

event_id_t CollectionImpl::put_code_from_file(const std::string &key, const std::string &filename)
{
    std::ifstream file(filename);
    std::stringstream code;

    if(!file.good())
    {
        throw std::runtime_error("Failed to open file");
    }

    code << file.rdbuf();
    return put_code(key, code.str());
}

event_id_t CollectionImpl::put_from_file(const std::string &key, const std::string &filename)
{
    auto doc = parse_document_file(filename);
    return put(key, doc);
}

bool CollectionImpl::drop_index(const std::string &index_name)
{
    auto op_id = m_client.get_next_operation_id();
    auto req = m_client.generate_op_request(op_id, OperationType::DropIndex);
    req << m_name << index_name;

    m_client.send_encrypted(req);

    PendingBooleanResponse resp(op_id, m_client);
    resp.wait();
    return resp.success();
}

std::vector<json::Document>
CollectionImpl::diff(const std::string &key, version_number_t version1, version_number_t version2)
{
    auto op_id = m_client.get_next_operation_id();
    auto req = m_client.generate_op_request(op_id, OperationType::DiffVersions);
    req << m_name << key << version1 << version2;

    m_client.send_encrypted(req);

    PendingListResponse<json::Document> resp(op_id, m_client);
    resp.wait();

    if(!resp.success())
    {
        throw std::runtime_error("Diff request failed");
    }

    return resp.result();
}

uint32_t CollectionImpl::count(const json::Document &predicates)
{
    auto op_id = m_client.get_next_operation_id();
    auto req = m_client.generate_op_request(op_id, OperationType::CountObjects);
    req << m_name << predicates;

    m_client.send_encrypted(req);

    PendingSizeResponse resp(op_id, m_client);
    resp.wait();

    return resp.size();
}

std::tuple<std::string, event_id_t, json::Document>
CollectionImpl::internal_find_one(const json::Document &predicates, const std::vector<std::string> &projection)
{
    auto res = internal_find(predicates, projection, 1);

    if(res.empty())
    {
        throw std::runtime_error("didn't find anything!");
    }

    return std::move(res[0]);
}

std::tuple<std::string, json::Document>
CollectionImpl::find_one(const json::Document &predicates, const std::vector<std::string> &projection)
{
    auto res = find(predicates, projection, 1);

    if(res.empty())
    {
        throw std::runtime_error("didn't find anything!");
    }

    return std::move(res[0]);
}

std::vector<std::tuple<std::string, event_id_t, json::Document>>
CollectionImpl::internal_find(const json::Document &predicates, const std::vector<std::string> &projection, int32_t limit)
{
    auto op_id = m_client.get_next_operation_id();
    auto req = m_client.generate_op_request(op_id, OperationType::FindObjects);
    req << m_name;
    req << predicates;
    req << projection;
    req << limit;

    m_client.send_encrypted(req);

    PendingInternalFindResponse resp(op_id, m_client);
    resp.wait();

    return resp.result();
}

std::vector<std::tuple<std::string, json::Document>>
CollectionImpl::find(const json::Document &predicates, const std::vector<std::string> &projection, int32_t limit)
{
    auto op_id = m_client.get_next_operation_id();
    auto req = m_client.generate_op_request(op_id, OperationType::FindObjects);
    req << m_name;
    req << predicates;
    req << projection;
    req << limit;

    m_client.send_encrypted(req);

    PendingFindResponse resp(op_id, m_client);
    resp.wait();

    return resp.result();
}

event_id_t CollectionImpl::add(const std::string &key, const json::Document &value)
{
    auto op_id = m_client.get_next_operation_id();
    auto req = m_client.generate_op_request(op_id, OperationType::AddToObject);

    req << m_name << key << value;
    m_client.send_encrypted(req);

    PendingEventIdResponse resp(op_id, m_client);
    resp.wait();

    return resp.event_id();
}

event_id_t CollectionImpl::put(const std::string &key, const json::Document &document)
{
    auto op_id = m_client.get_next_operation_id();
    auto req = m_client.generate_op_request(op_id, OperationType::PutObject);
    req << m_name << key << document;

    m_client.send_encrypted(req);

    PendingEventIdResponse resp(op_id, m_client);
    resp.wait();

    return resp.event_id();
}

} // namespace credb
