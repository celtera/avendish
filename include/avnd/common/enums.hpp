#pragma once
#include <avnd/common/tag.hpp>

/**
 * This file defines a nifty set of utilities: AVND_ENUM_MATCHER and AVND_TAG_MATCHER.
 *
 * AVND_ENUM_MATCHER:
 * Usage:
 *
 * Given a target enum:
 * enum my_enum { foo, bar, baz };
 *
 * And a set of words that should match to each enumerator:
 * for instance, foo or Foo for the first enumerator, bar or Bar for the second, etc.
 * in order to introspect a class that may define a custom enum:
 *
 * ```
 * auto matcher = AVND_ENUM_MATCHER(
 *     (my_enum::foo, foo, Foo),
 *     (my_enum::bar, bar, Bar)
 *     );
 * ```
 *
 * Then:
 *
 * ```
 * auto res = matcher(
 *     a_value_of_unknown_enum_type  //< input
 *   , my_enum::foo                  //< default value when input not found
 * );
 * ```
 *
 * `res` will be the corresponding enumerator in `my_enum`.
 *
 * e.g. one can do
 *
 * ```
 * enum { Foo, Bar } x = Foo;
 * ```
 *
 * and convert that to a my_enum::foo or my_enum::bar.
 *
 *
 * AVND_TAG_MATCHER: the same, but looks for tags in structs:
 * struct Foo {
 *   enum { ui, test };
 * };
 *
 * should tell us if there is an "ui" tag in Foo.
 *
 *
 * AVND_ENUM_OR_TAG_MATCHER: combines both the static and dynamic cases
 *
 * AVND_*_CONVERTER: does the reverse: it tries to convert to a matching case in the
 * target enum
 *
 */

#define AVND_NUM_ARGS_(                                                                \
    _100, _99, _98, _97, _96, _95, _94, _93, _92, _91, _90, _89, _88, _87, _86, _85,   \
    _84, _83, _82, _81, _80, _79, _78, _77, _76, _75, _74, _73, _72, _71, _70, _69,    \
    _68, _67, _66, _65, _64, _63, _62, _61, _60, _59, _58, _57, _56, _55, _54, _53,    \
    _52, _51, _50, _49, _48, _47, _46, _45, _44, _43, _42, _41, _40, _39, _38, _37,    \
    _36, _35, _34, _33, _32, _31, _30, _29, _28, _27, _26, _25, _24, _23, _22, _21,    \
    _20, _19, _18, _17, _16, _15, _14, _13, _12, _11, _10, _9, _8, _7, _6, _5, _4, _3, \
    _2, _1, N, ...)                                                                    \
  N
#define AVND_NUM_ARGS(...)                                                              \
  AVND_NUM_ARGS_(                                                                       \
      __VA_ARGS__, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87, 86, 85, 84, \
      83, 82, 81, 80, 79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 69, 68, 67, 66, 65, 64,   \
      63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44,   \
      43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24,   \
      23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2,   \
      1, 0)
#define AVND_FOREACH(MACRO, ...) \
  AVND_FOREACH_(AVND_NUM_ARGS(__VA_ARGS__), MACRO, __VA_ARGS__)
