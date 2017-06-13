// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include <vespa/vespalib/testkit/testapp.h>
#include <vespa/vespalib/stllike/asciistream.h>
#include <vespa/searchlib/common/growablebitvector.h>
#include <vespa/searchlib/common/partialbitvector.h>
#include <vespa/searchlib/common/rankedhit.h>
#include <vespa/searchlib/common/bitvectoriterator.h>
#include <vespa/searchlib/fef/termfieldmatchdata.h>
#include <vespa/searchlib/fef/termfieldmatchdataarray.h>
#include <vespa/searchlib/util/rand48.h>

using namespace search;

namespace {

std::string
toString(const BitVector & bv)
{
    std::stringstream ss;
    ss << "[";
    bool first = true;
    uint32_t nextBit = bv.getStartIndex();
    for (;;) {
        nextBit = bv.getNextTrueBit(nextBit);
        if (nextBit >= bv.size()) {
            break;
        }
        if (!first) {
            ss << ",";
        }
        ss << nextBit++;
        first = false;
    }
    ss << "]";
    return ss.str();
}


std::string
toString(BitVectorIterator &b)
{
    std::stringstream ss;
    ss << "[";
    bool first = true;
    b.initFullRange();
    for (uint32_t docId = 1; ! b.isAtEnd(docId); ) {
        if (!b.seek(docId)) {
            docId = std::max(docId + 1, b.getDocId());
            if (b.isAtEnd(docId))
                break;
            continue;
        }
        if (!first) {
            ss << ",";
        }
        b.unpack(docId);
        ss << docId++;
        first = false;
    }
    ss << "]";
    return ss.str();
}



uint32_t
myCountInterval(const BitVector &bv, uint32_t low, uint32_t high)
{
    uint32_t res = 0u;
    if (bv.size() == 0u)
        return 0u;
    if (high >= bv.size())
        high = bv.size() - 1;
    for (; low <= high; ++low) {
        if (bv.testBit(low))
            ++res;
    }
    return res;
}

void
scan(uint32_t count, uint32_t offset, uint32_t size, Rand48 &rnd)
{
    std::vector<uint32_t> lids;
    lids.reserve(count);
    uint32_t end = size + offset;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t lid = offset + (rnd.lrand48() % (size - 1)) + 1;
        lids.push_back(lid);
    }
    std::sort(lids.begin(), lids.end());
    lids.resize(std::unique(lids.begin(), lids.end()) - lids.begin());
    BitVector::UP bv(BitVector::create(offset, end));
    for (auto lid : lids) {
        bv->setBit(lid);
    }
    EXPECT_EQUAL(bv->getFirstTrueBit(), bv->getNextTrueBit(bv->getStartIndex()));
    uint32_t prevLid = bv->getStartIndex();
    for (auto lid : lids) {
        EXPECT_EQUAL(lid, bv->getNextTrueBit(prevLid + 1));
        EXPECT_EQUAL(prevLid, bv->getPrevTrueBit(lid - 1));
        prevLid = lid;
    }
    EXPECT_TRUE(bv->getNextTrueBit(prevLid + 1) >= end);
    EXPECT_EQUAL(prevLid, bv->getPrevTrueBit(end - 1));
}

void
scanWithOffset(uint32_t offset)
{
    Rand48 rnd;

    rnd.srand48(32);
    scan(10,      offset, 1000000, rnd);
    scan(100,     offset, 1000000, rnd);
    scan(1000,    offset, 1000000, rnd);
    scan(10000,   offset, 1000000, rnd);
    scan(100000,  offset, 1000000, rnd);
    scan(500000,  offset, 1000000, rnd);
    scan(1000000, offset, 1000000, rnd);
}

}

bool
assertBV(const std::string & exp, const BitVector & act)
{
    bool res1 = EXPECT_EQUAL(exp, toString(act));
    search::fef::TermFieldMatchData f;
    search::fef::TermFieldMatchDataArray a;
    a.add(&f);
    queryeval::SearchIterator::UP it(BitVectorIterator::create(&act, a, true));
    BitVectorIterator & b(dynamic_cast<BitVectorIterator &>(*it));
    bool res2 = EXPECT_EQUAL(exp, toString(b));
    return res1 && res2;
}

