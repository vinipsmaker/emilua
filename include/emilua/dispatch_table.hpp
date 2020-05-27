#pragma once

#include <emilua/core.hpp>

#include <boost/hana/functional/overload_linearly.hpp>
#include <boost/hana/functional/overload.hpp>
#include <boost/hana/functional/always.hpp>
#include <boost/hana/fold_right.hpp>
#include <boost/hana/drop_back.hpp>
#include <boost/hana/not_equal.hpp>
#include <boost/hana/greater.hpp>
#include <boost/hana/minimum.hpp>
#include <boost/hana/reverse.hpp>
#include <boost/hana/string.hpp>
#include <boost/hana/front.hpp>
#include <boost/hana/minus.hpp>
#include <boost/hana/range.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/fold.hpp>
#include <boost/hana/mult.hpp>
#include <boost/hana/size.hpp>
#include <boost/hana/sort.hpp>
#include <boost/hana/set.hpp>
#include <boost/hana/zip.hpp>

#include <string_view>

namespace emilua {

namespace hana = boost::hana;

namespace dispatch_table {
namespace detail {
using hana::literals::operator""_c;

// string_view compares string size before string contents. Therefore, string
// size here already performs work similar to the hash table usage. On the event
// where all string sizes differ, we don't generate another hash function.
struct ConstantHash
{
    static constexpr auto value = hana::always(hana::size_c<0>);
    static constexpr auto first_pass = hana::always(hana::true_c);
};

template<class Strings>
struct Hash
{
    static constexpr std::size_t smallest_pattern_size = hana::minimum(
        hana::transform(Strings{}, hana::size));

    // We're after a cheap hash for our static search structure, not a perfect
    // hash.
    static constexpr auto value = []() {
        // The algorithm to find the suitable indexes is roughly divided into
        // these steps:
        //
        // * Produce indexes [0, N).
        // * Produce reverse indexes [N, 1].
        // * Whether and index is reversed is indicated by the boolean constant
        //   that follows the index. `true` means reversed. Reverse index count
        //   from the end iterator.
        // * We use N=smallest_pattern_size because we don't want to perform
        //   bounds checking later.
        // * Concat the two sequences.
        auto initial_idxs = hana::concat(
            hana::transform(
                hana::to_tuple(
                    hana::range_c<std::size_t, 0, smallest_pattern_size>),
                [](auto&& i) { return hana::make_tuple(i, hana::false_c); }
            ),
            hana::reverse(hana::transform(
                hana::to_tuple(
                    hana::range_c<std::size_t, 1, smallest_pattern_size + 1>),
                [](auto&& i) { return hana::make_tuple(i, hana::true_c); }
            ))
        );
        // * Apply all strings to each index. The application will compute the
        //   hash value, store it into a set and append the set's length to the
        //   end of the index tuple. This length indicates a score.
        // * Remove all indexes whose score is 1. They can't be used to
        //   differentiate between any two strings.
        auto idxs_plus_score = initial_idxs |
            [](auto&& e) {
                auto score = hana::size(hana::to_set(hana::transform(
                    Strings{},
                    [idx=e[0_c],rev=e[1_c]](auto&& str) {
                        return hana::if_(
                            rev, str[hana::size(str) - idx], str[idx]);
                    }
                )));
                return hana::if_(
                    score == hana::size_c<1>,
                    hana::make_tuple(),
                    hana::make_tuple(hana::insert(e, hana::int_c<2>, score)));
            };
        // * Apply a stable sort by decreasing score.
        // * Limit the indexes list to length 4 (i.e. bytes in an int32_t).
        // * Discard score.
        auto uncompressed_hash = hana::transform(
            hana::take_front_c<4>(hana::sort(
                idxs_plus_score,
                [](auto&& l, auto&& r) { return hana::greater(l[2_c], r[2_c]); }
            )),
            hana::take_front_c<2>
        );
        // * Consider the result as a single hash. Generate more hashes by
        //   discarding tail until new permutations are no longer possible.
        auto final_round_choices = hana::while_(
            [](auto&& e) { return hana::size(e[0_c]) != hana::size_c<1>; },
            hana::make_tuple(uncompressed_hash),
            [](auto&& e) {
                return hana::insert(e, 0_c, hana::drop_back(e[0_c]));
            }
        );
        // * Compute a score again by applying all the strings to each hash
        //   candidate. But first we create the function that generate the hash
        //   function out of the hash spec.
        // * Sort by score plus hash complexity (i.e. all else being equal,
        //   cheaper hashes stay at the front) and get the best one.
        auto hash_gen = [](auto&& spec) {
            return hana::fold(
                hana::zip(
                    spec,
                    hana::transform(
                        hana::to_tuple(
                            hana::make_range(hana::size_c<0>, hana::size(spec))
                        ),
                        hana::_ * hana::integral_c<std::uint32_t, 8>)),
                hana::always(hana::integral_c<std::uint32_t, 0>),
                [](auto&& acc, auto&& subhash) {
                    return [acc,subhash](const auto& string) {
                        auto size = hana::overload(
                            [](std::string_view str) { return str.size(); },
                            hana::size
                        );
                        auto at = hana::overload_linearly(
                            [](std::string_view str, std::size_t i) {
                                return std::uint32_t(str[i]);
                            },
                            [](auto&& xs, auto&& i) {
                                return hana::to<
                                    hana::integral_constant_tag<std::uint32_t>
                                >(hana::at(xs, i));
                            }
                        );
                        auto idx = hana::if_(
                            /*reverse_index=*/subhash[0_c][1_c],
                            size(string) - subhash[0_c][0_c],
                            subhash[0_c][0_c]);
                        return acc(string) + (at(string, idx) << subhash[1_c]);
                    };
                }
            );
        };
        return hash_gen(/*spec=*/hana::first(hana::front(hana::sort(
            hana::transform(
                final_round_choices,
                [hash_gen](auto&& e) {
                    auto score = hana::size(hana::to_set(
                        hana::transform(Strings{}, hash_gen(e))));
                    return hana::make_pair(e, score);
                }),
            [](auto&& l, auto&& r) {
                return hana::greater(hana::second(l), hana::second(r));
            }
        ))));
    }();

