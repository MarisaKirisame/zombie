#include "common.hpp"
#include "zombie/heap/heap.hpp"

#include <gtest/gtest.h>

#include <unordered_map>
#include <random>
#include <limits>

const uint_fast32_t RANDOM_SEED = 114514;
const int ITERATION = 2000000;

std::unordered_map<int64_t, size_t> valueToIdx;

std::mt19937 eng(RANDOM_SEED);
std::uniform_int_distribution<uint64_t> distrib(std::numeric_limits<uint64_t>::min(), std::numeric_limits<uint64_t>::max());
std::uniform_int_distribution<int64_t> choiceDistrib(1, 100);
std::uniform_int_distribution<int64_t> idDistrib(std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max());

AffFunction random_aff()
{
    __int128_t slope = choiceDistrib(eng);
    int64_t off = choiceDistrib(eng);
    if (slope > 0)
    {
        slope = -slope;
    }
    if (off > 0)
    {
        off = -off;
    }
    return AffFunction(slope, off);
}

struct NotifyIndexChangedCustom
{
    void operator()(const int64_t& n, const size_t& idx)
    {
        valueToIdx.insert(std::make_pair(n, idx));
    }
};

template <KineticHeapImpl impl>
void IndexSyncTest()
{
    valueToIdx.clear();
    KineticHeap<impl, int64_t, NotifyIndexChangedCustom> kh(0);

    for (uint64_t i = 0; i < ITERATION; i++)
    {
        auto choice = choiceDistrib(eng);
        kh.advance_to(i);
        if (choice <= 60) {
            kh.push(idDistrib(eng), random_aff());
        } else if (choice <= 80) {
            if (!kh.empty())
            {
                auto k = kh.pop();
                valueToIdx.erase(k);
            }
        } else {
            for (const auto& entry : valueToIdx)
            {
                EXPECT_TRUE(kh.has_value(entry.second));
                EXPECT_EQ(kh[entry.second], entry.first);
            }
        }
    }
}

TEST(DtrTest, IndexSync)
{
    IndexSyncTest<KineticHeapImpl::Hanger>();
}