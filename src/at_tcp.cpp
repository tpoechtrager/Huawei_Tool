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
#include "huawei_tools.h"
#include "tools.h"
#include "cli_tools.h"

#include <cstdio>
#include <SDL2/SDL_net.h>

extern TimeType now;

namespace at_tcp {

char routerIP[128] = "";
int routerPort = 0;

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
    constexpr TimeType NO_CERSSI_WARN_INTERVAL = 15000;

    if (timeElapsedGE(lastRecv, NO_DATA_WARN_INTERVAL) &&
        timeElapsedGE(lastRecvWarning, NO_DATA_WARN_INTERVAL))
    {
        warn.linef("Received no data within %llu seconds",
                   getTimeDiffInSeconds(TP(now, 0), TP(lastRecv, connectMillis)));

        lastRecvWarning = now;
    }

    if (timeElapsedGE(lastCERSSI, NO_CERSSI_WARN_INTERVAL) &&
        timeElapsedGE(lastCERSSIWarning, NO_CERSSI_WARN_INTERVAL) &&
        timeElapsedGE(connectMillis, NO_CERSSI_WARN_INTERVAL))
    {
        warn.linef("Received no ^CERSSI within %llu seconds",
                   getTimeDiffInSeconds(TP(now, 0), TP(lastCERSSI, connectMillis)));
#warning cerssi.reset();
        lastCERSSIWarning = now;
    }
}

} // anonymous namespace

AT_TCP_Error connect()
{
    resetTimeValues();

    dbg.linef("Resolving %s ...", routerIP);

    IPaddress address;
    if (SDLNet_ResolveHost(&address, routerIP, routerPort) == -1)
    {
        dbg.linef("Could not resolve hostname");
        errfunf("Could not resolve hostname");
        return AT_TCP_Error::COULD_NOT_RESOLVE_HOSTNAME;
    }

    dbg.linef("... done");

    if (!(sock = SDLNet_TCP_Open(&address)))
    {
        dbg.linef("%s", SDLNet_GetError());
        errfunf("Could not connect");
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
    dbg.linef("Disconnecting ...");
    SDLNet_TCP_Close(sock);
    dbg.linef("... done");
    sock = nullptr;
}

bool process(unsigned wait)
{
    updateTime();
    printWarnings();

    // TODO: Handle interrupt

    if (SDLNet_CheckSockets(sockset, wait) < 0)
    {
        dbg.linef("%s", SDLNet_GetError());
        errfunf("Checking sockets failed");
        return false;
    }

    if (!SDLNet_SocketReady(sock)) return true;

    char msg[4096];
    char line[4096];
    const char *msgPtr = msg;

    int recvLength = SDLNet_TCP_Recv(sock, msg, sizeof(msg) - 1);

    if (recvLength <= 0)
    {
        dbg.linef("%s", SDLNet_GetError());
        errfunf("Receiving data failed");
        return false;
    }

    msg[recvLength] = '\0';

    updateTime();
    lastRecv = now;

    while (getLine(msgPtr, line, sizeof(line)))
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

    dbg.linef("Parsing: %s", msg);

    if (!strncmp(msg, "^CERSSI", 6))
        lastCERSSI = now;

    int val[16];

    // TODO: WCDMA, ...

    // Avoid name clash with ::signal
    using x::signal;

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
         * 8:  RSRP2
         * 9:  RSRP3
         * 10: RSRP4
         * 11: SINR1
         * 12: SINR2
         * 13: SINR3
         * 14: SINR4
         */

        Signal::AT::CERSSI_LTE &cerssi = signal.at.cerssiLTE;

        cerssi.numAntennas = val[6];

        if (cerssi.numAntennas > Signal::AT::CERSSI_LTE::MAX_ANTENNAS)
            cerssi.numAntennas = Signal::AT::CERSSI_LTE::MAX_ANTENNAS;
        else if (cerssi.numAntennas < 0)
            cerssi.numAntennas = 0;

        cerssi.RSRQ.update(val[1]);
        cerssi.RSRP[0].update(val[7]);
        cerssi.RSRP[1].update(val[8]);
        cerssi.RSRP[2].update(val[9]);
        cerssi.RSRP[3].update(val[10]);
        cerssi.SINR[0].update(val[11]);
        cerssi.SINR[1].update(val[12]);
        cerssi.SINR[2].update(val[13]);
        cerssi.SINR[3].update(val[14]);
        cerssi.RI.update(val[3]);
        cerssi.CQI[0].update(val[4]);
        cerssi.CQI[1].update(val[5]);
    }
    else if (sscanf(msg, "^CERSSI:0,%d,%d,0,0,0", &val[0], &val[1]) == 2)
    {
        /*
         * 0:  RSCP
         * 1:  ECIO
         */

        Signal::AT::CERSSI_WCDMA &cerssi = signal.at.cerssiWCDMA;

        cerssi.RSCP.update(val[0]);
        cerssi.ECIO.update(val[1]);
    }
    else if (sscanf(msg, "^CERSSI:%d,0,255,0,0,0,0", &val[0]) == 1)
    {
        /*
         * 0:  RSSI
         */

        Signal::AT::CERSSI_GSM &cerssi = signal.at.cerssiGSM;

        cerssi.RSSI.update(val[0]);
    }
    else if (sscanf(msg, "^HCSQ:\"LTE\",%d,%d,%d,%d",
                         &val[0], &val[1], &val[2], &val[3]) == 4)
    {
        /*
         * 0:  RSSI
         * 1:  RSRP
         * 2:  SINR
         * 3:  RSRQ
         */

        Signal::AT::HCSQ_LTE &hcsq = signal.at.hcsqLTE;

        hcsq.RSRP.update(val[1] - 141);
        hcsq.RSRQ.update((val[3] * 0.5f) - 19.5f);
        hcsq.RSSI.update(val[0] - 120);
        hcsq.SINR.update((val[2] * 0.2f) - 20.f);
    }
    else if (sscanf(msg, "^HCSQ:\"WCDMA\",%d,%d,%d", &val[0], &val[1], &val[2]) == 3)
    {
        /*
         * 0:  RSSI
         * 1:  RSCP
         * 2:  ECIO
         */

        Signal::AT::HCSQ_WCDMA &hcsq = signal.at.hcsqWCDMA;

        hcsq.RSSI.update(val[0] - 120);
        hcsq.RSCP.update(val[1] - 120);
        hcsq.ECIO.update((val[2] * 0.5f) - 32.f);
    }
    else if (sscanf(msg, "^HCSQ:\"GSM\",%d", &val[0]) == 1)
    {
        /*
         * 0:  RSSI
         */

        Signal::AT::HCSQ_GSM &hcsq = signal.at.hcsqGSM;

        hcsq.RSSI.update(val[0] - 120);
    }
    else if (sscanf(msg, "^RSSI: %d", &val[0]) == 1)
    {
        Signal::AT::RSSI &rssi = signal.at.rssi;
        rssi.RSSILevel.update(val[0]);
    }
}

} // anonymous namespace

