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
#include <cmath>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#ifdef _WIN32
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

#include "nsk.h"
#include <marnav/ais/message_01.hpp>
#include <marnav/ais/message_04.hpp>
#include <marnav/ais/message_05.hpp>
#include <marnav/ais/message_18.hpp>
#include <marnav/ais/message_19.hpp>
#include <marnav/ais/message_21.hpp>
#include <marnav/ais/message_24.hpp>
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

void AddValue(rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator, const char* path,
    Value& value)
{
    Value val(kObjectType);
    val.AddMember("path", Value(path, allocator), allocator);
    val.AddMember("value", value, allocator);
    values_array.PushBack(val, allocator);
}

void AddPosition(rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator, const char* path, double lat,
    double lon)
{
    Value pos(kObjectType);
    pos.AddMember("latitude", lat, allocator);
    pos.AddMember("longitude", lon, allocator);
    AddValue(values_array, allocator, path, pos);
}

/// Notification object as produced by the SignalK NMEA0183 parser
void AddNotification(rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator, const char* path,
    const std::string& message, const char* state = "alarm")
{
    Value method(kArrayType);
    method.PushBack("sound", allocator);
    method.PushBack("visual", allocator);
    Value n(kObjectType);
    n.AddMember("method", method, allocator);
    n.AddMember("state", Value(state, allocator), allocator);
    n.AddMember("message", Value(message.c_str(), allocator), allocator);
    AddValue(values_array, allocator, path, n);
}

/// Signed magnetic variation/deviation in degrees (East positive)
double MagneticDegrees(const magnetic& m)
{
    return m.hemisphere() == direction::west ? -m.angle() : m.angle();
}

/// ISO8601 UTC timestamp from NMEA date and time
std::string IsoDateTime(const nmea::date& d, const nmea::time& t)
{
    auto year = d.year() < 100 ? d.year() + 2000 : d.year();
    char buf[32];
    snprintf(buf, sizeof(buf), "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ", year,
        static_cast<unsigned int>(d.mon()), d.day(), t.hour(), t.minutes(),
        t.seconds(), t.milliseconds());
    return buf;
}

/// Wind angle normalized to -180..180 degrees
double WindAngle(double deg)
{
    deg = std::fmod(deg, 360.0);
    return deg > 180.0 ? deg - 360.0 : deg;
}

