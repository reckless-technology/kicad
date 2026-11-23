/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <reporter.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>

#include <import_design_rules.h>


BOOST_AUTO_TEST_SUITE( ImportDesignRules )


// A board with no name has nowhere to put rules but a hidden file, which must not be written
BOOST_AUTO_TEST_CASE( NamelessBoardWritesNothing )
{
    wxFileName dir = wxFileName::DirName( wxStandardPaths::Get().GetTempDir() );
    dir.AppendDir( wxString::Format( wxS( "import_design_rules-qa-%lu" ), wxGetProcessId() ) );
    dir.Mkdir( wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL );

    wxFileName board( dir.GetPath(), wxEmptyString, wxS( "kicad_pcb" ) );
    wxFileName hidden( dir.GetPath(), wxEmptyString, wxS( "kicad_dru" ) );

    BOOST_CHECK( !SaveImportedDesignRules( wxS( "(version 1)" ), board, NULL_REPORTER::GetInstance() ) );
    BOOST_CHECK( !hidden.FileExists() );
}


// A rule on one net names it as the source design did, and must follow the board's renaming of that net
BOOST_AUTO_TEST_CASE( RulesFollowRenamedNets )
{
    const wxString rules = wxS( "(version 1)\n"
                                "(rule \"a\"\n    (constraint clearance (min 0.3mm))\n"
                                "    (condition \"A.NetName == 'NetR34_1'\"))\n"
                                "(rule \"b\"\n    (constraint clearance (min 0.2mm))\n"
                                "    (condition \"A.NetName == 'NetR34_1' && B.NetName!='GND'\"))\n"
                                "(rule \"c\"\n    (constraint clearance (min 0.2mm))\n"
                                "    (condition \"A.NetName == B.NetName && A.NetName == 'NetR34_10'\"))\n"
                                "(rule \"d\"\n    (constraint clearance (min 0.2mm))\n"
                                "    (condition \"A.NetName == 'it\\'s'\"))\n" );

    const std::map<wxString, wxString> names = { { wxS( "NetR34_1" ), wxS( "Net-(R34-Pad1)" ) },
                                                 { wxS( "GND" ), wxS( "NetR34_1" ) },
                                                 { wxS( "it's" ), wxS( "don't" ) } };

    const wxString renamed = RenameNetsInDesignRules( rules, names );

    // the renames do not chain, and a comparison with another net or a longer name is left alone
    BOOST_CHECK( renamed.Contains( wxS( "(condition \"A.NetName == 'Net-(R34-Pad1)'\")" ) ) );
    BOOST_CHECK( renamed.Contains( wxS( "A.NetName == 'Net-(R34-Pad1)' && B.NetName!='NetR34_1'" ) ) );
    BOOST_CHECK( renamed.Contains( wxS( "A.NetName == B.NetName && A.NetName == 'NetR34_10'" ) ) );
    BOOST_CHECK( renamed.Contains( wxS( "A.NetName == 'don\\'t'" ) ) );
    BOOST_CHECK( !renamed.Contains( wxS( "it\\'s" ) ) );

    BOOST_CHECK_EQUAL( RenameNetsInDesignRules( rules, {} ), rules );
}


// The rules an import wrote beside the board are changed in place
BOOST_AUTO_TEST_CASE( RulesFileFollowsRenamedNets )
{
    wxFileName dir = wxFileName::DirName( wxStandardPaths::Get().GetTempDir() );
    dir.AppendDir( wxString::Format( wxS( "import_design_rules_rename-qa-%lu" ), wxGetProcessId() ) );
    dir.Mkdir( wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL );

    wxFileName rulesPath( dir.GetPath(), wxS( "board" ), wxS( "kicad_dru" ) );
    wxFile     rulesFile( rulesPath.GetFullPath(), wxFile::write );
    BOOST_REQUIRE( rulesFile.IsOpened() );
    BOOST_REQUIRE( rulesFile.Write( wxS( "(version 1)\n(rule \"a\" (condition \"A.NetName == 'X'\"))\n" ) ) );
    BOOST_REQUIRE( rulesFile.Close() );

    const std::map<wxString, wxString> names = { { wxS( "X" ), wxS( "Net-(R1-Pad1)" ) } };

    BOOST_CHECK( RenameNetsInDesignRulesFile( rulesPath, names, NULL_REPORTER::GetInstance() ) );

    wxString changed;
    wxFile   reread( rulesPath.GetFullPath() );
    BOOST_REQUIRE( reread.ReadAll( &changed, wxConvUTF8 ) );
    BOOST_CHECK( changed.Contains( wxS( "A.NetName == 'Net-(R1-Pad1)'" ) ) );

    // nothing is left to rename the second time, and a board with no rules has nothing to change
    BOOST_CHECK( !RenameNetsInDesignRulesFile( rulesPath, names, NULL_REPORTER::GetInstance() ) );
    BOOST_CHECK( !RenameNetsInDesignRulesFile( wxFileName( dir.GetPath(), wxS( "none" ), wxS( "kicad_dru" ) ), names,
                                               NULL_REPORTER::GetInstance() ) );

    wxRemoveFile( rulesPath.GetFullPath() );
    wxFileName::Rmdir( dir.GetPath() );
}


BOOST_AUTO_TEST_SUITE_END()
