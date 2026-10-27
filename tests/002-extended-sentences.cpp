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
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

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
    n.ProcessNMEASentence("$SDDBK,7.2,f,2.2,M,1.2,F*1F", &d);

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

namespace {
/// Process the sentence and return the value with the given path (null if
/// missing)
const Value& Find(Document& d, const char* path)
{
    static const Value null_value;
    if (!d.IsObject() || !d.HasMember("updates")) {
        return null_value;
    }
    for (auto& v : d["updates"][0]["values"].GetArray()) {
        if (std::string(v["path"].GetString()) == path) {
            return v["value"];
        }
    }
    return null_value;
}
}

TEST_CASE("GGA sentence produces GNSS details")
{
    NSK n;
    Document d;
    n.ProcessNMEASentence(
        "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47",
        &d);
    REQUIRE(Find(d, "navigation.position").IsObject());
    REQUIRE(std::string(Find(d, "navigation.gnss.methodQuality").GetString())
        == "GNSS Fix");
    REQUIRE(Find(d, "navigation.gnss.satellites").GetDouble() == 8);
    REQUIRE(Find(d, "navigation.gnss.antennaAltitude").GetDouble() == 545.4);
}

TEST_CASE("RMC sentence produces course over ground and ISO datetime")
{
    NSK n;
    Document d;
    n.ProcessNMEASentence("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,"
                          "230324,003.1,W*61",
        &d);
    REQUIRE(Find(d, "navigation.courseOverGroundTrue").IsNumber());
    REQUIRE(Find(d, "navigation.headingTrue").IsNull());
    REQUIRE(Find(d, "navigation.magneticVariation").GetDouble() < 0);
    REQUIRE(std::string(Find(d, "navigation.datetime").GetString())
        == "2024-03-23T12:35:19.000Z");
}

TEST_CASE("VTG sentence produces course over ground")
{
    NSK n;
    Document d;
    n.ProcessNMEASentence("$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48", &d);
    REQUIRE(Find(d, "navigation.courseOverGroundTrue").IsNumber());
    REQUIRE(Find(d, "navigation.courseOverGroundMagnetic").IsNumber());
}

TEST_CASE("HDG sentence applies deviation and variation")
{
    NSK n;
    Document d;
    n.ProcessNMEASentence("$HCHDG,98.3,0.0,E,12.6,W*57", &d);
    REQUIRE(Find(d, "navigation.headingTrue").GetDouble()
        == Catch::Approx(85.7 * M_PI / 180.0));
}

TEST_CASE("GSV sentences are aggregated into satellitesInView")
{
    NSK n;
    Document d;
    n.ProcessNMEASentence(
        "$GPGSV,2,1,05,01,40,083,46,02,17,308,41,12,07,344,39,14,22,228,45*78",
        &d);
    REQUIRE(!d.IsObject());
    n.ProcessNMEASentence("$GPGSV,2,2,05,15,10,100,30*4B", &d);
    const auto& v = Find(d, "navigation.gnss.satellitesInView");
    REQUIRE(v["count"].GetInt() == 5);
    REQUIRE(v["satellites"].Size() == 5);
}

TEST_CASE("APB and XTE cross track error sign follows SignalK")
{
    NSK n;
    Document d;
    n.ProcessNMEASentence(
        "$GPAPB,A,A,0.10,R,N,V,V,011,M,DEST,011,M,011,M*3C", &d);
    REQUIRE(Find(d, "navigation.courseRhumbline.crossTrackError").GetDouble()
        == Catch::Approx(-185.2));
    REQUIRE(std::string(
                Find(d, "navigation.courseRhumbline.nextPoint.ID").GetString())
        == "DEST");
    REQUIRE(Find(d, "steering.autopilot.target.headingMagnetic").IsNumber());
    n.ProcessNMEASentence("$GPXTE,A,A,0.67,L,N*6F", &d);
    REQUIRE(Find(d, "navigation.courseRhumbline.crossTrackError").GetDouble()
        == Catch::Approx(0.67 * 1852.0));
}

TEST_CASE("VDM sentence produces AIS target delta with context")
{
    NSK n;
    Document d;
    n.ProcessNMEASentence(
        "!AIVDM,1,1,,A,13u?etPv2;0n:dDPwUM1U1Cb069D,0*24", &d);
    REQUIRE(std::string(d["context"].GetString())
        == "vessels.urn:mrn:imo:mmsi:265547250");
    REQUIRE(Find(d, "navigation.position").IsObject());
    REQUIRE(std::string(Find(d, "sensors.ais.class").GetString()) == "A");
    // Own sentences right after must not inherit the AIS context
    n.ProcessNMEASentence("$GPXTE,A,A,0.67,L,N*6F", &d);
    REQUIRE(!d.HasMember("context"));
}

TEST_CASE("Other sentences map to SignalK paths")
{
    NSK n;
    Document d;
    n.ProcessNMEASentence("$STALK,52,01,64,00*45", &d);
    REQUIRE(Find(d, "navigation.speedOverGround").GetDouble()
        == Catch::Approx(10.0 * 1852.0 / 3600.0));
    n.ProcessNMEASentence("$IIRPM,E,1,2418.2,10.5,A*5F", &d);
    REQUIRE(Find(d, "propulsion.engine_1.revolutions").GetDouble()
        == Catch::Approx(2418.2 / 60.0));
    n.ProcessNMEASentence(
        "$IIXDR,P,1.02481,B,Barometer,C,19.52,C,AirTemp*7E", &d);
    REQUIRE(Find(d, "environment.outside.pressure").GetDouble()
        == Catch::Approx(102481.0));
    REQUIRE(Find(d, "environment.outside.temperature").GetDouble()
        == Catch::Approx(292.67));
    n.ProcessNMEASentence("$IIVBW,5.5,0.1,A,6.0,0.2,A*46", &d);
    REQUIRE(Find(d, "navigation.speedThroughWaterTransverse").IsNumber());
    n.ProcessNMEASentence("$GPZTG,145832.12,011500.00,WPT*2E", &d);
    REQUIRE(Find(d, "navigation.courseRhumbline.nextPoint.timeToGo").GetDouble()
        == Catch::Approx(4500.0));
    n.ProcessNMEASentence("$GPZDA,160012.71,11,03,2004,-1,00*7D", &d);
    REQUIRE(std::string(Find(d, "navigation.datetime").GetString())
        == "2004-03-11T16:00:12.710Z");
}
