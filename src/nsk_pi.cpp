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

#include <marnav/nmea/angle.hpp>
#include <marnav/nmea/gga.hpp>
#include <marnav/nmea/gll.hpp>
#include <marnav/nmea/gsa.hpp>
#include <marnav/nmea/gsv.hpp>
#include <marnav/nmea/io.hpp>
#include <marnav/nmea/nmea.hpp>
#include <marnav/nmea/rmc.hpp>
#include <marnav/nmea/vtg.hpp>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "nsk_pi.h"
#include <wx/filename.h>

PLUGIN_BEGIN_NAMESPACE

#define kn2ms(x) (0.51444444444 * (x))

using namespace rapidjson;

using namespace marnav;
using namespace nmea;

/**
 * Generate a UTC ISO8601-formatted timestamp
 * and return as std::string
 */
std::string currentISO8601TimeUTC()
{
    auto now = std::chrono::system_clock::now();
    auto itt = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(gmtime(&itt), "%FT%TZ");
    return ss.str();
}

// the class factories, used to create and destroy instances of the PlugIn

extern "C" DECL_EXP opencpn_plugin* create_pi(void* ppimgr)
{
    return static_cast<opencpn_plugin*>(new nsk_pi(ppimgr));
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p) { delete p; }

//---------------------------------------------------------------------------------------------------------
//
//    NSK PlugIn Implementation
//
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
//
//          PlugIn initialization and de-init
//
//---------------------------------------------------------------------------------------------------------

