/** @file */

#pragma once

#ifdef IS_ENCLAVE
#include <sgx_tcrypto.h>
#else
#include "base64.h"
#include "ucrypto/ucrypto.h"
#include <cstdint>
#include <istream>
#include <limits>
#include <ostream>
#include <sstream>
#endif

#include "event_id.h"
#include <json/json.h>
#include <tuple>
#include <vector>

namespace credb
{

/**
 * @label{Witness}
 * @brief Witnesses hold a record about event(s) on the datastore's timeline
 */
class Witness
{
private:
    static constexpr const char *BEGIN_MESSAGE = "-----BEGIN CREDB WITNESS MESSAGE-----";
    static constexpr const char *END_MESSAGE = "-----END CREDB WITNESS MESSAGE-----";

public:
    /// Name of the JSON-field that holds the operations
    static constexpr const char *OP_FIELD_NAME = "operations";

    /// Name of the JSON-field that holds the shard number
    static constexpr const char *SHARD_FIELD_NAME = "shard";

    /// Name of the JSON-field that holds the block number
    static constexpr const char *BLOCK_FIELD_NAME = "block";

    /// Name of the JSON-field holding the block offset
    static constexpr const char *INDEX_FIELD_NAME = "index";
 

    /**
     * Default constructor
     */
    Witness() = default;

    /**
     * Copy constructor
     */
    Witness(const Witness &other) : m_data(other.m_data.duplicate(true)), m_signature(other.m_signature)
    {
    }

    /**
     * Move constructor
     */
    Witness(Witness &&other) : m_data(std::move(other.m_data)), m_signature(other.m_signature) {}

    /**
     * Copy using assignment
     */
    Witness &operator=(const Witness &other)
    {
        m_data = other.m_data.duplicate(true);
        m_signature = other.m_signature;
        return *this;
    }

    /**
     * @label{Witness_valid}
     * @brief Verify the witness against a signature
     */
    bool valid(const sgx_ec256_public_t &public_key) const;

    /**
     * @label{Witness_digest}
     * @brief Returns the data in JSON form
     */
    json::Document digest() const
    {
        return json::Document(m_data.data(), m_data.size(), json::DocumentMode::ReadOnly);
    }

    /**
     * @label{Witness_data}
     * @brief Returns the raw underlying data
     */
    const bitstream &data() const { return m_data; }

    /**
     * @label{Witness_set_data}
     * @brief Set the witness data
     */
    void set_data(bitstream &&data) { m_data = std::move(data); }

    /**
     * Get the signature (ensuring authenticity) of the witness
     */
    const sgx_ec256_signature_t &signature() const { return m_signature; }

    /**
     * Read a witness from a bitstream
     */
    friend bitstream &operator>>(bitstream &bs, Witness &witness)
    {
        return bs >> witness.m_data >> witness.m_signature;
    }

    /**
     * Write a witness to a bitstream
     */
    friend bitstream &operator<<(bitstream &bs, Witness &witness)
    {
        return bs << witness.m_data << witness.m_signature;
    }

    /**
     * @label{Witness_get_boundaries}
     * @brief Get the extends of the witness on the datastore's timeline
     */
    std::vector<event_range_t> get_boundaries() const;

#ifndef IS_ENCLAVE
    /**
     * @label{Witness_is_valid}
     * @brief Validate the witness against a sever's public key
     */ 
    bool is_valid(const std::string &public_key_base64) const
    {
        auto decoded = base64_decode(public_key_base64);
        if(decoded.size() != sizeof(sgx_ec256_public_t))
            throw std::runtime_error("invalid public key length");
        sgx_ec256_public_t key;
        memcpy(&key, decoded.c_str(), sizeof(key));
        return valid(key);
    }

    /**
     * @label{Witness_armor}
     * @brief Returns a base 64 encoded representation of the witness
     */
    std::string armor() const;

    /**
     * Convert a stream to a string
     */
    friend std::istream &operator>>(std::istream &in, Witness &witness)
    {
        std::string line;
        if(!std::getline(in, line))
        {
            throw std::runtime_error("Failed to getline");
        }

        if(line != Witness::BEGIN_MESSAGE)
        {
            throw std::runtime_error("The line doesn't start with BEGIN_MESSAGE");
        }

        std::string s;
        while(true)
        {
            if(!std::getline(in, line))
            {
                throw std::runtime_error("Failed to getline");
            }

            if(line == END_MESSAGE)
            {
                break;
            }
            else
            {
                s += line;
            }
        }

        const std::string decoded = base64_decode(s);
        bitstream bstream;
        bstream.assign(reinterpret_cast<const uint8_t *>(decoded.c_str()), decoded.size(), true);
        bstream >> witness;

        return in;
    }

    /**
     * Explicitly construct the witness from a string representation
     */
    explicit Witness(const std::string &base64)
    {
        std::istringstream ss(base64);
        ss >> *this;
    }

    /**
     * @label{Witness_pretty_print_content}
     * @brief Create a human-readable textual representation of the witness
     */
    std::string pretty_print_content(int indent) const;
#endif

private:
    bitstream m_data;
    sgx_ec256_signature_t m_signature;
};

/**
 * @brief Order two witnesses with respect to each other
 */
OrderResult order(const Witness &w1, const Witness &w2);

/**
 * @brief Order two event ranges with respect to each other
 */
inline OrderResult order(const event_range_t &first, const event_range_t &second)
{
    // FIXME detect whether even relates to the same server
    if(first.shard != second.shard)
    {
        return OrderResult::Unknown;
    }

    if((first.end_block < second.start_block) ||
       (first.end_block == second.start_block && first.end_index < second.start_index))
    {
        return OrderResult::OlderThan;
    }
    else if((first.start_block > second.end_block) ||
            (first.end_block == second.start_block && first.start_index > second.end_index))
    {
        return OrderResult::NewerThan;
    }
    else
    {
        return OrderResult::Concurrent;
    }
}
} // namespace credb