#define AVND_FOREACH_(N, M, ...) AVND_FOREACH__(N, M, __VA_ARGS__)
#define AVND_FOREACH__(N, M, ...) AVND_FOREACH_##N(M, __VA_ARGS__)
#define AVND_FOREACH_1(M, A) M(A)
#define AVND_FOREACH_2(M, A, ...) M(A) AVND_FOREACH_1(M, __VA_ARGS__)
#define AVND_FOREACH_3(M, A, ...) M(A) AVND_FOREACH_2(M, __VA_ARGS__)
#define AVND_FOREACH_4(M, A, ...) M(A) AVND_FOREACH_3(M, __VA_ARGS__)
#define AVND_FOREACH_5(M, A, ...) M(A) AVND_FOREACH_4(M, __VA_ARGS__)
#define AVND_FOREACH_6(M, A, ...) M(A) AVND_FOREACH_5(M, __VA_ARGS__)
#define AVND_FOREACH_7(M, A, ...) M(A) AVND_FOREACH_6(M, __VA_ARGS__)
#define AVND_FOREACH_8(M, A, ...) M(A) AVND_FOREACH_7(M, __VA_ARGS__)
#define AVND_FOREACH_9(M, A, ...) M(A) AVND_FOREACH_8(M, __VA_ARGS__)
#define AVND_FOREACH_10(M, A, ...) M(A) AVND_FOREACH_9(M, __VA_ARGS__)
#define AVND_FOREACH_11(M, A, ...) M(A) AVND_FOREACH_10(M, __VA_ARGS__)
#define AVND_FOREACH_12(M, A, ...) M(A) AVND_FOREACH_11(M, __VA_ARGS__)
#define AVND_FOREACH_13(M, A, ...) M(A) AVND_FOREACH_12(M, __VA_ARGS__)
#define AVND_FOREACH_14(M, A, ...) M(A) AVND_FOREACH_13(M, __VA_ARGS__)
#define AVND_FOREACH_15(M, A, ...) M(A) AVND_FOREACH_14(M, __VA_ARGS__)
#define AVND_FOREACH_16(M, A, ...) M(A) AVND_FOREACH_15(M, __VA_ARGS__)
#define AVND_FOREACH_17(M, A, ...) M(A) AVND_FOREACH_16(M, __VA_ARGS__)
#define AVND_FOREACH_18(M, A, ...) M(A) AVND_FOREACH_17(M, __VA_ARGS__)
#define AVND_FOREACH_19(M, A, ...) M(A) AVND_FOREACH_18(M, __VA_ARGS__)
#define AVND_FOREACH_20(M, A, ...) M(A) AVND_FOREACH_19(M, __VA_ARGS__)
#define AVND_FOREACH_21(M, A, ...) M(A) AVND_FOREACH_20(M, __VA_ARGS__)
#define AVND_FOREACH_22(M, A, ...) M(A) AVND_FOREACH_21(M, __VA_ARGS__)
#define AVND_FOREACH_23(M, A, ...) M(A) AVND_FOREACH_22(M, __VA_ARGS__)
#define AVND_FOREACH_24(M, A, ...) M(A) AVND_FOREACH_23(M, __VA_ARGS__)
#define AVND_FOREACH_25(M, A, ...) M(A) AVND_FOREACH_24(M, __VA_ARGS__)
#define AVND_FOREACH_26(M, A, ...) M(A) AVND_FOREACH_25(M, __VA_ARGS__)
#define AVND_FOREACH_27(M, A, ...) M(A) AVND_FOREACH_26(M, __VA_ARGS__)
#define AVND_FOREACH_28(M, A, ...) M(A) AVND_FOREACH_27(M, __VA_ARGS__)
#define AVND_FOREACH_29(M, A, ...) M(A) AVND_FOREACH_28(M, __VA_ARGS__)
#define AVND_FOREACH_30(M, A, ...) M(A) AVND_FOREACH_29(M, __VA_ARGS__)
#define AVND_FOREACH_31(M, A, ...) M(A) AVND_FOREACH_30(M, __VA_ARGS__)
#define AVND_FOREACH_32(M, A, ...) M(A) AVND_FOREACH_31(M, __VA_ARGS__)
#define AVND_FOREACH_33(M, A, ...) M(A) AVND_FOREACH_32(M, __VA_ARGS__)
#define AVND_FOREACH_34(M, A, ...) M(A) AVND_FOREACH_33(M, __VA_ARGS__)
#define AVND_FOREACH_35(M, A, ...) M(A) AVND_FOREACH_34(M, __VA_ARGS__)
#define AVND_FOREACH_36(M, A, ...) M(A) AVND_FOREACH_35(M, __VA_ARGS__)
#define AVND_FOREACH_37(M, A, ...) M(A) AVND_FOREACH_36(M, __VA_ARGS__)
#define AVND_FOREACH_38(M, A, ...) M(A) AVND_FOREACH_37(M, __VA_ARGS__)
#define AVND_FOREACH_39(M, A, ...) M(A) AVND_FOREACH_38(M, __VA_ARGS__)
#define AVND_FOREACH_40(M, A, ...) M(A) AVND_FOREACH_39(M, __VA_ARGS__)
#define AVND_FOREACH_41(M, A, ...) M(A) AVND_FOREACH_40(M, __VA_ARGS__)
#define AVND_FOREACH_42(M, A, ...) M(A) AVND_FOREACH_41(M, __VA_ARGS__)
#define AVND_FOREACH_43(M, A, ...) M(A) AVND_FOREACH_42(M, __VA_ARGS__)
#define AVND_FOREACH_44(M, A, ...) M(A) AVND_FOREACH_43(M, __VA_ARGS__)
#define AVND_FOREACH_45(M, A, ...) M(A) AVND_FOREACH_44(M, __VA_ARGS__)
#define AVND_FOREACH_46(M, A, ...) M(A) AVND_FOREACH_45(M, __VA_ARGS__)
#define AVND_FOREACH_47(M, A, ...) M(A) AVND_FOREACH_46(M, __VA_ARGS__)
#define AVND_FOREACH_48(M, A, ...) M(A) AVND_FOREACH_47(M, __VA_ARGS__)
#define AVND_FOREACH_49(M, A, ...) M(A) AVND_FOREACH_48(M, __VA_ARGS__)
#define AVND_FOREACH_50(M, A, ...) M(A) AVND_FOREACH_49(M, __VA_ARGS__)
#define AVND_FOREACH_51(M, A, ...) M(A) AVND_FOREACH_50(M, __VA_ARGS__)
#define AVND_FOREACH_52(M, A, ...) M(A) AVND_FOREACH_51(M, __VA_ARGS__)
#define AVND_FOREACH_53(M, A, ...) M(A) AVND_FOREACH_52(M, __VA_ARGS__)
#define AVND_FOREACH_54(M, A, ...) M(A) AVND_FOREACH_53(M, __VA_ARGS__)
#define AVND_FOREACH_55(M, A, ...) M(A) AVND_FOREACH_54(M, __VA_ARGS__)
#define AVND_FOREACH_56(M, A, ...) M(A) AVND_FOREACH_55(M, __VA_ARGS__)
#define AVND_FOREACH_57(M, A, ...) M(A) AVND_FOREACH_56(M, __VA_ARGS__)
#define AVND_FOREACH_58(M, A, ...) M(A) AVND_FOREACH_57(M, __VA_ARGS__)
#define AVND_FOREACH_59(M, A, ...) M(A) AVND_FOREACH_58(M, __VA_ARGS__)
#define AVND_FOREACH_60(M, A, ...) M(A) AVND_FOREACH_59(M, __VA_ARGS__)
#define AVND_FOREACH_61(M, A, ...) M(A) AVND_FOREACH_60(M, __VA_ARGS__)
#define AVND_FOREACH_62(M, A, ...) M(A) AVND_FOREACH_61(M, __VA_ARGS__)
#define AVND_FOREACH_63(M, A, ...) M(A) AVND_FOREACH_62(M, __VA_ARGS__)
#define AVND_FOREACH_64(M, A, ...) M(A) AVND_FOREACH_63(M, __VA_ARGS__)
#define AVND_FOREACH_65(M, A, ...) M(A) AVND_FOREACH_64(M, __VA_ARGS__)
#define AVND_FOREACH_66(M, A, ...) M(A) AVND_FOREACH_65(M, __VA_ARGS__)
#define AVND_FOREACH_67(M, A, ...) M(A) AVND_FOREACH_66(M, __VA_ARGS__)
#define AVND_FOREACH_68(M, A, ...) M(A) AVND_FOREACH_67(M, __VA_ARGS__)
#define AVND_FOREACH_69(M, A, ...) M(A) AVND_FOREACH_68(M, __VA_ARGS__)
#define AVND_FOREACH_70(M, A, ...) M(A) AVND_FOREACH_69(M, __VA_ARGS__)
#define AVND_FOREACH_71(M, A, ...) M(A) AVND_FOREACH_70(M, __VA_ARGS__)
#define AVND_FOREACH_72(M, A, ...) M(A) AVND_FOREACH_71(M, __VA_ARGS__)
#define AVND_FOREACH_73(M, A, ...) M(A) AVND_FOREACH_72(M, __VA_ARGS__)
#define AVND_FOREACH_74(M, A, ...) M(A) AVND_FOREACH_73(M, __VA_ARGS__)
#define AVND_FOREACH_75(M, A, ...) M(A) AVND_FOREACH_74(M, __VA_ARGS__)
#define AVND_FOREACH_76(M, A, ...) M(A) AVND_FOREACH_75(M, __VA_ARGS__)
#define AVND_FOREACH_77(M, A, ...) M(A) AVND_FOREACH_76(M, __VA_ARGS__)
#define AVND_FOREACH_78(M, A, ...) M(A) AVND_FOREACH_77(M, __VA_ARGS__)
#define AVND_FOREACH_79(M, A, ...) M(A) AVND_FOREACH_78(M, __VA_ARGS__)
#define AVND_FOREACH_80(M, A, ...) M(A) AVND_FOREACH_79(M, __VA_ARGS__)
#define AVND_FOREACH_81(M, A, ...) M(A) AVND_FOREACH_80(M, __VA_ARGS__)
#define AVND_FOREACH_82(M, A, ...) M(A) AVND_FOREACH_81(M, __VA_ARGS__)
#define AVND_FOREACH_83(M, A, ...) M(A) AVND_FOREACH_82(M, __VA_ARGS__)
#define AVND_FOREACH_84(M, A, ...) M(A) AVND_FOREACH_83(M, __VA_ARGS__)
#define AVND_FOREACH_85(M, A, ...) M(A) AVND_FOREACH_84(M, __VA_ARGS__)
#define AVND_FOREACH_86(M, A, ...) M(A) AVND_FOREACH_85(M, __VA_ARGS__)
#define AVND_FOREACH_87(M, A, ...) M(A) AVND_FOREACH_86(M, __VA_ARGS__)
#define AVND_FOREACH_88(M, A, ...) M(A) AVND_FOREACH_87(M, __VA_ARGS__)
#define AVND_FOREACH_89(M, A, ...) M(A) AVND_FOREACH_88(M, __VA_ARGS__)
#define AVND_FOREACH_90(M, A, ...) M(A) AVND_FOREACH_89(M, __VA_ARGS__)
#define AVND_FOREACH_91(M, A, ...) M(A) AVND_FOREACH_90(M, __VA_ARGS__)
#define AVND_FOREACH_92(M, A, ...) M(A) AVND_FOREACH_91(M, __VA_ARGS__)
#define AVND_FOREACH_93(M, A, ...) M(A) AVND_FOREACH_92(M, __VA_ARGS__)
#define AVND_FOREACH_94(M, A, ...) M(A) AVND_FOREACH_93(M, __VA_ARGS__)
#define AVND_FOREACH_95(M, A, ...) M(A) AVND_FOREACH_94(M, __VA_ARGS__)
#define AVND_FOREACH_96(M, A, ...) M(A) AVND_FOREACH_95(M, __VA_ARGS__)
#define AVND_FOREACH_97(M, A, ...) M(A) AVND_FOREACH_96(M, __VA_ARGS__)
#define AVND_FOREACH_98(M, A, ...) M(A) AVND_FOREACH_97(M, __VA_ARGS__)
#define AVND_FOREACH_99(M, A, ...) M(A) AVND_FOREACH_98(M, __VA_ARGS__)
#define AVND_FOREACH_100(M, A, ...) M(A) AVND_FOREACH_99(M, __VA_ARGS__)

