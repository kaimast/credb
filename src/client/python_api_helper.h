/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information

#include "credb/Client.h"
#include "credb/Collection.h"
#include "credb/Transaction.h"

#include <boost/python.hpp>
#include <memory>

#include <cowlang/cow.h>
#include <cowlang/unpack.h>

#include "python_converter.h"

using credb::Client;
using credb::Collection;
using credb::IsolationLevel;
using credb::Transaction;
using credb::Witness;

namespace py = boost::python;

template <class T> struct VecToList
{
    static PyObject *convert(const std::vector<T> &vector)
    {
        py::list l;
        for(auto &e : vector)
        {
            l.append(e);
        }

        return py::incref(l.ptr());
    }
};

template <class T> struct SetToSet
{
    static PyObject *convert(const std::set<T> &set)
    {
        py::dict d;
        for(auto &e : set)
        {
            d.items()[e] = 1;
        }

        return py::incref(d.ptr());
    }
};

struct EventIdConverter
{
    static PyObject *convert(const credb::event_id_t &id)
    {
        auto obj = py::make_tuple(id.block, id.index);
        return py::incref(obj.ptr());
    }
};

struct TransactionResultConverter
{
    static PyObject *convert(const credb::TransactionResult &res)
    {
        py::dict d;
        d["success"] = res.success;
        d["error"] = res.error;
        d["witness"] = py::object(res.witness);

        return py::incref(d.ptr());
    }
};

struct DocToDoc
{
    static PyObject *convert(const json::Document &doc)
    {
        DocumentToPythonConverter converter;
        doc.iterate(converter);
        return py::incref(converter.get_result().ptr());
    }
};

struct VecDocToDoc
{
    static PyObject *convert(const std::tuple<std::string, json::Document> &tuple)
    {
        const auto & [key, doc] = tuple;
        if(doc.empty())
        {
            return py::incref(py::object().ptr());
        }

        py::tuple t = py::make_tuple(key, doc);
        return py::incref(t.ptr());
    }

    static PyObject *convert(const std::vector<std::tuple<std::string, json::Document>> &vector)
    {
        py::list l;

        for(auto &it : vector)
        {
            l.append(it);
        }

        return py::incref(l.ptr());
    }

    static PyObject *convert(const std::vector<json::Document> &docs)
    {
        py::list l;

        for(auto &doc : docs)
        {
            l.append(convert(doc));
        }

        return py::incref(l.ptr());
    }

    static py::object convert(const json::Document &doc)
    {
        DocumentToPythonConverter converter;
        doc.iterate(converter);
        return converter.get_result();
    }
};

inline PyObject *extract_value(cow::ValuePtr val)
{
    switch(val->type())
    {
    case cow::ValueType::String:
    {
        py::object obj(cow::unpack_string(val));
        return py::incref(obj.ptr());
    }
    case cow::ValueType::Integer:
    {
        py::object obj(cow::unpack_integer(val));
        return py::incref(obj.ptr());
    }
    case cow::ValueType::Bool:
    {
        py::object obj(cow::unpack_bool(val));
        return py::incref(obj.ptr());
    }
    case cow::ValueType::None:
    {
        return nullptr;
    }
    default:
        throw std::runtime_error("Cannot extract value.");
    }
}

inline PyObject *Transaction_commit_python(Transaction *tx, bool generate_witness = true)
{
    auto res = tx->commit(generate_witness);

    if(res.success)
    {
        py::tuple t = py::make_tuple(true, res.witness);
        return py::incref(t.ptr());
    }
    else
    {
        py::tuple t = py::make_tuple(false, res.error);
        return py::incref(t.ptr());
    }
}

inline PyObject *execute_python(credb::Client *client, const std::string &code, const py::object &pyargs)
{
    std::vector<std::string> args = {};
    for(uint32_t i = 0; i < py::len(pyargs); ++i)
    {
        args.push_back(py::extract<std::string>(pyargs[i]));
    }

    auto val = client->execute(code, args);
    return extract_value(val);
}

inline PyObject *call_python(credb::Collection *c, const std::string &key, const py::object &pyargs)
{
    std::vector<std::string> args = {};
    for(uint32_t i = 0; i < py::len(pyargs); ++i)
    {
        args.push_back(py::extract<std::string>(pyargs[i]));
    }

    auto val = c->call(key, args);
    return extract_value(val);
}

inline json::Document get_python(credb::Collection *c, const std::string &key) { return c->get(key); }

inline PyObject *get_with_witness_python(credb::Collection *c, const std::string &key)
{
    credb::event_id_t eid;
    Witness witness;
    json::Document doc = c->get_with_witness(key, eid, witness);

    py::tuple t = py::make_tuple(doc, eid, witness);
    return py::incref(t.ptr());
}

bool put_python(credb::Collection *c, const std::string &key, const py::object &pyvalue)
{
    PythonToDocumentConverter converter(pyvalue);
    converter.run();

    return static_cast<bool>(c->put(key, converter.get_result()));
}

std::tuple<std::string, json::Document> find_one_python(Collection *c,
                                                        const py::object &predicates = py::object(),
                                                        const py::list &pyprojection = py::list())
{
    std::vector<std::string> projection = {};
    for(uint32_t i = 0; i < py::len(pyprojection); ++i)
    {
        projection.push_back(py::extract<std::string>(pyprojection[i]));
    }

    try
    {
        PythonToDocumentConverter converter(predicates);
        converter.run();
        return c->find_one(converter.get_result(), projection);
    }
    catch(const std::runtime_error &e)
    {
        return { "", json::Document("") };
    }
}

bool create_index_python(credb::Collection *c, const std::string &name, const py::list &pyfields)
{
    std::vector<std::string> fields = {};
    for(uint32_t i = 0; i < py::len(pyfields); ++i)
    {
        fields.push_back(py::extract<std::string>(pyfields[i]));
    }

    return c->create_index(name, fields);
}

size_t count_python(credb::Collection *c, const py::object &predicates = py::object())
{
    if(predicates.is_none())
    {
        return c->count();
    }
    else
    {
        PythonToDocumentConverter converter(predicates);
        converter.run();
        return c->count(converter.get_result());
    }
}

std::vector<std::tuple<std::string, json::Document>> find_python(Collection *c,
                                                                 const py::object &predicates = py::object(),
                                                                 const py::list &pyprojection = py::list(),
                                                                 const int limit = -1)
{
    std::vector<std::string> projection = {};
    for(uint32_t i = 0; i < py::len(pyprojection); ++i)
    {
        projection.push_back(py::extract<std::string>(pyprojection[i]));
    }

    PythonToDocumentConverter converter(predicates);
    converter.run();
    return c->find(converter.get_result(), projection, limit);
}

bool add_python(credb::Collection *c, const std::string &path, const py::object &object)
{
    PythonToDocumentConverter converter(object);
    converter.run();
    return static_cast<bool>(c->add(path, converter.get_result()));
}

std::shared_ptr<Transaction> init_transaction_python(credb::Client *c, IsolationLevel isolation_level = IsolationLevel::Serializable)
{
    return c->init_transaction(isolation_level);
}

