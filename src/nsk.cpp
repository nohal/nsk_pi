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

#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include <fstream>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "nsk.h"
#include <ocpn_plugin.h>

PLUGIN_BEGIN_NAMESPACE

#define kn2ms(x) (0.51444444444 * (x))
#define kmh2ms(x) (0.27777777778 * (x))
#define FATHOM2METER 1.8288
#define FOOT2METER 0.3048
#define NM2METER 1852.0
#define KELVIN_OFFSET 273.15

using namespace rapidjson;

using namespace marnav;
using namespace nmea;

namespace {
void AddNumber(rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator, const char* path,
    double value)
{
    Value val(kObjectType);
    val.AddMember("path", Value(path, allocator), allocator);
    val.AddMember("value", value, allocator);
    values_array.PushBack(val, allocator);
}

void AddString(rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator, const char* path,
    const std::string& value)
{
    Value val(kObjectType);
    val.AddMember("path", Value(path, allocator), allocator);
    val.AddMember(
        "value", Value(value.c_str(), value.length(), allocator), allocator);
    values_array.PushBack(val, allocator);
}
}

/**
 * Generate a UTC ISO8601-formatted timestamp
 * and return as std::string
 */
inline std::string currentISO8601TimeUTC()
{
    auto now = std::chrono::system_clock::now();
    auto itt = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(gmtime(&itt), "%FT%TZ");
    return ss.str();
}