void
fill(BitVector & bv, const std::vector<uint32_t> & bits, uint32_t offset, bool fill=true)
{
    for (uint32_t bit : bits) {
        if (fill) {
            bv.setBit(bit + offset);
        } else {
            bv.clearBit(bit + offset);
        }
    }
}

vespalib::string
fill(const std::vector<uint32_t> & bits, uint32_t offset)
{
    vespalib::asciistream os;
    os << "[";
    size_t count(0);
    for (uint32_t bit : bits) {
        count++;
        os << bit + offset;
        if (count != bits.size()) { os << ","; }
    }
    os << "]";
    return os.str();
}

std::vector<uint32_t> A = {7, 39, 71, 103};
std::vector<uint32_t> B = {15, 39, 71, 100};

void
testAnd(uint32_t offset)
{
    uint32_t end = offset + 128;
    BitVector::UP v1(BitVector::create(offset, end));
    BitVector::UP v2(BitVector::create(offset, end));
    BitVector::UP v3(BitVector::create(offset, end));

    fill(*v1, A, offset);
    fill(*v3, A, offset);
    fill(*v2, B, offset);
    EXPECT_TRUE(assertBV(fill(A, offset), *v1));
    EXPECT_TRUE(assertBV(fill(B, offset), *v2));

    EXPECT_TRUE(assertBV(fill(A, offset), *v3));
    v3->andWith(*v2);
    EXPECT_TRUE(assertBV(fill({39,71}, offset), *v3));

    EXPECT_TRUE(assertBV(fill(A, offset), *v1));
    EXPECT_TRUE(assertBV(fill(B, offset), *v2));
}

void
testOr(uint32_t offset)
{
    uint32_t end = offset + 128;
    BitVector::UP v1(BitVector::create(offset, end));
    BitVector::UP v2(BitVector::create(offset, end));
    BitVector::UP v3(BitVector::create(offset, end));

    fill(*v1, A, offset);
    fill(*v3, A, offset);
    fill(*v2, B, offset);
    EXPECT_TRUE(assertBV(fill(A, offset), *v1));
    EXPECT_TRUE(assertBV(fill(B, offset), *v2));

    EXPECT_TRUE(assertBV(fill(A, offset), *v3));
    v3->orWith(*v2);
    EXPECT_TRUE(assertBV(fill({7,15,39,71,100,103}, offset), *v3));

    EXPECT_TRUE(assertBV(fill(A, offset), *v1));
    EXPECT_TRUE(assertBV(fill(B, offset), *v2));
}

void
testAndNot(uint32_t offset)
{
    uint32_t end = offset + 128;
    BitVector::UP v1(BitVector::create(offset, end));
    BitVector::UP v2(BitVector::create(offset, end));
    BitVector::UP v3(BitVector::create(offset, end));

    fill(*v1, A, offset);
    fill(*v3, A, offset);
    fill(*v2, B, offset);
    EXPECT_TRUE(assertBV(fill(A, offset), *v1));
    EXPECT_TRUE(assertBV(fill(B, offset), *v2));

    EXPECT_TRUE(assertBV(fill(A, offset), *v3));
    v3->andNotWith(*v2);
    EXPECT_TRUE(assertBV(fill({7,103}, offset), *v3));

    EXPECT_TRUE(assertBV(fill(A, offset), *v1));
    EXPECT_TRUE(assertBV(fill(B, offset), *v2));

    v3->clear();
    fill(*v3, A, offset);
    EXPECT_TRUE(assertBV(fill(A, offset), *v3));


    std::vector<RankedHit> rh;
    rh.emplace_back(15u+offset, 0.0);
    rh.emplace_back(39u+offset, 0.0);
    rh.emplace_back(71u+offset, 0.0);
    rh.emplace_back(100u+offset, 0.0);

    v3->andNotWithT(RankedHitIterator(&rh[0], 4));
    EXPECT_TRUE(assertBV(fill({7,103}, offset), *v3));
}

