// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include "match_phase_limiter.h"
#include <vespa/searchlib/queryeval/andsearchstrict.h>
#include <vespa/searchlib/queryeval/andnotsearch.h>
#include <vespa/searchlib/queryeval/ranksearch.h>
#include <vespa/log/log.h>

LOG_SETUP(".proton.matching.match_phase_limiter");

using search::queryeval::SearchIterator;
using search::queryeval::Searchable;
using search::queryeval::IRequestContext;
using search::queryeval::AndSearchStrict;
using search::queryeval::NoUnpack;

namespace proton::matching {

namespace {

template<bool PRE_FILTER>
class LimitedSearchT : public LimitedSearch {
public:
    LimitedSearchT(SearchIterator::UP limiter, SearchIterator::UP search) :
        LimitedSearch(std::move(PRE_FILTER ? limiter : search),
                      std::move(PRE_FILTER ? search : limiter))
    {
    }
    void doUnpack(uint32_t docId) override { 
        if (PRE_FILTER) {
            getSecond().doUnpack(docId);
        } else {
            getFirst().doUnpack(docId);
        }
    }
};

} // namespace proton::matching::<unnamed>

void
LimitedSearch::doSeek(uint32_t docId)
{
    
    uint32_t currentId(docId);
    for (; !isAtEnd(currentId); currentId++) {
        _first->seek(currentId);
        currentId = _first->getDocId();
        if (isAtEnd(currentId)) {
            break;
        }
        if (_second->seek(currentId)) {
            break;
        }
    }
    setDocId(currentId);
}

void
LimitedSearch::initRange(uint32_t begin, uint32_t end) {
    SearchIterator::initRange(begin, end);
    getFirst().initRange(begin, end);
    getSecond().initRange(begin, end);
}

void
LimitedSearch::visitMembers(vespalib::ObjectVisitor &visitor) const
{
    visit(visitor, "first", getFirst());
    visit(visitor, "second", getSecond());
}

MatchPhaseLimiter::MatchPhaseLimiter(uint32_t docIdLimit,
                                     Searchable &searchable_attributes,
                                     IRequestContext & requestContext,
                                     const vespalib::string &attribute_name,
                                     size_t max_hits, bool descending,
                                     double max_filter_coverage,
                                     double samplePercentage, double postFilterMultiplier,
                                     const vespalib::string &diversity_attribute,
                                     uint32_t diversity_min_groups,
                                     double diversify_cutoff_factor,
                                     AttributeLimiter::DiversityCutoffStrategy diversity_cutoff_strategy)
    : _postFilterMultiplier(postFilterMultiplier),
      _maxFilterCoverage(max_filter_coverage),
      _enablePreFilter(false),
      _calculator(max_hits, diversity_min_groups, samplePercentage),
      _limiter_factory(searchable_attributes, requestContext, attribute_name, descending,
                       diversity_attribute, diversify_cutoff_factor, diversity_cutoff_strategy),
      _coverage(docIdLimit)
{
}

namespace {

const search::BitVector *
findPotentialPreFilter(const SearchIterator &search) {
    const search::BitVector *filter = search.precomputedBitVector();
    if (filter != nullptr) {
        return filter;
    }
    const search::queryeval::MultiSearch * multiSearch = dynamic_cast<const search::queryeval::MultiSearch *>(&search);
    if (multiSearch) {
        if (multiSearch->isAndNot()) {
            return findPotentialPreFilter(*static_cast<const search::queryeval::AndNotSearch *>(multiSearch)->positive());
        } else if (multiSearch->isRank()) {
            return findPotentialPreFilter(*static_cast<const search::queryeval::RankSearch *>(multiSearch)->mandatory());
        }
    }

    return nullptr;
}

template <bool PRE_FILTER>
SearchIterator::UP
do_limit(const search::BitVector * filter, AttributeLimiter &limiter_factory, SearchIterator::UP search,
         size_t wanted_num_docs, size_t max_group_size, uint32_t current_id, uint32_t end_id)
{
    SearchIterator::UP limiter = limiter_factory.create_search(filter, wanted_num_docs, max_group_size, PRE_FILTER);
    limiter = search->andWith(std::move(limiter), wanted_num_docs);
    if (limiter) {
        search.reset(new LimitedSearchT<PRE_FILTER>(std::move(limiter), std::move(search)));
    }
    search->initRange(current_id + 1, end_id);
    return search;
}

} // namespace proton::matching::<unnamed>

SearchIterator::UP
MatchPhaseLimiter::maybe_limit(SearchIterator::UP search,
                               double match_freq, size_t num_docs)
{
    const search::BitVector * filter = _enablePreFilter ? findPotentialPreFilter(*search) : nullptr;
    size_t wanted_num_docs = filter ? _calculator.max_hits() : _calculator.wanted_num_docs(match_freq);
    size_t max_filter_docs = static_cast<size_t>(num_docs * _maxFilterCoverage);
    size_t upper_limited_corpus_size = std::min(num_docs, max_filter_docs);
    if (upper_limited_corpus_size <= wanted_num_docs) {
        LOG(debug, "Will not limit ! maybe_limit(hit_rate=%g, num_docs=%ld, max_filter_docs=%ld) = wanted_num_docs=%ld",
            match_freq, num_docs, max_filter_docs, wanted_num_docs);
        return search;
    }
    uint32_t current_id = search->getDocId();
    uint32_t end_id = search->getEndId();
    size_t total_query_hits = _calculator.estimated_hits(match_freq, num_docs);
    size_t max_group_size = _calculator.max_group_size(wanted_num_docs);
    bool use_pre_filter = (wanted_num_docs < (total_query_hits * _postFilterMultiplier));
    LOG(debug, "Will do %s filter :  maybe_limit(hit_rate=%g, num_docs=%zu, max_filter_docs=%ld) = wanted_num_docs=%zu,"
        " max_group_size=%zu, current_docid=%u, end_docid=%u, total_query_hits=%ld",
        use_pre_filter ? "pre" : "post", match_freq, num_docs, max_filter_docs, wanted_num_docs,
        max_group_size, current_id, end_id, total_query_hits);

    return (use_pre_filter)
        ? do_limit<true>(filter, _limiter_factory, std::move(search), wanted_num_docs, max_group_size, current_id, end_id)
        : do_limit<false>(filter, _limiter_factory, std::move(search), wanted_num_docs, max_group_size, current_id, end_id);
}

void
MatchPhaseLimiter::updateDocIdSpaceEstimate(size_t searchedDocIdSpace, size_t remainingDocIdSpace)
{
    _coverage.update(searchedDocIdSpace, remainingDocIdSpace, _limiter_factory.getEstimatedHits());
}

size_t
MatchPhaseLimiter::getDocIdSpaceEstimate() const
{
    return _coverage.getEstimate();
}

}
