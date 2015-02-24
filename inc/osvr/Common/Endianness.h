/** @file
    @brief Header

    @date 2015

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

// Copyright 2015 Sensics, Inc.
//
// All rights reserved.
//
// (Final version intended to be licensed under
// the Apache License, Version 2.0)

#ifndef INCLUDED_Endianness_h_GUID_A8D4BB43_3F1B_46AA_8044_BD082A07C299
#define INCLUDED_Endianness_h_GUID_A8D4BB43_3F1B_46AA_8044_BD082A07C299

// Internal Includes
#include <osvr/Util/StdInt.h>
#include <osvr/Common/ConfigByteSwapping.h>

// Library/third-party includes
#include <boost/version.hpp>

#if BOOST_VERSION >= 105500
#include <boost/predef/other/endian.h>
#if BOOST_ENDIAN_LITTLE_BYTE
#define OSVR_IS_LITTLE_ENDIAN
#define OSVR_BYTE_ORDER_ABCD
#elif BOOST_ENDIAN_BIG_BYTE
#define OSVR_IS_BIG_ENDIAN
#define OSVR_BYTE_ORDER_DCBA
#else
#error "Unsupported byte order!"
#endif

#else
#include <boost/detail/endian.h>

#if defined(BOOST_LITTLE_ENDIAN)
#define OSVR_IS_LITTLE_ENDIAN
#define OSVR_BYTE_ORDER_ABCD
#elif defined(BOOST_BIG_ENDIAN)
#define OSVR_IS_BIG_ENDIAN
#define OSVR_BYTE_ORDER_DCBA
#else
#error "Unsupported byte order!"
#endif

#endif

#include <boost/utility/enable_if.hpp>
#include <boost/type_traits/is_integral.hpp>
#include <boost/type_traits/is_signed.hpp>
#include <boost/type_traits/is_unsigned.hpp>
#include <boost/type_traits/is_floating_point.hpp>
#include <boost/type_traits/remove_cv.hpp>
#include <boost/type_traits/remove_reference.hpp>
#include <boost/integer.hpp>
#include <boost/mpl/if.hpp>

#if !defined(OSVR_HAVE_BYTESWAP_H) && defined(_MSC_VER)
#define OSVR_HAVE_MSC_BYTESWAPS
#include <intrin.h>
#endif

#ifdef OSVR_HAVE_BYTESWAP_H
#include <byteswap.h>
#endif

#if defined(__FLOAT_WORD_ORDER) && defined(__BYTE_ORDER)
#if (__FLOAT_WORD_ORDER != __BYTE_ORDER)
/// Handle weird mixed-order doubles like some ARM systems.
#define OSVR_FLOAT_ORDER_MIXED
#endif
#endif

#include <string.h>

namespace osvr {
namespace common {

    namespace serialization {
        template <typename T, typename Dummy = void>
        struct NetworkByteOrderTraits;

        namespace detail {
            /// @brief Stock implementation of a no-op host-network conversion
            template <typename T> struct ByteOrderNoOp {
                static T apply(T v) { return v; }
            };

            template <typename T> struct ByteSwap : boost::false_type {
                typedef void not_specialized;
            };

            struct NoOpFunction {
                template <typename ArgType> static ArgType apply(ArgType v) {
                    return v;
                }
            };
            /// @brief no-op swap for 1 byte.
            template <>
            struct ByteSwap<uint8_t> : NoOpFunction, boost::true_type {};

            template <>
            struct ByteSwap<int8_t> : NoOpFunction, boost::true_type {};
#if defined(OSVR_HAVE_MSC_BYTESWAPS)
            template <> struct ByteSwap<uint16_t> : boost::true_type {
                static uint16_t apply(uint16_t v) {
                    return _byteswap_ushort(v);
                }
            };

            template <> struct ByteSwap<uint32_t> : boost::true_type {
                static uint32_t apply(uint32_t v) { return _byteswap_ulong(v); }
            };
            template <> struct ByteSwap<uint64_t> : boost::true_type {
                static uint64_t apply(uint64_t v) {
                    return _byteswap_uint64(v);
                }
            };
#elif defined(OSVR_HAVE_BYTESWAP_H)
            template <> struct ByteSwap<uint16_t> : boost::true_type {
                static uint16_t apply(uint16_t v) { return __bswap_16(v); }
            };

            template <> struct ByteSwap<uint32_t> : boost::true_type {
                static uint32_t apply(uint32_t v) { return __bswap_32(v); }
            };
            template <> struct ByteSwap<uint64_t> : boost::true_type {
                static uint64_t apply(uint64_t v) { return __bswap_64(v); }
            };
#endif
            template <typename T> inline T genericByteSwap(T const v) {
                T ret;
                for (size_t i = 0; i < sizeof(T); ++i) {
                    reinterpret_cast<char *>(&ret)[i] =
                        reinterpret_cast<char const *>(&v)[sizeof(T) - 1 - i];
                }
            }
            /// @brief Implementation of integerByteSwap(): Gets called when we
            /// have an exact match to an explicit specialization
            template <typename T, typename OppositeSignType,
                      typename ByteSwapper, typename OppositeByteSwapper>
            inline T integerByteSwapImpl(
                T const v,
                typename boost::enable_if<ByteSwapper>::type * = nullptr) {
                return ByteSwapper::apply(v);
            }

            /// @brief Implementation of integerByteSwap(): Gets called when we
            /// DO NOT have an exact match to an explicit specialization, but we
            /// do have a match if we switch the signedness.
            template <typename T, typename OppositeSignType,
                      typename ByteSwapper, typename OppositeByteSwapper>
            inline T integerByteSwapImpl(
                T const v,
                typename boost::disable_if<ByteSwapper>::type * = nullptr,
                typename boost::enable_if<OppositeByteSwapper>::type * =
                    nullptr) {
                return static_cast<T>(OppositeByteSwapper::apply(
                    static_cast<OppositeSignType>(v)));
            }

            /// @brief Implementation of integerByteSwap(): Gets called if no
            /// explicit specialization exists (for the type or its
            /// opposite-signedness relative)
            template <typename T, typename OppositeSignType,
                      typename ByteSwapper, typename OppositeByteSwapper>
            inline T integerByteSwapImpl(
                T v, typename boost::disable_if<ByteSwapper>::type * = nullptr,
                typename boost::disable_if<OppositeByteSwapper>::type * =
                    nullptr) {
                return genericByteSwap(v);
            }
        } // namespace detail

        /// @brief Take the binary representation of a value with one type,
        /// and return it as another type, without breaking strict aliasing.
        template <typename Dest, typename Src>
        inline Dest safe_pun(Src const src) {
            BOOST_STATIC_ASSERT(sizeof(Src) == sizeof(Dest));
            Dest ret;
            memcpy(reinterpret_cast<char *>(&ret),
                   reinterpret_cast<char const *>(&src), sizeof(src));
            return ret;
        }

        namespace detail {
            /// @brief Stock implementation of a no-op host-network conversion
            template <typename T> struct NoOpHostNetworkConversion {
                static T hton(T const v) { return v; }
                static T ntoh(T const v) { return v; }
            };
            /// @brief Stock implementation of a byte-swapping host-network
            /// conversion
            template <typename T> struct IntegerByteOrderSwap {
                static T hton(T const v) { return integerByteSwap(v); }
                static T ntoh(T const v) { return hton(v); }
            };
            /// @brief Stock implementation of a type-punning host-network
            /// conversion
            template <typename T, typename UnsignedIntType>
            struct TypePunByteOrder {

                typedef T type;
                typedef UnsignedIntType uint_t;

                BOOST_STATIC_ASSERT(sizeof(T) == sizeof(UnsignedIntType));
                BOOST_STATIC_ASSERT(boost::is_unsigned<UnsignedIntType>::value);

                typedef NetworkByteOrderTraits<uint_t> IntTraits;
                static type hton(type const v) {
                    return safe_pun<type>(IntTraits::hton(safe_pun<uint_t>(v)));
                }
                static type ntoh(type const v) { return hton(v); }
            };

            template <size_t N> struct Factorial {
                static const size_t value = N * (N - 1);
            };
            template <> struct Factorial<0> { static const size_t value = 1; };

            typedef uint16_t ByteOrderId;
            template <size_t Size> struct BaseByteOrders {
                static const size_t size = Size;
                /// @todo static assert that Size is some 2^n here
            };
            template <size_t Size> struct ByteOrders {};
            template <> struct ByteOrders<2> : BaseByteOrders<2> {
                //// @brief Storage order of 0x0A0B
                enum Orders : ByteOrderId { AB = 1, BA = 2 };
                static const Orders BIG_ENDIAN = AB;
                static const Orders BIG_BYTE = AB;
                static const Orders LITTLE_ENDIAN = BA;
                static const Orders LITTLE_BYTE = BA;
            };

            template <> struct ByteOrders<4> : BaseByteOrders<4> {
                //// @brief Storage order of 0x0A0B0C0D
                enum Orders : ByteOrderId {
                    ABCD = 1,
                    DCBA = 2,
                    // CDBA = 3, // not used?
                    BADC = 4,
                };
                static const Orders BIG_ENDIAN = ABCD;
                static const Orders BIG_BYTE = ABCD;
                static const Orders LITTLE_ENDIAN = DCBA;
                static const Orders LITTLE_BYTE = DCBA;
                static const Orders PDP_ENDIAN = BADC;
                static const Orders BIG_WORD =
                    BADC; ///< Little endian with words swapped
            };

            template <typename ByteOrderStruct> struct Permutations;

            template <size_t Size> struct Permutations<ByteOrders<Size> > {
                static const size_t value = Factorial<Size>::value;
            };

            /// @brief Metafunction to swap halves of a data type with a given
            /// byte order, to get the resulting byte order identifier.
            template <typename ByteOrderStruct,
                      typename ByteOrderStruct::Orders MainByteOrder>
            struct SwapHalves;
            template <size_t Size,
                      typename ByteOrders<Size>::Orders MainByteOrder>
            struct SwapHalves<ByteOrders<Size>, MainByteOrder> {
                typedef typename ByteOrders<Size>::Orders Orders;
                static const Orders value = (MainByteOrder + Size / 2) % Size;
            };

            template <> struct ByteOrders<8> {
                //// @brief Storage order of ABCDEFGH
                enum Orders : uint8_t {
                    ABCDEFGH = 1,
                    HGFEDCBA = 2 //,
                    // EFGHABCD =
                };
                static const uint8_t BIG_ENDIAN = ABCDEFGH;
                static const uint8_t BIG_BYTE = ABCDEFGH;
                static const uint8_t LITTLE_ENDIAN = HGFEDCBA;
                static const uint8_t LITTLE_BYTE = HGFEDCBA;
            };

            template <typename T> struct LocalByteOrder {};

        } // namespace detail

        /// @brief Swap the order of bytes of an arbitrary integer type
        ///
        /// @internal
        /// Primarily a thin wrapper to centralize type manipulation before
        /// calling a detail::integerByteSwapImpl() overload selected by
        /// enable_if/disable_if.
        template <typename Type> inline Type integerByteSwap(Type const v) {
            BOOST_STATIC_ASSERT(boost::is_integral<Type>::value);
            typedef typename boost::remove_cv<
                typename boost::remove_reference<Type>::type>::type T;

            typedef typename boost::int_t<sizeof(T) * 8>::exact int_t;
            typedef typename boost::uint_t<sizeof(T) * 8>::exact uint_t;
            typedef
                typename boost::mpl::if_<boost::is_signed<T>, uint_t,
                                         int_t>::type opposite_signedness_type;

            typedef detail::ByteSwap<T> ByteSwapper;
            typedef detail::ByteSwap<opposite_signedness_type>
                OppositeByteSwapper;

            return detail::integerByteSwapImpl<
                T, opposite_signedness_type, ByteSwapper, OppositeByteSwapper>(
                v);
        }
#if defined(OSVR_IS_BIG_ENDIAN)
        template <typename T>
        struct NetworkByteOrderTraits<
            T, typename boost::enable_if<boost::is_integral<T> >::type>
            : detail::NoOpHostNetworkConversion<T> {};

#elif defined(OSVR_IS_LITTLE_ENDIAN)
        template <typename T>
        struct NetworkByteOrderTraits<
            T, typename boost::enable_if<boost::is_integral<T> >::type>
            : detail::IntegerByteOrderSwap<T> {};
#endif

#if 0
        /// @todo not sure how to do this on arm mixed-endian.
        template<>
        struct NetworkByteOrderTraits<float, void> : detail::TypePunByteOrder<float, uint32_t> {
        };
#endif

#if defined(OSVR_FLOAT_ORDER_MIXED)
        template <> struct NetworkByteOrderTraits<double, void> {
            typedef detail::TypePunByteOrder<double, uint64_t> ByteOrder;
            typedef ByteOrder::type type;
            static const size_t WORD_SIZE = sizeof(type) / 2;
            static inline type hton(type const v) {
                /// First change byte order as needed.
                type src = ByteOrder::hton(v);
                type ret;
                /// Now swap word order
                memcpy(reinterpret_cast<char *>(&ret),
                       reinterpret_cast<char const *>(&src) + WORD_SIZE,
                       WORD_SIZE);
                memcpy(reinterpret_cast<char *>(&ret) + WORD_SIZE,
                       reinterpret_cast<char const *>(&src), WORD_SIZE);
                return ret;
            }
            static type ntoh(type const v) { return hton(v); }
        };
#else
        template <>
        struct NetworkByteOrderTraits<double, void>
            : detail::TypePunByteOrder<double, uint64_t> {};
#endif

        /// @brief Convert network byte order (big endian) to host byte order
        template <typename T> inline T ntoh(T const v) {
            return NetworkByteOrderTraits<T>::ntoh(v);
        }

        /// @brief Convert host byte order to network byte order (big endian)
        template <typename T> inline T hton(T const v) {
            return NetworkByteOrderTraits<T>::hton(v);
        }
    } // namespace serialization
} // namespace common
} // namespace osvr

#endif // INCLUDED_Endianness_h_GUID_A8D4BB43_3F1B_46AA_8044_BD082A07C299
