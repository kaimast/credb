#pragma once

#include <cowlang/Module.h>

namespace credb::trusted
{

class OpContext;

namespace bindings
{

class OpContext : public cow::Module
{
public:
    OpContext(cow::MemoryManager &mem, const credb::trusted::OpContext &context)
    : cow::Module(mem), m_context(context)
    {
    }

    cow::ValuePtr get_member(const std::string &name) override;

private:
    const credb::trusted::OpContext &m_context;
};

} // namespace bindings
} // namespace credb::trusted
