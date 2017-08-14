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

#include "tools.h"
#include "huawei_tools.h"

namespace web {
extern char routerIP[128];
extern char routerUser[128];
extern char routerPass[128];

HuaweiErrorCode login();
HuaweiErrorCode logout();

bool getAntennaType(AntennaType &type);
bool setAntennaType(const AntennaType type);

namespace cli {

bool showAntennaType();
bool setAntennaType(const char *antennaType);
bool showPlmn();
bool setPlmn(const char *plmn, const char *plmnMode, const char *plmnRat);
bool selectPlmn(bool select = false);
bool showNetworkMode();
bool setNetworkMode(const char *networkMode, const char *networkBand, const char *lteBand);
bool relay(const char *request, bool loop, int loopDelay);

namespace experimental {
bool showSignalStrength(bool noClearScreen);
} // namespace experimental

} // namespace cli

void init();
void deinit();

} // namespace web
