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

#ifndef _NSK_H_
#define _NSK_H_

#include <chrono>
#include <set>

#include <marnav/nmea/angle.hpp>
#include <marnav/nmea/io.hpp>
#include <marnav/nmea/nmea.hpp>
// Sentences supported
#include <marnav/nmea/gga.hpp>
#include <marnav/nmea/gll.hpp>
#include <marnav/nmea/gsa.hpp>
#include <marnav/nmea/gsv.hpp>
#include <marnav/nmea/rmc.hpp>
#include <marnav/nmea/vtg.hpp>

#include "rapidjson/document.h"

#include "pi_common.h"

PLUGIN_BEGIN_NAMESPACE

struct known_sentence {
    /// @brief Talker ID and message tag (eg. GPRMC)
    std::string talker_tag;
    /// @brief Whether the sentences with this talker ID and tag are enabled for
    /// processing
    bool enabled;

    /// @brief Constructor
    /// @param s Reference to the Marnav NMEA sentence
    known_sentence(marnav::nmea::sentence& s)
    {
        talker_tag = to_string(s.get_talker()) + s.tag();
        enabled = true;
    }

    /// @brief Constructor
    /// @param tt talker+tag string
    /// @param en enabled?
    known_sentence(const std::string& tt, bool en)
    {
        talker_tag = tt;
        enabled = en;
    }

    /// @brief Less than operator
    /// @param other Other known_sentence struct
    /// @return result of talker+tag string comparison
    bool operator<(const known_sentence& other) const
    {
        return (talker_tag < other.talker_tag);
    }

    /// @brief Equality operator
    /// @param other Other known_sentence struct
    /// @return true if talker+tag are the same
    bool operator==(const known_sentence& other) const
    {
        return (talker_tag == other.talker_tag);
    }
};

/// The NMEA0183->SignalK converter
class NSK {
private:
    /// Number of NMEA0183 sentences received
    size_t m_nmea_received;
    /// Number of SignalK deltas produced
    size_t m_sk_produced;
    /// Number of exceptions while processing the NMEA0183 messages
    size_t m_nmea_errors;
    /// Number of sentences ignored due to configuration
    size_t m_ignored;
    size_t m_nmea_received_total;
    size_t m_sk_produced_total;
    size_t m_unimplemented_count;
    /// Start timestamp of the sentence counters
    std::chrono::system_clock::time_point m_counters_start;
    /// List of sentences received in the data feed, known to Marnav, but not
    /// implemented by NSK
    std::set<std::string> m_unimplemented;
    /// List of sentences causing an exception while being processed by Marnav
    std::set<std::string> m_unknown;
    /// List of sentences we are able to process + flag whether we want to
    std::set<known_sentence> m_known;

