#pragma once

#include "IsolationLevel.h"
#include "defines.h"

namespace credb
{

class Collection;
class Client;
class WitnessHandle;

/**
 * Holds information about the outcome of a transaction commit
 */
struct TransactionResult
{
    /// Did the commit of the transaction succeed
    bool success;

    /// (If unsuccessful) what was the error?
    std::string error;

    /// The witness certifying the execution of the transaction (if requested)
    Witness witness;
};

/**
 * @brief Interface to a serializeable transaction
 */
class Transaction
{
public:
    Transaction(const Transaction &other) = delete;

    virtual ~Transaction() = default;

    /**
     * @label{Transaction_get_collection}
     * @brief Get the handle for a collection. This may create the collection on the server side.
     *
     * @note there are no isolation guarantees for creating a collection
     */
    virtual std::shared_ptr<Collection> get_collection(const std::string &name) = 0;

    /**
     * @brief Shorthand for get_collection
     */
    std::shared_ptr<Collection> operator[](const std::string &name)
    {
        return get_collection(name);
    }

    /**
     * @label{Transaction_commit}
     * @brief Commit the transaction
     */
    virtual TransactionResult commit(bool generate_witness) = 0;

protected:
    /**
     * Constructor
     *
     * @param isolation
     *      The isolation level to use
     */
    Transaction(IsolationLevel isolation) : m_isolation(isolation) {}

    /// The isolation level of the transaction
    const IsolationLevel m_isolation;
};
} // namespace credb
