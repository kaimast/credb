/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information

#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <stack>
#include PYTHON_DATETIME
#include <credb/defines.h>
#include <credb/event_id.h>
#include <json/json.h>

#include <glog/logging.h>

namespace py = pybind11;

namespace
{

class DocumentToPythonConverter : public json::Iterator
{
public:
    DocumentToPythonConverter() = default;

    ~DocumentToPythonConverter()
    {
        while(!parse_stack.empty())
        {
            parse_stack.pop();
        }
    }

    PyObject *get_result()
    {
        // Not a valid document?
        if(parse_stack.size() != 1)
        {
            throw std::runtime_error("Json converter in an invalid child");
        }

        return parse_stack.top();
    }

    void handle_datetime(const std::string &key, const tm &value) override
    {
        auto obj = PyDateTime_FromDateAndTime(
            value.tm_year, value.tm_mon, value.tm_mday,
            value.tm_hour, value.tm_min, value.tm_sec, 0);

        add_value(key, obj);
    }

    void handle_string(const std::string &key, const std::string &value) override
    {
        add_value(key, PyUnicode_FromString(value.c_str()));
    }

    void handle_integer(const std::string &key, int64_t value) override
    {
        add_value(key, PyLong_FromLong(value));
    }

    void handle_float(const std::string &key, const double value) override
    {
        add_value(key, PyFloat_FromDouble(value));
    }

    void handle_boolean(const std::string &key, const bool value) override
    {
        add_value(key, PyBool_FromLong(value));
    }

    void add_value(const std::string &key, PyObject *obj)
    {
        if(key.empty())
        {
            parse_stack.push(obj);
        }
        else
        {
            append_child(key, obj);
        }
    }

    void handle_null(const std::string &key) override
    {
        PyObject *obj = nullptr;
        
        if(!key.empty())
        {
            append_child(key, obj);
        }
        else
        {
            parse_stack.push(obj);
        }
    }

    void handle_map_start(const std::string &key) override
    {
        auto dict = PyDict_New();

        if(!key.empty())
        {
            append_child(key, dict);
        }

        parse_stack.push(dict);
    }

    void handle_map_end() override
    {
        if(parse_stack.size() > 1)
        {
            parse_stack.pop();
        }
    }

    void handle_array_start(const std::string &key) override
    {
        auto list = PyList_New(0);
        
        if(!key.empty())
        {
            append_child(key, list);
        }

        parse_stack.push(list);
    }

    void handle_array_end() override
    {
        if(parse_stack.size() > 1)
        {
            parse_stack.pop();
        }
    }

    void handle_binary(const std::string &key, const uint8_t *data, uint32_t len) override
    {
        (void)data;
        (void)len;

        auto obj = PyBytes_FromStringAndSize(reinterpret_cast<const char*>(data), len);
        add_value(key, obj);
    }

private:
    void append_child(const std::string key, PyObject *obj)
    {
        if(parse_stack.empty())
        {
            throw std::runtime_error("Cannot append child at this point!");
        }

        auto top = parse_stack.top();

        if(PyDict_Check(top))
        {
            PyDict_SetItem(top, PyUnicode_FromString(key.c_str()), obj);
            return;
        }

        if(PyList_Check(top))
        {
            PyList_Append(top, obj);
            return;
        }

        throw std::runtime_error("Failed to convert from binary to python document: Cannot append "
                                 "child to this type of object.");
    }

    std::stack<PyObject*> parse_stack;
};

class PythonToDocumentConverter
{
public:
    PythonToDocumentConverter(const py::handle &root_) : root(root_), result(), writer(result) {}

    void run()
    {
        parse_next("", root);
    }

