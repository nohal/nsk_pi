/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  NSK Plugin
 * Author:   Pavel Kalian
 *
 ******************************************************************************
 * This file is part of the NSK plugin
 * (https://github.com/nohal/nsk_pi).
 *   Copyright (C) 2022 by Pavel Kalian
 *   https://github.com/nohal
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3, or (at your option) any later
 * version of the license.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

#include "nsk.h"
#include "opencpn_mock.h"
#include "rapidjson/document.h"
#include "utils.h"
#include <catch2/catch_test_macros.hpp>

using namespace NSKPlugin;
using namespace rapidjson;

TEST_CASE("GLL Parsing")
{
    NSK n;
    Document d;
    n.ProcessNMEASentence(
        "$GPGLL,3723.2475,N,12158.3416,W,161229.487,A,A*41", &d);
    DumpJSON(d);

    REQUIRE(d.IsObject());
    REQUIRE(d.HasMember("updates"));
    REQUIRE(d["updates"].IsArray());
    REQUIRE(d["updates"][0].HasMember("source"));
    REQUIRE(d["updates"][0].HasMember("timestamp"));
    REQUIRE(d["updates"][0].HasMember("values"));
    // TODO
}
