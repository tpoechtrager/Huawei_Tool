/***************************************************************************
 *  Huawei Tool                                                            *
 *  Copyright (c) 2017 unknown (unknown.lteforum@gmail.com)                *
 *                                                                         *
 *  This program is free software: you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 **************************************************************************/

#ifndef __COMPILER_H__
#define __COMPILER_H__

#ifndef __has_include
#define __has_include(x) 0
#endif

#if __cplusplus >= 201402L
#define cxx14
#define cxx14_constexpr constexpr
#else
#define cxx14_constexpr static inline
#endif

#ifdef __clang__
#define OPTNONE [[clang::optnone]]
#elif defined(__GNUC__)
#define OPTNONE __attribute__ ((optimize("0")))
#else
#define OPTNONE
#endif

#ifdef __GNUC__
#define USED __attribute__((used))
#define MAYBE_UNUSED __attribute__((unused))
#define NORETURN __attribute__((noreturn))
#else
#define USED
#define MAYBE_UNUSED
#define PRINTFARGS(fmt, args)
#define NOTRETURN
#endif

#ifdef __GNUC__
#ifdef __clang__
#define PRINTFARGS(fmt, args) __attribute__((format(printf, fmt, args)))
#else
#define PRINTFARGS(fmt, args) __attribute__((format(gnu_printf, fmt, args)))
#endif
#endif

#endif // __COMPILER_H__
