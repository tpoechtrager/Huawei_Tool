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

#ifdef WORK_IN_PROGRESS

#include "at_tcp.h"
#include "tools.h"

#include <cstdio>
#include <SDL2/SDL_net.h>

extern TimeType now;

namespace at_tcp {

char routerIP[128] = "homerouter.cpe";
int routerPort = 20249;

namespace {
TCPsocket sock = nullptr;
SDLNet_SocketSet sockset = nullptr;
TimeType lastRecv;
TimeType lastRecvWarning;
TimeType lastCERSSI;
TimeType lastCERSSIWarning;
TimeType connectMillis;
static void parse(const char *msg);

void resetTimeValues()
{
    lastRecv = 0;
    lastRecvWarning = 0;
    lastCERSSI = 0;
    lastCERSSIWarning = 0;
    connectMillis = 0;
}

void printWarnings()
{
    constexpr TimeType NO_DATA_WARN_INTERVAL = 5000;
    constexpr TimeType CERSSI_WARN_INTERVAL = 15000;

    if (timeElapsedGE(lastRecv, NO_DATA_WARN_INTERVAL) &&
        timeElapsedGE(lastRecvWarning, NO_DATA_WARN_INTERVAL))
    {
        warn << "Received no data within "
             << getTimeDiffInSeconds(TT(now, 0), TT(lastRecv, connectMillis))
             << " seconds" << warn.endl();

        lastRecvWarning = now;
    }

    if (timeElapsedGE(lastCERSSI, CERSSI_WARN_INTERVAL) &&
        timeElapsedGE(lastCERSSIWarning, CERSSI_WARN_INTERVAL) &&
        timeElapsedGE(connectMillis, CERSSI_WARN_INTERVAL))
    {
        warn << "Received no ^CERSSI within "
             << getTimeDiffInSeconds(TT(now, 0), TT(lastCERSSI, connectMillis))
             << " seconds" << warn.endl();

#warning TODO: B618 info

        lastCERSSIWarning = now;
    }
}

} // anonymous namespace

AT_TCP_Error connect()
{
    resetTimeValues();

    dbglog << "Resolving " << routerIP << " ..." << dbg.endl();

    IPaddress address;
    if (SDLNet_ResolveHost(&address, routerIP, routerPort) == -1)
    {
        dbglog << SDLNet_GetError() << err.endl();
        errfun << "Could not resolve hostname" << dbg.endl();
        return AT_TCP_Error::COULD_NOT_RESOLVE_HOSTNAME;
    }

    dbglog << "... done" << dbg.endl();

    if (!(sock = SDLNet_TCP_Open(&address)))
    {
        dbglog << SDLNet_GetError() << dbg.endl();
        errfun << "Could not connect" << dbg.endl();
        return AT_TCP_Error::COULD_NOT_CONNECT;
    }

    updateTime();
    connectMillis = now;

    SDLNet_TCP_AddSocket(sockset, sock);

    return AT_TCP_Error::OK;
}

void disconnect()
{
    if (!sock) return;
    dbg << "Disconnecting ..." << dbg.endl();
    SDLNet_TCP_Close(sock);
    dbg << "... done" << dbg.endl();
    sock = nullptr;
}

bool process(unsigned wait)
{
    updateTime();
    printWarnings();

    // TODO: Handle interrupt

    if (SDLNet_CheckSockets(sockset, wait) < 0)
    {
        dbglog << SDLNet_GetError() << dbg.endl();
        errfun << "Checking sockets failed" << dbg.endl();
        return false;
    }

    if (!SDLNet_SocketReady(sock)) return true;

    char msg[4096];
    char line[4096];
    const char *msgPtr = msg;

    int recvLength = SDLNet_TCP_Recv(sock, msg, sizeof(msg) - 1);

    if (recvLength <= 0)
    {
        dbglog << SDLNet_GetError() << dbg.endl();
        errfun << "Receiving data failed" << dbg.endl();
        return false;
    }

    msg[recvLength] = '\0';

    updateTime();
    lastRecv = now;

    while (getLine(msgPtr, line))
    {
        if (!*line) continue;
        parse(line);
    }

    return true;
}

namespace {

void parse(const char *msg)
{
    if (*msg != '^') return;

#warning TODO: Limit.
    dbglog << "Parsing: " << msg << dbg.endl();

    if (!strncmp(msg, "^CERSSI", 6))
        lastCERSSI = now;

    if (!strcmp(msg, "PASSWORD: "))
    {
        abort();
        return;
    }

    int val[16];

    if (sscanf(msg, "^CERSSI:0,0,255,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,",
                    &val[0], &val[1], &val[2], &val[3], &val[4], &val[5], &val[6],
                    &val[7], &val[8], &val[9], &val[10], &val[11], &val[12],
                    &val[13], &val[14]) == 15)
    {
        /*
         * 0:  RSRP
         * 1:  RSRQ
         * 2:  SINR
         * 3:  RI
         * 4:  CQI1
         * 5:  CQI2
         * 6:  Num Antennas
         * 7:  RSRP1
         * 8 : RSRP2
         * 9:  RSRP3
         * 10: RSRP4
         * 11: SINR1
         * 12: SINR2
         * 13: SINR3
         * 14: SINR4
         */

        printf("%u: %d %d %d %d\n", (unsigned)getMilliSeconds(), val[11], val[12], val[13], val[14]);
    }

    // TODO: WCDMA, ...
}

} // anonymous namespace

namespace cli {

} // namespace cli

void init()
{
    SDLNet_Init();
    sockset = SDLNet_AllocSocketSet(1);
}

void deinit()
{
    SDLNet_FreeSocketSet(sockset);
    sockset = nullptr;
    SDLNet_Quit();
}

} // namespace at_tcp

#endif
