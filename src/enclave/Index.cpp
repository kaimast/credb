#include "Index.h"

namespace credb
{
namespace trusted
{

Index::Index(const std::string &name, const std::vector<std::string> &paths)
: m_name(name), m_paths(paths)
{
}

Index::~Index() = default;

const std::vector<std::string> &Index::paths() const { return m_paths; }

const std::string &Index::name() const { return m_name; }


HashIndex::HashIndex(BufferManager &buffer, const std::string &name, const std::vector<std::string> &paths)
: Index(name, paths), m_map(buffer, name)
{
    if(paths.size() != 1)
    {
        throw std::runtime_error("HashIndex doesn't support multiple paths for now");
    }
}

HashIndex::~HashIndex() = default;

void HashIndex::dump_metadata(bitstream &output)
{
    output << m_name << m_paths;
}

HashIndex *HashIndex::new_from_metadata(BufferManager &buffer, bitstream &input)
{
    std::string name, prefix;
    std::vector<std::string> paths;
    input >> name >> paths;
    auto index = new HashIndex(buffer, name, paths);
    //index->m_map.load_metadata(input);
    return index;
}

bool HashIndex::matches_query(const json::Document &predicate) const
{
    try
    {
        for(const auto &path : paths())
        {
            json::Document view(predicate, path, true);
            const auto type = view.get_type();
            if(type == json::ObjectType::Map)
            {
                for(uint32_t pos = 0; pos < view.get_size(); ++pos)
                {
                    auto key = view.get_key(pos);
                    
                    if(!key.empty() && key[0] == '$')
                    {
                        // check operators
                        // TODO: recursively check operators (maybe need to derive from
                        // json::Iterator)
                        // TODO after TODO: recursive checking is slow, but the predicate may be in
                        // the same format.
                        //                  some mechanism to speed up? e.g. pre-compiling
                        //                  predicate?
                        if(key != "$in")
                        {
                            return false;
                        }
                    }
                }
            }
        }
        return true;
    }
    catch(json_error &e)
    {
        return false; // not all paths found
    }
}

bool HashIndex::insert(const json::Document &document, const std::string &key)
{
    try
    {
        json::Document view(document, paths(), true);
        m_map.insert(view.hash(), key);
        return true;
    }
    catch(json_error &e)
    {
        return false;
    }
}

void HashIndex::clear() { m_map.clear(); }

bool HashIndex::remove(const json::Document &document, const std::string &key)
{
    try
    {
        json::Document view(document, paths(), true);
        return m_map.remove(view.hash(), key);
    }
    catch(json_error &)
    {
        return false;
    }
}

void HashIndex::find(const json::Document &predicate, std::unordered_set<std::string> &out, SetOperation op)
{
    assert(paths().size() == 1);
    const std::string &vkey = paths()[0];
    json::Document in(predicate, vkey + ".$in");
    if(in.empty())
    {
        // only equality test
        m_map.find(predicate.hash(), out, op);
    }
    else
    {
        // $in operator support
        // note: $in with SetOperation::Intersect is slower and use extra space
        if(in.get_type() != json::ObjectType::Array)
        {
            throw std::runtime_error("$in operand is not an array");
        }

        auto *set = op == SetOperation::Intersect ? new std::unordered_set<std::string> : &out;

        for(uint32_t i = 0; i < in.get_size(); ++i)
        {
            json::Document view2(in, to_string(i));

            bitstream bstream;
            json::Writer writer(bstream);
            writer.start_map("");
            writer.write_document(vkey, view2);
            writer.end_map();

            json::Document doc(bstream.data(), bstream.size(), json::DocumentMode::ReadOnly);
            m_map.find(doc.hash(), *set, SetOperation::Union); // union first
        }

        if(op == SetOperation::Intersect)
        {
            // manually do set intersection here
            for(auto it = out.begin(); it != out.end();)
            {
                if(!set->count(*it))
                {
                    it = out.erase(it);
                }
                else
                {
                    ++it;
                }
            }
            delete set;
        }
    }
}

size_t HashIndex::estimate_value_count(const json::Document &predicate)
{
    assert(paths().size() == 1);
    const std::string &vkey = paths()[0];
    json::Document in(predicate, vkey + ".$in");
    if(in.empty())
    {
        // only equality test
        return m_map.estimate_value_count(predicate.hash());
    }
    else
    {
        // $in operator support
        // because $in with SetOperation::Intersect will fetch the whole value set anyway,
        // the heuristic here is that returning the smallest number,
        // making sure this hash index is (highly probably) the first one to evaluate,
        // thus $in will be (highly probably) with SetOperation::Union.
        // this heuristic will not reduce time costs, but will avoid one times extra space.
        return 0;
    }
}


} // namespace trusted
} // namespace credb