nsk_pi::nsk_pi(void* ppimgr)
    : opencpn_plugin_118(ppimgr)
    , m_color_scheme(PI_GLOBAL_COLOR_SCHEME_RGB)
{
    if (!wxDirExists(GetDataDir())) {
        wxFileName::Mkdir(GetDataDir(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    }
    m_config_file = GetDataDir() + "config.json";
    m_logo = GetBitmapFromSVGFile(GetDataDir() + "nsk_pi.svg", 32, 32);
}

nsk_pi::~nsk_pi() { }

int nsk_pi::Init()
{
    LoadConfig();

    wxString _svg_nsk = GetDataDir() + "nsk_pi.svg";
    AddLocaleCatalog(_T("opencpn-nsk_pi"));

    return (WANTS_PREFERENCES | WANTS_NMEA_SENTENCES | WANTS_AIS_SENTENCES
        | WANTS_PLUGIN_MESSAGING);
}

bool nsk_pi::DeInit()
{
    SaveConfig();
    return true;
}

int nsk_pi::GetAPIVersionMajor() { return MY_API_VERSION_MAJOR; }

int nsk_pi::GetAPIVersionMinor() { return MY_API_VERSION_MINOR; }

int nsk_pi::GetPlugInVersionMajor() { return PLUGIN_VERSION_MAJOR; }

int nsk_pi::GetPlugInVersionMinor() { return PLUGIN_VERSION_MINOR; }

wxBitmap* nsk_pi::GetPlugInBitmap() { return &m_logo; }
wxString nsk_pi::GetCommonName() { return _("NSK"); }

wxString nsk_pi::GetShortDescription()
{
    return _("NMEA 0183 to SignalK converter PlugIn for OpenCPN");
}

wxString nsk_pi::GetLongDescription()
{
    return _("Simple converter of NMEA 0183 messages to SignalK deltas.\n");
}

void nsk_pi::ShowPreferencesDialog(wxWindow* parent)
{
    // TODO, for now we have no settings
    wxMessageBox("TBD: Preferences dialog");
}

void nsk_pi::SetColorScheme(PI_ColorScheme cs)
{
    m_color_scheme = cs;
    // TODO, for now we have no windows
}

void nsk_pi::LoadConfig()
{
    // TODO, for now we have no configuration
}
void nsk_pi::SaveConfig()
{
    // TODO, for now we have no configuration
}

void nsk_pi::SetNMEASentence(wxString& sentence)
{
    std::string stc = sentence.ToStdString();
    stc.erase(std::remove(stc.begin(), stc.end(), '\n'), stc.cend());
    stc.erase(std::remove(stc.begin(), stc.end(), '\r'), stc.cend());
    try {
        bool processed = true;
        auto s = make_sentence(stc);
        Document d;
        d.SetObject();
        rapidjson::Document::AllocatorType& allocator = d.GetAllocator();
        // size_t sz = allocator.Size();
        //  TODO (maybe, context is optional and we actually don't need it):
        //  "context": "vessels.urn:mrn:imo:mmsi:234567890",
        Value src(kObjectType);
        Value upd(kObjectType);
        Value values(kArrayType);
        Value val(kObjectType);
        src.AddMember("sentence", s->tag(), allocator);
        src.AddMember("talker", to_string(s->get_talker()), allocator);
        if (s->id() == sentence_id::RMC) {
            auto rmc = sentence_cast<nmea::rmc>(s);
            Value pos(kObjectType);

            pos.AddMember("latitude", rmc->get_lat()->get(), allocator);
            pos.AddMember("longitude", rmc->get_lon()->get(), allocator);

            val.AddMember("path", "navigation.position", allocator);
            val.AddMember("value", pos, allocator);
            values.PushBack(val, allocator);
            if (rmc->get_heading()) {
                Value hdg(kObjectType);
                hdg.AddMember("path", "navigation.headingTrue", allocator);
                hdg.AddMember("value", deg2rad(*rmc->get_heading()), allocator);
                values.PushBack(hdg, allocator);
            }
            if (rmc->get_sog()) {
                Value sog(kObjectType);
                sog.AddMember("path", "navigation.speedOverGround", allocator);
                sog.AddMember(
                    "value", kn2ms((*rmc->get_sog()).value()), allocator);
                values.PushBack(sog, allocator);
            }
        } else if (s->id() == sentence_id::GSV) {
            auto gsv = sentence_cast<nmea::gsv>(s);
            val.AddMember("path", "navigation.gnss.satellites", allocator);
            val.AddMember("value", gsv->get_n_satellites_in_view(), allocator);
            values.PushBack(val, allocator);
        } else if (s->id() == sentence_id::GLL) {
            auto gll = sentence_cast<nmea::gll>(s);
            Value pos(kObjectType);

            pos.AddMember("latitude", gll->get_lat()->get(), allocator);
            pos.AddMember("longitude", gll->get_lon()->get(), allocator);

            val.AddMember("path", "navigation.position", allocator);
            val.AddMember("value", pos, allocator);
            values.PushBack(val, allocator);
            /*} else if (s->id() == sentence_id::GSA) {
                auto gsa = sentence_cast<nmea::gsa>(s);
                Value pos(kObjectType);

                //TODO

                val.AddMember("path", "navigation.position", allocator);
                val.AddMember("value", pos, allocator);
                values.PushBack(val, allocator);
            } else if (s->id() == sentence_id::GGA) {
                auto gga = sentence_cast<nmea::gga>(s);
                Value pos(kObjectType);

                //TODO

                val.AddMember("path", "navigation.position", allocator);
                val.AddMember("value", pos, allocator);
                values.PushBack(val, allocator);
            } else if (s->id() == sentence_id::VTG) {
                auto vtg = sentence_cast<nmea::vtg>(s);
                Value pos(kObjectType);

                //TODO

                val.AddMember("path", "navigation.position", allocator);
                val.AddMember("value", pos, allocator);
                values.PushBack(val, allocator);*/
        } else {
            // TODO: And all the other sentences
            // Unknown sentence
            // std::cout << "Unprocessed: " << sentence.c_str() << std::endl;
            processed = false;
        }

        if (processed) {
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
            std::cout << buffer.GetString() << std::endl;
            SendPluginMessage("NSK_SIGNALK", buffer.GetString());
        }
    } catch (...) {
        // std::cout << "Exception while processing " << sentence.c_str() <<
        // std::endl;
        return;
    }
}

void nsk_pi::SetAISSentence(wxString& sentence)
{
    // TODO
}

void nsk_pi::SetPluginMessage(wxString& message_id, wxString& message_body)
{
    if (message_id.IsSameAs(
            "OCPN_CORE_SIGNALK")) { // From the core application we receive
        // "OCPN_CORE_SIGNALK", be prepared for other future
        // sources following common naming convention
        // TODO: If contains "self" and we do not have self configured, set it
    }
}

wxString nsk_pi::GetDataDir()
{
    return GetPluginDataDir("NSK_pi") + wxFileName::GetPathSeparator() + "data"
        + wxFileName::GetPathSeparator();
}

wxBitmap nsk_pi::GetBitmapFromSVG(
    const wxString& filename, const wxCoord w, const wxCoord h)
{
    return GetBitmapFromSVGFile(GetDataDir() + filename, w, h);
}

PLUGIN_END_NAMESPACE
