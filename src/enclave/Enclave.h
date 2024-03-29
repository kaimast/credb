/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#pragma once

#ifdef FAKE_ENCLAVE
#include "credb/ucrypto/ucrypto.h"
#else
#include <sgx_tcrypto.h>
#endif

#include "BufferManager.h"
#include "Counter.h"
#include "EncryptedIO.h"
#include "Ledger.h"
#include "TransactionLedger.h"
#include "RemoteParties.h"
#include "TaskManager.h"
#include "TransactionManager.h"
#include "util/IdentityDatabase.h"
#include "util/status.h"
#include "util/types.h"
#include <bitstream.h>
#include <string>
#include <unordered_set>

namespace credb
{
namespace trusted
{

/// This is the main enclave class
/// It mostly keeps track of encryption keys
class Enclave
{
public:
    Enclave();
    Enclave(const Enclave &other) = delete;
    Enclave &operator=(const Enclave &other) = delete;

    ~Enclave();

    // This should only be called by credb_enclave_init;
    credb_status_t init(const std::string &name);

    const std::string &name() const { return m_identity->name(); }

    const sgx_ec256_public_t &public_key() const { return m_identity->public_key(); }

    const sgx_ec256_public_t &upstream_public_key() const { return m_upstream_public_key; }

    uint64_t get_unique_id() const;

    const sgx_ec256_private_t& private_key()
    {
        return m_private_key;
    }

    /**
     * Get identity information about the node this enclave represents
     */
    const Identity &identity() const
    {
        assert(m_identity);
        return *m_identity;
    }

    EncryptedIO &encrypted_io() { return *m_encrypted_io; }
    RemoteParties &remote_parties() { return m_remote_parties; }

    BufferManager &buffer_manager() { return m_buffer_manager; }
    TaskManager &task_manager() { return m_task_manager; }
    TransactionManager &transaction_manager() { return m_transaction_manager; }
    Ledger &ledger() { return m_ledger; }
    TransactionLedger &transaction_ledger() { return m_transaction_ledger; }

    bool read_from_disk(const std::string &filename, bitstream &data);
    bool read_from_local_disk(const std::string &filename, bitstream &data);
    bool read_from_upstream_disk(const std::string &filename, bitstream &data);
    bool write_to_disk(const std::string &filename, const bitstream &data);
    void remove_from_disk(const std::string &filename);
    bool dump_everything(const std::string &filename); // for debug purpose
    bool load_everything(const std::string &filename); // for debug purpose

    void set_upstream(remote_party_id upstream_id);
    bool is_downstream_mode() const;
    remote_party_id get_upstream_id() const { return m_upstream_id; }

    IdentityDatabase &identity_database() { return m_identity_database; }

    //TODO move upstream specific stuff to its own class

    bool set_trigger(const std::string &collection, remote_party_id identifier);
    bool unset_trigger(const std::string &collection, remote_party_id identifier);

    std::unordered_set<remote_party_id> get_triggers(const std::string &collection);

private:
    std::unique_ptr<EncryptedIO> m_encrypted_io;
    TaskManager m_task_manager;
    TransactionManager m_transaction_manager;
    BufferManager m_buffer_manager;
    Ledger m_ledger;
    TransactionLedger m_transaction_ledger;
    IdentityDatabase m_identity_database;

    /**
     * Pointer to the identity of the CreDB instance
     * Memory will be managed by the identity database
     */
    Identity *m_identity;

    RemoteParties m_remote_parties;

    sgx_ec256_private_t m_private_key;
    sgx_ec256_public_t m_upstream_public_key;

    //Only used for downstream
    std::unordered_map<std::string, std::unordered_set<remote_party_id>> m_triggers;
    std::mutex m_trigger_mutex;

    bool m_downstream_mode = false;
    remote_party_id m_upstream_id = INVALID_REMOTE_PARTY;
};

extern Enclave *g_enclave;

} // namespace trusted
} // namespace credb
