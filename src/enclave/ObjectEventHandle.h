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
        FIELD_VERSION_NO = 5,
        FIELD_READ_SET = 6,
        FIELD_WRITE_SET = 7
    };

    ObjectEventHandle();
    ObjectEventHandle(const ObjectEventHandle &other) = delete;
    ObjectEventHandle(ObjectEventHandle &&other) noexcept;
    ObjectEventHandle& operator=(ObjectEventHandle &&other) noexcept;

    /**
     * Create a new object event handle from a json document
     * That document shall contain a view to the part of the datablock that contains the event
     */
    explicit ObjectEventHandle(json::Document &&doc);

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
    
    json::Document get_policy() const;

    ObjectEventHandle duplicate() const
    {
        return ObjectEventHandle(m_content.duplicate());
    }

    /**
     * Does this handle hold a reference to a valid event? 
     */
    bool valid() const
    {
        return m_content.valid();
    }

private:
    json::Document m_content;
};

} // namespace trusted
} // namespace credb
