/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  NSK Plugin
 * Author:   Pavel Kalian
 *
 *******************************************************************************
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
#include <catch2/catch_test_macros.hpp>

using namespace NSKPlugin;
using namespace rapidjson;

TEST_CASE("HDT sentence produces headingTrue")
{
    NSK n;
    Document d;
    n.ProcessNMEASentence("$GPHDT,123.456,T*32", &d);

    REQUIRE(d.IsObject());
    REQUIRE(d.HasMember("updates"));
    REQUIRE(d["updates"].IsArray());
    REQUIRE(d["updates"].Size() > 0);
    REQUIRE(d["updates"][0].HasMember("values"));
    REQUIRE(d["updates"][0]["values"].IsArray());
    REQUIRE(d["updates"][0]["values"].Size() > 0);
    REQUIRE(std::string(d["updates"][0]["values"][0]["path"].GetString())
        == "navigation.headingTrue");
}

TEST_CASE("DBK sentence produces belowKeel depth")
{
    NSK n;
    Document d;
    n.ProcessNMEASentence("$SDDBK,7.2,f,2.2,M,1.2,F*3B", &d);

    REQUIRE(d.IsObject());
    REQUIRE(d.HasMember("updates"));
    REQUIRE(d["updates"].IsArray());
    REQUIRE(d["updates"].Size() > 0);
    REQUIRE(d["updates"][0].HasMember("values"));
    REQUIRE(d["updates"][0]["values"].IsArray());
    REQUIRE(d["updates"][0]["values"].Size() > 0);
    REQUIRE(std::string(d["updates"][0]["values"][0]["path"].GetString())
        == "environment.depth.belowKeel");
}

TEST_CASE("VWR sentence produces apparent wind values")
{
    NSK n;
    Document d;
    n.ProcessNMEASentence("$IIVWR,75,R,1.0,N,0.51,M,1.85,K*6C", &d);

    REQUIRE(d.IsObject());
    REQUIRE(d.HasMember("updates"));
    REQUIRE(d["updates"].IsArray());
    REQUIRE(d["updates"].Size() > 0);
    REQUIRE(d["updates"][0]["values"].IsArray());
    REQUIRE(d["updates"][0]["values"].Size() >= 2);
    REQUIRE(std::string(d["updates"][0]["values"][0]["path"].GetString())
        == "environment.wind.angleApparent");
    REQUIRE(std::string(d["updates"][0]["values"][1]["path"].GetString())
        == "environment.wind.speedApparent");
}

TEST_CASE("DSC sentence produces DSC notification")
{
    NSK n;
    Document d;
    n.ProcessNMEASentence(
        "$CDDSC,20,3380210040,00,21,26,1394807410,2231,,,B,E*75", &d);

    REQUIRE(d.IsObject());
    REQUIRE(d.HasMember("updates"));
    REQUIRE(d["updates"].IsArray());
    REQUIRE(d["updates"].Size() > 0);
    REQUIRE(d["updates"][0]["values"].IsArray());
    REQUIRE(d["updates"][0]["values"].Size() > 0);
    REQUIRE(std::string(d["updates"][0]["values"][0]["path"].GetString())
        == "notifications.dsc");
}
