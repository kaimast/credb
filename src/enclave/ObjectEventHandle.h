#pragma once

#include "credb/defines.h"
#include <json/json.h>

namespace credb
{
namespace trusted
{

class ObjectEventHandle
{
public:
    enum
    {
        FIELD_TYPE = 0,
        FIELD_SOURCE = 1,
        FIELD_PREVIOUS_BLOCK = 2,
        FIELD_PREVIOUS_INDEX = 3,
        FIELD_VALUE = 4,
        FIELD_VERSION_NO = 5
    };

    ObjectEventHandle();
    ObjectEventHandle(const ObjectEventHandle &other) = delete;
    ObjectEventHandle(ObjectEventHandle &&other) noexcept;
    ObjectEventHandle& operator=(ObjectEventHandle &&other) noexcept;

    bool valid() const;
    void clear();
    std::string source() const;
    bool has_predecessor() const;
    event_index_t previous_index() const;
    block_id_t previous_block() const;
    ObjectEventType get_type() const;
    bool is_initial_version() const;
    version_number_t version_number() const;
    json::Document value() const;
    json::Document value(const std::string &path) const;
    bool get_policy(json::Document &out) const;
    void assign(json::Document &&doc);

    ObjectEventHandle duplicate() const
    {
        ObjectEventHandle hdl;
        hdl.assign(m_content.duplicate());
        return hdl;
    }

private:
    json::Document m_content;
};

} // namespace trusted
} // namespace credb