    static constexpr bool first_pass(std::string_view str)
    {
        // So later code won't have to perform bounds checking when computing
        // hash
        if (str.size() < smallest_pattern_size)
            return false;

        return true;
    }
};

template<class Xs>
constexpr bool all_sizes_differ(const Xs& xs)
{
    return hana::size(hana::to_set(
        hana::transform(hana::transform(xs, hana::first), hana::size)
    )) == hana::size(xs);
}

template<class Xs>
constexpr bool all_first_letters_differ(const Xs& xs)
{
    return hana::size(hana::to_set(
        hana::transform(hana::transform(xs, hana::first), hana::front)
    )) == hana::size(xs);
}
} // namespace detail

// Even if a perfect-hash is generated, a walk through the whole table is
// done. The rationale is to keep the generated code small. Also, for our
// use-case, not-found is an exceptional case, so we purposefully don't optimize
// for it.
template<class Xs, class Fallback, class... Args>
auto dispatch(Xs xs, Fallback fallback, std::string_view key, Args&&... args)
    -> decltype(fallback(key, std::forward<Args>(args)...))
{
    std::conditional_t<
        detail::all_sizes_differ(xs) || detail::all_first_letters_differ(xs),
        detail::ConstantHash,
        detail::Hash<decltype(hana::transform(xs, hana::first))>
    > hash;
    if (!hash.first_pass(key))
        return fallback(key, std::forward<Args>(args)...);

    return hana::fold_right(
        xs,
        fallback,
        [hash](const auto& arm, const auto& acc) {
            return [hash,arm,acc](std::string_view key, auto&&... args) {
                auto pattern_ct = hana::first(arm);
                std::string_view pattern{
                    pattern_ct.c_str(), hana::size(pattern_ct)};
                if (hash.value(pattern_ct) == hash.value(key) &&
                    pattern == key) {
                    return hana::second(arm)(
                        std::forward<decltype(args)>(args)...);
                } else {
                    return acc(key, std::forward<decltype(args)>(args)...);
                }
            };
        }
    )(key, std::forward<Args>(args)...);
}
} // namespace dispatch_table

} // namespace emilua