    /// @brief Process the GGA NMEA0183 sentence
    /// @param s sentence pointer
    /// @param values_array SignalK values array object reference
    /// @param allocator Allocator reference
    void ProcessSentence(std::unique_ptr<marnav::nmea::gga> s,
        rapidjson::Value& values_array,
        rapidjson::Document::AllocatorType& allocator);
    /// @brief Process the GLL NMEA0183 sentence
    /// @param s sentence pointer
    /// @param values_array SignalK values array object reference
    /// @param allocator Allocator reference
    void ProcessSentence(std::unique_ptr<marnav::nmea::gll> s,
        rapidjson::Value& values_array,
        rapidjson::Document::AllocatorType& allocator);
    /// @brief Process the GSA NMEA0183 sentence
    /// @param s sentence pointer
    /// @param values_array SignalK values array object reference
    /// @param allocator Allocator reference
    void ProcessSentence(std::unique_ptr<marnav::nmea::gsa> s,
        rapidjson::Value& values_array,
        rapidjson::Document::AllocatorType& allocator);
    /// @brief Process the GSV NMEA0183 sentence
    /// @param s sentence pointer
    /// @param values_array SignalK values array object reference
    /// @param allocator Allocator reference
    void ProcessSentence(std::unique_ptr<marnav::nmea::gsv> s,
        rapidjson::Value& values_array,
        rapidjson::Document::AllocatorType& allocator);
    /// @brief Process the RMC NMEA0183 sentence
    /// @param s sentence pointer
    /// @param values_array SignalK values array object reference
    /// @param allocator Allocator reference
    void ProcessSentence(std::unique_ptr<marnav::nmea::rmc> s,
        rapidjson::Value& values_array,
        rapidjson::Document::AllocatorType& allocator);
    /// @brief Process the VTG NMEA0183 sentence
    /// @param s sentence pointer
    /// @param values_array SignalK values array object reference
    /// @param allocator Allocator reference
    void ProcessSentence(std::unique_ptr<marnav::nmea::vtg> s,
        rapidjson::Value& values_array,
        rapidjson::Document::AllocatorType& allocator);

public:
    /// @brief Constructor
    NSK()
        : m_nmea_received(0)
        , m_sk_produced(0)
        , m_nmea_errors(0)
        , m_ignored(0)
        , m_nmea_received_total(0)
        , m_sk_produced_total(0)
        , m_unimplemented_count(0)
        , m_counters_start(std::chrono::system_clock::now()) {};
    /// @brief Process NMEA 0183 sentence string
    /// @param stc NMEA 0183 sentence without the trailing "\r\n"
    void ProcessNMEASentence(const std::string& stc);
    /// @brief Get the current rate of incoming NMEA sentences
    /// @return Sentences/second
    size_t NMEARate()
    {
        return 1000.0 * m_nmea_received
            / std::chrono::duration_cast<std::chrono::milliseconds>(
                chrono::system_clock::now() - m_counters_start)
                  .count();
    };
    /// @brief Get the current rate of outgoing SignalK deltas
    /// @return Deltas/second
    size_t SKRate()
    {
        return 1000.0 * m_sk_produced
            / std::chrono::duration_cast<std::chrono::milliseconds>(
                chrono::system_clock::now() - m_counters_start)
                  .count();
    };
    /// @brief Return the unimplemented sentences that appeared in the data
    /// stream
    /// @return String with one tag per line
    std::string Unimplemented() const
    {
        std::string str = "";
        for (auto s : m_unimplemented) {
            str.append(s).append("\n");
        }
        return str;
    };
    /// @brief Return the sentences that appeared in the data stream and caused
    /// Marnav exceptions
    /// @return String with one tag per line
    std::string Unknown() const
    {
        std::string str = "";
        for (auto s : m_unknown) {
            str.append(s).append("\n");
        }
        return str;
    };
    /// @brief Return the set of known sentences and processing settings
    /// @return set of talker+tags and their settings
    std::set<known_sentence>& Known() { return m_known; }
    /// @brief Return total number of NMEA 0183 sentences received since start
    /// @return Number of sentences
    size_t NMEATotal() { return m_nmea_received_total; };
    /// @brief Return total number of SignalK deltas produced since start
    /// @return Number of deltas
    size_t SKTotal() { return m_sk_produced_total; };
    /// @brief Return total number of unimplemented NMEA 0183 sentenced received since start
    /// @return Number of sentences
    size_t TotalUnimplemented() { return m_unimplemented_count; };
    /// @brief Return total number of sentences not supported by Marnav received since start
    /// @return Number of sentences
    size_t TotalUnknown() { return m_nmea_errors; };
    /// @brief Load configuration from file
    /// @param path Path to the JSON file with configuration
    void LoadConfig(const std::string& path);
    /// @brief Save configuration to file
    /// @param path Path to the file with configuration
    void SaveConfig(const std::string& path);
    /// @brief Update a known sentence or add new one to the list
    /// @param stc Sentence to be updated or added
    void UpdateKnown(const known_sentence& stc)
    {
        auto it = m_known.find(stc);
        if (it != m_known.end()) {
            m_known.erase(it);
        }
        m_known.emplace(stc);
    };
};
PLUGIN_END_NAMESPACE

#endif //_NSK_H_