std::string MmsiContext(const utils::mmsi& mmsi, const char* prefix)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%surn:mrn:imo:mmsi:%09u", prefix,
        static_cast<unsigned int>(
            static_cast<marnav::utils::mmsi::value_type>(mmsi)));
    return buf;
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
    if (s->get_lat().has_value() && s->get_lon().has_value()) {
        AddPosition(values_array, allocator, "navigation.position",
            s->get_lat()->get(), s->get_lon()->get());
    }
    if (s->get_quality_indicator().has_value()) {
        static const char* quality[] = { "no GPS", "GNSS Fix", "DGNSS fix",
            "Precise GNSS", "RTK fixed integer", "RTK float",
            "Estimated (DR) mode", "Manual input", "Simulator mode", "Error" };
        const auto q = static_cast<uint32_t>(*s->get_quality_indicator());
        if (q < std::size(quality)) {
            AddString(values_array, allocator, "navigation.gnss.methodQuality",
                quality[q]);
        }
    }
    if (s->get_n_satellites().has_value()) {
        AddNumber(values_array, allocator, "navigation.gnss.satellites",
            *s->get_n_satellites());
    }
    if (s->get_altitude().has_value()) {
        AddNumber(values_array, allocator, "navigation.gnss.antennaAltitude",
            s->get_altitude()->get<units::meters>().value());
    }
    if (s->get_hor_dilution().has_value()) {
        AddNumber(values_array, allocator, "navigation.gnss.horizontalDilution",
            *s->get_hor_dilution());
    }
    if (s->get_geodial_separation().has_value()) {
        AddNumber(values_array, allocator, "navigation.gnss.geoidalSeparation",
            s->get_geodial_separation()->get<units::meters>().value());
    }
    if (s->get_dgps_age().has_value()) {
        AddNumber(values_array, allocator, "navigation.gnss.differentialAge",
            *s->get_dgps_age());
    }
    if (s->get_dgps_ref().has_value()) {
        AddNumber(values_array, allocator,
            "navigation.gnss.differentialReference", *s->get_dgps_ref());
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
    // Satellites in view are spread over a group of sentences, publish once
    // the last one of the group arrives
    if (s->get_message_number() != m_gsv_next) {
        m_gsv_sats.clear();
        m_gsv_next = 1;
        if (s->get_message_number() != 1) {
            return;
        }
    }
    for (int i = 0; i < 4; ++i) {
        if (s->get_sat(i).has_value()) {
            m_gsv_sats.push_back(*s->get_sat(i));
        }
    }
    ++m_gsv_next;
    if (s->get_message_number() < s->get_n_messages()) {
        return;
    }
    Value sats(kArrayType);
    for (const auto& sat : m_gsv_sats) {
        Value v(kObjectType);
        v.AddMember("id", sat.prn, allocator);
        v.AddMember("elevation", deg2rad(sat.elevation), allocator);
        v.AddMember("azimuth", deg2rad(sat.azimuth), allocator);
        if (sat.snr.has_value()) {
            v.AddMember("SNR", *sat.snr, allocator);
        } else {
            v.AddMember("SNR", Value(), allocator);
        }
        sats.PushBack(v, allocator);
    }
    Value inview(kObjectType);
    inview.AddMember("count", s->get_n_satellites_in_view(), allocator);
    inview.AddMember("satellites", sats, allocator);
    AddValue(
        values_array, allocator, "navigation.gnss.satellitesInView", inview);
    m_gsv_sats.clear();
    m_gsv_next = 1;
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::rmc> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_status().has_value() && *s->get_status() == 'V') {
        return;
    }
    if (s->get_lat().has_value() && s->get_lon().has_value()) {
        AddPosition(values_array, allocator, "navigation.position",
            s->get_lat()->get(), s->get_lon()->get());
    }
    if (s->get_heading().has_value()) {
        AddNumber(values_array, allocator, "navigation.courseOverGroundTrue",
            deg2rad(*s->get_heading()));
    }
    if (s->get_sog().has_value()) {
        AddNumber(values_array, allocator, "navigation.speedOverGround",
            kn2ms(s->get_sog()->get<marnav::units::knots>().value()));
    }
    if (s->get_mag().has_value()) {
        AddNumber(values_array, allocator, "navigation.magneticVariation",
            deg2rad(MagneticDegrees(*s->get_mag())));
    }
    if (s->get_time_utc().has_value() && s->get_date().has_value()) {
        AddString(values_array, allocator, "navigation.datetime",
            IsoDateTime(*s->get_date(), *s->get_time_utc()));
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::vtg> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_track_true().has_value()) {
        AddNumber(values_array, allocator, "navigation.courseOverGroundTrue",
            deg2rad(*s->get_track_true()));
    }
    if (s->get_track_magn().has_value()) {
        AddNumber(values_array, allocator,
            "navigation.courseOverGroundMagnetic",
            deg2rad(*s->get_track_magn()));
    }
    if (s->get_speed_kn().has_value()) {
        AddNumber(values_array, allocator, "navigation.speedOverGround",
            kn2ms(s->get_speed_kn()->value()));
    } else if (s->get_speed_kmh().has_value()) {
        AddNumber(values_array, allocator, "navigation.speedOverGround",
            kmh2ms(s->get_speed_kmh()->value()));
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
    const auto dev = s->get_magn_dev().has_value()
        ? std::optional<double>(MagneticDegrees(*s->get_magn_dev()))
        : std::nullopt;
    const auto var = s->get_magn_var().has_value()
        ? std::optional<double>(MagneticDegrees(*s->get_magn_var()))
        : std::nullopt;
    if (s->get_heading().has_value()) {
        const auto magnetic = *s->get_heading() + dev.value_or(0.0);
        AddNumber(values_array, allocator, "navigation.headingMagnetic",
            deg2rad(magnetic));
        if (dev.has_value()) {
            AddNumber(values_array, allocator, "navigation.headingCompass",
                deg2rad(*s->get_heading()));
        }
        if (var.has_value()) {
            AddNumber(values_array, allocator, "navigation.headingTrue",
                deg2rad(magnetic + *var));
        }
    }
    if (var.has_value()) {
        AddNumber(values_array, allocator, "navigation.magneticVariation",
            deg2rad(*var));
    }
    if (dev.has_value()) {
        AddNumber(values_array, allocator, "navigation.magneticDeviation",
            deg2rad(*dev));
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
        || !s->get_angle_ref().has_value()
        || s->get_data_valid() != status::ok) {
        return;
    }
    const auto angle_ref = to_string(*s->get_angle_ref());
    const auto speed = s->get_speed()->get<units::meters_per_second>().value();
    if (angle_ref == "R") {
        AddNumber(values_array, allocator, "environment.wind.angleApparent",
            deg2rad(WindAngle(*s->get_angle())));
        AddNumber(
            values_array, allocator, "environment.wind.speedApparent", speed);
    } else {
        AddNumber(values_array, allocator, "environment.wind.angleTrueWater",
            deg2rad(WindAngle(*s->get_angle())));
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
    if (s->get_waypoint_to().has_value()) {
        AddString(values_array, allocator,
            "navigation.courseRhumbline.nextPoint.ID",
            s->get_waypoint_to()->c_str());
    }
    if (s->get_waypoint_from().has_value()) {
        AddString(values_array, allocator,
            "navigation.courseRhumbline.previousPoint.ID",
            s->get_waypoint_from()->c_str());
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
        auto xte
            = s->get_cross_track_error()->get<units::nautical_miles>().value()
            * NM2METER;
        if (s->get_steer_dir() == side::right) {
            xte *= -1.0;
        }
        AddNumber(values_array, allocator,
            "navigation.courseRhumbline.crossTrackError", xte);
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
    if (s->get_revolutions().has_value() && s->get_source().has_value()
        && s->get_source_number().has_value()) {
        const auto path = std::string("propulsion.")
            + (*s->get_source() == rpm::source_id::shaft ? "shaft_" : "engine_")
            + std::to_string(*s->get_source_number()) + ".revolutions";
        AddNumber(values_array, allocator, path.c_str(),
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
    // SignalK: positive when the vessel is right of track (steer Left)
    if (s->get_direction_to_steer().has_value()
        && to_string(*s->get_direction_to_steer()) == "R") {
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
            IsoDateTime(*s->get_date(), *s->get_time_utc()));
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
    if (s->get_waypoint_to().has_value()) {
        AddString(values_array, allocator,
            "navigation.courseRhumbline.nextPoint.ID",
            s->get_waypoint_to()->c_str());
    }
    if (s->get_waypoint_from().has_value()) {
        AddString(values_array, allocator,
            "navigation.courseRhumbline.previousPoint.ID",
            s->get_waypoint_from()->c_str());
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
        } else {
            xte *= 1000.0;
        }
        if (s->get_direction_to_steer().has_value()
            && to_string(*s->get_direction_to_steer()) == "R") {
            xte *= -1.0;
        }
        AddNumber(values_array, allocator,
            "navigation.courseRhumbline.crossTrackError", xte);
    }
    auto ref = [](const std::optional<reference>& r) {
        return r.has_value() && *r == reference::MAGNETIC
            ? std::string("Magnetic")
            : std::string("True");
    };
    if (s->get_bearing_origin_to_destination().has_value()) {
        const auto r = ref(s->get_bearing_origin_to_destination_ref());
        const auto v = deg2rad(*s->get_bearing_origin_to_destination());
        AddNumber(values_array, allocator,
            ("navigation.courseRhumbline.bearingTrack" + r).c_str(), v);
        AddNumber(values_array, allocator,
            ("navigation.courseRhumbline.bearingOriginToDestination" + r)
                .c_str(),
            v);
    }
    if (s->get_bearing_pos_to_destination().has_value()) {
        const auto r = ref(s->get_bearing_pos_to_destination_ref());
        const auto v = deg2rad(*s->get_bearing_pos_to_destination());
        AddNumber(values_array, allocator,
            ("navigation.courseRhumbline.bearingToDestination" + r).c_str(), v);
        AddNumber(values_array, allocator,
            ("navigation.courseRhumbline.nextPoint.bearing" + r).c_str(), v);
    }
    if (s->get_waypoint_id().has_value()) {
        AddString(values_array, allocator,
            "navigation.courseRhumbline.nextPoint.ID",
            s->get_waypoint_id()->c_str());
    }
    if (s->get_heading_to_steer_to_destination().has_value()) {
        AddNumber(values_array, allocator,
            ("steering.autopilot.target.heading"
                + ref(s->get_heading_to_steer_to_destination_ref()))
                .c_str(),
            deg2rad(*s->get_heading_to_steer_to_destination()));
    }
    if (s->get_status_arrival() == status::ok) {
        AddNotification(values_array, allocator,
            "notifications.arrivalCircleEntered", "WP arrival circle entered!");
    }
    if (s->get_status_perpendicular_passing() == status::ok) {
        AddNotification(values_array, allocator,
            "notifications.perpendicularPassed", "Perpendicular passed!");
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::aam> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    AddString(values_array, allocator,
        "navigation.courseRhumbline.nextPoint.ID",
        s->get_waypoint_id().c_str());
    AddNumber(values_array, allocator,
        "navigation.courseRhumbline.arrivalCircle",
        s->get_arrival_circle_radius().get<units::nautical_miles>().value()
            * NM2METER);
    if (s->get_arrival_circle_entered() == status::ok) {
        AddNotification(values_array, allocator,
            "notifications.arrivalCircleEntered", "WP arrival circle entered!");
    }
    if (s->get_perpendicualar_passed() == status::ok) {
        AddNotification(values_array, allocator,
            "notifications.perpendicularPassed", "Perpendicular passed!");
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::bec> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    // Dead reckoning variant of BWC
    AddPosition(values_array, allocator,
        "navigation.courseGreatCircle.nextPoint.position", s->get_lat().get(),
        s->get_lon().get());
    AddNumber(values_array, allocator,
        "navigation.courseGreatCircle.nextPoint.distance",
        s->get_distance().get<units::nautical_miles>().value() * NM2METER);
    AddNumber(values_array, allocator,
        "navigation.courseGreatCircle.bearingTrackTrue",
        deg2rad(s->get_bearing_true()));
    AddNumber(values_array, allocator,
        "navigation.courseGreatCircle.bearingTrackMagnetic",
        deg2rad(s->get_bearing_magn()));
    AddString(values_array, allocator,
        "navigation.courseGreatCircle.nextPoint.ID", s->get_waypoint().c_str());
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::mob> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    const auto st = s->get_mob_status();
    if (st != mob::mob_status::mob_activated
        && st != mob::mob_status::manual_button) {
        return;
    }
    char msg[128];
    snprintf(msg, sizeof(msg), "Man overboard! Position %.5f, %.5f",
        s->get_lat().get(), s->get_lon().get());
    AddNotification(
        values_array, allocator, "notifications.mob", msg, "emergency");
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::osd> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_data_valid() != status::ok) {
        return;
    }
    if (s->get_heading().has_value()) {
        AddNumber(values_array, allocator, "navigation.headingTrue",
            deg2rad(*s->get_heading()));
    }
    if (s->get_course().has_value()) {
        AddNumber(values_array, allocator,
            s->get_course_ref() == reference::MAGNETIC
                ? "navigation.courseOverGroundMagnetic"
                : "navigation.courseOverGroundTrue",
            deg2rad(*s->get_course()));
    }
    if (s->get_speed().has_value() && s->get_speed_unit().has_value()) {
        auto speed = *s->get_speed();
        switch (*s->get_speed_unit()) {
        case unit::velocity::knot:
            speed = kn2ms(speed);
            break;
        case unit::velocity::kmh:
            speed = kmh2ms(speed);
            break;
        default:
            break;
        }
        AddNumber(values_array, allocator,
            s->get_speed_ref() == 'W' ? "navigation.speedThroughWater"
                                      : "navigation.speedOverGround",
            speed);
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::pgrmz> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    AddNumber(values_array, allocator, "navigation.gnss.antennaAltitude",
        s->get_altitude().get<units::meters>().value());
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::rma> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    // Loran-C equivalent of RMC
    if (s->get_blink_warning() == 'V') {
        return;
    }
    if (s->get_lat().has_value() && s->get_lon().has_value()) {
        AddPosition(values_array, allocator, "navigation.position",
            s->get_lat()->get(), s->get_lon()->get());
    }
    if (s->get_track().has_value()) {
        AddNumber(values_array, allocator, "navigation.courseOverGroundTrue",
            deg2rad(*s->get_track()));
    }
    if (s->get_sog().has_value()) {
        AddNumber(values_array, allocator, "navigation.speedOverGround",
            kn2ms(s->get_sog()->get<units::knots>().value()));
    }
    if (s->get_magnetic_var().has_value()) {
        AddNumber(values_array, allocator, "navigation.magneticVariation",
            deg2rad(MagneticDegrees(*s->get_magnetic_var())));
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::stalk> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    // Decoded from the raw datagram following the SignalK parser, Marnav's
    // seatalk decoders read the 16 bit values MSB first while SeaTalk sends
    // them LSB first
    const auto d = s->get_data();
    auto at = [&d](size_t i) -> unsigned { return i < d.size() ? d[i] : 0; };
    auto le16 = [&at](size_t i) { return at(i) + 256 * at(i + 1); };
    // Compass heading from the U nibble and VW byte (datagrams 84, 9C)
    auto compass = [&at]() {
        const auto u = at(1) >> 4;
        return (u & 0x3) * 90.0 + (at(2) & 0x3f) * 2.0 + ((u >> 2) & 0x1)
            + ((u >> 3) & 0x1);
    };
    auto add_datetime = [&]() {
        if (!m_seatalk_date.empty() && !m_seatalk_time.empty()) {
            AddString(values_array, allocator, "navigation.datetime",
                m_seatalk_date + m_seatalk_time);
        }
    };
    auto add_position = [&]() {
        if (m_seatalk_lat.has_value() && m_seatalk_lon.has_value()) {
            AddPosition(values_array, allocator, "navigation.position",
                *m_seatalk_lat, *m_seatalk_lon);
        }
    };
    char buf[32];
    switch (at(0)) {
    case 0x00: // Depth below transducer, 1/10 ft
        AddNumber(values_array, allocator, "environment.depth.belowTransducer",
            (at(3) + ((at(4) & 0x7f) << 8)) / 10.0 * FOOT2METER);
        break;
    case 0x10: // Apparent wind angle, 1/2 degree
        AddNumber(values_array, allocator, "environment.wind.angleApparent",
            deg2rad(WindAngle((256 * at(2) + at(3)) / 2.0)));
        break;
    case 0x11: // Apparent wind speed
        AddNumber(values_array, allocator, "environment.wind.speedApparent",
            kn2ms((at(2) & 0x7f) + at(3) / 10.0));
        break;
    case 0x20: // Speed through water, 1/10 kn
        AddNumber(values_array, allocator, "navigation.speedThroughWater",
            kn2ms(le16(2) / 10.0));
        break;
    case 0x21: // Trip mileage, 1/100 NM
        AddNumber(values_array, allocator, "navigation.trip.log",
            (le16(2) + (at(4) & 0x0f) * 65536) / 100.0 * NM2METER);
        break;
    case 0x22: // Total mileage, 1/10 NM
        AddNumber(values_array, allocator, "navigation.log",
            le16(2) / 10.0 * NM2METER);
        break;
    case 0x25: // Total and trip log
        AddNumber(values_array, allocator, "navigation.log",
            (le16(2) + (at(1) >> 4) * 65536) / 10.0 * NM2METER);
        AddNumber(values_array, allocator, "navigation.trip.log",
            (le16(4) + (at(6) & 0x0f) * 65536) / 100.0 * NM2METER);
        break;
    case 0x26: { // Speed through water, 1/100 kn
        const auto flags = at(6) >> 4;
        if (flags & 4) {
            AddNumber(values_array, allocator, "navigation.speedThroughWater",
                kn2ms(le16(2) / 100.0));
        }
        if (!(flags & 8)) {
            AddNumber(values_array, allocator,
                "navigation.averageSpeedThroughWater", kn2ms(le16(4) / 100.0));
        }
        break;
    }
    case 0x27: // Water temperature, (XXXX-100)/10 Celsius
        AddNumber(values_array, allocator, "environment.water.temperature",
            (le16(2) - 100.0) / 10.0 + KELVIN_OFFSET);
        break;
    case 0x50: { // Latitude
        const auto y = le16(3);
        m_seatalk_lat
            = (y & 0x8000 ? -1.0 : 1.0) * (at(2) + (y & 0x7fff) / 100.0 / 60.0);
        add_position();
        break;
    }
    case 0x51: { // Longitude
        const auto y = le16(3);
        m_seatalk_lon
            = (y & 0x8000 ? 1.0 : -1.0) * (at(2) + (y & 0x7fff) / 100.0 / 60.0);
        add_position();
        break;
    }
    case 0x52: // Speed over ground, 1/10 kn
        AddNumber(values_array, allocator, "navigation.speedOverGround",
            kn2ms(le16(2) / 10.0));
        break;
    case 0x53: { // Magnetic course
        const auto u = at(1) >> 4;
        AddNumber(values_array, allocator,
            "navigation.courseOverGroundMagnetic",
            deg2rad((u & 0x3) * 90.0 + (at(2) & 0x3f) * 2.0 + (u & 0xc) / 8.0));
        break;
    }
    case 0x54: // GMT time
        snprintf(buf, sizeof(buf), "T%02u:%02u:%02u.000Z", at(3),
            (at(2) & 0xfc) / 4, (((at(2) & 0x0f) << 4) + (at(1) >> 4)) & 0x3f);
        m_seatalk_time = buf;
        add_datetime();
        break;
    case 0x56: // Date
        snprintf(buf, sizeof(buf), "%04u-%02u-%02u", 2000 + at(3), at(1) >> 4,
            at(2));
        m_seatalk_date = buf;
        add_datetime();
        break;
    case 0x57: { // Satellite info
        const auto sats = at(1) >> 4;
        AddNumber(values_array, allocator, "navigation.gnss.satellites", sats);
        AddNumber(values_array, allocator, "navigation.gnss.horizontalDilution",
            sats == 1 ? 0x94 : at(2));
        break;
    }
    case 0x84: { // Autopilot status
        if (d.size() != 9) {
            break;
        }
        const auto z = at(4) & 0x0f;
        const char* mode = z & 0x8 ? "route"
            : z & 0x4              ? "wind"
            : z & 0x2              ? "auto"
                                   : "standby";
        AddNumber(values_array, allocator, "navigation.headingMagnetic",
            deg2rad(compass()));
        AddNumber(values_array, allocator,
            "steering.autopilot.target.headingMagnetic",
            deg2rad(((at(2) >> 4 & 0xc) >> 2) * 90.0 + at(3) / 2.0));
        AddNumber(values_array, allocator, "steering.rudderAngle",
            deg2rad(static_cast<int8_t>(at(6))));
        AddString(values_array, allocator, "steering.autopilot.state", mode);
        break;
    }
    case 0x99: // Magnetic variation, West positive
        AddNumber(values_array, allocator, "navigation.magneticVariation",
            deg2rad(-static_cast<int8_t>(at(2))));
        break;
    case 0x9C: // Compass heading and rudder position
        AddNumber(values_array, allocator, "navigation.headingMagnetic",
            deg2rad(compass()));
        AddNumber(values_array, allocator, "steering.rudderAngle",
            deg2rad(static_cast<int8_t>(at(3))));
        break;
    default:
        break;
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::vbw> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_water_speed_status() == status::ok) {
        if (s->get_water_speed_longitudinal().has_value()) {
            AddNumber(values_array, allocator, "navigation.speedThroughWater",
                s->get_water_speed_longitudinal()
                    ->get<units::meters_per_second>()
                    .value());
        }
        if (s->get_water_speed_transveral().has_value()) {
            AddNumber(values_array, allocator,
                "navigation.speedThroughWaterTransverse",
                s->get_water_speed_transveral()
                    ->get<units::meters_per_second>()
                    .value());
        }
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::vdm> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    // ponytail: one fragment buffer, interleaved multi-part messages from
    // several channels get dropped, key by seq_msg_id if that shows up
    if (s->get_fragment() == 1) {
        m_ais_fragments.clear();
    } else if (m_ais_fragments.size() + 1 != s->get_fragment()) {
        m_ais_fragments.clear();
        return;
    }
    m_ais_fragments.emplace_back(s->get_payload(), s->get_n_fill_bits());
    if (s->get_fragment() < s->get_n_fragments()) {
        return;
    }
    const auto payload = std::move(m_ais_fragments);
    m_ais_fragments.clear();

    std::unique_ptr<ais::message> msg;
    try {
        msg = ais::make_message(payload);
    } catch (...) {
        return; // AIS message types not supported by Marnav
    }

    static const char* states[] = { "motoring", "anchored", "not under command",
        "restricted manouverability", "constrained by draft", "moored",
        "aground", "fishing", "sailing", "hazardous material high speed",
        "hazardous material wing in ground", "", "", "", "ais-sart",
        "default" };
    const char* prefix = "vessels.";
    utils::mmsi mmsi;

    auto add_root = [&](const char* key, Value v) {
        Value obj(kObjectType);
        obj.AddMember(Value(key, allocator), v, allocator);
        AddValue(values_array, allocator, "", obj);
    };
    auto add_str = [&](const char* key, const std::string& str) {
        if (!str.empty()) {
            add_root(key, Value(str.c_str(), allocator));
        }
    };
    auto add_pos = [&](const auto* m) {
        if (m->get_lat().has_value() && m->get_lon().has_value()) {
            AddPosition(values_array, allocator, "navigation.position",
                m->get_lat()->get(), m->get_lon()->get());
        }
    };
    auto add_dim = [&](const ais::vessel_dimension& d) {
        const auto length = d.get_to_bow().value() + d.get_to_stern().value();
        const auto beam
            = d.get_to_port().value() + d.get_to_starboard().value();
        if (length > 0) {
            Value l(kObjectType);
            l.AddMember("overall", length, allocator);
            AddValue(values_array, allocator, "design.length", l);
        }
        if (beam > 0) {
            AddNumber(values_array, allocator, "design.beam", beam);
        }
        if (d.get_to_bow().value() > 0) {
            AddNumber(values_array, allocator, "sensors.ais.fromBow",
                d.get_to_bow().value());
        }
        if (beam > 0 && d.get_to_starboard().value() > 0) {
            AddNumber(values_array, allocator, "sensors.ais.fromCenter",
                beam / 2 - d.get_to_starboard().value());
        }
    };
    auto add_ship_type = [&](ais::ship_type t) {
        if (t != ais::ship_type::not_available) {
            Value v(kObjectType);
            v.AddMember("id", static_cast<unsigned>(t), allocator);
            AddValue(values_array, allocator, "design.aisShipType", v);
        }
    };
    auto add_motion
        = [&](std::optional<units::knots> sog, std::optional<double> cog,
              std::optional<uint32_t> hdg) {
              if (sog.has_value()) {
                  AddNumber(values_array, allocator,
                      "navigation.speedOverGround", kn2ms(sog->value()));
              }
              if (cog.has_value()) {
                  AddNumber(values_array, allocator,
                      "navigation.courseOverGroundTrue", deg2rad(*cog));
              }
              if (hdg.has_value()) {
                  AddNumber(values_array, allocator, "navigation.headingTrue",
                      deg2rad(*hdg));
              }
          };

    switch (msg->type()) {
    case ais::message_id::position_report_class_a:
    case ais::message_id::position_report_class_a_assigned_schedule:
    case ais::message_id::position_report_class_a_response_to_interrogation: {
        const auto m = static_cast<const ais::message_01*>(msg.get());
        mmsi = m->get_mmsi();
        add_motion(m->get_sog(), m->get_cog(), m->get_hdg());
        const auto rot = m->get_rot();
        if (rot.available() && !rot.is_more_5deg30s_left()
            && !rot.is_more_5deg30s_right()) {
            AddNumber(values_array, allocator, "navigation.rateOfTurn",
                deg2rad(rot.value()) / 60.0);
        }
        const auto st = static_cast<size_t>(m->get_nav_status());
        if (st < std::size(states) && *states[st]) {
            AddString(values_array, allocator, "navigation.state", states[st]);
        }
        add_pos(m);
        AddString(values_array, allocator, "sensors.ais.class", "A");
        break;
    }
    case ais::message_id::base_station_report: {
        const auto m = ais::message_cast<ais::message_04>(msg.get());
        mmsi = m->get_mmsi();
        prefix = "atons.";
        add_pos(m);
        AddString(values_array, allocator, "sensors.ais.class", "BASE");
        break;
    }
    case ais::message_id::static_and_voyage_related_data: {
        const auto m = ais::message_cast<ais::message_05>(msg.get());
        mmsi = m->get_mmsi();
        add_str("name", m->get_shipname());
        if (!m->get_callsign().empty()) {
            Value comm(kObjectType);
            comm.AddMember("callsignVhf",
                Value(m->get_callsign().c_str(), allocator), allocator);
            add_root("communication", std::move(comm));
        }
        if (m->get_imo_number() != 0) {
            Value reg(kObjectType);
            reg.AddMember("imo",
                Value(("IMO " + std::to_string(m->get_imo_number())).c_str(),
                    allocator),
                allocator);
            add_root("registrations", std::move(reg));
        }
        add_dim(m->get_vessel_dimension());
        add_ship_type(m->get_shiptype());
        if (m->get_draught().value() > 0) {
            Value d(kObjectType);
            d.AddMember("current", m->get_draught().value(), allocator);
            AddValue(values_array, allocator, "design.draft", d);
        }
        if (!m->get_destination().empty()) {
            AddString(values_array, allocator,
                "navigation.destination.commonName", m->get_destination());
        }
        const auto mon = m->get_eta_month(), day = m->get_eta_day(),
                   hr = m->get_eta_hour(), min = m->get_eta_minute();
        if (mon >= 1 && mon <= 12 && day >= 1 && day <= 31 && hr < 24
            && min < 60) {
            // ETA has no year, take the next occurrence
            const auto now = std::time(nullptr);
            auto year = gmtime(&now)->tm_year + 1900;
            char buf[32];
            snprintf(buf, sizeof(buf), "%04d-%02u-%02uT%02u:%02u:00.000Z", year,
                mon, day, hr, min);
            if (std::string(buf) < currentISO8601TimeUTC()) {
                snprintf(buf, sizeof(buf), "%04d-%02u-%02uT%02u:%02u:00.000Z",
                    year + 1, mon, day, hr, min);
            }
            AddString(
                values_array, allocator, "navigation.destination.eta", buf);
        }
        AddString(values_array, allocator, "sensors.ais.class", "A");
        break;
    }
    case ais::message_id::standard_class_b_cs_position_report: {
        const auto m = ais::message_cast<ais::message_18>(msg.get());
        mmsi = m->get_mmsi();
        add_motion(m->get_sog(), m->get_cog(), m->get_hdg());
        add_pos(m);
        AddString(values_array, allocator, "sensors.ais.class", "B");
        break;
    }
    case ais::message_id::extended_class_b_equipment_position_report: {
        const auto m = ais::message_cast<ais::message_19>(msg.get());
        mmsi = m->get_mmsi();
        add_motion(m->get_sog(),
            m->get_cog() < 3600 ? std::optional<double>(m->get_cog() / 10.0)
                                : std::nullopt,
            m->get_hdg() < 360 ? std::optional<uint32_t>(m->get_hdg())
                               : std::nullopt);
        add_str("name", m->get_shipname());
        add_dim(m->get_vessel_dimension());
        add_ship_type(m->get_shiptype());
        add_pos(m);
        AddString(values_array, allocator, "sensors.ais.class", "B");
        break;
    }
    case ais::message_id::aid_to_navigation_report: {
        const auto m = ais::message_cast<ais::message_21>(msg.get());
        mmsi = m->get_mmsi();
        prefix = "atons.";
        add_str("name", m->get_name() + m->get_name_extension());
        Value t(kObjectType);
        t.AddMember("id", static_cast<unsigned>(m->get_aid_type()), allocator);
        AddValue(values_array, allocator, "atonType", t);
        Value off(
            m->get_off_position() == ais::off_position_indicator::off_position);
        AddValue(values_array, allocator, "offPosition", off);
        Value virt(m->get_virtual_aid_flag() == ais::virtual_aid::virtual_aid);
        AddValue(values_array, allocator, "virtual", virt);
        add_pos(m);
        AddString(values_array, allocator, "sensors.ais.class", "ATON");
        break;
    }
    case ais::message_id::static_data_report: {
        const auto m = ais::message_cast<ais::message_24>(msg.get());
        mmsi = m->get_mmsi();
        if (m->get_part_number() == ais::message_24::part::A) {
            add_str("name", m->get_shipname());
        } else {
            add_ship_type(m->get_shiptype());
            if (!m->get_callsign().empty()) {
                Value comm(kObjectType);
                comm.AddMember("callsignVhf",
                    Value(m->get_callsign().c_str(), allocator), allocator);
                add_root("communication", std::move(comm));
            }
            add_dim(m->get_vessel_dimension());
        }
        break;
    }
    default:
        return;
    }
    add_root("mmsi",
        Value(MmsiContext(mmsi, "")
                  .substr(std::strlen("urn:mrn:imo:mmsi:"))
                  .c_str(),
            allocator));
    m_context = MmsiContext(mmsi, prefix);
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::wcv> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_speed().has_value()) {
        AddNumber(values_array, allocator,
            "navigation.courseRhumbline.nextPoint.velocityMadeGood",
            s->get_speed()->get<units::meters_per_second>().value());
    }
    if (s->get_waypoint_id().has_value()) {
        AddString(values_array, allocator,
            "navigation.courseRhumbline.nextPoint.ID",
            s->get_waypoint_id()->c_str());
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::xdr> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    // ponytail: XDR has no SignalK reference mapping, only the common
    // transducers are mapped, extend the name lists as devices show up
    auto is
        = [](const std::string& name, std::initializer_list<const char*> l) {
              for (auto n : l) {
                  if (strcasecmp(name.c_str(), n) == 0) {
                      return true;
                  }
              }
              return false;
          };
    Value attitude(kObjectType);
    for (int i = 0; i < xdr::max_transducer_info; ++i) {
        const auto info = s->get_info(i);
        if (!info.has_value()) {
            continue;
        }
        const auto v = info->measurement_data;
        switch (info->transducer_type) {
        case 'P': // Pressure, Bar or Pascal
            if (is(info->name, { "", "Barometer", "ENV_ATMOS_P", "AirPres" })) {
                AddNumber(values_array, allocator,
                    "environment.outside.pressure",
                    info->units_of_measurement == 'B' ? v * 100000.0 : v);
            }
            break;
        case 'C': // Temperature, Celsius
            if (is(info->name, { "AirTemp", "TempAir", "ENV_OUTAIR_T" })) {
                AddNumber(values_array, allocator,
                    "environment.outside.temperature", v + KELVIN_OFFSET);
            } else if (is(info->name,
                           { "WaterTemp", "TempWater", "ENV_WATER_T" })) {
                AddNumber(values_array, allocator,
                    "environment.water.temperature", v + KELVIN_OFFSET);
            }
            break;
        case 'H': // Humidity, percent
            AddNumber(values_array, allocator,
                "environment.outside.relativeHumidity", v / 100.0);
            break;
        case 'A': // Angle, degrees
            if (is(info->name, { "PITCH", "PTCH" })) {
                attitude.AddMember("pitch", deg2rad(v), allocator);
            } else if (is(info->name, { "ROLL" })) {
                attitude.AddMember("roll", deg2rad(v), allocator);
            } else if (is(info->name, { "YAW" })) {
                attitude.AddMember("yaw", deg2rad(v), allocator);
            }
            break;
        default:
            break;
        }
    }
    if (attitude.MemberCount() > 0) {
        AddValue(values_array, allocator, "navigation.attitude", attitude);
    }
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::xtr> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    // Dead reckoning variant of XTE
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
    if (s->get_direction_to_steer() == side::right) {
        xte *= -1.0;
    }
    AddNumber(values_array, allocator,
        "navigation.courseRhumbline.crossTrackError", xte);
}

void NSK::ProcessSentence(std::unique_ptr<marnav::nmea::ztg> s,
    rapidjson::Value& values_array,
    rapidjson::Document::AllocatorType& allocator)
{
    if (s->get_time_remaining().has_value()) {
        const auto& t = *s->get_time_remaining();
        AddNumber(values_array, allocator,
            "navigation.courseRhumbline.nextPoint.timeToGo",
            t.hour() * 3600.0 + t.minutes() * 60.0 + t.seconds()
                + t.milliseconds() / 1000.0);
    }
    if (s->get_waypoint_id().has_value()) {
        AddString(values_array, allocator,
            "navigation.courseRhumbline.nextPoint.ID",
            s->get_waypoint_id()->c_str());
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
        m_context.clear();
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
            case sentence_id::AAM:
                ProcessSentence(sentence_cast<nmea::aam>(s), values, allocator);
                break;
            case sentence_id::BEC:
                ProcessSentence(sentence_cast<nmea::bec>(s), values, allocator);
                break;
            case sentence_id::MOB:
                ProcessSentence(sentence_cast<nmea::mob>(s), values, allocator);
                break;
            case sentence_id::OSD:
                ProcessSentence(sentence_cast<nmea::osd>(s), values, allocator);
                break;
            case sentence_id::PGRMZ:
                ProcessSentence(
                    sentence_cast<nmea::pgrmz>(s), values, allocator);
                break;
            case sentence_id::RMA:
                ProcessSentence(sentence_cast<nmea::rma>(s), values, allocator);
                break;
            case sentence_id::STALK:
                ProcessSentence(
                    sentence_cast<nmea::stalk>(s), values, allocator);
                break;
            case sentence_id::VBW:
                ProcessSentence(sentence_cast<nmea::vbw>(s), values, allocator);
                break;
            case sentence_id::VDM:
                ProcessSentence(sentence_cast<nmea::vdm>(s), values, allocator);
                break;
            case sentence_id::WCV:
                ProcessSentence(sentence_cast<nmea::wcv>(s), values, allocator);
                break;
            case sentence_id::XDR:
                ProcessSentence(sentence_cast<nmea::xdr>(s), values, allocator);
                break;
            case sentence_id::XTR:
                ProcessSentence(sentence_cast<nmea::xtr>(s), values, allocator);
                break;
            case sentence_id::ZTG:
                ProcessSentence(sentence_cast<nmea::ztg>(s), values, allocator);
                break;
            case sentence_id::VDO:
                ProcessSentence(
                    std::unique_ptr<nmea::vdm>(sentence_cast<nmea::vdo>(s)),
                    values, allocator);
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
            if (!m_context.empty()) {
                d.AddMember("context", m_context, allocator);
            }
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
