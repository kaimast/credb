#include "credb/Witness.h"

#ifdef IS_ENCLAVE
#include <sgx_tcrypto.h>
#else
#include "credb/ucrypto/ucrypto.h"
#endif

namespace credb
{

transaction_bounds_t Witness::get_boundaries() const
{
    transaction_bounds_t result;

    bitstream view;
    view.assign(data().data(), data().size(), true);

    json::Document doc(view);
    json::Document ops(doc, OP_FIELD_NAME);

    auto size = ops.get_size();

    for(size_t i = 0; i < size; ++i)
    {
        json::Document entry(ops, i);

        shard_id_t shard = json::Document(entry, SHARD_FIELD_NAME).as_integer();
        block_id_t block = json::Document(entry, BLOCK_FIELD_NAME).as_integer();
        block_index_t index = json::Document(entry, INDEX_FIELD_NAME).as_integer();

        auto it = result.find(shard);

        if(it == result.end())
        {
            event_range_t range = {block, block, index, index };
            result.emplace(shard, range);
        }
        else
        {
            auto &e = it->second;

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
            break;
        }
    }

    return result;
}

bool Witness::valid(const sgx_ec256_public_t &public_key) const
{
    sgx_ecc_state_handle_t ecc_state = nullptr;
    sgx_ecc256_open_context(&ecc_state);

    uint8_t ecresult;

    auto ret = sgx_ecdsa_verify(m_data.data(), m_data.size(), &public_key, const_cast<sgx_ec256_signature_t*>(&m_signature), &ecresult, ecc_state);

    sgx_ecc256_close_context(ecc_state);

    return ret == SGX_SUCCESS && ecresult == SGX_EC_VALID;
}

std::istream& operator>>(std::istream &in, Witness &witness)
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

        if(line == Witness::END_MESSAGE)
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

    return order(b1, b2);

}

} // namespace credb
