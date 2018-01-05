/** @file */
#pragma once

#include <cowlang/cow.h>
#include <cstdint>
#include <json/json.h>
#include <memory>
#include <set>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "IsolationLevel.h"
#include "Witness.h"
#include "defines.h"

#include "ucrypto/ucrypto.h"

namespace credb
{

class Collection;
class Transaction;

/**
 * Main interface to interact with a server through the client API
 */
class Client
{
public:
    Client() {}
    virtual ~Client() = default;

    Client(const Client &other) = delete;

    /**
     * @label{Client_get_collection}
     * @brief Get the handle for a collection. This may create the collection on the server side
     */
    virtual std::shared_ptr<Collection> get_collection(const std::string &name) = 0;

    /**
     * @brief Shorthand for get_collection
     */
    std::shared_ptr<Collection> operator[](const std::string &name) { return get_collection(name); }

    /**
     * Ship code to the server and execute it
     */
    virtual cow::ValuePtr execute(const std::string &code, const std::vector<std::string> &args = {}) = 0;

    /**
     * Get the certificate of the server's public key
     */
    virtual const sgx_ec256_public_t &get_server_cert() const = 0;

    /**
     * Get the certificate of the server's public key
     * (base 64 encoded)
     */
    virtual std::string get_server_cert_base64() const = 0;

    /**
     * @brief Initialize a new transaction object
     * @note isolation
     *      The isolation level to use
     */
    virtual std::shared_ptr<Transaction> init_transaction(IsolationLevel isolation = IsolationLevel::Serializable) = 0;

    /**
     * @brief Establish a connection between the server and a remove party
     */
    virtual bool peer(const std::string &remote_addr) = 0;
 
    /**
     * @brief List all other servers the server is connected to
     */
    virtual std::vector<json::Document> list_peers() = 0;

    /**
     * Send a no-op (an operation that doesn't do anything) to the server
     * @note this function is provided for debugging purposes only
     */
    virtual bool nop(const std::string &garbage)  = 0;
   
    /// For development TODO REMOVE 
    virtual bool dump_everything(const std::string &filename) = 0;
 
    /// For development TODO REMOVE 
    virtual bool load_everything(const std::string &filename) = 0;

    /**
     * Get the name of this client
     */
    virtual const std::string &name() const = 0;
};

/**
 * @brief Shorthand for a pointer to a client connection
 */
typedef std::shared_ptr<Client> ClientPtr;

/**
 * @label{create_client}
 * @brief Create a new client connection
 *
 * @param client_name
 *      Name to identify the clien twith
 * @param server_name
 *      Name of the server to connect to
 * @param server_addr
 *      Hostname of the machine hosting the server
 * @param server_port [optional]
 *      Port of the server (if non-standard)
 */
ClientPtr create_client(const std::string &client_name, const std::string &server_name,
                        const std::string &server_addr, uint16_t server_port = 0);

} // namespace credb