#define AVND_FOREACHSUB1(MACRO, ...) \
  AVND_FOREACHSUB1_(AVND_NUM_ARGS(__VA_ARGS__), MACRO, __VA_ARGS__)
#define AVND_FOREACHSUB1_(N, M, ...) AVND_FOREACHSUB1__(N, M, __VA_ARGS__)
#define AVND_FOREACHSUB1__(N, M, ...) AVND_FOREACHSUB1_##N(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_1(M, A) M(A)
#define AVND_FOREACHSUB1_2(M, A, ...) M(A) AVND_FOREACHSUB1_1(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_3(M, A, ...) M(A) AVND_FOREACHSUB1_2(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_4(M, A, ...) M(A) AVND_FOREACHSUB1_3(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_5(M, A, ...) M(A) AVND_FOREACHSUB1_4(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_6(M, A, ...) M(A) AVND_FOREACHSUB1_5(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_7(M, A, ...) M(A) AVND_FOREACHSUB1_6(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_8(M, A, ...) M(A) AVND_FOREACHSUB1_7(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_9(M, A, ...) M(A) AVND_FOREACHSUB1_8(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_10(M, A, ...) M(A) AVND_FOREACHSUB1_9(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_11(M, A, ...) M(A) AVND_FOREACHSUB1_10(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_12(M, A, ...) M(A) AVND_FOREACHSUB1_11(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_13(M, A, ...) M(A) AVND_FOREACHSUB1_12(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_14(M, A, ...) M(A) AVND_FOREACHSUB1_13(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_15(M, A, ...) M(A) AVND_FOREACHSUB1_14(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_16(M, A, ...) M(A) AVND_FOREACHSUB1_15(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_17(M, A, ...) M(A) AVND_FOREACHSUB1_16(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_18(M, A, ...) M(A) AVND_FOREACHSUB1_17(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_19(M, A, ...) M(A) AVND_FOREACHSUB1_18(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_20(M, A, ...) M(A) AVND_FOREACHSUB1_19(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_21(M, A, ...) M(A) AVND_FOREACHSUB1_20(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_22(M, A, ...) M(A) AVND_FOREACHSUB1_21(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_23(M, A, ...) M(A) AVND_FOREACHSUB1_22(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_24(M, A, ...) M(A) AVND_FOREACHSUB1_23(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_25(M, A, ...) M(A) AVND_FOREACHSUB1_24(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_26(M, A, ...) M(A) AVND_FOREACHSUB1_25(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_27(M, A, ...) M(A) AVND_FOREACHSUB1_26(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_28(M, A, ...) M(A) AVND_FOREACHSUB1_27(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_29(M, A, ...) M(A) AVND_FOREACHSUB1_28(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_30(M, A, ...) M(A) AVND_FOREACHSUB1_29(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_31(M, A, ...) M(A) AVND_FOREACHSUB1_30(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_32(M, A, ...) M(A) AVND_FOREACHSUB1_31(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_33(M, A, ...) M(A) AVND_FOREACHSUB1_32(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_34(M, A, ...) M(A) AVND_FOREACHSUB1_33(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_35(M, A, ...) M(A) AVND_FOREACHSUB1_34(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_36(M, A, ...) M(A) AVND_FOREACHSUB1_35(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_37(M, A, ...) M(A) AVND_FOREACHSUB1_36(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_38(M, A, ...) M(A) AVND_FOREACHSUB1_37(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_39(M, A, ...) M(A) AVND_FOREACHSUB1_38(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_40(M, A, ...) M(A) AVND_FOREACHSUB1_39(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_41(M, A, ...) M(A) AVND_FOREACHSUB1_40(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_42(M, A, ...) M(A) AVND_FOREACHSUB1_41(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_43(M, A, ...) M(A) AVND_FOREACHSUB1_42(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_44(M, A, ...) M(A) AVND_FOREACHSUB1_43(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_45(M, A, ...) M(A) AVND_FOREACHSUB1_44(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_46(M, A, ...) M(A) AVND_FOREACHSUB1_45(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_47(M, A, ...) M(A) AVND_FOREACHSUB1_46(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_48(M, A, ...) M(A) AVND_FOREACHSUB1_47(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_49(M, A, ...) M(A) AVND_FOREACHSUB1_48(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_50(M, A, ...) M(A) AVND_FOREACHSUB1_49(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_51(M, A, ...) M(A) AVND_FOREACHSUB1_50(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_52(M, A, ...) M(A) AVND_FOREACHSUB1_51(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_53(M, A, ...) M(A) AVND_FOREACHSUB1_52(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_54(M, A, ...) M(A) AVND_FOREACHSUB1_53(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_55(M, A, ...) M(A) AVND_FOREACHSUB1_54(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_56(M, A, ...) M(A) AVND_FOREACHSUB1_55(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_57(M, A, ...) M(A) AVND_FOREACHSUB1_56(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_58(M, A, ...) M(A) AVND_FOREACHSUB1_57(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_59(M, A, ...) M(A) AVND_FOREACHSUB1_58(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_60(M, A, ...) M(A) AVND_FOREACHSUB1_59(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_61(M, A, ...) M(A) AVND_FOREACHSUB1_60(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_62(M, A, ...) M(A) AVND_FOREACHSUB1_61(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_63(M, A, ...) M(A) AVND_FOREACHSUB1_62(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_64(M, A, ...) M(A) AVND_FOREACHSUB1_63(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_65(M, A, ...) M(A) AVND_FOREACHSUB1_64(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_66(M, A, ...) M(A) AVND_FOREACHSUB1_65(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_67(M, A, ...) M(A) AVND_FOREACHSUB1_66(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_68(M, A, ...) M(A) AVND_FOREACHSUB1_67(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_69(M, A, ...) M(A) AVND_FOREACHSUB1_68(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_70(M, A, ...) M(A) AVND_FOREACHSUB1_69(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_71(M, A, ...) M(A) AVND_FOREACHSUB1_70(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_72(M, A, ...) M(A) AVND_FOREACHSUB1_71(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_73(M, A, ...) M(A) AVND_FOREACHSUB1_72(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_74(M, A, ...) M(A) AVND_FOREACHSUB1_73(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_75(M, A, ...) M(A) AVND_FOREACHSUB1_74(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_76(M, A, ...) M(A) AVND_FOREACHSUB1_75(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_77(M, A, ...) M(A) AVND_FOREACHSUB1_76(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_78(M, A, ...) M(A) AVND_FOREACHSUB1_77(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_79(M, A, ...) M(A) AVND_FOREACHSUB1_78(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_80(M, A, ...) M(A) AVND_FOREACHSUB1_79(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_81(M, A, ...) M(A) AVND_FOREACHSUB1_80(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_82(M, A, ...) M(A) AVND_FOREACHSUB1_81(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_83(M, A, ...) M(A) AVND_FOREACHSUB1_82(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_84(M, A, ...) M(A) AVND_FOREACHSUB1_83(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_85(M, A, ...) M(A) AVND_FOREACHSUB1_84(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_86(M, A, ...) M(A) AVND_FOREACHSUB1_85(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_87(M, A, ...) M(A) AVND_FOREACHSUB1_86(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_88(M, A, ...) M(A) AVND_FOREACHSUB1_87(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_89(M, A, ...) M(A) AVND_FOREACHSUB1_88(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_90(M, A, ...) M(A) AVND_FOREACHSUB1_89(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_91(M, A, ...) M(A) AVND_FOREACHSUB1_90(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_92(M, A, ...) M(A) AVND_FOREACHSUB1_91(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_93(M, A, ...) M(A) AVND_FOREACHSUB1_92(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_94(M, A, ...) M(A) AVND_FOREACHSUB1_93(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_95(M, A, ...) M(A) AVND_FOREACHSUB1_94(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_96(M, A, ...) M(A) AVND_FOREACHSUB1_95(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_97(M, A, ...) M(A) AVND_FOREACHSUB1_96(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_98(M, A, ...) M(A) AVND_FOREACHSUB1_97(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_99(M, A, ...) M(A) AVND_FOREACHSUB1_98(M, __VA_ARGS__)
#define AVND_FOREACHSUB1_100(M, A, ...) M(A) AVND_FOREACHSUB1_99(M, __VA_ARGS__)

