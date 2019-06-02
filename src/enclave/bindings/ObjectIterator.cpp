#include "ObjectIterator.h"

#include <cowlang/cow.h>

namespace credb::trusted::bindings
{

ObjectListIterator::ObjectListIterator(cow::MemoryManager &mem, credb::trusted::ObjectListIterator &&it)
: cow::Generator(mem), m_it(std::move(it))
{
}


cow::ValuePtr ObjectListIterator::next()
{
    auto &mem = memory_manager();

    std::string str;
    ObjectEventHandle hdl;

    if(!m_it.next(str, hdl))
    {
        throw cow::stop_iteration_exception();
    }

    auto key = mem.create_string(str);
    auto value = mem.create_from_document(hdl.value());

    auto t = mem.create_tuple();
    t->append(key);
    t->append(value);

    return t;
}

} // namespace credb::trusted::bindings
