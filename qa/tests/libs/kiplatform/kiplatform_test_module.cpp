/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#define BOOST_TEST_MODULE KiPlatform
#include <boost/test/unit_test.hpp>

#include <wx/app.h>
#include <wx/init.h>
#include <wx/log.h>


namespace
{
/// wx has to be up for wxSystemSettings to answer; it needs a display, so run these under xvfb-run.
struct WX_FIXTURE
{
    WX_FIXTURE()
    {
        // The GTK port opens its display when the application object is initialized.
        wxApp::SetInstance( new wxApp );
        m_isReady = wxInitialize();
    }
    ~WX_FIXTURE()
    {
        if( m_isReady )
            wxUninitialize();
    }

    bool m_isReady = false;
};
} // namespace

BOOST_GLOBAL_FIXTURE( WX_FIXTURE );
