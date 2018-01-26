#include "TransactionImpl.h"
#include "ClientImpl.h"
#include "PendingBitstreamResponse.h"
#include "TransactionCollectionImpl.h"

namespace credb
{

TransactionImpl::TransactionImpl(ClientImpl &client, IsolationLevel isolation)
: Transaction(isolation), m_client(client), m_done(false)
{
}

TransactionImpl::~TransactionImpl()
{
    for(operation_info_t *info : m_ops)
    {
        delete info;
    }
}

TransactionResult TransactionImpl::commit(bool generate_witness)
{
    if(is_done())
    {
        throw std::runtime_error("Already committed this transaction!");
    }

    auto op_id = m_client.get_next_operation_id();
    auto req = m_client.generate_op_request(op_id, OperationType::CommitTransaction);

    req << static_cast<uint8_t>(m_isolation);
    req << generate_witness;
    req << static_cast<uint32_t>(m_ops.size());

    for(operation_info_t *op : m_ops)
    {
        req << static_cast<uint8_t>(op->type);
        op->write_to_req(req);
    }

    m_client.send_encrypted(req);

    bitstream bstream;
    PendingBitstreamResponse resp(op_id, m_client, bstream);
    resp.wait();

    m_done = true;
    TransactionResult res;
    res.success = !bstream.empty();

    if(!res.success)
    {
        res.error = "empty response";
        return res;
    }

    bstream >> res.success;
    if(res.success)
    {
        bstream >> res.witness;
    }
    else
    {
        bstream >> res.error;
    }

    return res;
}

std::shared_ptr<Collection> TransactionImpl::get_collection(const std::string &name)
{
    return std::make_shared<TransactionCollectionImpl>(*this, m_client, name);
}


} // namespace credb
