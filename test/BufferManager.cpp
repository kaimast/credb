#include <cstdlib>
#include <algorithm>
#include <gtest/gtest.h>

#include "../src/enclave/Enclave.h"
#include "../src/enclave/LocalEncryptedIO.h"
#include "../src/enclave/BufferManager.h"
#include "../src/enclave/EvictionAlgorithm.h"

using namespace credb;
using namespace credb::trusted;

class BufferManagerTest : public testing::Test
{
};

TEST(BufferManagerTest, LruEviction)
{
    LruEviction algo;
    for (page_no_t i = 0; i < 10; ++i)
        algo.touch(i);

    ASSERT_NO_THROW(algo.remove(1000));
    ASSERT_EQ(algo.evict(), 0);
    ASSERT_EQ(algo.evict(), 1);
    algo.remove(2);
    ASSERT_EQ(algo.evict(), 3);
    algo.touch(4);
    ASSERT_EQ(algo.evict(), 5);
}

namespace {

class TestPage : public Page
{
    size_t m_size;
    uint8_t *m_data;

public:
    TestPage(BufferManager& buffer, page_no_t page_no, size_t size):
        Page(buffer, page_no), m_size(size), m_data(new uint8_t[size])
    {
        for (size_t i = 0; i < m_size; i++)
            m_data[i] = static_cast<uint8_t>(rand());
    }

    TestPage(BufferManager& buffer, page_no_t page_no, bitstream &bstream):
        Page(buffer, page_no)
    {
        bstream >> m_size;
        uint8_t *data = nullptr;
        bstream.read_raw_data(&data, m_size);
        m_data = new uint8_t[m_size];
        memcpy(m_data, data, m_size);
    }

    bitstream serialize() const override
    {
        bitstream bstream;
        bstream << m_size;
        bstream.write_raw_data(m_data, m_size);
        return bstream;
    }

    size_t byte_size() const override
    {
        return sizeof(*this) + m_size;
    }

    uint8_t get(size_t i) const
    {
        return m_data[i];
    }

    void set(size_t i, uint8_t v) const
    {
        m_data[i] = v;
        buffer_manager().mark_page_dirty(this->page_no());
    }
};

} // anonymous namespace

TEST(BufferManagerTest, new_and_get_page)
{
    LocalEncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<10);
    const size_t size = 100;

    auto page1 = buffer.new_page<TestPage>(size);
    auto page2 = buffer.get_page<TestPage>(page1->page_no());
    ASSERT_EQ(&*page1, &*page2);
}

TEST(BufferManagerTest, handle_point_to_same_page)
{
    LocalEncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<10);
    const size_t size = 100;

    auto page1 = buffer.new_page<TestPage>(size);
    auto page2 = buffer.get_page<TestPage>(page1->page_no());

    const size_t pos = rand() % size;
    const uint8_t new_value = rand();
    ASSERT_EQ(page1->get(pos), page2->get(pos));
    page1->set(pos, new_value);
    ASSERT_EQ(page1->get(pos), page2->get(pos));
}

TEST(BufferManagerTest, reload_page)
{
    const int n = 100;
    const size_t buffer_size = 1<<10;
    const size_t page_size = buffer_size / (n / 10);
    LocalEncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", buffer_size);

    std::vector<std::tuple<page_no_t, bitstream>> v1;
    for (int i = 0; i < n; ++i)
    {
        auto page = buffer.new_page<TestPage>(page_size);
        v1.emplace_back(page->page_no(), page->serialize());
    }

    std::vector<std::tuple<page_no_t, bitstream>> v2;
    std::random_shuffle(v1.begin(), v1.end());
    for (const auto& [page_no, data] : v1)
    {
        auto page = buffer.get_page<TestPage>(page_no);
        ASSERT_EQ(page->serialize(), data);

        const size_t pos = rand() % page_size;
        const uint8_t new_value = rand();
        page->set(pos, new_value);
        v2.emplace_back(page->page_no(), page->serialize());
    }

    std::random_shuffle(v2.begin(), v2.end());
    for (const auto& [page_no, data] : v2)
    {
        auto page = buffer.get_page<TestPage>(page_no);
        ASSERT_EQ(page->serialize(), data);
    }
}

