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

// SignalValue

template<>
const char *const SignalValue<>::getTypeStrs[] =
{
    "Current", "Min", "Worst", "Max", "Best", "First",
    "Previous", "Average", "Invalid",
    nullptr
};

template<>
const char *SignalValue<>::getGetTypeStr(SignalValue<>::GetType type)
{
    return getTypeStrs[type];
}

template<>
SignalValue<>::GetType SignalValue<>::getGetTypeByStr(const char *type)
{
    for (size_t i = 0; getTypeStrs[i]; i++)
    {
        if (!strcasecmp(type, getTypeStrs[i])) return (GetType)i;
    }

    return GET_INVALID;
}

// Signal

Signal sig;

// AT

// CERSSI

bool Signal::AT::CERSSI_LTE::isSet() const
{
    if (!RSRQ.isSet()) return false;

    for (int i = 0; i < numAntennas; i++)
    {
        if (!RSRP[i].isSet()) return false;
    }

    if (numAntennas > 2)
    {
        for (int i = 0; i < numAntennas; i++)
        {
            if (!SINR[i].isSet()) return false;
        }
    }
    else if (!SINR[0].isSet()) return false;

    return RI.isSet() && CQI[0].isSet() && CQI[1].isSet();
}

bool Signal::AT::CERSSI_WCDMA::isSet() const
{
    return RSCP.isSet() && ECIO.isSet();
}

bool Signal::AT::CERSSI_GSM::isSet() const
{
    return RSSI.isSet();
}

// HCSQ

bool Signal::AT::HCSQ_LTE::isSet() const
{
    if (!RSRP.isSet() || !RSRQ.isSet()) return false;
    return RSSI.isSet() && SINR.isSet();
}

bool Signal::AT::HCSQ_WCDMA::isSet() const
{
    if (!RSSI.isSet() || !RSCP.isSet()) return false;
    return ECIO.isSet();
}

bool Signal::AT::HCSQ_GSM::isSet() const
{
    return RSSI.isSet();
}

// CQI / RSSI

bool Signal::AT::RSSI::isSet() const
{
    return RSSILevel.isSet();
}

// AT

NetType Signal::AT::getNetType() const
{
    if (cerssiLTE.isSet()) return NetType::LTE;
    else if (cerssiWCDMA.isSet()) return NetType::WCDMA;
    else if (cerssiGSM.isSet()) return NetType::GSM;
    return NetType::INVALID;
}

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
    std::vector<std::string> bands;
    unsigned long long lteBand = 0;

    if (band[0] == '+') band++;

    if (splitStr(bands, band, "+", false) > 0)
    {
        for (auto &band : bands)
        {
            LTEBand bits = getLTEBandFromStr2(band.c_str());

            if (bits == LTEBand::LTE_BAND_ERROR)
            {
                lteBand = LTEBand::LTE_BAND_ERROR;
                break;
            }

            lteBand |= bits;
        }
    }

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

    StrBuf str;
    str.format("Unknown error code: %d", errcode);

    return std::move(str);
}
