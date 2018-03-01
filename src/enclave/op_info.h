/**
 * Structures holding information about pending operations in a transaction
 */

#pragma once

#include <string>
#include <array>
#include <json/Document.h>
#include "credb/event_id.h"
#include "util/OperationType.h"
#include "Ledger.h"

namespace credb
{
namespace trusted
{

class Transaction;

/**
 * Abstract interface for a operation to be processed by a transaction
 */
struct operation_info_t
{
public:
    virtual OperationType type() const = 0;
    virtual void collect_shard_lock_type() = 0;

    virtual void extract_reads(std::unordered_set<event_id_t> &read_set) = 0;

    virtual void extract_writes(std::array<uint16_t, NUM_SHARDS> &write_set) = 0;

    /**
     * validate that all the reads of this operations are up to date
     * and/or that the policy allows the access/modification
     *
     * @note the semantics of this depend on the isolation level
     */ 
    virtual bool validate() = 0;

    /**
     * Apply operation to the ledger
     *
     * For all read-only operation this should be no-op
     */
    virtual bool do_write(std::unordered_set<event_id_t> &read_set,
                          std::unordered_set<event_id_t> &write_set) = 0;

    operation_info_t(operation_info_t &other) = delete;
    operation_info_t() = default;
    virtual ~operation_info_t() = default;

protected:
    operation_info_t(Transaction &tx)
        : m_transaction(tx)
    {}

    Transaction& transaction()
    {
        return m_transaction;
    }

    shard_id_t get_shard(const std::string &collection, const std::string &full_path) const;

private:
    Transaction &m_transaction;
};

/**
 * A write-only operation that doesn't perform any reads
 */
struct write_op_t : public operation_info_t
{
public:
    void extract_reads(std::unordered_set<event_id_t> &read_set) override
    {
        (void)read_set;
    }
 
protected:
    using operation_info_t::operation_info_t;
};

/**
 * A read-only operation that doesn't perform any writes
 */ 
struct read_op_t : public operation_info_t
{
public:
    bool do_write(std::unordered_set<event_id_t> &read_set,
                  std::unordered_set<event_id_t> &write_set) override
    {
        (void)read_set;
        (void)write_set;
        return true;
    }

    void extract_writes(std::array<uint16_t, NUM_SHARDS> &write_set) override
    {
        (void) write_set;
    }

protected:
    using operation_info_t::operation_info_t;
};

struct check_obj_info_t : public read_op_t
{
public:
    check_obj_info_t(Transaction &tx, bitstream &req);

    check_obj_info_t(Transaction &tx, const std::string &collection, const std::string &key, const json::Document &predicates, bool result);

    OperationType type() const override
    {
        return OperationType::CheckObject;
    }

    void extract_reads(std::unordered_set<event_id_t> &read_set) override;

    void collect_shard_lock_type() override;

    bool validate() override;

private:
    std::string m_collection;
    std::string m_key;
    json::Document m_predicates;
    bool m_result;
    shard_id_t m_sid;
};

struct has_obj_info_t : public read_op_t
{
public:
    has_obj_info_t(Transaction &tx, bitstream &req);

    has_obj_info_t(Transaction &tx, const std::string &collection, const std::string &key, bool result);


    OperationType type() const override
    {
        return OperationType::HasObject;
    }

    void extract_reads(std::unordered_set<event_id_t> &read_set) override;

    void collect_shard_lock_type() override;

    bool validate() override;

private:
    std::string m_collection;
    std::string m_key;
    bool m_result;
    shard_id_t m_sid;
};

struct get_info_t : public read_op_t
{
public:
    get_info_t(Transaction &tx, bitstream &req);

    get_info_t(Transaction &tx, const std::string &collection, const std::string &key, const event_id_t eid);

    OperationType type() const override
    {
        return OperationType::GetObject;
    }

    void extract_reads(std::unordered_set<event_id_t> &read_set) override;

    void collect_shard_lock_type() override;

    bool validate() override;

private:
    std::string m_collection;
    std::string m_key;
    event_id_t m_eid;
    shard_id_t m_sid;
};

struct put_info_t : public write_op_t
{
public:
    put_info_t(Transaction &tx, bitstream &req);

    put_info_t(Transaction &tx, const std::string &collection, const std::string &key, const json::Document &doc);

    OperationType type() const override
    {
        return OperationType::PutObject;
    }

    bool validate() override;

    void extract_writes(std::array<uint16_t, NUM_SHARDS> &write_set) override;


    void collect_shard_lock_type() override;

    bool do_write(std::unordered_set<event_id_t> &read_set,
                  std::unordered_set<event_id_t> &write_set) override;

private:
    std::string m_collection;
    std::string m_key;
    shard_id_t m_sid;
    json::Document m_doc;
};

struct add_info_t : public write_op_t
{
public:
    add_info_t(Transaction &tx, bitstream &req);

    add_info_t(Transaction &tx, const std::string &collection, const std::string &key, const json::Document &doc);

    OperationType type() const override
    {
        return OperationType::AddToObject;
    }

    bool validate() override;

    void extract_writes(std::array<uint16_t, NUM_SHARDS> &write_set) override;
    
    void collect_shard_lock_type() override;

    bool do_write(std::unordered_set<event_id_t> &read_set,
                  std::unordered_set<event_id_t> &write_set) override;

private:
    std::string m_collection;
    std::string m_key;
    shard_id_t m_sid;
    json::Document m_doc;
};

struct remove_info_t : public write_op_t
{
public:
    remove_info_t(Transaction &tx, bitstream &req);

    OperationType type() const override
    {
        return OperationType::RemoveObject;
    }

    bool validate() override;

    void extract_writes(std::array<uint16_t, NUM_SHARDS> &write_set) override;
 
    void collect_shard_lock_type() override;

    bool do_write(std::unordered_set<event_id_t> &read_set,
                  std::unordered_set<event_id_t> &write_set) override;

private:
    std::string m_collection;
    std::string m_key;
    shard_id_t m_sid;
};

struct find_info_t : public read_op_t
{
public:
    find_info_t(Transaction &tx, bitstream &req);

    OperationType type() const override
    {
        return OperationType::FindObjects;
    }

    void collect_shard_lock_type() override;

    bool validate() override;
 
    void extract_reads(std::unordered_set<event_id_t> &read_set) override;

private:
    void write_witness(const std::string &key,
                       const event_id_t &eid,
                       const json::Document &value);

    bool validate_no_dirty_read();

    bool validate_repeatable_read();

    bool validate_no_phantom();

    std::string collection;
    json::Document predicates;
    std::vector<std::string> projection;
    std::vector<std::tuple<std::string, shard_id_t, event_id_t>> res;
    int32_t limit;
};

}
}