    json::Document get_result()
    {
        uint8_t *data = nullptr;
        uint32_t size = 0;

        result.detach(data, size);

        return json::Document(data, size, json::DocumentMode::ReadWrite);
    }

private:
    void parse_next(const std::string &key, const py::handle &obj)
    {
        if(obj.ptr() == Py_None)
        {
            writer.write_null(key);
            return;
        }

        if(py::isinstance<py::dict>(obj))
        {
            auto dict = py::cast<py::dict>(obj);

            writer.start_map(key);

            for(const auto &[key, val] : dict)
            {
                if(!py::isinstance<py::str>(key))
                {
                    throw std::runtime_error("Failed to extract key");
                }

                auto k = py::cast<std::string>(key);
                parse_next(k, val);
            }

            writer.end_map();
        }
        else if(py::isinstance<py::list>(obj))
        {
            const auto list = py::cast<py::list>(obj);
            writer.start_array(key);

            for(uint32_t i = 0; i < py::len(list); ++i)
            {
                parse_next(std::to_string(i), list[i]);
            }

            writer.end_array();
        }
        else if(py::isinstance<py::int_>(obj))
        {
            auto i = py::cast<json::integer_t>(obj);
            writer.write_integer(key, i);
        }
        else if(py::isinstance<py::str>(obj))
        {
            auto str = py::cast<py::str>(obj);
            writer.write_string(key, str);
        }
        else if(py::isinstance<py::float_>(obj))
        {
            auto f = py::cast<json::float_t>(obj);
            writer.write_float(key, f);
        }
        else if(py::isinstance<bool>(obj))
        {
            auto b = py::cast<bool>(obj);
            writer.write_boolean(key, b);
            return;
        }
        else
        {
            auto pobj = obj.ptr();

            if(PyByteArray_Check(pobj))
            {
                auto size = PyByteArray_Size(pobj);
                auto data = PyByteArray_AsString(pobj);

                bitstream value;
                value.assign(reinterpret_cast<uint8_t*>(data), static_cast<uint32_t>(size), true);
                
                writer.write_binary(key, value);
                return;
            }


            // auto datetime = py::import("datetime");
            PyDateTime_IMPORT;
            bool is_datetime = Py_TYPE(pobj) == PyDateTimeAPI->DateTimeType;

            // just assuming it is datetime for now
            if(is_datetime)
            {
                tm value;
                value.tm_year = py::cast<int>(obj.attr("year"));
                value.tm_mon = py::cast<int>(obj.attr("month"));
                value.tm_mday = py::cast<int>(obj.attr("day"));
                value.tm_hour = py::cast<int>(obj.attr("hour"));
                value.tm_min = py::cast<int>(obj.attr("minute"));
                value.tm_sec = py::cast<int>(obj.attr("second"));

                writer.write_datetime(key, value);
                return;
            }

            throw std::runtime_error("Python to BSON conversion failed: Unknown object type");
        }
    }

    const py::handle &root;
    std::stack<const py::handle *> parse_stack;

    bitstream result;
    json::Writer writer;
};

}

namespace pybind11::detail
{

/*
template <> struct type_caster<credb::event_id_t>
{
public:
    bool load(handle src, bool)
    {
        if(!py::isinstance<py::tuple>(src))
        {
            return false;
        }

        auto t = py::cast<py::tuple>(src);

        if(py::len(t) != 3)
        {
            return false;
        }

        if(!py::isinstance<py::int_>(t[0])
            || !py::isinstance<py::int_>(t[1])
            || !py::isinstance<py::int_>(t[2]))
        {
            return false;
        }

        auto shard = py::cast<credb::shard_id_t>(t[0]);
        auto block = py::cast<credb::block_id_t>(t[1]);
        auto index = py::cast<credb::block_index_t>(t[2]);

        value = credb::event_id_t(shard, block, index);
        return true;
    }

    static handle cast(const credb::event_id_t &eid, return_value_policy return_policy, handle parent)
    {
        (void)return_policy;
        (void)parent;

        auto shard = PyLong_FromLong(eid.shard);
        auto block = PyLong_FromLong(eid.block);
        auto index = PyLong_FromLong(eid.index);

        return PyTuple_Pack(3, shard, block, index);
    }

    PYBIND11_TYPE_CASTER(credb::event_id_t, _("credb::event_id_t"));
};*/

template <> struct type_caster<json::Document>
{
public:
    PYBIND11_TYPE_CASTER(json::Document, _("json::Document"));

    bool load(handle src, bool)
    {
        PythonToDocumentConverter converter(src);

        try
        {
            converter.run();
        }
        catch(json_error &e)
        {
            DLOG(INFO) << e.what();
            return false;
        }

        value = converter.get_result();
        return true;
    }

    static handle cast(const json::Document &src, return_value_policy, handle)
    {
        if(src.empty() || !src.valid())
        {
            return py::handle();
        }

        DocumentToPythonConverter converter;
        src.iterate(converter);

        return converter.get_result();
    }
};

} // namespace pybind11::detail