#define AVND_ENUM_MATCHER_IMPL_IMPL(N)           \
  if constexpr(requires { decltype(value)::N; }) \
  {                                              \
    ok = ok || (value == (decltype(value)::N));  \
  }

#define AVND_ENUM_MATCHER_IMPL2(id, ...)                        \
  {                                                             \
    bool ok = false;                                            \
    AVND_FOREACHSUB1(AVND_ENUM_MATCHER_IMPL_IMPL, __VA_ARGS__); \
    if(ok)                                                      \
      return id;                                                \
  }

#define AVND_ENUM_MATCHER_IMPL(...) AVND_ENUM_MATCHER_IMPL2 __VA_ARGS__

#define AVND_MATCH_ALL_ENUMS(...) AVND_FOREACH(AVND_ENUM_MATCHER_IMPL, __VA_ARGS__)

#define AVND_ENUM_MATCHER(...)                                                     \
  [](auto value, auto defaultvalue) constexpr noexcept -> decltype(defaultvalue) { \
    AVND_MATCH_ALL_ENUMS(__VA_ARGS__);                                             \
    return defaultvalue;                                                           \
  };

#define AVND_TAG_MATCHER_IMPL_IMPL(N) || requires { TMatcher::N; }

#define AVND_TAG_MATCHER_IMPL2(id, ...)                                       \
  {                                                                           \
    if constexpr(0 AVND_FOREACHSUB1(AVND_TAG_MATCHER_IMPL_IMPL, __VA_ARGS__)) \
      return id;                                                              \
  }

