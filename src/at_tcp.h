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

#ifndef __AT_NET_H__
#define __AT_NET_H__

#ifdef WORK_IN_PROGRESS

namespace at_tcp {

extern char routerIP[128];
extern int routerPort;

enum AT_TCP_Error
{
    OK,
    COULD_NOT_RESOLVE_HOSTNAME,
    COULD_NOT_CONNECT
};

AT_TCP_Error connect();
void disconnect();
bool process(unsigned wait);

namespace cli {
extern char columns[64];
extern int columnSpacing;
bool showSignalStrength();
} // namespace cli

void init();
void deinit();

} // namespace at_tcp

#endif

#endif // __AT_NET_H__