void
testNot(uint32_t offset)
{
    uint32_t end = offset + 128;
    BitVector::UP v1(BitVector::create(offset, end));
    v1->setInterval(offset, end);
    fill(*v1, A, offset, false);

    v1->notSelf();
    EXPECT_TRUE(assertBV(fill(A, offset), *v1));
}

TEST("requireThatSequentialOperationsOnPartialWorks")
{
    PartialBitVector p1(717,919);

    EXPECT_FALSE(p1.hasTrueBits());
    EXPECT_EQUAL(0u, p1.countTrueBits());
    p1.setBit(719);
    EXPECT_EQUAL(0u, p1.countTrueBits());
    p1.invalidateCachedCount();
    EXPECT_TRUE(p1.hasTrueBits());
    EXPECT_EQUAL(1u, p1.countTrueBits());
    p1.slowSetBit(718);
    p1.slowSetBit(739);
    p1.slowSetBit(871);
    p1.slowSetBit(903);
    EXPECT_EQUAL(5u, p1.countTrueBits());
    EXPECT_TRUE(assertBV("[718,719,739,871,903]", p1));

    PartialBitVector p2(717,919);
    EXPECT_FALSE(p1 == p2);
    p2.slowSetBit(719);
    p2.slowSetBit(718);
    p2.slowSetBit(739);
    p2.slowSetBit(871);
    EXPECT_FALSE(p1 == p2);
    p2.slowSetBit(903);
    EXPECT_TRUE(p1 == p2);

    AllocatedBitVector full(1000);
    full.setInterval(0, 1000);
    EXPECT_EQUAL(5u, p2.countTrueBits());
    p2.orWith(full);
    EXPECT_EQUAL(202u, p2.countTrueBits());
}

TEST("requireThatInitRangeStaysWithinBounds") {
    AllocatedBitVector v1(128);
    search::fef::TermFieldMatchData f;
    search::fef::TermFieldMatchDataArray a;
    a.add(&f);
    queryeval::SearchIterator::UP it(BitVectorIterator::create(&v1, a, true));
    it->initRange(700, 800);
    EXPECT_TRUE(it->isAtEnd());
}

TEST("requireThatAndWorks") {
    for (uint32_t offset(0); offset < 100; offset++) {
        testAnd(offset);
    }
}

TEST("requireThatOrWorks") {
    for (uint32_t offset(0); offset < 100; offset++) {
        testOr(offset);
    }
}


TEST("requireThatAndNotWorks") {
    for (uint32_t offset(0); offset < 100; offset++) {
        testAndNot(offset);
    }
}


TEST("requireThatNotWorks") {
    for (uint32_t offset(0); offset < 100; offset++) {
        testNot(offset);
    }
}

TEST("requireThatClearWorks")
{
    AllocatedBitVector v1(128);

    v1.setBit(7);
    v1.setBit(39);
    v1.setBit(71);
    v1.setBit(103);
    EXPECT_TRUE(assertBV("[7,39,71,103]", v1));

    v1.clear();
    EXPECT_TRUE(assertBV("[]", v1));
}

TEST("requireThatForEachWorks") {
    AllocatedBitVector v1(128);

    v1.setBit(7);
    v1.setBit(39);
    v1.setBit(71);
    v1.setBit(103);
    EXPECT_EQUAL(128u, v1.size());

    size_t sum(0);
    v1.foreach_truebit([&](uint32_t key) { sum += key; });
    EXPECT_EQUAL(220u, sum);

    sum = 0;
    v1.foreach_truebit([&](uint32_t key) { sum += key; }, 7);
    EXPECT_EQUAL(220u, sum);

    sum = 0;
    v1.foreach_truebit([&](uint32_t key) { sum += key; }, 6, 7);
    EXPECT_EQUAL(0u, sum);
    sum = 0;
    v1.foreach_truebit([&](uint32_t key) { sum += key; }, 7, 8);
    EXPECT_EQUAL(7u, sum);
    sum = 0;
    v1.foreach_truebit([&](uint32_t key) { sum += key; }, 8, 9);
    EXPECT_EQUAL(0u, sum);

    sum = 0;
    v1.foreach_truebit([&](uint32_t key) { sum += key; }, 8);
    EXPECT_EQUAL(213u, sum);

    sum = 0;
    v1.foreach_falsebit([&](uint32_t key) { sum += key; }, 5, 6);
    EXPECT_EQUAL(5u, sum);

    sum = 0;
    v1.foreach_falsebit([&](uint32_t key) { sum += key; }, 5, 7);
    EXPECT_EQUAL(11u, sum);

    sum = 0;
    v1.foreach_falsebit([&](uint32_t key) { sum += key; }, 5, 8);
    EXPECT_EQUAL(11u, sum);

    sum = 0;
    v1.foreach_falsebit([&](uint32_t key) { sum += key; }, 5, 9);
    EXPECT_EQUAL(19u, sum);

    sum = 0;
    v1.foreach_falsebit([&](uint32_t key) { sum += key; }, 6);
    EXPECT_EQUAL(size_t((((6+127)*(127-6 + 1)) >> 1) - 220), sum);
}


