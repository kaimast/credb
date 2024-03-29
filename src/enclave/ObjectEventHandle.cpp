/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "ObjectEventHandle.h"

namespace credb::trusted
{

ObjectEventHandle::ObjectEventHandle() = default;

ObjectEventHandle::ObjectEventHandle(ObjectEventHandle &&other) noexcept
    : m_content(std::move(other.m_content))
{
}

ObjectEventHandle::ObjectEventHandle(json::Document &&doc)
    : m_content(std::move(doc))
{
}

ObjectEventHandle& ObjectEventHandle::operator=(ObjectEventHandle &&other) noexcept
{
    m_content = std::move(other.m_content);
    return *this;
}

void ObjectEventHandle::clear() { m_content.clear(); }

std::string ObjectEventHandle::source() const
{
    json::Document view(m_content, FIELD_SOURCE);
    return view.as_string();
}

bool ObjectEventHandle::has_predecessor() const { return previous_block() != INVALID_BLOCK; }

block_index_t ObjectEventHandle::previous_index() const
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

ledger_pos_t ObjectEventHandle::transaction_ref() const
{
    json::Document v1(m_content, FIELD_TRANSACTION_BLOCK);
    json::Document v2(m_content, FIELD_TRANSACTION_INDEX);

    auto block = static_cast<block_id_t>(v1.as_integer());
    auto index = static_cast<block_index_t>(v2.as_integer());

    return {block, index};
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

json::Document ObjectEventHandle::get_policy() const
{
    return json::Document(value(), "policy", false);
}

} // namespace credb::trusted
