#include "credb/Witness.h"

#ifdef IS_ENCLAVE
#include <sgx_tcrypto.h>
#else
#include "credb/ucrypto/ucrypto.h"
#endif

namespace credb
{

std::vector<event_range_t> Witness::get_boundaries() const
{
    std::vector<event_range_t> result;

    bitstream view;
    view.assign(data().data(), data().size(), true);

    json::Document doc(view);
    json::Document ops(doc, OP_FIELD_NAME);

    auto size = ops.get_size();

    for(uint32_t i = 0; i < size; ++i)
    {
        json::Document entry(ops, i);

        shard_id_t shard = json::Document(entry, SHARD_FIELD_NAME).as_integer();
        block_id_t block = json::Document(entry, BLOCK_FIELD_NAME).as_integer();
        event_index_t index = json::Document(entry, INDEX_FIELD_NAME).as_integer();

        bool found = false;

        for(auto &e : result)
        {
            if(e.shard != shard)
            {
                continue;
            }

            if(e.start_block > block)
            {
                e.start_block = block;
                e.start_index = index;
            }
            else if(e.start_block == block || e.start_index > index)
            {
                e.start_index = index;
            }

            if(e.end_block < block)
            {
                e.end_block = block;
                e.end_index = index;
            }
            else if(e.end_block == block || e.end_index < index)
            {
                e.end_index = index;
            }

            found = true;
            break;
        }

        if(!found)
        {
            result.push_back({ shard, block, block, index, index });
        }
    }

    return result;
}

bool Witness::valid(const sgx_ec256_public_t &public_key) const
{
    sgx_ecc_state_handle_t ecc_state = nullptr;
    sgx_ecc256_open_context(&ecc_state);

    sgx_generic_ecresult_t ecresult;

    auto ret = sgx_ecdsa_verify(m_data.data(), m_data.size(), &public_key, &m_signature, &ecresult, ecc_state);

    sgx_ecc256_close_context(ecc_state);

    return ret == Status::SUCCESS && ecresult == SGX_EC_VALID;
}

#ifndef IS_ENCLAVE
std::string Witness::pretty_print_content(int indent) const
{
    return digest().pretty_str(indent);
}

std::string Witness::armor() const
{
    bitstream bstream;
    bstream << data() << signature();
    const std::string base64 =
    base64_encode(reinterpret_cast<unsigned char const *>(bstream.data()), bstream.size());

    std::string s;
    s += BEGIN_MESSAGE;
    s += '\n';
    std::string::size_type pos = 0;
    const std::string::size_type per_line = 64;
    for(; pos + per_line < base64.size(); pos += per_line)
    {
        s += base64.substr(pos, per_line);
        s += '\n';
    }
    if(pos < base64.size())
    {
        s += base64.substr(pos);
        s += '\n';
    }
    s += END_MESSAGE;
    s += '\n';

    return s;
}
#endif

OrderResult order(Witness &w1, Witness &w2)
{
    auto b1 = w1.get_boundaries();
    auto b2 = w2.get_boundaries();

    // Always compare the smaller to the bigger one
    if(b2.size() < b1.size())
    {
        std::swap(b1, b2);
    }

    for(auto &e1 : b1)
    {
        bool found = false;

        for(auto &e2 : b2)
        {
            if(e1.shard == e2.shard)
            {
                found = true;
                break;
            }
        }

        if(!found)
        {
            return OrderResult::Unknown;
        }
    }

    OrderResult r = OrderResult::Unknown;

    for(auto &e1 : b1)
    {
        for(auto &e2 : b2)
        {
            if(e1.shard == e2.shard)
            {
                auto r_ = order(e1, e2);

                if(r == OrderResult::Unknown)
                {
                    r = r_;
                }
                else if(r != r_)
                {
                    return OrderResult::Concurrent;
                }

                break;
            }
        }
    }

    return r;
}

} // namespace credb
