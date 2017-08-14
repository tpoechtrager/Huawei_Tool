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

#ifndef __ATOMIC_H__
#define __ATOMIC_H__

#include "compiler.h"

#if __has_include(<atomic>)
#include <atomic>
using std::atomic;
#else
template<class T> class atomic
{
public:
    void store(T val)
    {
        __atomic_store(&var, &val, __ATOMIC_SEQ_CST);
    }
    T load() const
    {
        T val;
        __atomic_load(&var, &val, __ATOMIC_SEQ_CST);
        return val;
    }
    operator T() const { return load(); }
    T operator++(int) { return __atomic_fetch_add(&var, 1, __ATOMIC_SEQ_CST); }
    T operator--(int) { return __atomic_fetch_sub(&var, 1, __ATOMIC_SEQ_CST); }
    T operator+=(T val) { return __atomic_fetch_add(&var, val, __ATOMIC_SEQ_CST); }
    T operator-=(T val) { return __atomic_fetch_sub(&var, val, __ATOMIC_SEQ_CST); }
    void operator=(T val) { store(val); }
    atomic(T val = T(0)) { store(val); }
    atomic(const atomic<T> &in) { store(in.load()); }
private:
    T var;
};
#endif

#endif // __ATOMIC_H__
