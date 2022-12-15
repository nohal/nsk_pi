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

#ifndef _NSKPI_H_
#define _NSKPI_H_

#include "config.h"
#include "nsk.h"
#include "ocpn_plugin.h"
#include "pi_common.h"

#define MY_API_VERSION_MAJOR 1
#define MY_API_VERSION_MINOR 18

PLUGIN_BEGIN_NAMESPACE

//----------------------------------------------------------------------------------------------------------
//    The PlugIn Class Definition
//----------------------------------------------------------------------------------------------------------
/// Class representing the plugin for OpenCPN Plugin API
class nsk_pi : public opencpn_plugin_118 {
private:
    /// Color scheme used by the plugin
    int m_color_scheme;
    /// Bitmap representation of the logo of the plugin
    wxBitmap m_logo;
    /// Path to the configuration file
    wxString m_config_file;

    NSK m_nsk;

    /// Load the configuration from disk
    void LoadConfig();

public:
    /// Constructor
    ///
    /// \param ppimgr Pointer to the plugin manager
    explicit nsk_pi(void* ppimgr);

    /// Destructor
    ~nsk_pi() override;

    //    The required PlugIn Methods
    /// Initialize the plugin
    ///
    /// \return
    int Init() override;

    /// Deinitialize the plugin
    ///
    /// \return
    bool DeInit() override;

    /// Save the configuration to disk
    void SaveConfig();

    /// Get major version of the plugin API the plugin requires
    ///
    /// \return Major version of the API
    int GetAPIVersionMajor() override;

    /// Get minor version of the plugin API the plugin requires
    ///
    /// \return Minor version of the API
    int GetAPIVersionMinor() override;

    /// Get major version of the plugin
    ///
    /// \return MAjor version of the plugin
    int GetPlugInVersionMajor() override;

    /// Get minor version of the plugin
    ///
    /// \return Minor version of the plugin
    int GetPlugInVersionMinor() override;

    /// Get bitmap icon of the plugin logo
    ///
    /// \return pointer to the bitmap containing the logo
    wxBitmap* GetPlugInBitmap() override;

    /// Get the name of the plugin
    ///
    /// \return Name of the plugin
    wxString GetCommonName() override;

    /// Get short description of the plugin
    /// The description should be a short single line text that fits the list
    /// view in the OpenCPN plugin manager tab of the Toolbox
    ///
    /// \return Short description of the plugin
    wxString GetShortDescription() override;

    /// Get long description of the plugin
    ///
    /// \return Longer text describing the plugin displayed on the plugin detail
    /// tile
    ///         in the OpenCPN plugin manager tab of the Toolbox once the plugin
    ///         is selected.
    wxString GetLongDescription() override;

    void ShowPreferencesDialog(wxWindow* parent) override;

    /// Set color scheme the plugin should use
    /// Invoked when the core application color scheme is changed
    ///
    /// \param cs Color scheme
    void SetColorScheme(PI_ColorScheme cs) override;

    /// Callback delivering NMEA messages from the core application
    ///
    /// \param sentence The NMEA 0183 message
    void SetNMEASentence(wxString& sentence) override;

    /// Callback delivering NMEA AIS messages from the core application
    ///
    /// \param sentence The NMEA 0183 message
    void SetAISSentence(wxString& sentence) override;

    /// Callback delivering JSON messages from the core application
    ///
    /// \param message_id id of the message (We are interested at least in
    /// OCPN_CORE_SIGNAL, but there might be more to come) \param message_body
    /// The actual JSON message
    void SetPluginMessage(
        wxString& message_id, wxString& message_body) override;

    /// Get Path to the plaugin data
    ///
    /// \return Path to the plugin data including the trailing separator
    wxString GetDataDir();

    /// Get bitmap from SVG file
    ///
    /// \param filename Path to the SVG file
    /// \param w Width of the requested bitmap
    /// \param h Height of the requested bitmal
    /// \return Generated bitmap
    wxBitmap GetBitmapFromSVG(
        const wxString& filename, const wxCoord w, const wxCoord h);
};

PLUGIN_END_NAMESPACE

#endif //_NSKPI_H_
