#include "ObjectEventHandle.h"

namespace credb
{
namespace trusted
{

ObjectEventHandle::ObjectEventHandle() = default;

ObjectEventHandle::ObjectEventHandle(ObjectEventHandle &&other) noexcept
: m_content(std::move(other.m_content))
{
}

ObjectEventHandle& ObjectEventHandle::operator=(ObjectEventHandle &&other) noexcept
{
    m_content = std::move(other.m_content);
    return *this;
}

bool ObjectEventHandle::valid() const { return !m_content.empty(); }

void ObjectEventHandle::clear() { m_content.clear(); }

std::string ObjectEventHandle::source() const
{
    json::Document view(m_content, FIELD_SOURCE);
    return view.as_string();
}

bool ObjectEventHandle::has_predecessor() const { return previous_block() != INVALID_BLOCK; }

event_index_t ObjectEventHandle::previous_index() const
{
    json::Document view(m_content, FIELD_PREVIOUS_INDEX);
    return view.as_integer();
}

block_id_t ObjectEventHandle::previous_block() const
{
    json::Document view(m_content, FIELD_PREVIOUS_BLOCK);
    return view.as_integer();
}

ObjectEventType ObjectEventHandle::get_type() const
{
    json::Document view(m_content, FIELD_TYPE);
    auto i = view.as_integer();
    return static_cast<ObjectEventType>(i);
}

bool ObjectEventHandle::is_initial_version() const
{
    return version_number() == INITIAL_VERSION_NO;
}

version_number_t ObjectEventHandle::version_number() const
{
    json::Document view(m_content, FIELD_VERSION_NO);
    return view.as_integer();
}

json::Document ObjectEventHandle::value() const
{
    if(!valid())
    {
        throw std::runtime_error("Cannot get object value: not a valid handle!");
    }

    json::Document view(m_content, FIELD_VALUE);
    return view;
}

json::Document ObjectEventHandle::value(const std::string &path) const
{
    if(path.empty())
    {
        return value();
    }

    json::Document view(m_content, FIELD_VALUE);
    return json::Document(view, path, false);
}

bool ObjectEventHandle::get_policy(json::Document &out) const
{
    json::Document val = value();
    json::Document view(val, "policy", false);
    if(view.empty())
    {
        return false;
    }

    out = std::move(view);
    return true;
}

void ObjectEventHandle::assign(json::Document &&doc) { m_content = std::move(doc); }

} // namespace trusted
} // namespace credb