TEST("requireThatSetWorks")
{
    AllocatedBitVector v1(128);

    v1.setBit(7);
    v1.setBit(39);
    v1.setBit(71);
    v1.setBit(103);
    EXPECT_TRUE(assertBV("[7,39,71,103]", v1));
    v1.invalidateCachedCount();
    EXPECT_EQUAL(4u, v1.countTrueBits());

    v1.setBit(80);
    EXPECT_EQUAL(4u, v1.countTrueBits());
    v1.invalidateCachedCount();
    EXPECT_EQUAL(5u, v1.countTrueBits());
    EXPECT_TRUE(assertBV("[7,39,71,80,103]", v1));

    v1.clearBit(35);
    EXPECT_EQUAL(5u, v1.countTrueBits());
    v1.invalidateCachedCount();
    EXPECT_EQUAL(5u, v1.countTrueBits());
    EXPECT_TRUE(assertBV("[7,39,71,80,103]", v1));
    v1.clearBit(71);
    EXPECT_EQUAL(5u, v1.countTrueBits());
    v1.invalidateCachedCount();
    EXPECT_EQUAL(4u, v1.countTrueBits());
    EXPECT_TRUE(assertBV("[7,39,80,103]", v1));

    v1.slowSetBit(39);
    EXPECT_EQUAL(4u, v1.countTrueBits());
    EXPECT_TRUE(assertBV("[7,39,80,103]", v1));
    v1.slowSetBit(57);
    EXPECT_EQUAL(5u, v1.countTrueBits());
    EXPECT_TRUE(assertBV("[7,39,57,80,103]", v1));
}


TEST("requireThatClearIntervalWorks")
{
    AllocatedBitVector v1(1200);
    
    v1.setBit(7);
    v1.setBit(39);
    v1.setBit(71);
    v1.setBit(103);
    v1.setBit(200);
    v1.setBit(500);
    EXPECT_TRUE(assertBV("[7,39,71,103,200,500]", v1));

    v1.clearInterval(40, 70);
    EXPECT_TRUE(assertBV("[7,39,71,103,200,500]", v1));
    v1.clearInterval(39, 71);
    EXPECT_TRUE(assertBV("[7,71,103,200,500]", v1));
    v1.clearInterval(39, 72);
    EXPECT_TRUE(assertBV("[7,103,200,500]", v1));
    v1.clearInterval(20, 501);
    EXPECT_TRUE(assertBV("[7]", v1));
}