#define AVND_TAG_MATCHER_IMPL(...) AVND_TAG_MATCHER_IMPL2 __VA_ARGS__

#define AVND_MATCH_ALL_TAGS(...) AVND_FOREACH(AVND_TAG_MATCHER_IMPL, __VA_ARGS__)
#define AVND_TAG_MATCHER(...)                                           \
  []<typename TMatcher>(                                                \
      TMatcher& container,                                              \
      auto defaultvalue) constexpr noexcept -> decltype(defaultvalue) { \
    AVND_MATCH_ALL_TAGS(__VA_ARGS__);                                   \
    return defaultvalue;                                                \
  };

#define AVND_ENUM_OR_TAG_MATCHER(member_name, ...)                   \
  [](auto& container, auto defaultvalue) -> decltype(defaultvalue) { \
    if constexpr(requires { container.member_name; })                \
    {                                                                \
      static constexpr auto m = AVND_ENUM_MATCHER(__VA_ARGS__);             \
      return m(container.member_name, defaultvalue);                 \
    }                                                                \
    else                                                             \
    {                                                                \
      static constexpr auto m = AVND_TAG_MATCHER(__VA_ARGS__);              \
      return m(container, defaultvalue);                             \
    }                                                                \
  };

#define AVND_ENUM_CONVERTER_IMPL_IMPL(N)                \
  if constexpr(requires { decltype(defaultvalue)::N; }) \
  {                                                     \
    return decltype(defaultvalue)::N;                   \
  }

#define AVND_ENUM_CONVERTER_IMPL2(id, ...)                        \
  case id: {                                                      \
    AVND_FOREACHSUB1(AVND_ENUM_CONVERTER_IMPL_IMPL, __VA_ARGS__); \
    break;                                                        \
  }

#define AVND_ENUM_CONVERTER_IMPL(...) AVND_ENUM_CONVERTER_IMPL2 __VA_ARGS__

#define AVND_CONVERT_ALL_ENUMS(...) AVND_FOREACH(AVND_ENUM_CONVERTER_IMPL, __VA_ARGS__)

#define AVND_ENUM_CONVERTER(...)                                                   \
  [](auto value, auto defaultvalue) constexpr noexcept -> decltype(defaultvalue) { \
    switch(value)                                                                  \
    {                                                                              \
      AVND_CONVERT_ALL_ENUMS(__VA_ARGS__);                                         \
    }                                                                              \
    return defaultvalue;                                                           \
  };
