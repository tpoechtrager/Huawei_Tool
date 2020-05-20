/***************************************************************************
 *  Huawei Tool                                                            *
 *  Copyright (c) 2017-2020 unknown (unknown.lteforum@gmail.com)           *
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

#include "tools.h"
#include "huawei_tools.h"

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <utility>

namespace web {
extern char routerIP[128];
extern char routerUser[128];
extern char routerPass[128];

HuaweiErrorCode login();
HuaweiErrorCode logout();

bool getAntennaType(AntennaType &type);
bool setAntennaType(const AntennaType type);

namespace wlan {

struct Client;
typedef std::vector<Client> ClientVec;
typedef std::unique_ptr<ClientVec> uClientVec;

struct Client
{
    std::string macAddress;
    std::string ipAddress;
    std::string hostName;
    TimeType connectionDuration;
    TimeType lastUpdate;
    bool referenced;

    TimeType getInterpolatedConnectionDuration() const
    {
        TimeType timeDiff = getElapsedTime(lastUpdate);
        TimeType duration = (connectionDuration * oneSecond);
        if (timeDiff > MAX_INTERPOLATION_MS) return duration;
        return duration + timeDiff;
    }

    Client(std::string &macAddress, bool referenced = true) :
        macAddress(std::move(macAddress)), referenced(referenced) {}
};

struct Clients
{
    uClientVec clients;
    bool referenced;

    Clients(uClientVec &&clients) :
        clients(std::move(clients)), referenced(true) {}
};

extern std::map<std::string, Clients> ssids;

bool updateClients();

} // namespace wlan

namespace cli {

extern int trafficColumnSpacing;

bool showAntennaType();
bool setAntennaType(const char *antennaType);
bool showPlmn();
bool setPlmn(const char *plmn, const char *plmnMode, const char *plmnRat);
bool selectPlmn(bool select = false);
bool showNetworkMode();
bool setNetworkMode(const char *networkMode, const char *networkBand, const char *lteBand);
bool relay(const char *request, const char *data, bool loop, int loopDelay);

namespace experimental {
bool showSignalStrength();
} // namespace experimental

bool showWlanClients();
bool showTraffic();

bool connect(const int action = 1);
bool disconnect();

bool reboot();

} // namespace cli

void init();
void deinit();

} // namespace web