// Sentence processing implementations
void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::gga> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    Value val(kObjectType);
    Value pos(kObjectType);
    if (s->get_lat().has_value() && s->get_lon().has_value()) {
        pos.AddMember("latitude", s->get_lat()->get(), allocator);
        pos.AddMember("longitude", s->get_lon()->get(), allocator);
        if (s->get_altitude().has_value()) {
            pos.AddMember("altitude", s->get_altitude()->value(), allocator);
        }
        val.AddMember("path", "navigation.position", allocator);
        val.AddMember("value", pos, allocator);
        values_array.PushBack(val, allocator);
    }
    if (s->get_time().has_value()) {
        Value utc(kObjectType);
        utc.AddMember("path", "environment.time", allocator);
        utc.AddMember("value", to_string(s->get_time()), allocator);
        values_array.PushBack(utc, allocator);
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::gll> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    Value val(kObjectType);
    Value pos(kObjectType);
    if (s->get_lat().has_value() && s->get_lon().has_value()) {
        pos.AddMember("latitude", s->get_lat()->get(), allocator);
        pos.AddMember("longitude", s->get_lon()->get(), allocator);

        val.AddMember("path", "navigation.position", allocator);
        val.AddMember("value", pos, allocator);
        values_array.PushBack(val, allocator);
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::gsa> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_hdop().has_value()) {
        Value val(kObjectType);
        val.AddMember("path", "navigation.gnss.horizontalDilution", allocator);
        val.AddMember("value", s->get_hdop().value(), allocator);
        values_array.PushBack(val, allocator);
    }
    if (s->get_pdop().has_value()) {
        Value val(kObjectType);
        val.AddMember("path", "navigation.gnss.positionDilution", allocator);
        val.AddMember("value", s->get_pdop().value(), allocator);
        values_array.PushBack(val, allocator);
    }
    // TODO: There is more info available
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::gsv> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    Value val(kObjectType);
    val.AddMember("path", "navigation.gnss.satellites", allocator);
    val.AddMember("value", s->get_n_satellites_in_view(), allocator);
    values_array.PushBack(val, allocator);
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::rmc> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_lat().has_value() && s->get_lon().has_value()) {
        Value val(kObjectType);
        Value pos(kObjectType);
        pos.AddMember("latitude", s->get_lat()->get(), allocator);
        pos.AddMember("longitude", s->get_lon()->get(), allocator);
        val.AddMember("path", "navigation.position", allocator);
        val.AddMember("value", pos, allocator);
        values_array.PushBack(val, allocator);
    }
    if (s->get_heading().has_value()) {
        Value hdg(kObjectType);
        hdg.AddMember("path", "navigation.headingTrue", allocator);
        hdg.AddMember("value", deg2rad(*s->get_heading()), allocator);
        values_array.PushBack(hdg, allocator);
    }

    auto rmc_sog = s->get_sog();
    if (rmc_sog.has_value()) {
        Value sog(kObjectType);
        sog.AddMember("path", "navigation.speedOverGround", allocator);
        sog.AddMember("value",
            kn2ms(rmc_sog->get<marnav::units::knots>().value()), allocator);
        values_array.PushBack(sog, allocator);
    }
    if (s->get_time_utc().has_value()) {
        Value utc(kObjectType);
        utc.AddMember("path", "navigation.datetime", allocator);
        utc.AddMember("value", to_string(s->get_time_utc()), allocator);
        values_array.PushBack(utc, allocator);
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::vtg> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_track_true().has_value()) {
        Value hdt(kObjectType);
        hdt.AddMember("path", "navigation.headingTrue", allocator);
        hdt.AddMember("value", deg2rad(s->get_track_true().value()), allocator);
        values_array.PushBack(hdt, allocator);
    }
    if (s->get_track_magn().has_value()) {
        Value trm(kObjectType);
        trm.AddMember("path", "navigation.headingMagnetic", allocator);
        trm.AddMember("value", deg2rad(s->get_track_magn().value()), allocator);
        values_array.PushBack(trm, allocator);
    }
    if (s->get_speed_kn().has_value()) {
        Value sog(kObjectType);
        sog.AddMember("path", "navigation.speedOverGround", allocator);
        sog.AddMember("value", kn2ms(s->get_speed_kn()->value()), allocator);
        values_array.PushBack(sog, allocator);
    } else if (s->get_speed_kmh().has_value()) {
        Value sog(kObjectType);
        sog.AddMember("path", "navigation.speedOverGround", allocator);
        sog.AddMember("value", kmh2ms(s->get_speed_kmh()->value()), allocator);
        values_array.PushBack(sog, allocator);
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::dbt> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_depth_meter().has_value()) {
        Value dbt(kObjectType);
        dbt.AddMember("path", "environment.depth.belowTransducer", allocator);
        dbt.AddMember("value", s->get_depth_meter().value().value(), allocator);
        values_array.PushBack(dbt, allocator);
    } else if (s->get_depth_feet().has_value()) {
        Value dbt(kObjectType);
        dbt.AddMember("path", "environment.depth.belowTransducer", allocator);
        dbt.AddMember("value", s->get_depth_feet().value().value() * FOOT2METER,
            allocator);
        values_array.PushBack(dbt, allocator);
    } else if (s->get_depth_fathom().has_value()) {
        Value dbt(kObjectType);
        dbt.AddMember("path", "environment.depth.belowTransducer", allocator);
        dbt.AddMember("value",
            s->get_depth_fathom().value().value() * FATHOM2METER, allocator);
        values_array.PushBack(dbt, allocator);
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::dbk> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_depth_meter().has_value()) {
        AddNumber(values_array, allocator, "environment.depth.belowKeel",
            s->get_depth_meter()->get<units::meters>().value());
    } else if (s->get_depth_feet().has_value()) {
        AddNumber(values_array, allocator, "environment.depth.belowKeel",
            s->get_depth_feet()->get<units::feet>().value() * FOOT2METER);
    } else if (s->get_depth_fathom().has_value()) {
        AddNumber(values_array, allocator, "environment.depth.belowKeel",
            s->get_depth_fathom()->get<units::fathoms>().value()
                * FATHOM2METER);
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::dsc> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    const auto category = to_name(s->get_cat());
    const auto mmsi = std::to_string(
        static_cast<marnav::utils::mmsi::value_type>(s->get_mmsi()));
    AddString(values_array, allocator, "notifications.dsc",
        "DSC " + category + " message from MMSI " + mmsi);
    if (s->get_cat() == nmea::dsc::category::distress) {
        AddString(values_array, allocator, "notifications.distress",
            "DSC distress message from MMSI " + mmsi);
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::dpt> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    const auto depth = s->get_depth_meter().get<units::meters>().value();
    AddNumber(
        values_array, allocator, "environment.depth.belowTransducer", depth);

    const auto offset = s->get_transducer_offset().get<units::meters>().value();
    AddNumber(values_array, allocator, "environment.depth.surfaceToTransducer",
        offset);
    if (offset < 0) {
        AddNumber(values_array, allocator, "environment.depth.transducerToKeel",
            -offset);
        AddNumber(values_array, allocator, "environment.depth.belowKeel",
            depth + offset);
    } else {
        AddNumber(values_array, allocator, "environment.depth.belowSurface",
            depth + offset);
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::gns> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_lat().has_value() && s->get_lon().has_value()) {
        Value pos(kObjectType);
        pos.AddMember("latitude", s->get_lat()->get(), allocator);
        pos.AddMember("longitude", s->get_lon()->get(), allocator);
        Value val(kObjectType);
        val.AddMember("path", "navigation.position", allocator);
        val.AddMember("value", pos, allocator);
        values_array.PushBack(val, allocator);
    }
    if (s->get_hdrop().has_value()) {
        AddNumber(values_array, allocator, "navigation.gnss.horizontalDilution",
            *s->get_hdrop());
    }
    if (s->get_number_of_satellites().has_value()) {
        AddNumber(values_array, allocator, "navigation.gnss.satellites",
            *s->get_number_of_satellites());
    }
    if (s->get_antenna_altitude().has_value()) {
        AddNumber(values_array, allocator, "navigation.gnss.antennaAltitude",
            s->get_antenna_altitude()->get<units::meters>().value());
    }
    if (s->get_geodial_separation().has_value()) {
        AddNumber(values_array, allocator, "navigation.gnss.geoidalSeparation",
            s->get_geodial_separation()->get<units::meters>().value());
    }
    if (s->get_age_of_differential_data().has_value()) {
        AddNumber(values_array, allocator, "navigation.gnss.differentialAge",
            *s->get_age_of_differential_data());
    }
    if (s->get_differential_ref_station_id().has_value()) {
        AddNumber(values_array, allocator,
            "navigation.gnss.differentialReference",
            *s->get_differential_ref_station_id());
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::hdg> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_heading().has_value()) {
        AddNumber(values_array, allocator, "navigation.headingMagnetic",
            deg2rad(*s->get_heading()));
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::hdm> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_heading().has_value()) {
        AddNumber(values_array, allocator, "navigation.headingMagnetic",
            deg2rad(*s->get_heading()));
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::hdt> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_heading().has_value()) {
        AddNumber(values_array, allocator, "navigation.headingTrue",
            deg2rad(*s->get_heading()));
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::hsc> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_heading_true().has_value()) {
        AddNumber(values_array, allocator,
            "steering.autopilot.target.headingTrue",
            deg2rad(*s->get_heading_true()));
    }
    if (s->get_heading_mag().has_value()) {
        AddNumber(values_array, allocator,
            "steering.autopilot.target.headingMagnetic",
            deg2rad(*s->get_heading_mag()));
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::mta> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    AddNumber(values_array, allocator, "environment.outside.temperature",
        s->get_temperature().value() + KELVIN_OFFSET);
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::mtw> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    AddNumber(values_array, allocator, "environment.water.temperature",
        s->get_temperature().get<units::celsius>().value() + KELVIN_OFFSET);
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::mwd> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_direction_true().has_value()) {
        AddNumber(values_array, allocator, "environment.wind.directionTrue",
            deg2rad(*s->get_direction_true()));
    }
    if (s->get_direction_mag().has_value()) {
        AddNumber(values_array, allocator, "environment.wind.directionMagnetic",
            deg2rad(*s->get_direction_mag()));
    }
    if (s->get_speed_ms().has_value()) {
        AddNumber(values_array, allocator, "environment.wind.speedTrue",
            s->get_speed_ms()->get<units::meters_per_second>().value());
    } else if (s->get_speed_kn().has_value()) {
        AddNumber(values_array, allocator, "environment.wind.speedTrue",
            s->get_speed_kn()->get<units::knots>().value() * kn2ms(1));
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::mwv> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (!s->get_angle().has_value() || !s->get_speed().has_value()
        || !s->get_angle_ref().has_value()) {
        return;
    }
    const auto angle_ref = to_string(*s->get_angle_ref());
    const auto speed = s->get_speed()->get<units::meters_per_second>().value();
    if (angle_ref == "R") {
        AddNumber(values_array, allocator, "environment.wind.angleApparent",
            deg2rad(*s->get_angle()));
        AddNumber(
            values_array, allocator, "environment.wind.speedApparent", speed);
    } else {
        AddNumber(values_array, allocator, "environment.wind.angleTrueWater",
            deg2rad(*s->get_angle()));
        AddNumber(values_array, allocator, "environment.wind.speedTrue", speed);
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::rmb> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_lat().has_value() && s->get_lon().has_value()) {
        Value pos(kObjectType);
        pos.AddMember("latitude", s->get_lat()->get(), allocator);
        pos.AddMember("longitude", s->get_lon()->get(), allocator);
        Value val(kObjectType);
        val.AddMember(
            "path", "navigation.courseRhumbline.nextPoint.position", allocator);
        val.AddMember("value", pos, allocator);
        values_array.PushBack(val, allocator);
    }
    if (s->get_bearing().has_value()) {
        AddNumber(values_array, allocator,
            "navigation.courseRhumbline.nextPoint.bearingTrue",
            deg2rad(*s->get_bearing()));
    }
    if (s->get_dst_velocity().has_value()) {
        AddNumber(values_array, allocator,
            "navigation.courseRhumbline.nextPoint.velocityMadeGood",
            s->get_dst_velocity()->get<units::knots>().value() * kn2ms(1));
    }
    if (s->get_range().has_value()) {
        AddNumber(values_array, allocator,
            "navigation.courseRhumbline.nextPoint.distance",
            s->get_range()->get<units::nautical_miles>().value() * NM2METER);
    }
    if (s->get_cross_track_error().has_value()) {
        AddNumber(values_array, allocator,
            "navigation.courseRhumbline.crossTrackError",
            s->get_cross_track_error()->get<units::nautical_miles>().value()
                * NM2METER);
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::rot> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_deg_per_minute().has_value()) {
        AddNumber(values_array, allocator, "navigation.rateOfTurn",
            deg2rad(*s->get_deg_per_minute()) / 60.0);
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::rpm> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_revolutions().has_value()) {
        AddNumber(values_array, allocator, "propulsion.main.revolutions",
            *s->get_revolutions() / 60.0);
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::rsa> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_rudder1().has_value()) {
        AddNumber(values_array, allocator, "steering.rudderAngle",
            deg2rad(*s->get_rudder1()));
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::vdr> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_degrees_true().has_value() && s->get_speed().has_value()) {
        Value cur(kObjectType);
        cur.AddMember("setTrue", deg2rad(*s->get_degrees_true()), allocator);
        cur.AddMember("drift",
            s->get_speed()->get<units::knots>().value() * kn2ms(1), allocator);
        Value val(kObjectType);
        val.AddMember("path", "environment.current", allocator);
        val.AddMember("value", cur, allocator);
        values_array.PushBack(val, allocator);
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::vhw> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_heading_true().has_value()) {
        AddNumber(values_array, allocator, "navigation.headingTrue",
            deg2rad(*s->get_heading_true()));
    }
    if (s->get_heading_magn().has_value()) {
        AddNumber(values_array, allocator, "navigation.headingMagnetic",
            deg2rad(*s->get_heading_magn()));
    }
    if (s->get_speed_knots().has_value()) {
        AddNumber(values_array, allocator, "navigation.speedThroughWater",
            s->get_speed_knots()->value() * kn2ms(1));
    } else if (s->get_speed_kmh().has_value()) {
        AddNumber(values_array, allocator, "navigation.speedThroughWater",
            s->get_speed_kmh()->value() * kmh2ms(1));
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::vlw> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_distance_cum().has_value()) {
        AddNumber(values_array, allocator, "navigation.log",
            s->get_distance_cum()->get<units::nautical_miles>().value()
                * NM2METER);
    }
    if (s->get_distance_reset().has_value()) {
        AddNumber(values_array, allocator, "navigation.trip.log",
            s->get_distance_reset()->get<units::nautical_miles>().value()
                * NM2METER);
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::vpw> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_speed_meters_per_second().has_value()) {
        AddNumber(values_array, allocator, "performance.velocityMadeGood",
            s->get_speed_meters_per_second()->value());
    } else if (s->get_speed_knots().has_value()) {
        AddNumber(values_array, allocator, "performance.velocityMadeGood",
            s->get_speed_knots()->value() * kn2ms(1));
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::vwr> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (!s->get_angle().has_value() || !s->get_angle_side().has_value()) {
        return;
    }
    auto angle = *s->get_angle();
    if (to_string(*s->get_angle_side()) == "L") {
        angle *= -1.0;
    }
    AddNumber(values_array, allocator, "environment.wind.angleApparent",
        deg2rad(angle));

    if (s->get_speed_knots().has_value()) {
        AddNumber(values_array, allocator, "environment.wind.speedApparent",
            s->get_speed_knots()->get<units::knots>().value() * kn2ms(1));
    } else if (s->get_speed_mps().has_value()) {
        AddNumber(values_array, allocator, "environment.wind.speedApparent",
            s->get_speed_mps()->get<units::meters_per_second>().value());
    } else if (s->get_speed_kmh().has_value()) {
        AddNumber(values_array, allocator, "environment.wind.speedApparent",
            s->get_speed_kmh()->get<units::kilometers_per_hour>().value()
                * kmh2ms(1));
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::xte> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (!s->get_cross_track_error_magnitude().has_value()) {
        return;
    }
    auto xte = *s->get_cross_track_error_magnitude();
    if (s->get_cross_track_unit().has_value()) {
        const auto unit = to_string(*s->get_cross_track_unit());
        if (unit == "N") {
            xte *= NM2METER;
        } else if (unit == "K") {
            xte *= 1000.0;
        }
    }
    if (s->get_direction_to_steer().has_value()
        && to_string(*s->get_direction_to_steer()) == "L") {
        xte *= -1.0;
    }
    AddNumber(values_array, allocator,
        "navigation.courseRhumbline.crossTrackError", xte);
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::zda> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_time_utc().has_value() && s->get_date().has_value()) {
        AddString(values_array, allocator, "navigation.datetime",
            to_string(*s->get_date()) + "T" + to_string(*s->get_time_utc())
                + "Z");
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::bod> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_bearing_true().has_value()) {
        AddNumber(values_array, allocator,
            "navigation.courseRhumbline.bearingTrackTrue",
            deg2rad(*s->get_bearing_true()));
    }
    if (s->get_bearing_magn().has_value()) {
        AddNumber(values_array, allocator,
            "navigation.courseRhumbline.bearingTrackMagnetic",
            deg2rad(*s->get_bearing_magn()));
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::bwc> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_lat().has_value() && s->get_lon().has_value()) {
        Value pos(kObjectType);
        pos.AddMember("latitude", s->get_lat()->get(), allocator);
        pos.AddMember("longitude", s->get_lon()->get(), allocator);
        Value val(kObjectType);
        val.AddMember("path", "navigation.courseGreatCircle.nextPoint.position",
            allocator);
        val.AddMember("value", pos, allocator);
        values_array.PushBack(val, allocator);
    }
    if (s->get_distance().has_value()) {
        AddNumber(values_array, allocator,
            "navigation.courseGreatCircle.nextPoint.distance",
            s->get_distance()->get<units::nautical_miles>().value() * NM2METER);
    }
    if (s->get_bearing_true().has_value()) {
        AddNumber(values_array, allocator,
            "navigation.courseGreatCircle.bearingTrackTrue",
            deg2rad(*s->get_bearing_true()));
    }
    if (s->get_bearing_mag().has_value()) {
        AddNumber(values_array, allocator,
            "navigation.courseGreatCircle.bearingTrackMagnetic",
            deg2rad(*s->get_bearing_mag()));
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::bwr> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_lat().has_value() && s->get_lon().has_value()) {
        Value pos(kObjectType);
        pos.AddMember("latitude", s->get_lat()->get(), allocator);
        pos.AddMember("longitude", s->get_lon()->get(), allocator);
        Value val(kObjectType);
        val.AddMember(
            "path", "navigation.courseRhumbline.nextPoint.position", allocator);
        val.AddMember("value", pos, allocator);
        values_array.PushBack(val, allocator);
    }
    if (s->get_bearing_true().has_value()) {
        AddNumber(values_array, allocator,
            "navigation.courseRhumbline.bearingTrackTrue",
            deg2rad(*s->get_bearing_true()));
    }
    if (s->get_bearing_mag().has_value()) {
        AddNumber(values_array, allocator,
            "navigation.courseRhumbline.bearingTrackMagnetic",
            deg2rad(*s->get_bearing_mag()));
    }
    if (s->get_distance().has_value()) {
        AddNumber(values_array, allocator,
            "navigation.courseRhumbline.nextPoint.distance",
            s->get_distance()->get<units::nautical_miles>().value() * NM2METER);
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::apb> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_cross_track_error_magnitude().has_value()) {
        auto xte = *s->get_cross_track_error_magnitude();
        if (s->get_cross_track_unit().has_value()
            && to_string(*s->get_cross_track_unit()) == "N") {
            xte *= NM2METER;
        }
        if (s->get_direction_to_steer().has_value()
            && to_string(*s->get_direction_to_steer()) == "L") {
            xte *= -1.0;
        }
        AddNumber(values_array, allocator,
            "navigation.courseRhumbline.crossTrackError", xte);
    }
    if (s->get_bearing_origin_to_destination().has_value()) {
        AddNumber(values_array, allocator,
            "navigation.courseRhumbline.bearingTrackTrue",
            deg2rad(*s->get_bearing_origin_to_destination()));
    }
    if (s->get_bearing_pos_to_destination().has_value()) {
        AddNumber(values_array, allocator,
            "navigation.courseRhumbline.nextPoint.bearingTrue",
            deg2rad(*s->get_bearing_pos_to_destination()));
    }
    if (s->get_heading_to_steer_to_destination().has_value()) {
        AddNumber(values_array, allocator,
            "steering.autopilot.target.headingTrue",
            deg2rad(*s->get_heading_to_steer_to_destination()));
    }
}

// --- End of sentence processing implementations

void NSK::ProcessNMEASentence(
    const std::string& stc, rapidjson::Document* outdoc)
{
    if (chrono::system_clock::now() - m_counters_start > 5s) {
        m_counters_start = chrono::system_clock::now();
        m_nmea_received = 0;
        m_sk_produced = 0;
    }
    ++m_nmea_received;
    ++m_nmea_received_total;
    try {
        Document d;
        Value src(kObjectType);
        Value upd(kObjectType);
        Value values(kArrayType);
        d.SetObject();
        rapidjson::Document::AllocatorType& allocator = d.GetAllocator();

        bool processed = true;
        auto s = make_sentence(stc);
        known_sentence ks(*s);
        auto ksit = m_known.find(ks);
        if (ksit == m_known.end() || ksit->enabled) {
            //  TODO (maybe, context is optional and we actually don't need it):
            //  "context": "vessels.urn:mrn:imo:mmsi:234567890",
            src.AddMember("sentence", s->tag(), allocator);
            src.AddMember("talker", to_string(s->get_talker()), allocator);

            switch (s->id()) {
            // Newly implemented sentences have to be added bellow
            case sentence_id::GGA:
                ProcessSentence(sentence_cast<nmea::gga>(s), values, allocator);
                break;
            case sentence_id::GLL:
                ProcessSentence(sentence_cast<nmea::gll>(s), values, allocator);
                break;
            case sentence_id::GSA:
                ProcessSentence(sentence_cast<nmea::gsa>(s), values, allocator);
                break;
            case sentence_id::GSV:
                ProcessSentence(sentence_cast<nmea::gsv>(s), values, allocator);
                break;
            case sentence_id::RMC:
                ProcessSentence(sentence_cast<nmea::rmc>(s), values, allocator);
                break;
            case sentence_id::VTG:
                ProcessSentence(sentence_cast<nmea::vtg>(s), values, allocator);
                break;
            case sentence_id::DBT:
                ProcessSentence(sentence_cast<nmea::dbt>(s), values, allocator);
                break;
            case sentence_id::DBK:
                ProcessSentence(sentence_cast<nmea::dbk>(s), values, allocator);
                break;
            case sentence_id::DSC:
                ProcessSentence(sentence_cast<nmea::dsc>(s), values, allocator);
                break;
            case sentence_id::DPT:
                ProcessSentence(sentence_cast<nmea::dpt>(s), values, allocator);
                break;
            case sentence_id::GNS:
                ProcessSentence(sentence_cast<nmea::gns>(s), values, allocator);
                break;
            case sentence_id::HDG:
                ProcessSentence(sentence_cast<nmea::hdg>(s), values, allocator);
                break;
            case sentence_id::HDM:
                ProcessSentence(sentence_cast<nmea::hdm>(s), values, allocator);
                break;
            case sentence_id::HDT:
                ProcessSentence(sentence_cast<nmea::hdt>(s), values, allocator);
                break;
            case sentence_id::HSC:
                ProcessSentence(sentence_cast<nmea::hsc>(s), values, allocator);
                break;
            case sentence_id::MTA:
                ProcessSentence(sentence_cast<nmea::mta>(s), values, allocator);
                break;
            case sentence_id::MTW:
                ProcessSentence(sentence_cast<nmea::mtw>(s), values, allocator);
                break;
            case sentence_id::MWD:
                ProcessSentence(sentence_cast<nmea::mwd>(s), values, allocator);
                break;
            case sentence_id::MWV:
                ProcessSentence(sentence_cast<nmea::mwv>(s), values, allocator);
                break;
            case sentence_id::RMB:
                ProcessSentence(sentence_cast<nmea::rmb>(s), values, allocator);
                break;
            case sentence_id::ROT:
                ProcessSentence(sentence_cast<nmea::rot>(s), values, allocator);
                break;
            case sentence_id::RPM:
                ProcessSentence(sentence_cast<nmea::rpm>(s), values, allocator);
                break;
            case sentence_id::RSA:
                ProcessSentence(sentence_cast<nmea::rsa>(s), values, allocator);
                break;
            case sentence_id::VDR:
                ProcessSentence(sentence_cast<nmea::vdr>(s), values, allocator);
                break;
            case sentence_id::VHW:
                ProcessSentence(sentence_cast<nmea::vhw>(s), values, allocator);
                break;
            case sentence_id::VLW:
                ProcessSentence(sentence_cast<nmea::vlw>(s), values, allocator);
                break;
            case sentence_id::VPW:
                ProcessSentence(sentence_cast<nmea::vpw>(s), values, allocator);
                break;
            case sentence_id::VWR:
                ProcessSentence(sentence_cast<nmea::vwr>(s), values, allocator);
                break;
            case sentence_id::XTE:
                ProcessSentence(sentence_cast<nmea::xte>(s), values, allocator);
                break;
            case sentence_id::ZDA:
                ProcessSentence(sentence_cast<nmea::zda>(s), values, allocator);
                break;
            case sentence_id::BOD:
                ProcessSentence(sentence_cast<nmea::bod>(s), values, allocator);
                break;
            case sentence_id::BWC:
                ProcessSentence(sentence_cast<nmea::bwc>(s), values, allocator);
                break;
            case sentence_id::BWR:
                ProcessSentence(sentence_cast<nmea::bwr>(s), values, allocator);
                break;
            case sentence_id::APB:
                ProcessSentence(sentence_cast<nmea::apb>(s), values, allocator);
                break;
            default:
                ++m_unimplemented_count;
                m_unimplemented.emplace(s->tag());
                processed = false;
            }
            if (values
                    .Empty()) { // Processing this known sentence did not yield
                                // any values (Probably we don't have a fix)
                processed = false;
                ++m_ignored;
            }
        } else {
            ++m_ignored;
            processed = false;
        }

        if (processed) {
            m_known.emplace(ks);
            Value updates(kArrayType);
            src.AddMember("label", "NSK", allocator);
            src.AddMember("type", "NMEA0183", allocator);
            upd.AddMember("source", src, allocator);
            upd.AddMember("timestamp", currentISO8601TimeUTC(), allocator);
            upd.AddMember("values", values, allocator);
            updates.PushBack(upd, allocator);
            d.AddMember("updates", updates, allocator);
            StringBuffer buffer;
            Writer<StringBuffer> writer(buffer);
            d.Accept(writer);
            // std::cout << buffer.GetString() << std::endl;
            ++m_sk_produced;
            ++m_sk_produced_total;
            if (outdoc != nullptr) {
                outdoc->Parse<0>(buffer.GetString());
            }
            SendPluginMessage("NSK_PI_SIGNALK", buffer.GetString());
        }
    } catch (...) {
        // std::cout << "Exception while processing " << sentence.c_str() <<
        // std::endl;
        m_unknown.emplace(stc.substr(0, 6));
        ++m_nmea_errors;
        return;
    }
}

void NSK::LoadConfig(const std::string& path)
{
    std::ifstream ifs { path };
    if (!ifs.is_open()) {
        std::cout << "Can't open the JSON file!" << std::endl;
        return;
        // throw std::runtime_error ("Can't open the JSON file!");
    }

    rapidjson::IStreamWrapper isw { ifs };

    Document d {};
    d.ParseStream(isw);
    if (d.HasMember("known_sentences") && d["known_sentences"].IsArray()) {
        m_known.clear();
        for (auto& stc : d["known_sentences"].GetArray()) {
            m_known.emplace(known_sentence(
                stc["talker_tag"].GetString(), stc["enabled"].GetBool()));
        }
    }
}

void NSK::SaveConfig(const std::string& path)
{
    Document d;
    d.SetObject();
    Value values(kArrayType);
    Document::AllocatorType& allocator = d.GetAllocator();
    for (auto& stc : m_known) {
        Value sentence(kObjectType);
        sentence.AddMember("talker_tag", stc.talker_tag, allocator);
        sentence.AddMember("enabled", stc.enabled, allocator);
        values.PushBack(sentence, allocator);
    }
    d.AddMember("known_sentences", values, allocator);

    rapidjson::StringBuffer buf;
    rapidjson::Writer<StringBuffer> writer(buf);
    d.Accept(writer);

    std::ofstream of(path);
    of << buf.GetString();
    if (!of.good()) {
        std::cout << "Can't write the JSON string to the file!" << std::endl;
        // throw std::runtime_error ("Can't write the JSON string to the
        // file!");
    }
}

PLUGIN_END_NAMESPACE