namespace cli {
using namespace ::cli;

char columns[64] = "Current";
int columnSpacing = 30;

namespace {

std::vector<std::string> formatSignal(const SignalValue<>::GetType type)
{
    // Avoid name clash with ::signal
    using x::signal;

    StrBuf str;

    const NetType netType = signal.at.getNetType();

    if (netType == NetType::LTE)
    {
        const Signal::AT::CERSSI_LTE &cerssi = signal.at.cerssiLTE;
        const Signal::AT::HCSQ_LTE &hcsq = signal.at.hcsqLTE;

        if (cerssi.isSet())
        {
            int numAntennas = cerssi.numAntennas;

            if (numAntennas <= 2)
            {
                /* Old ^CERSSI */
                numAntennas = 1;
            }

            for (int i = 0; i < numAntennas; i++)
            {
                str.format("RSRP: %d | SINR: %d\n",
                           cerssi.RSRP[i].getVal(type),
                           cerssi.SINR[i].getVal(type));
            }

            str.addChar('\n');
        }

        if (hcsq.isSet())
        {
            str.format("RSSI: %d\nRSRQ: %d\n\n",
                        hcsq.RSSI.getVal(type),
                        hcsq.RSRQ.getVal(type));
        }

        if (cerssi.isSet())
        {
            str.format("CQI1: %d | %s\nCQI2: %d | %s\n\nMIMO: %s\n\n",
                       cerssi.CQI[0].getVal(type),
                       getLTEModulationStr(cerssi.CQI[0].getVal(type)),
                       cerssi.CQI[1].getVal(type),
                       getLTEModulationStr(cerssi.CQI[1].getVal(type)),
                       cerssi.RI.getVal(type) == 2 ? "Yes" : "No");
        }
    }
    else if (netType == NetType::WCDMA)
    {
        const Signal::AT::CERSSI_WCDMA &cerssi = signal.at.cerssiWCDMA;
        const Signal::AT::HCSQ_WCDMA &hcsq = signal.at.hcsqWCDMA;

        if (cerssi.isSet())
        {
            str.format("RSCP: %d\n\nECIO: %d\n\n",
                       cerssi.RSCP.getVal(type),
                       cerssi.ECIO.getVal(type));
        }

        if (hcsq.isSet())
        {
            str.format("RSSI: %d\n\n", hcsq.RSSI.getVal(type));
        }
    }
    else if (netType == NetType::GSM)
    {
        const Signal::AT::CERSSI_GSM &cerssi = signal.at.cerssiGSM;

        if (cerssi.isSet())
        {
            str.format("RSSI: %d\n\n", cerssi.RSSI.getVal(type));
        }
    }

    return str.getLines();
}

bool printStats()
{
    std::vector<status::Column> signalColumns;
    std::vector<std::string> wantedColumns;

    if (!splitStr(wantedColumns, columns, ", ", false))
    {
        errfunf("Empty columns");
        return false;
    }

    for (auto column : wantedColumns)
    {
        SignalValue<>::GetType type = SignalValue<>::getGetTypeByStr(column.c_str());

        if (type == SignalValue<>::GET_INVALID)
        {
            errfunf("Invalid column: %s", column.c_str());
            return false;
        }

        signalColumns.push_back({std::move(column), formatSignal(type)});
    }

    status::addColumns(signalColumns, columnSpacing);
    status::show();

    return true;
}

} // anonymous namespace

bool showSignalStrength()
{
    outf("Waiting for signal strength data... This may take up to 10 seconds.\n");

    do
    {
        if (!process(50)) return false;
        if (!printStats()) return false;
    } while (!checkExit());

    return true;
}

} // namespace cli

void init()
{
    SDLNet_Init();
    sockset = SDLNet_AllocSocketSet(1);
    if (!sockset) abort();
}

void deinit()
{
    if (!sockset) return;
    SDLNet_FreeSocketSet(sockset);
    sockset = nullptr;
    SDLNet_Quit();
}

} // namespace at_tcp

#endif
