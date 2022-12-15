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

using namespace rapidjson;

using namespace marnav;
using namespace nmea;

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
    if (s->get_sog().has_value()) {
        Value sog(kObjectType);
        sog.AddMember("path", "navigation.speedOverGround", allocator);
        sog.AddMember("value", kn2ms((*s->get_sog()).value()), allocator);
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
        Value sog(kObjectType);
        sog.AddMember("path", "navigation.headingTrue", allocator);
        sog.AddMember("value", s->get_track_true().value(), allocator);
        values_array.PushBack(sog, allocator);
    }
    if (s->get_track_magn().has_value()) {
        Value sog(kObjectType);
        sog.AddMember("path", "navigation.headingMagnetic", allocator);
        sog.AddMember("value", s->get_track_magn().value(), allocator);
        values_array.PushBack(sog, allocator);
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

// --- End of sentence processing implementations

void NSK::ProcessNMEASentence(const std::string& stc)
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