TEST("requireThatSetIntervalWorks")
{
    AllocatedBitVector v1(1200);
    
    EXPECT_FALSE(v1.hasTrueBits());
    v1.setBit(7);
    v1.setBit(39);
    v1.setBit(71);
    v1.setBit(103);
    v1.setBit(200);
    v1.setBit(500);
    EXPECT_TRUE(assertBV("[7,39,71,103,200,500]", v1));

    v1.setInterval(40, 46);
    EXPECT_TRUE(assertBV("[7,39,40,41,42,43,44,45,71,103,200,500]", v1));
    EXPECT_TRUE(v1.hasTrueBits());
    v1.invalidateCachedCount();
    EXPECT_EQUAL(12u, v1.countTrueBits());
    EXPECT_EQUAL(12u, v1.countInterval(1, 1199));
    EXPECT_EQUAL(12u, myCountInterval(v1, 1, 1199));
    
    v1.setInterval(40, 200);
    EXPECT_EQUAL(164u, v1.countInterval(1, 1199));
    EXPECT_EQUAL(164u, myCountInterval(v1, 1, 1199));
    EXPECT_EQUAL(163u, v1.countInterval(1, 201));
    EXPECT_EQUAL(162u, v1.countInterval(1, 200));
    EXPECT_EQUAL(163u, v1.countInterval(7, 201));
    EXPECT_EQUAL(162u, v1.countInterval(8, 201));
    EXPECT_EQUAL(161u, v1.countInterval(8, 200));
    v1.clearInterval(72, 174);
    EXPECT_EQUAL(62u, v1.countInterval(1, 1199));
    EXPECT_EQUAL(62u, myCountInterval(v1, 1, 1199));
    EXPECT_EQUAL(61u, v1.countInterval(1, 201));
    EXPECT_EQUAL(60u, v1.countInterval(1, 200));
    EXPECT_EQUAL(61u, v1.countInterval(7, 201));
    EXPECT_EQUAL(60u, v1.countInterval(8, 201));
    EXPECT_EQUAL(59u, v1.countInterval(8, 200));
    EXPECT_EQUAL(51u, v1.countInterval(8, 192));
    EXPECT_EQUAL(50u, v1.countInterval(8, 191));

    EXPECT_EQUAL(1u, v1.countInterval(1, 20));
    EXPECT_EQUAL(1u, v1.countInterval(7, 20));
    EXPECT_EQUAL(0u, v1.countInterval(8, 20));
    EXPECT_EQUAL(1u, v1.countInterval(1, 8));
    EXPECT_EQUAL(0u, v1.countInterval(1, 7));
}

TEST("requireThatScanWorks")
{
    scanWithOffset(0);
    scanWithOffset(19876);
}

TEST("requireThatGrowWorks")
{
    vespalib::GenerationHolder g;
    GrowableBitVector v(200, 200, g);
    
    v.setBit(7);
    v.setBit(39);
    v.setBit(71);
    v.setBit(103);
   
    EXPECT_EQUAL(200u, v.size()); 
    v.invalidateCachedCount();
    EXPECT_TRUE(assertBV("[7,39,71,103]", v));
    EXPECT_EQUAL(4u, v.countTrueBits());
    v.reserve(204);
    EXPECT_EQUAL(200u, v.size()); 
    EXPECT_EQUAL(204u, v.capacity()); 
    EXPECT_TRUE(assertBV("[7,39,71,103]", v));
    EXPECT_EQUAL(4u, v.countTrueBits());
    v.extend(202);
    EXPECT_EQUAL(202u, v.size()); 
    EXPECT_EQUAL(204u, v.capacity()); 
    EXPECT_TRUE(assertBV("[7,39,71,103]", v));
    EXPECT_EQUAL(4u, v.countTrueBits());
    v.shrink(200);
    EXPECT_EQUAL(200u, v.size()); 
    EXPECT_EQUAL(204u, v.capacity()); 
    EXPECT_TRUE(assertBV("[7,39,71,103]", v));
    EXPECT_EQUAL(4u, v.countTrueBits());
    v.reserve(204);
    EXPECT_EQUAL(200u, v.size()); 
    EXPECT_EQUAL(204u, v.capacity()); 
    EXPECT_TRUE(assertBV("[7,39,71,103]", v));
    EXPECT_EQUAL(4u, v.countTrueBits());
    v.shrink(202);
    EXPECT_EQUAL(202u, v.size()); 
    EXPECT_EQUAL(204u, v.capacity()); 
    EXPECT_TRUE(assertBV("[7,39,71,103]", v));
    EXPECT_EQUAL(4u, v.countTrueBits());

    v.shrink(100);
    EXPECT_EQUAL(100u, v.size()); 
    EXPECT_EQUAL(204u, v.capacity()); 
    EXPECT_TRUE(assertBV("[7,39,71]", v));
    EXPECT_EQUAL(3u, v.countTrueBits());
    g.transferHoldLists(1);
    g.trimHoldLists(2);
}


TEST_MAIN() { TEST_RUN_ALL(); }
