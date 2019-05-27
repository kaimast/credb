/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#pragma once

#include "BufferManager.h"
#include "MultiMap.h"
#include "util/defines.h"
#include <json/json.h>
#include <unordered_set>

namespace credb
{
namespace trusted
{

class Enclave;

/// Generic index interface
class Index
{
public:
    Index(const std::string &name, const std::vector<std::string> &paths);
    virtual ~Index();

    const std::vector<std::string> &paths() const;
    const std::string &name() const;

    /**
     * Check if two documents are equal for the paths relevant to this index
     */
    bool compare(const json::Document &first, const json::Document &other) const;

    virtual bool matches_query(const json::Document &predicate) const = 0;
    virtual void clear() = 0;
    virtual bool insert(const json::Document &document, const std::string &key) = 0;
    virtual bool remove(const json::Document &document, const std::string &key) = 0;
    virtual void
    find(const json::Document &predicate, std::unordered_set<std::string> &out, SetOperation op) = 0;
    virtual size_t estimate_value_count(const json::Document &predicate) = 0;
    virtual void dump_metadata(bitstream &output) = 0; // for debug purpose

protected:
    const std::string m_name;
    const std::vector<std::string> m_paths;
};


/// Index using a hash map internally
/// (only supports equality operations)
class HashIndex : public Index
{
public:
    HashIndex(BufferManager &buffer, const std::string &name, const std::vector<std::string> &paths);
    ~HashIndex();
    static HashIndex *new_from_metadata(BufferManager &buffer, bitstream &input);
    void dump_metadata(bitstream &output) override; // for debug purpose

    bool matches_query(const json::Document &predicate) const override;
    bool insert(const json::Document &document, const std::string &key) override;
    void clear() override;
    bool remove(const json::Document &document, const std::string &key) override;
    void find(const json::Document &predicate, std::unordered_set<std::string> &out, SetOperation op) override;
    size_t estimate_value_count(const json::Document &predicate) override;

private:
    MultiMap m_map;
};

inline bool Index::compare(const json::Document &first, const json::Document &second) const
{
    json::Document view1(first, paths());
    json::Document view2(second, paths());

    return view1 == view2;
}

 

} // namespace trusted
} // namespace credb
