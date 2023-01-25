#ifndef JASMIN_INTERNAL_TYPE_LIST_H
#define JASMIN_INTERNAL_TYPE_LIST_H

#include <type_traits>

#include "jasmin/internal/type_traits.h"
#include "nth/meta/sequence.h"
#include "nth/meta/type.h"

// Jasmin uses the type `void (*)(Ts*...)` to represent type-lists. This has the
// benefit over the slightly more common practice of `template <typename...>
// struct TypeList {}` of reducing template instantiations. Moreover, by using
// pointers to the types in question we are able to have incomplete types in our
// type lists.
namespace jasmin::internal {

template <typename... Ts>
using type_list = void (*)(Ts *...);

namespace internal_type_list {

// Implementation of `TypeList` concept, defined below.
template <typename>
struct IsTypeList : std::false_type {};
template <typename... Ts>
struct IsTypeList<type_list<Ts...>> : std::true_type {};

}  // namespace internal_type_list

// A concept identifying which types are type-lists
template <typename TL>
concept TypeList = internal_type_list::IsTypeList<TL>::value;

namespace internal_type_list {

// Implementation of `Apply`, defined below.
template <template <typename...> typename, typename>
struct ApplyImpl;
template <template <typename...> typename F, typename... Ts>
struct ApplyImpl<F, type_list<Ts...>> {
  using type = F<Ts...>;
};

template <auto, TypeList>
struct InvokeImpl;

template <auto Lambda, typename... Ts>
struct InvokeImpl<Lambda, type_list<Ts...>> {
  static constexpr auto value = Lambda.template operator()<Ts...>();
};

template <TypeList>
struct ForEachImpl;

template <typename... Ts>
struct ForEachImpl<type_list<Ts...>> {
  void operator()(auto &&f) const { (f.template operator()<Ts>(), ...); }
};

}  // namespace internal_type_list

// Given a type-function `F` and a type-list `type_list<Ts...>`, evaluates to
// the type `F<Ts...>`;
template <template <typename...> typename F, typename TypeList>
using Apply = typename internal_type_list::ApplyImpl<F, TypeList>::type;

// Given an invocable `lambda` that can be invoked by passing a bunch of
// template arguments, invokes `lambda` with the arguments from the given type
// list `L`.
template <auto Lambda, TypeList L>
inline constexpr auto Invoke = internal_type_list::InvokeImpl<Lambda, L>::value;

// Given an invocable `f` that can be invoked by passing a single template
// arguments, invokes `f` once with each template argument given by each element
// of the type list `L`.
template <TypeList L, typename F>
inline constexpr auto ForEach(F &&f) {
  internal_type_list::ForEachImpl<L>{}(std::forward<F>(f));
}

// A concept indicating that the matching type is contained in the given type
// list.
template <typename T, typename L>
concept ContainedIn = TypeList<L> and
    Invoke<[]<typename... Ts>() { return (std::is_same_v<T, Ts> or ...); }, L>;

template <int &..., typename... Ts>
constexpr auto ToNth(type_list<Ts...>) {
  return nth::type_sequence<Ts...>;
}

template <typename T>
struct FromNthImpl;

template <auto... Vs>
struct FromNthImpl<nth::internal_meta::Sequence<Vs...>> {
  using type = type_list<nth::type_t<Vs>...>;
};

template <nth::Sequence auto X>
using FromNth =typename FromNthImpl<decltype(X)>::type;

}  // namespace jasmin::internal

#endif  // JASMIN_INTERNAL_TYPE_LIST_H
