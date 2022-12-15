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
#include "nskguiimpl.h"
#include <wx/filename.h>

PLUGIN_BEGIN_NAMESPACE

#define kn2ms(x) (0.51444444444 * (x))

using namespace rapidjson;

using namespace marnav;
using namespace nmea;

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
    NSKPreferencesDialogImpl dlg(&m_nsk, parent);
    dlg.ShowModal();
}

void nsk_pi::SetColorScheme(PI_ColorScheme cs)
{
    m_color_scheme = cs;
    // TODO, for now we have no windows
}

void nsk_pi::LoadConfig()
{
    m_nsk.LoadConfig(GetDataDir().ToStdString() + "nsk.json");
}
void nsk_pi::SaveConfig()
{
    m_nsk.SaveConfig(GetDataDir().ToStdString() + "nsk.json");
}

void nsk_pi::SetNMEASentence(wxString& sentence)
{
    std::string stc = sentence.ToStdString();
    stc.erase(std::remove(stc.begin(), stc.end(), '\n'), stc.cend());
    stc.erase(std::remove(stc.begin(), stc.end(), '\r'), stc.cend());
    m_nsk.ProcessNMEASentence(stc);
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
