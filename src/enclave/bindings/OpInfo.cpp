#include "OpInfo.h"

using namespace cow;

namespace credb::trusted::bindings
{

ValuePtr OpInfo::get_member(const std::string &name)
{
    auto &mem = memory_manager();

    // FIXME support enum in language
    if(name == "type")
    {
        switch(m_type)
        {
        case OperationType::PutObject:
            return mem.create_string("put");
        case OperationType::AddToObject:
            return mem.create_string("add");
        case OperationType::GetObject:
            return mem.create_string("get");
        case OperationType::CallProgram:
            return mem.create_string("call");
        default:
            throw std::runtime_error("Unknown op type");
        }
    }
    else if(name == "is_modification")
    {
        auto val = (m_type == OperationType::PutObject || m_type == OperationType::AddToObject);
        return mem.create_boolean(val);
    }
    else if(name == "target")
    {
        return mem.create_string(m_target);
    }
    else
    {
        throw std::runtime_error("Failed to get member: " + name);
    }
}

} // namespace credb::trusted::bindings
