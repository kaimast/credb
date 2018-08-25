#include "DocParser.h"

#define BOOST_RESULT_OF_USE_DECLTYPE
#define BOOST_SPIRIT_USE_PHOENIX_V3

#include <boost/phoenix/bind/bind_function.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/support_istream_iterator.hpp>
#include <cowlang/cow.h>
#include <fstream>
#include <glog/logging.h>

namespace credb
{

using namespace boost::spirit;

void print(const std::string &str) { std::cout << str << std::endl; }

template <typename Iterator>
struct obj_grammar : qi::grammar<Iterator, qi::unused_type(), qi::space_type>
{
    obj_grammar() : obj_grammar::base_type(start)
    {
        using ascii::char_;
        using boost::phoenix::bind;
        using qi::_1;
        using qi::eps;
        using qi::int_;
        using qi::lexeme;
        using qi::lit;
        using qi::no_skip;
        using qi::string;

        array = char_('[') >> *(char_(' ')) >> char_(']') >> *(char_('\n'));

        dictentry = key >> ':' >> string("dict") >> string(":=") >>
                    char_('{')[bind([&]() { writer.start_map(current_key); })] >> *(char_(' ')) >>
                    char_('}')[bind([&]() { writer.end_map(); })];

        code = +~char_(";");

        key = (+char_("a-zA-Z_"))[bind(
        [&](const std::vector<char> &v) { current_key = std::string(v.begin(), v.end()); }, _1)];

        intentry = key >> ':' >> string("int") >> string(":=") >>
                   int_[bind([&](int val) { writer.write_integer(current_key, val); }, _1)];

        funcentry = key >> ':' >> string("func") >> string(":=") >>
                    no_skip[code[bind(
                    [&](const std::string &c) {
                        try
                        {
                            auto bin = cow::compile_string(c);
                            writer.write_binary(current_key, bin);
                        }
                        catch(std::runtime_error &e)
                        {
                            LOG(ERROR) << "failed to compile code: \n" << c;
                        }
                    },
                    _1)]] >>
                    char_(";");

        start = eps > char_('{')[bind([&]() { writer.start_map(""); })] >>
                *(intentry | funcentry | dictentry) >>
                lit('}')[bind([&]() { writer.end_map(); })] >> *(char_('\n'));

        key.name("key");
        dictentry.name("dict entry");
        intentry.name("int entry");
        funcentry.name("func entry");
        start.name("root");

        qi::on_error<qi::fail>(start, boost::phoenix::ref(std::cout)
                                      << "Error! Expected " << qi::_4 << " but got: '"
                                      << boost::phoenix::construct<std::string>(qi::_3, qi::_2) << "'\n");
    }

    qi::rule<Iterator, qi::unused_type(), qi::space_type> dictentry, array, intentry, funcentry, key, start;
    qi::rule<Iterator, std::string()> code;

    json::Writer writer;
    std::string current_key;
    std::string current_code;
};

json::Document parse_document_file(const std::string &filename)
{
    using qi::char_;
    using qi::phrase_parse;
    using qi::space;

    std::ifstream file(filename);
    file.unsetf(std::ios::skipws);

    if(!file)
    {
        throw std::runtime_error("cannot open object file");
    }

    auto fstart = istream_iterator(file);
    auto fend = istream_iterator();

    obj_grammar<istream_iterator> parser;

    auto res = phrase_parse(fstart, fend, parser, space);

    if(!res)
    {
        throw std::runtime_error("Failed to parse object file");
    }

    if(fstart != fend)
    {
        throw std::runtime_error("Couldn't read until the end");
    }

    return parser.writer.make_document();
}

} // namespace credb
