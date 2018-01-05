#pragma once

#include <boost/python.hpp>
#include <stack>
#include PYTHON_DATETIME
#include <credb/defines.h>
#include <json/json.h>

namespace py = boost::python;

class DocumentToPythonConverter : public json::Iterator
{
public:
    DocumentToPythonConverter() {}

    ~DocumentToPythonConverter()
    {
        while(!parse_stack.empty())
        {
            parse_stack.pop();
        }
    }

    const py::object &get_result() const
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
        auto module = py::import("datetime");

        try
        {
            py::object pyval = module.attr("datetime")(value.tm_year, value.tm_mon, value.tm_mday,
                                                       value.tm_hour, value.tm_min, value.tm_sec);
            append_child(key, pyval);
        }
        catch(...)
        {
            PyErr_Print();
            PyErr_Clear();
        }
    }

    void handle_string(const std::string &key, const std::string &value) override
    {
        add_value(key, value);
    }

    void handle_integer(const std::string &key, int64_t value) override { add_value(key, value); }

    void handle_float(const std::string &key, const double value) override
    {
        add_value(key, value);
    }

    void handle_boolean(const std::string &key, const bool value) override
    {
        add_value(key, value);
    }

    template <typename T> void add_value(const std::string &key, const T &value)
    {
        py::object obj(value);

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
        py::object obj;
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
        py::dict dict;

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
        py::list list;
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
        (void)key;
        (void)data;
        (void)len;
        // FIXME convert to byte object
    }

private:
    void append_child(const std::string key, boost::python::object &obj)
    {
        if(parse_stack.empty())
        {
            throw std::runtime_error("Cannot append child at this point!");
        }

        auto &top = parse_stack.top();
        py::extract<py::dict> d(*top);

        if(d.check())
        {
            d()[key] = obj;
            return;
        }

        py::extract<py::list> l(*top);

        if(l.check())
        {
            l().append(obj);
            return;
        }

        throw std::runtime_error("Failed to convert from binary to python document: Cannot append "
                                 "child to this type of object.");
    }

    std::stack<py::object> parse_stack;
};

class PythonToDocumentConverter
{
public:
    PythonToDocumentConverter(const py::object &root_) : root(root_), result(), writer(result) {}

    void run()
    {
        parse_next("", root);
    }

    json::Document get_result() const
    {
        return json::Document(result.data(), result.size(), json::DocumentMode::ReadOnly);
    }

private:
    void parse_next(const std::string &key, const py::object &obj)
    {
        if(obj.ptr() == Py_None)
        {
            writer.write_null(key);
            return;
        }

        py::extract<py::dict> d(obj);

        if(d.check())
        {
            const py::dict &dict = d();
            const py::list &keys = dict.keys();

            writer.start_map(key);

            for(uint32_t i = 0; i < py::len(keys); ++i)
            {
                py::extract<std::string> child_key(keys[i]);
                if(!child_key.check())
                {
                    throw std::runtime_error("Failed to extract key");
                }

                std::string k = child_key();
                parse_next(k, dict.get(k));
            }

            writer.end_map();
            return;
        }

        py::extract<py::list> l(obj);

        if(l.check())
        {
            const py::list &list = l();
            writer.start_array(key);

            for(uint32_t i = 0; i < py::len(list); ++i)
            {
                parse_next(std::to_string(i), list[i]);
            }

            writer.end_array();
            return;
        }

        py::extract<json::integer_t> i(obj);

        if(i.check())
        {
            writer.write_integer(key, i());
            return;
        }

        py::extract<std::string> str(obj);

        if(str.check())
        {
            writer.write_string(key, str());
            return;
        }

        py::extract<json::float_t> f(obj);

        if(f.check())
        {
            writer.write_float(key, f());
            return;
        }

        py::extract<bool> b(obj);

        if(b.check())
        {
            writer.write_boolean(key, b());
            return;
        }


        // auto datetime = py::import("datetime");
        PyDateTime_IMPORT;
        bool is_datetime = Py_TYPE(obj.ptr()) == PyDateTimeAPI->DateTimeType;

        // just assuming it is datetime for now
        if(is_datetime)
        {
            tm value;
            value.tm_year = py::extract<int>(obj.attr("year"));
            value.tm_mon = py::extract<int>(obj.attr("month"));
            value.tm_mday = py::extract<int>(obj.attr("day"));
            value.tm_hour = py::extract<int>(obj.attr("hour"));
            value.tm_min = py::extract<int>(obj.attr("minute"));
            value.tm_sec = py::extract<int>(obj.attr("second"));

            writer.write_datetime(key, value);
            return;
        }

        throw std::runtime_error("Python to BSON conversion failed: Unknown object type");
    }

    const py::object &root;
    std::stack<const py::object *> parse_stack;

    bitstream result;
    json::Writer writer;
};
