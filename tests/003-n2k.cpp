/******************************************************************************
 * Copyright (C) 2026 Pavel Kalian
 * SPDX-License-Identifier: GPL-3.0-or-later
 ******************************************************************************/

#include "N2kMessages.h"
#include "nsk.h"
#include "opencpn_mock.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace Catch;
using namespace NSKPlugin;
using namespace rapidjson;

namespace {
std::vector<unsigned char> Frame(tN2kMsg& msg)
{
    msg.ForceSource(42);
    std::vector<unsigned char> frame { 0x93, 0x13, msg.Priority,
        static_cast<unsigned char>(msg.PGN),
        static_cast<unsigned char>(msg.PGN >> 8),
        static_cast<unsigned char>(msg.PGN >> 16), msg.Destination, msg.Source,
        0, 0, 0, 0, static_cast<unsigned char>(msg.DataLen) };
    frame.insert(frame.end(), msg.Data, msg.Data + msg.DataLen);
    frame.push_back(0);
    return frame;
}

const Value* FindValue(const Document& d, const char* path)
{
    if (!d.HasMember("updates")) {
        return nullptr;
    }
    for (const auto& value : d["updates"][0]["values"].GetArray()) {
        if (std::string(value["path"].GetString()) == path) {
            return &value["value"];
        }
    }
    return nullptr;
}

Document Convert(tN2kMsg& msg)
{
    NSK converter;
    Document result;
    converter.ProcessN2KMessage(Frame(msg), &result);
    return result;
}
}

TEST_CASE("NMEA 2000 heading, speed and position mappings")
{
    tN2kMsg msg;

    SECTION("magnetic heading")
    {
        SetN2kPGN127250(msg, 1, 1.2, 0.03, -0.1, N2khr_magnetic);
        auto d = Convert(msg);
        REQUIRE(FindValue(d, "navigation.headingMagnetic")->GetDouble()
            == Approx(1.2));
        REQUIRE(FindValue(d, "navigation.magneticDeviation")->GetDouble()
            == Approx(0.03));
        REQUIRE(FindValue(d, "navigation.magneticVariation")->GetDouble()
            == Approx(-0.1));
    }

    SECTION("water and ground speed, including zero")
    {
        SetN2kPGN128259(msg, 2, 0.0, 3.4);
        auto d = Convert(msg);
        REQUIRE(FindValue(d, "navigation.speedThroughWater")->GetDouble()
            == Approx(0.0));
        REQUIRE(FindValue(d, "navigation.speedOverGround")->GetDouble()
            == Approx(3.4));
    }

    SECTION("position")
    {
        SetN2kPGN129025(msg, -54.8, -68.3);
        auto d = Convert(msg);
        const auto* position = FindValue(d, "navigation.position");
        REQUIRE(d["updates"][0]["source"]["type"] == "NMEA2000");
        REQUIRE(d["updates"][0]["source"]["pgn"] == 129025);
        REQUIRE(d["updates"][0]["source"]["src"] == 42);
        REQUIRE((*position)["latitude"].GetDouble() == Approx(-54.8));
        REQUIRE((*position)["longitude"].GetDouble() == Approx(-68.3));
    }

    SECTION("magnetic course and speed")
    {
        SetN2kPGN129026(msg, 3, N2khr_magnetic, 2.1, 4.2);
        auto d = Convert(msg);
        REQUIRE(FindValue(d, "navigation.courseOverGroundMagnetic")->GetDouble()
            == Approx(2.1));
        REQUIRE(FindValue(d, "navigation.speedOverGround")->GetDouble()
            == Approx(4.2));
    }
}

TEST_CASE("NMEA 2000 depth mappings honor offset direction")
{
    tN2kMsg msg;
    SetN2kPGN128267(msg, 1, 8.2, 0.3);
    auto surface = Convert(msg);
    REQUIRE(FindValue(surface, "environment.depth.belowSurface")->GetDouble()
        == Approx(8.5));
    REQUIRE(
        FindValue(surface, "environment.depth.surfaceToTransducer")->GetDouble()
        == Approx(0.3));

    SetN2kPGN128267(msg, 1, 8.2, -0.4);
    auto keel = Convert(msg);
    REQUIRE(FindValue(keel, "environment.depth.belowKeel")->GetDouble()
        == Approx(7.8));
    REQUIRE(FindValue(keel, "environment.depth.transducerToKeel")->GetDouble()
        == Approx(0.4));
}

TEST_CASE("NMEA 2000 wind mappings follow the reference")
{
    tN2kMsg msg;
    SetN2kPGN130306(msg, 1, 6.2, 4.0, N2kWind_Apparent);
    auto apparent = Convert(msg);
    REQUIRE(FindValue(apparent, "environment.wind.speedApparent")->GetDouble()
        == Approx(6.2));
    REQUIRE(FindValue(apparent, "environment.wind.angleApparent")->GetDouble()
        == Approx(4.0 - 2 * 3.14159265358979323846));

    SetN2kPGN130306(msg, 1, 5.1, 0.7, N2kWind_True_North);
    auto ground = Convert(msg);
    REQUIRE(FindValue(ground, "environment.wind.speedOverGround")->GetDouble()
        == Approx(5.1));
    REQUIRE(FindValue(ground, "environment.wind.directionTrue")->GetDouble()
        == Approx(0.7));
}

TEST_CASE("Invalid and unsupported NMEA 2000 messages are ignored")
{
    NSK converter;
    Document result;
    converter.ProcessN2KMessage({ 0x93 }, &result);
    REQUIRE(result.IsNull());

    tN2kMsg msg;
    msg.SetPGN(130310);
    msg.AddByte(0);
    converter.ProcessN2KMessage(Frame(msg), &result);
    REQUIRE(result.IsNull());

    SetN2kPGN127250(
        msg, 1, N2kDoubleNA, N2kDoubleNA, N2kDoubleNA, N2khr_Unavailable);
    converter.ProcessN2KMessage(Frame(msg), &result);
    REQUIRE(result.IsNull());
    REQUIRE(converter.N2KTotal() == 3);
    REQUIRE(converter.TotalN2KErrors() == 1);
    REQUIRE(converter.TotalN2KIgnored() == 1);
    REQUIRE(converter.SKTotal() == 0);
}

TEST_CASE("NMEA 2000 PGNs are listed and can be ignored")
{
    NSK converter;
    auto pgn = converter.Known().find(known_sentence("PGN 129025", true));
    REQUIRE(pgn != converter.Known().end());
    REQUIRE(pgn->enabled);

    converter.UpdateKnown(known_sentence("PGN 129025", false));
    tN2kMsg msg;
    SetN2kPGN129025(msg, -54.8, -68.3);
    Document result;
    converter.ProcessN2KMessage(Frame(msg), &result);

    REQUIRE(result.IsNull());
    REQUIRE(converter.TotalN2KIgnored() == 1);
    REQUIRE(converter.SKTotal() == 0);
}

TEST_CASE("Oversized NMEA 2000 frames are rejected")
{
    NSK converter;
    Document result;

    auto oversized = std::vector<unsigned char>(269, 0);
    oversized[0] = 0x93;
    oversized[3] = 0x12;
    oversized[4] = 0xf2;
    oversized[5] = 0x01;
    oversized[12] = 255;
    converter.ProcessN2KMessage(oversized, &result);

    auto padded = oversized;
    padded.resize(300);
    padded[12] = 8;
    converter.ProcessN2KMessage(padded, &result);

    REQUIRE(result.IsNull());
    REQUIRE(converter.TotalN2KErrors() == 2);
}
