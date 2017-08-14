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

#include "huawei_tools.h"
#include <sstream>

std::string getLTEBandStr(LTEBand lteBand, std::string &str)
{
    for (size_t i = 1; i < sizeof(LTEBandTable)/sizeof(*LTEBandTable); ++i)
    {
        if ((lteBand & LTEBandTable[i]) == LTEBandTable[i])
        {
            if (!str.empty()) str += "+";
            str += LTEBandStrs[i];
        }
    }

    return str;
}

LTEBand getLTEBandFromStr(const char *band)
{
    char *token;
    char *str;
    char *mem;
    unsigned long long lteBand = 0;

    if (band[0] == '+') band++;
    mem = str = strdup(band);

    while ((token = strsep(&str, "+")))
    {
        LTEBand b = getLTEBandFromStr2(token);

        if (b == LTEBand::LTE_BAND_ERROR)
        {
            lteBand = LTEBand::LTE_BAND_ERROR;
            break;
        }

        lteBand |= b;
    }

    free(mem);
    return (LTEBand)lteBand;
}

std::string huaweiErrStr(HuaweiErrorCode errcode)
{
    switch (errcode)
    {
        case HuaweiErrorCode::ERROR: return "Generic program error";
        case HuaweiErrorCode::ERROR_LOGIN_NO_EXIST_USER: return "User does not exist";
        case HuaweiErrorCode::ERROR_LOGIN_PASSWORD_ERROR: return "Wrong password";
        case HuaweiErrorCode::ERROR_LOGIN_TOO_MANY_TIMES: return "Too many login attempts";
        case HuaweiErrorCode::ERROR_LOGIN_TOO_MANY_USERS_LOGINED: return "Too many users logged in";
        case HuaweiErrorCode::ERROR_LOGIN_USERNAME_OR_PASSWORD_ERROR: return "Wrong username or password";
        case HuaweiErrorCode::ERROR_LOGIN_ALREADY_LOGINED: return "Too many concurrent logins";
        case HuaweiErrorCode::ERROR_NO_RIGHT: return "Access denied";
        case HuaweiErrorCode::ERROR_NOT_SUPPORT: return "Not supported";
        case HuaweiErrorCode::ERROR_SYSTEM_BUSY: return "Device busy";
        case HuaweiErrorCode::ERROR_WRONG_TOKEN: return "Wrong token";
        case HuaweiErrorCode::ERROR_WRONG_SESSION: return "Wrong session";
        case HuaweiErrorCode::ERROR_WRONG_SESSION_TOKEN: return "Wrong session token";
        case HuaweiErrorCode::ERROR_PARAMETER_ERROR: return "Invalid parameter";
        case HuaweiErrorCode::ERROR_SET_NET_MODE_AND_BAND_FAILED: return "Setting net mode failed";
        case HuaweiErrorCode::ERROR_NET_REGISTER_NET_FAILED: return "Registering on provided PLMN failed";
        case HuaweiErrorCode::ERROR_FORMAT_ERROR: return "Invalid argument";
        default:;
    }

    std::stringstream sstr;
    sstr << "Unknown error code: " << errcode;
    return sstr.str().c_str();
}
