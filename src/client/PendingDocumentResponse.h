/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information

#pragma once

#include "PendingMessage.h"
#include "credb/defines.h"
#include <json/json.h>

namespace credb
{

class PendingDocumentResponse : public PendingMessage
{
public:
    PendingDocumentResponse(uint32_t msg_id, ClientImpl &client)
    : PendingMessage(msg_id, client), m_document()
    {
    }

    ~PendingDocumentResponse() {}

    json::Document document() { return std::move(m_document); }

protected:
    void parse(bitstream &msg) override
    {
        msg >> m_document;
    }

private:
    json::Document m_document;
};

} // namespace credb
