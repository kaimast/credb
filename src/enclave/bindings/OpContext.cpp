#include "OpContext.h"
#include "../OpContext.h"

using namespace cow;

namespace credb::trusted::bindings
{

ValuePtr OpContext::get_member(const std::string &name)
{
    auto &mem = memory_manager();

    if(name == "source_name")
    {
        return mem.create_string(m_context.identity().name());
    }
    else if(name == "source_uri")
    {
        return mem.create_string(m_context.to_string());
    }
    else if(name == "type")
    {
        auto type = m_context.identity().type();

        // FIXME support enums in ppy
        if(type == IdentityType::Server)
        {
            return mem.create_string("SERVER");
        }
        else if(type == IdentityType::Client)
        {
            return mem.create_string("CLIENT");
        }
        else
        {
            throw std::runtime_error("Unknown actor type!");
        }
    }
    else if(name == "runs_in_tee")
    {
        return make_value<Function>(mem, [&mem, this](const std::vector<ValuePtr> &args) -> ValuePtr {
            if(!args.empty())
            {
                throw std::runtime_error("invalid number of arguments");
            }
            return mem.create_boolean(runs_in_tee(m_context.identity().type()));
        });
    }
    else if(name == "called_by_program")
    {
        return make_value<Function>(mem, [&mem, this](const std::vector<ValuePtr> &args) -> ValuePtr {
            if(!args.empty())
            {
                throw std::runtime_error("invalid number of arguments");
            }
           
            return mem.create_boolean(m_context.called_by_program());
        });
    }
    else
    {
        throw std::runtime_error("Failed to get member: " + name);
    }
}

} // namespace credb::trusted::bindings
