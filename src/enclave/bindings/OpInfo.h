#pragma once

#include "util/OperationType.h"
#include <cowlang/Module.h>

namespace credb
{
namespace trusted
{
namespace bindings
{

class OpInfo : public cow::Module
{
public:
    OpInfo(cow::MemoryManager &mem, OperationType type, const std::string &target)
    : cow::Module(mem), m_type(type), m_target(target)
    {
    }

    cow::ValuePtr get_member(const std::string &name) override;

private:
    const OperationType m_type;
    const std::string m_target;
};

} // namespace bindings
} // namespace trusted
} // namespace credb
