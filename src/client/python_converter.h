/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information

#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <stack>
#include PYTHON_DATETIME
#include <credb/defines.h>
#include <credb/Transaction.h>
#include <credb/event_id.h>
#include <json/json.h>
#include <cowlang/cow.h>

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
        PyDateTime_IMPORT;

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
        PyObject *obj = Py_None;
        Py_INCREF(obj);
        
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
            Py_DECREF(obj);
            return;
        }

        if(PyList_Check(top))
        {
            PyList_Append(top, obj);
            Py_DECREF(obj);
            return;
        }

        throw std::runtime_error("Failed to convert from binary to python document: Cannot append child to this type of object.");
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

template <> struct type_caster<credb::TransactionResult>
{
public:
    bool load(handle, bool)
    {
        return false;
    }

    static handle cast(const credb::TransactionResult &result, return_value_policy return_policy, handle parent)
    {
        (void)return_policy;
        (void)parent;

        auto t = PyTuple_New(2);
        PyTuple_SetItem(t, 0, PyBool_FromLong(result.success));

        auto none_ptr = Py_None;
        Py_INCREF(none_ptr);

        if(result.success)
        {
            //FIXME expose witness
            PyTuple_SetItem(t, 1, none_ptr);
        }
        else
        {
            auto str = PyUnicode_FromString(result.error.c_str());
            PyTuple_SetItem(t, 1, str);
        }

        return t;
    }

    PYBIND11_TYPE_CASTER(credb::TransactionResult, _("credb::TransactionResult"));
};

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
        if(!src.valid() || src.empty())
        {
            PyObject *obj = Py_None;
            Py_INCREF(obj);
            return obj;
        }

        DocumentToPythonConverter converter;
        src.iterate(converter);

        return converter.get_result();
    }
};

template <> struct type_caster<cow::ValuePtr>
{
public:
    PYBIND11_TYPE_CASTER(cow::ValuePtr, _("cow::ValuePtr"));

    bool load(handle src, bool)
    {
        (void)src;
        //TODO
        return false;
    }

    static handle cast(const cow::ValuePtr& ptr, return_value_policy return_policy, handle parent)
    {
        auto doc = cow::value_to_document(ptr);
        return type_caster<json::Document>::cast(doc, return_policy, parent);
    }
};



} // namespace pybind11::detail
