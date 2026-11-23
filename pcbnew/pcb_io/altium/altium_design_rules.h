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

#ifndef ALTIUM_DESIGN_RULES_H
#define ALTIUM_DESIGN_RULES_H

#include <map>
#include <optional>
#include <set>
#include <variant>
#include <vector>

#include <wx/string.h>

#include <board.h>

struct ARULE6;
struct ACLASS6;


/**
 * The names on an imported board that Altium rule scopes can refer to.
 *
 * A scope naming a net, designator or footprint the board does not have can never match, so the
 * rule it belongs to is not written.
 */
struct ALTIUM_RULES_BOARD_CONTEXT
{
    std::set<wxString> m_NetNames;       ///< nets carried by at least one item
    std::set<wxString> m_References;     ///< footprint designators
    std::set<wxString> m_FootprintNames; ///< footprint names, without their library

    /// Copper layers in stack order: the canonical layer name and the layer's type
    std::vector<std::pair<wxString, LAYER_T>> m_CopperLayers;

    /**
     * Collect the context from a board the importer has finished building.
     */
    static ALTIUM_RULES_BOARD_CONTEXT FromBoard( BOARD& aBoard );
};


/**
 * What became of one Altium rule record.
 */
struct ALTIUM_RULE_OUTCOME
{
    wxString m_Kind; ///< Altium RULEKIND
    wxString m_Name;
    int      m_Priority = 1;
    bool     m_Enabled = false;

    /**
     * One of "ported", "ported-disabled" (written commented out, as Altium has it disabled),
     * "ported-importer" (carried by the board settings), "ported-implicit" (KiCad checks it
     * anyway), "shadowed" (never applies in Altium) or "omitted" (KiCad cannot express it).
     */
    wxString m_Status;
    wxString m_Detail;
};


/**
 * Translate the design rules of an Altium board into KiCad custom rules.
 *
 * Altium priority 1 is the highest, and KiCad checks the last matching rule of a file first, so
 * the rules are written in descending priority number; within one priority the earlier record is
 * written last.  A scope pair (First, Second) becomes a condition with First on A and Second on B,
 * which KiCad also tests the other way round, as Altium does.  Rules KiCad has no construct for,
 * or whose scope it cannot express, are reported and left out.
 *
 * An Altium "short circuit allowed" rule is written last, as a clearance rule with a negative
 * minimum, so that it overrides every clearance rule for its scope.  Where a higher priority
 * All/All "not allowed" rule exists the allowance never applies in Altium, and it is reported as
 * shadowed instead.
 */
class ALTIUM_DESIGN_RULES
{
public:
    /**
     * @param aRules    every Rules6 record, in the order of the file.
     * @param aClasses  every Classes6 record, in the order of the file.
     * @param aBoard    the names of the board the rules are for.
     */
    ALTIUM_DESIGN_RULES( const std::vector<ARULE6>& aRules, const std::vector<ACLASS6>& aClasses,
                         const ALTIUM_RULES_BOARD_CONTEXT& aBoard );

    ~ALTIUM_DESIGN_RULES();

    /**
     * @return the custom rules file, or an empty string when no rule could be written.
     * @param aSourceName the Altium file name, quoted in the header comment.
     */
    wxString FormatRules( const wxString& aSourceName ) const;

    /**
     * @return what became of each rule record, in the order of the file.
     */
    const std::vector<ALTIUM_RULE_OUTCOME>& GetOutcomes() const { return m_outcomes; }

    /**
     * @return the component classes a written rule tests for, each with the designators of its
     *         members the board has.  KiCad assigns component classes in the board setup rather
     *         than in the rules file, so these become the project's class assignments.
     */
    std::vector<std::pair<wxString, std::vector<wxString>>> GetComponentClasses() const;

    /**
     * Lower the board minimums to the smallest value any enabled rule that was written asks for.
     *
     * KiCad's board minimums are a floor under every rule, so a scoped Altium rule smaller than
     * the board default (a star point's zero clearance, say) would otherwise be raised to it.
     * Values are rounded to 0.1 um and never go below zero.
     */
    void ApplyBoardMinimums( BOARD_DESIGN_SETTINGS& aSettings ) const;

    /// A condition that never matches, one that always does, or the KiCad expression for it
    using CONDITION = std::variant<bool, wxString>;

    /**
     * Parse and translate one Altium scope expression for object variable @p aVariable
     * ("A" or "B").  Exposed for testing.
     *
     * @throw std::runtime_error when the scope has no KiCad equivalent.
     */
    CONDITION TranslateScope( const wxString& aScope, const wxString& aVariable );

private:
    friend class ALTIUM_SCOPE_PARSER;

    struct CLASS_ENTRY
    {
        wxString              m_Name;
        std::vector<wxString> m_Members;
        bool                  m_IsSuperClass = false;
    };

    struct EMITTED_RULE
    {
        wxString              m_Name;
        int                   m_Priority = 1;
        size_t                m_Index = 0;
        std::vector<wxString> m_Constraints;
        wxString              m_Condition;
        wxString              m_Layer;
        wxString              m_Comment;
        bool                  m_Enabled = false;
        int                   m_Group = 0;
        wxString              m_AltiumName;
        wxString              m_AltiumKind;
    };

    struct SCOPE_NODE;
    struct RULE;

    void run();
    void translate( const RULE& aRule );

    CONDITION emit( const SCOPE_NODE& aNode, const wxString& aVariable );

    /// The condition for a (First, Second) scope pair; nullopt when it never applies
    std::optional<wxString> pairCondition( const RULE& aRule, const wxString& aExtra, bool& aApplies, wxString& aWhy );

    wxString uniqueName( const wxString& aKind, const wxString& aName );

    void note( const RULE& aRule, const wxString& aStatus, const wxString& aDetail = wxEmptyString );
    void ported( const RULE& aRule, const wxString& aDetail );

    void add( const RULE& aRule, const wxString& aKindLabel, const std::vector<wxString>& aConstraints,
              const wxString& aCondition, const wxString& aLayer, int aGroup,
              const wxString& aExtraName = wxEmptyString );

    void zoneConnect( const RULE& aRule, const wxString& aStyleKey, const wxString& aWidthKey, const wxString& aGapKey,
                      const wxString& aKindLabel, const std::vector<wxString>& aLayers );

    std::vector<wxString> planeLayers() const;

    const ALTIUM_RULES_BOARD_CONTEXT& m_board;

    std::vector<RULE> m_rules;

    std::map<wxString, CLASS_ENTRY> m_netClasses; ///< keyed by lower-case name
    std::map<wxString, CLASS_ENTRY> m_componentClasses;
    std::vector<wxString>           m_componentClassOrder;
    std::map<wxString, CLASS_ENTRY> m_padClasses;
    std::map<wxString, CLASS_ENTRY> m_diffPairClasses;

    std::map<wxString, size_t> m_defaults; ///< kind to its All/All rule
    std::optional<int>         m_disallowShortPriority;

    std::set<wxString>      m_usedComponentClasses;
    std::vector<wxString>   m_scopeNotes;
    std::map<wxString, int> m_names;

    std::vector<EMITTED_RULE>               m_emitted;
    std::vector<ALTIUM_RULE_OUTCOME>        m_outcomes;
    std::map<wxString, std::vector<double>> m_boardFloors; ///< board minimum to rule values in mm
};

#endif // ALTIUM_DESIGN_RULES_H
