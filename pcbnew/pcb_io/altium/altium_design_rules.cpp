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

#include "altium_design_rules.h"
#include "altium_parser_pcb.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <wx/regex.h>

#include <base_units.h>
#include <board_design_settings.h>
#include <board_connected_item.h>
#include <footprint.h>
#include <lset.h>
#include <math/util.h>


namespace
{

/// A rule or scope KiCad has no construct for; the message says why
class UNSUPPORTED : public std::runtime_error
{
public:
    explicit UNSUPPORTED( const wxString& aWhy ) :
            std::runtime_error( aWhy.ToStdString( wxConvUTF8 ) )
    {
    }
};


/// A value missing from the record or not readable as a length
class UNREADABLE : public std::runtime_error
{
public:
    explicit UNREADABLE( const wxString& aWhy ) :
            std::runtime_error( aWhy.ToStdString( wxConvUTF8 ) )
    {
    }
};


wxString fromUtf8( const char* aText )
{
    return wxString::FromUTF8( aText );
}


/// A length as Altium writes it ("19.685mil", "0.5mm", or a bare number of mils) in millimetres
double toMillimetres( const wxString& aValue )
{
    static wxRegEx lengthRegex( wxT( "^[[:space:]]*(-?[0-9.]+(e-?[0-9]+)?)[[:space:]]*([a-z]*)[[:space:]]*$" ),
                                wxRE_ADVANCED | wxRE_ICASE );

    if( !lengthRegex.Matches( aValue ) )
        throw UNREADABLE( wxString::Format( wxT( "bad length '%s'" ), aValue ) );

    double   number;
    wxString unit = lengthRegex.GetMatch( aValue, 3 ).Lower();

    if( !lengthRegex.GetMatch( aValue, 1 ).ToCDouble( &number ) )
        throw UNREADABLE( wxString::Format( wxT( "bad length '%s'" ), aValue ) );

    static const std::map<wxString, double> millimetresPerUnit = { { wxT( "mil" ), 0.0254 },
                                                                   { wxT( "" ), 0.0254 },
                                                                   { wxT( "mm" ), 1.0 },
                                                                   { wxT( "in" ), 25.4 },
                                                                   { wxT( "cm" ), 10.0 } };

    auto it = millimetresPerUnit.find( unit );

    if( it == millimetresPerUnit.end() )
        throw UNREADABLE( wxString::Format( wxT( "unknown unit '%s'" ), unit ) );

    return number * it->second;
}


/// A length for a rule file: millimetres to 0.1 um with trailing zeros dropped
wxString formatMillimetres( double aValue )
{
    wxString text = wxString::FromCDouble( aValue, 4 );

    if( text.Contains( wxT( "." ) ) )
    {
        while( text.EndsWith( wxT( "0" ) ) )
            text.RemoveLast();

        if( text.EndsWith( wxT( "." ) ) )
            text.RemoveLast();
    }

    if( text.IsEmpty() || text == wxT( "-0" ) )
        text = wxT( "0" );

    return text + wxT( "mm" );
}


/// A length rounded as a project file would store it
double roundMillimetres( double aValue )
{
    double rounded;
    wxString::FromCDouble( aValue, 4 ).ToCDouble( &rounded );
    return rounded;
}


/// A string operand of a KiCad rule condition
wxString quoted( const wxString& aText )
{
    wxString escaped = aText;
    escaped.Replace( wxT( "'" ), wxT( "\\'" ) );
    return wxT( "'" ) + escaped + wxT( "'" );
}


wxString trimmed( const wxString& aText )
{
    wxString text = aText;
    return text.Trim( true ).Trim( false );
}


const std::set<wxString> builtinComponentClasses = { wxT( "all components" ), wxT( "inside board components" ),
                                                     wxT( "outside board components" ), wxT( "top side components" ),
                                                     wxT( "bottom side components" ) };


const std::map<wxString, wxString> unsupportedKinds = {
    { wxT( "FanoutControl" ), wxT( "BGA/SOIC/LCC fanout style is a router setting; no KiCad rule" ) },
    { wxT( "RoutingPriority" ), wxT( "net routing order has no KiCad equivalent" ) },
    { wxT( "RoutingTopology" ), wxT( "router topology has no KiCad equivalent" ) },
    { wxT( "RoutingCorners" ), wxT( "corner style / setback is a router mode in KiCad, not a rule" ) },
    { wxT( "LayerPairs" ), wxT( "layer pair enforcement has no KiCad equivalent" ) },
    { wxT( "Height" ), wxT( "component height (3D) is not a KiCad rule" ) },
    { wxT( "FabricationTestpoint" ), wxT( "test point style: no KiCad rule (pads can be flagged Test point pad)" ) },
    { wxT( "FabricationTestPointUsage" ), wxT( "test point usage: no KiCad rule" ) },
    { wxT( "AssemblyTestpoint" ), wxT( "test point style: no KiCad rule" ) },
    { wxT( "AssemblyTestPointUsage" ), wxT( "test point usage: no KiCad rule" ) },
    { wxT( "NetAntennae" ), wxT( "net antennae check: no KiCad equivalent" ) },
    { wxT( "UnpouredPolygon" ), wxT( "unpoured polygon check: no direct KiCad rule (zone fill/isolated copper "
                                     "checks exist in Board Setup)" ) },
    { wxT( "RoomDefinition" ), wxT( "room (placement confinement) has no KiCad equivalent" ) },
};


// Groups order rules of one priority; short-circuit allowances are written after everything else
constexpr int GROUP_SHORT_CIRCUIT = 9;

} // namespace


/**
 * An Altium rule record with the property accessors the translation needs.
 */
struct ALTIUM_DESIGN_RULES::RULE
{
    std::map<wxString, wxString> m_Props;
    size_t                       m_Index = 0;

    bool Has( const wxString& aKey ) const { return m_Props.count( aKey ) > 0; }

    /// The value, or @p aDefault when the record does not have the key
    wxString Get( const wxString& aKey, const wxString& aDefault = wxEmptyString ) const
    {
        auto it = m_Props.find( aKey );
        return it == m_Props.end() ? aDefault : it->second;
    }

    /// The value, or @p aDefault when the record does not have it or it is empty
    wxString GetOr( const wxString& aKey, const wxString& aDefault = wxEmptyString ) const
    {
        auto it = m_Props.find( aKey );
        return ( it == m_Props.end() || it->second.IsEmpty() ) ? aDefault : it->second;
    }

    /// The value; the record is unreadable without it
    wxString Require( const wxString& aKey ) const
    {
        auto it = m_Props.find( aKey );

        if( it == m_Props.end() )
            throw UNREADABLE( wxT( "'" ) + aKey + wxT( "'" ) );

        return it->second;
    }

    double Length( const wxString& aKey ) const { return toMillimetres( Require( aKey ) ); }

    wxString Kind() const { return Get( wxT( "RULEKIND" ) ); }
    wxString Name() const { return Get( wxT( "NAME" ) ); }

    int Priority() const
    {
        long priority = 1;
        Get( wxT( "PRIORITY" ), wxT( "1" ) ).ToLong( &priority );
        return static_cast<int>( priority );
    }

    bool IsEnabled() const { return Get( wxT( "ENABLED" ) ) == wxT( "TRUE" ); }

    /// Whether the record says it is enabled; the record is unreadable without the flag
    bool RequireEnabled() const { return Require( wxT( "ENABLED" ) ) == wxT( "TRUE" ); }

    wxString Scope1() const { return Get( wxT( "SCOPE1EXPRESSION" ), wxT( "All" ) ); }
    wxString Scope2() const { return Get( wxT( "SCOPE2EXPRESSION" ), wxT( "All" ) ); }

    bool IsAllAll() const
    {
        return trimmed( Get( wxT( "SCOPE1EXPRESSION" ) ) ).Lower() == wxT( "all" )
               && trimmed( Get( wxT( "SCOPE2EXPRESSION" ) ) ).Lower() == wxT( "all" );
    }
};


/**
 * A parsed Altium query: a combination of functions, possibly compared with a value.
 */
struct ALTIUM_DESIGN_RULES::SCOPE_NODE
{
    enum class TYPE
    {
        AND,
        OR,
        NOT,
        CALL,
        COMPARE
    };

    TYPE                    m_Type = TYPE::CALL;
    std::vector<SCOPE_NODE> m_Children;
    wxString                m_Function; ///< lower case, for CALL and the left side of COMPARE
    std::vector<wxString>   m_Arguments;
};


namespace
{

enum class SCOPE_TOKEN_TYPE
{
    STRING,
    OPERATOR,
    WORD,
    END
};


struct SCOPE_TOKEN
{
    SCOPE_TOKEN_TYPE m_Type = SCOPE_TOKEN_TYPE::END;
    wxString   m_Text;
};


std::vector<SCOPE_TOKEN> tokenizeScope( const wxString& aScope )
{
    std::vector<SCOPE_TOKEN> tokens;
    wxString                 text = trimmed( aScope );
    size_t                   pos = 0;

    auto isOperatorChar = []( wxUniChar aChar )
    {
        return wxString( wxT( "()!,<>=" ) ).Find( aChar ) != wxNOT_FOUND;
    };

    while( pos < text.length() )
    {
        while( pos < text.length() && wxIsspace( text[pos] ) )
            pos++;

        if( pos >= text.length() )
            break;

        wxUniChar ch = text[pos];

        if( ch == '\'' )
        {
            // a string runs to the next lone quote; a doubled quote stands for one
            size_t end = pos + 1;

            while( true )
            {
                if( end >= text.length() )
                    throw UNSUPPORTED( wxT( "cannot tokenise scope near '" ) + text.Mid( pos, 20 ) + wxT( "'" ) );

                if( text[end] == '\'' )
                {
                    if( end + 1 < text.length() && text[end + 1] == '\'' )
                    {
                        end += 2;
                        continue;
                    }

                    break;
                }

                end++;
            }

            tokens.push_back( { SCOPE_TOKEN_TYPE::STRING, text.Mid( pos, end + 1 - pos ) } );
            pos = end + 1;
            continue;
        }

        for( const wxString& op : { wxT( "&&" ), wxT( "||" ), wxT( "<>" ), wxT( "<=" ), wxT( ">=" ) } )
        {
            if( text.Mid( pos, 2 ) == op )
            {
                tokens.push_back( { SCOPE_TOKEN_TYPE::OPERATOR, op } );
                pos += 2;
                ch = 0;
                break;
            }
        }

        if( ch == 0 )
            continue;

        if( isOperatorChar( ch ) )
        {
            tokens.push_back( { SCOPE_TOKEN_TYPE::OPERATOR, wxString( ch ) } );
            pos++;
            continue;
        }

        size_t end = pos;

        while( end < text.length() && !wxIsspace( text[end] ) && !isOperatorChar( text[end] ) && text[end] != '\'' )
            end++;

        tokens.push_back( { SCOPE_TOKEN_TYPE::WORD, text.Mid( pos, end - pos ) } );
        pos = end;
    }

    return tokens;
}

} // namespace


/**
 * Recursive descent over the tokens of an Altium query: or, and, not, then a call or a
 * parenthesised expression, either of which may be compared with a value.
 */
class ALTIUM_SCOPE_PARSER
{
public:
    using NODE = ALTIUM_DESIGN_RULES::SCOPE_NODE;

    explicit ALTIUM_SCOPE_PARSER( const wxString& aScope ) :
            m_tokens( tokenizeScope( aScope ) ),
            m_pos( 0 )
    {
    }

    NODE Parse()
    {
        NODE node = parseOr();

        if( m_pos != m_tokens.size() )
            throw UNSUPPORTED( wxT( "trailing tokens in scope" ) );

        return node;
    }

private:
    const SCOPE_TOKEN& peek() const
    {
        static const SCOPE_TOKEN end;
        return m_pos < m_tokens.size() ? m_tokens[m_pos] : end;
    }

    SCOPE_TOKEN take()
    {
        SCOPE_TOKEN token = peek();
        m_pos++;
        return token;
    }

    bool peekIs( std::initializer_list<const wchar_t*> aWords ) const
    {
        if( peek().m_Type == SCOPE_TOKEN_TYPE::END )
            return false;

        wxString text = peek().m_Text.Lower();

        for( const wchar_t* word : aWords )
        {
            if( text == word )
                return true;
        }

        return false;
    }

    NODE parseOr()
    {
        std::vector<NODE> parts{ parseAnd() };

        while( peekIs( { L"or", L"||", L"xor" } ) )
        {
            if( take().m_Text.Lower() == wxT( "xor" ) )
                throw UNSUPPORTED( wxT( "xor in scope" ) );

            parts.push_back( parseAnd() );
        }

        if( parts.size() == 1 )
            return parts[0];

        NODE node;
        node.m_Type = NODE::TYPE::OR;
        node.m_Children = std::move( parts );
        return node;
    }

    NODE parseAnd()
    {
        std::vector<NODE> parts{ parseNot() };

        while( peekIs( { L"and", L"&&" } ) )
        {
            take();
            parts.push_back( parseNot() );
        }

        if( parts.size() == 1 )
            return parts[0];

        NODE node;
        node.m_Type = NODE::TYPE::AND;
        node.m_Children = std::move( parts );
        return node;
    }

    NODE parseNot()
    {
        if( peekIs( { L"not", L"!" } ) )
        {
            take();

            NODE node;
            node.m_Type = NODE::TYPE::NOT;
            node.m_Children.push_back( parseNot() );
            return node;
        }

        return parsePrimary();
    }

    NODE parsePrimary()
    {
        SCOPE_TOKEN token = take();

        if( token.m_Type == SCOPE_TOKEN_TYPE::OPERATOR && token.m_Text == wxT( "(" ) )
        {
            NODE node = parseOr();

            if( take().m_Text != wxT( ")" ) )
                throw UNSUPPORTED( wxT( "unbalanced parentheses in scope" ) );

            return maybeCompare( std::move( node ) );
        }

        if( token.m_Type == SCOPE_TOKEN_TYPE::END )
            throw UNSUPPORTED( wxT( "empty scope" ) );

        if( token.m_Type != SCOPE_TOKEN_TYPE::WORD )
            throw UNSUPPORTED( wxT( "unexpected token '" ) + token.m_Text + wxT( "'" ) );

        NODE node;
        node.m_Type = NODE::TYPE::CALL;
        node.m_Function = token.m_Text.Lower();

        if( peek().m_Type == SCOPE_TOKEN_TYPE::OPERATOR && peek().m_Text == wxT( "(" ) )
        {
            take();

            // Arguments are the tokens up to the closing parenthesis, split at commas; a string
            // loses its quotes, and the tokens of one argument are joined by spaces
            std::vector<wxString> current;
            bool                  any = false;

            auto joined = []( const std::vector<wxString>& aParts )
            {
                wxString text;

                for( size_t i = 0; i < aParts.size(); i++ )
                    text += ( i ? wxT( " " ) : wxT( "" ) ) + aParts[i];

                return trimmed( text );
            };

            while( true )
            {
                SCOPE_TOKEN arg = take();

                if( arg.m_Type == SCOPE_TOKEN_TYPE::END )
                    throw UNSUPPORTED( wxT( "unterminated call" ) );

                if( arg.m_Type == SCOPE_TOKEN_TYPE::OPERATOR && arg.m_Text == wxT( ")" ) )
                    break;

                if( arg.m_Type == SCOPE_TOKEN_TYPE::OPERATOR && arg.m_Text == wxT( "," ) )
                {
                    node.m_Arguments.push_back( joined( current ) );
                    current.clear();
                    any = true;
                    continue;
                }

                wxString value = arg.m_Text;

                if( arg.m_Type == SCOPE_TOKEN_TYPE::STRING )
                {
                    value = value.Mid( 1, value.length() - 2 );
                    value.Replace( wxT( "''" ), wxT( "'" ) );
                }

                current.push_back( value );
            }

            if( !current.empty() || any )
                node.m_Arguments.push_back( joined( current ) );

            return node;
        }

        return maybeCompare( std::move( node ) );
    }

    NODE maybeCompare( NODE aLeft )
    {
        static const std::set<wxString> comparisons = { wxT( "<" ),  wxT( ">" ),  wxT( "=" ),
                                                        wxT( "<>" ), wxT( "<=" ), wxT( ">=" ) };

        if( peek().m_Type != SCOPE_TOKEN_TYPE::END && comparisons.count( peek().m_Text ) )
        {
            take();
            take();

            NODE node;
            node.m_Type = NODE::TYPE::COMPARE;
            node.m_Function = aLeft.m_Type == NODE::TYPE::CALL ? aLeft.m_Function : wxString();
            node.m_Children.push_back( std::move( aLeft ) );
            return node;
        }

        return aLeft;
    }

    std::vector<SCOPE_TOKEN> m_tokens;
    size_t                   m_pos;
};


ALTIUM_RULES_BOARD_CONTEXT ALTIUM_RULES_BOARD_CONTEXT::FromBoard( BOARD& aBoard )
{
    ALTIUM_RULES_BOARD_CONTEXT context;

    for( BOARD_CONNECTED_ITEM* item : aBoard.AllConnectedItems() )
    {
        if( item->GetNetCode() > 0 )
            context.m_NetNames.insert( item->GetNetname() );
    }

    for( FOOTPRINT* footprint : aBoard.Footprints() )
    {
        context.m_References.insert( footprint->GetReference() );
        context.m_FootprintNames.insert( footprint->GetFPID().GetLibItemName().wx_str() );
    }

    for( PCB_LAYER_ID layer : aBoard.GetEnabledLayers().CuStack() )
        context.m_CopperLayers.emplace_back( LSET::Name( layer ), aBoard.GetLayerType( layer ) );

    return context;
}


ALTIUM_DESIGN_RULES::ALTIUM_DESIGN_RULES( const std::vector<ARULE6>& aRules, const std::vector<ACLASS6>& aClasses,
                                          const ALTIUM_RULES_BOARD_CONTEXT& aBoard ) :
        m_board( aBoard )
{
    for( const ARULE6& rule : aRules )
        m_rules.push_back( { rule.properties, m_rules.size() } );

    for( const ACLASS6& cls : aClasses )
    {
        CLASS_ENTRY entry{ cls.name, cls.names, cls.superClass };
        wxString    key = cls.name.Lower();

        switch( cls.kind )
        {
        case ALTIUM_CLASS_KIND::NET_CLASS: m_netClasses[key] = entry; break;
        case ALTIUM_CLASS_KIND::PAD_CLASS: m_padClasses[key] = entry; break;
        case ALTIUM_CLASS_KIND::DIFF_PAIR_CLASS: m_diffPairClasses[key] = entry; break;

        case ALTIUM_CLASS_KIND::SOURCE_SCHEMATIC_CLASS: // the component classes
            if( !m_componentClasses.count( key ) )
                m_componentClassOrder.push_back( key );

            m_componentClasses[key] = entry;
            break;

        default: break;
        }
    }

    run();
}


ALTIUM_DESIGN_RULES::~ALTIUM_DESIGN_RULES() = default;


std::vector<wxString> ALTIUM_DESIGN_RULES::planeLayers() const
{
    std::vector<wxString> planes;

    for( const auto& [name, type] : m_board.m_CopperLayers )
    {
        if( type == LT_POWER )
            planes.push_back( name );
    }

    return planes;
}


static wxString kicadLayerName( const wxString& aAltiumLayer, bool& aAnyLayer )
{
    static const std::map<wxString, wxString> layers = {
        { wxT( "top layer" ), wxT( "F.Cu" ) },      { wxT( "bottom layer" ), wxT( "B.Cu" ) },
        { wxT( "top overlay" ), wxT( "F.SilkS" ) }, { wxT( "bottom overlay" ), wxT( "B.SilkS" ) },
        { wxT( "top solder" ), wxT( "F.Mask" ) },   { wxT( "bottom solder" ), wxT( "B.Mask" ) },
        { wxT( "top paste" ), wxT( "F.Paste" ) },   { wxT( "bottom paste" ), wxT( "B.Paste" ) },
        { wxT( "multi-layer" ), wxT( "" ) },
    };

    wxString name = trimmed( aAltiumLayer ).Lower();
    aAnyLayer = false;

    if( auto it = layers.find( name ); it != layers.end() )
    {
        aAnyLayer = it->second.IsEmpty();
        return it->second;
    }

    static wxRegEx midLayer( wxT( "^mid[- ]?layer[[:space:]]*([0-9]+)$" ), wxRE_ADVANCED );

    if( midLayer.Matches( name ) )
        return wxT( "In" ) + midLayer.GetMatch( name, 1 ) + wxT( ".Cu" );

    throw UNSUPPORTED( wxString::Format( wxT( "layer '%s' has no KiCad equivalent" ), aAltiumLayer ) );
}


ALTIUM_DESIGN_RULES::CONDITION ALTIUM_DESIGN_RULES::emit( const SCOPE_NODE& aNode, const wxString& aVariable )
{
    const wxString& v = aVariable;

    switch( aNode.m_Type )
    {
    case SCOPE_NODE::TYPE::AND:
    case SCOPE_NODE::TYPE::OR:
    {
        const bool             isAnd = aNode.m_Type == SCOPE_NODE::TYPE::AND;
        std::vector<CONDITION> parts;

        for( const SCOPE_NODE& child : aNode.m_Children )
            parts.push_back( emit( child, v ) );

        // a false term decides an and, a true one an or; the other constant drops out
        std::vector<wxString> terms;

        for( const CONDITION& part : parts )
        {
            if( std::holds_alternative<bool>( part ) && std::get<bool>( part ) != isAnd )
                return !isAnd;
        }

        for( const CONDITION& part : parts )
        {
            if( std::holds_alternative<wxString>( part ) )
                terms.push_back( std::get<wxString>( part ) );
        }

        if( terms.empty() )
            return isAnd;

        if( terms.size() == 1 )
            return terms[0];

        wxString joined;

        for( size_t i = 0; i < terms.size(); i++ )
            joined += ( i ? ( isAnd ? wxT( " && " ) : wxT( " || " ) ) : wxT( "" ) ) + terms[i];

        return wxT( "(" ) + joined + wxT( ")" );
    }

    case SCOPE_NODE::TYPE::NOT:
    {
        CONDITION inner = emit( aNode.m_Children[0], v );

        if( std::holds_alternative<bool>( inner ) )
            return !std::get<bool>( inner );

        const wxString& text = std::get<wxString>( inner );
        return wxT( "!" ) + ( text.StartsWith( wxT( "(" ) ) ? text : wxT( "(" ) + text + wxT( ")" ) );
    }

    case SCOPE_NODE::TYPE::COMPARE:
        throw UNSUPPORTED( wxString::Format( wxT( "comparison '%s' has no KiCad equivalent" ), aNode.m_Function ) );

    case SCOPE_NODE::TYPE::CALL: break;
    }

    const wxString&              fn = aNode.m_Function;
    const std::vector<wxString>& args = aNode.m_Arguments;
    wxString                     a0 = args.empty() ? wxString() : args[0];

    auto caseInsensitiveHit = []( const std::set<wxString>& aNames, const wxString& aName ) -> std::optional<wxString>
    {
        for( const wxString& name : aNames )
        {
            if( name.Lower() == aName.Lower() )
                return name;
        }

        return std::nullopt;
    };

    if( fn == wxT( "all" ) )
        return true;

    if( fn == wxT( "innet" ) )
    {
        if( !m_board.m_NetNames.count( a0 ) )
        {
            std::optional<wxString> hit = caseInsensitiveHit( m_board.m_NetNames, a0 );

            if( !hit )
            {
                m_scopeNotes.push_back( wxString::Format( wxT( "net '%s' not on board" ), a0 ) );
                return false;
            }

            a0 = *hit;
        }

        return v + wxT( ".NetName == " ) + quoted( a0 );
    }

    if( fn == wxT( "innetclass" ) )
    {
        auto it = m_netClasses.find( a0.Lower() );

        if( it == m_netClasses.end() )
        {
            if( a0.Lower() == wxT( "all nets" ) )
                return true;

            m_scopeNotes.push_back( wxString::Format( wxT( "net class '%s' not defined on board" ), a0 ) );
            return false;
        }

        if( it->second.m_IsSuperClass )
        {
            if( it->second.m_Name.Lower() == wxT( "all nets" ) )
                return true;

            throw UNSUPPORTED( wxString::Format( wxT( "built-in net class '%s'" ), it->second.m_Name ) );
        }

        return v + wxT( ".hasNetclass(" ) + quoted( it->second.m_Name ) + wxT( ")" );
    }

    if( fn == wxT( "incomponent" ) )
    {
        std::optional<wxString> hit = caseInsensitiveHit( m_board.m_References, a0 );

        if( !hit )
        {
            m_scopeNotes.push_back( wxString::Format( wxT( "component '%s' not on board" ), a0 ) );
            return false;
        }

        return v + wxT( ".Reference == " ) + quoted( *hit );
    }

    if( fn == wxT( "incomponentclass" ) )
    {
        auto it = m_componentClasses.find( a0.Lower() );

        if( it == m_componentClasses.end() )
        {
            m_scopeNotes.push_back( wxString::Format( wxT( "component class '%s' not defined on board" ), a0 ) );
            return false;
        }

        if( it->second.m_IsSuperClass || builtinComponentClasses.count( it->second.m_Name.Lower() ) )
        {
            if( it->second.m_Name.Lower() == wxT( "all components" ) )
                return true;

            throw UNSUPPORTED( wxString::Format( wxT( "built-in component class '%s'" ), it->second.m_Name ) );
        }

        m_usedComponentClasses.insert( it->second.m_Name );
        return v + wxT( ".hasComponentClass(" ) + quoted( it->second.m_Name ) + wxT( ")" );
    }

    if( fn == wxT( "inpadclass" ) )
    {
        auto it = m_padClasses.find( a0.Lower() );

        if( it == m_padClasses.end() )
        {
            m_scopeNotes.push_back( wxString::Format( wxT( "pad class '%s' not defined on board" ), a0 ) );
            return false;
        }

        if( it->second.m_IsSuperClass )
        {
            if( it->second.m_Name.Lower() == wxT( "all pads" ) )
                return v + wxT( ".Type == 'Pad'" );

            throw UNSUPPORTED( wxString::Format( wxT( "built-in pad class '%s'" ), it->second.m_Name ) );
        }

        // members are "designator-pad number"
        std::set<wxString> terms;

        for( const wxString& member : it->second.m_Members )
        {
            wxString reference = member.BeforeFirst( '-' );
            wxString pad = member.Contains( wxT( "-" ) ) ? member.AfterFirst( '-' ) : wxString();

            terms.insert( wxString::Format( wxT( "(%s.Reference == %s && %s.Pad_Number == %s)" ), v,
                                            quoted( reference ), v, quoted( pad ) ) );
        }

        if( terms.empty() )
            return false;

        wxString joined;

        for( const wxString& term : terms )
            joined += ( joined.IsEmpty() ? wxT( "" ) : wxT( " || " ) ) + term;

        return wxT( "(" ) + joined + wxT( ")" );
    }

    if( fn == wxT( "hasfootprint" ) )
    {
        std::optional<wxString> hit = caseInsensitiveHit( m_board.m_FootprintNames, a0 );

        if( !hit )
        {
            m_scopeNotes.push_back( wxString::Format( wxT( "footprint '%s' not on board" ), a0 ) );
            return false;
        }

        return v + wxT( ".Library_Link == " ) + quoted( wxT( "*:" ) + *hit );
    }

    static const std::map<wxString, wxString> simpleTypes = {
        { wxT( "ispad" ), wxT( "Pad" ) },      { wxT( "isvia" ), wxT( "Via" ) },
        { wxT( "istrack" ), wxT( "Track" ) },  { wxT( "isarc" ), wxT( "Track" ) },
        { wxT( "isregion" ), wxT( "Zone" ) },  { wxT( "ispolygon" ), wxT( "Zone" ) },
        { wxT( "inpolygon" ), wxT( "Zone" ) }, { wxT( "inpoly" ), wxT( "Zone" ) },
        { wxT( "isfill" ), wxT( "Zone" ) },    { wxT( "istext" ), wxT( "Text" ) },
        { wxT( "isstring" ), wxT( "Text" ) },  { wxT( "iscomponent" ), wxT( "Footprint" ) },
    };

    if( auto it = simpleTypes.find( fn ); it != simpleTypes.end() && args.empty() )
        return v + wxT( ".Type == " ) + quoted( it->second );

    if( fn == wxT( "issmdpad" ) )
        return wxString::Format( wxT( "(%s.Type == 'Pad' && %s.Pad_Type == 'SMD')" ), v, v );

    if( fn == wxT( "isthroughholepad" ) || fn == wxT( "isthpad" ) )
        return wxString::Format( wxT( "(%s.Type == 'Pad' && %s.Pad_Type == 'Through-hole')" ), v, v );

    if( fn == wxT( "innamedpolygon" ) )
        return wxString::Format( wxT( "(%s.Type == 'Zone' && %s.Name == %s)" ), v, v, quoted( a0 ) );

    if( fn == wxT( "onlayer" ) )
    {
        bool     anyLayer;
        wxString layer = kicadLayerName( a0, anyLayer );

        // a multi-layer object is on every layer, which KiCad has no test for
        if( anyLayer )
            return wxString( wxT( "true" ) );

        return v + wxT( ".Layer == " ) + quoted( layer );
    }

    if( fn == wxT( "indifferentialpair" ) )
        return v + wxT( ".inDiffPair(" ) + quoted( a0.IsEmpty() ? wxString( wxT( "*" ) ) : a0 ) + wxT( ")" );

    wxString joinedArgs;

    for( size_t i = 0; i < args.size(); i++ )
        joinedArgs += ( i ? wxT( ", " ) : wxT( "" ) ) + args[i];

    throw UNSUPPORTED( wxString::Format( wxT( "scope function %s(%s) has no KiCad equivalent" ), fn, joinedArgs ) );
}


ALTIUM_DESIGN_RULES::CONDITION ALTIUM_DESIGN_RULES::TranslateScope( const wxString& aScope, const wxString& aVariable )
{
    return emit( ALTIUM_SCOPE_PARSER( aScope ).Parse(), aVariable );
}


std::optional<wxString> ALTIUM_DESIGN_RULES::pairCondition( const RULE& aRule, const wxString& aExtra, bool& aApplies,
                                                            wxString& aWhy )
{
    size_t   notesBefore = m_scopeNotes.size();
    wxString scope1 = aRule.Scope1();
    wxString scope2 = aRule.Scope2();

    SCOPE_NODE first = ALTIUM_SCOPE_PARSER( scope1 ).Parse();
    SCOPE_NODE second = ALTIUM_SCOPE_PARSER( scope2 ).Parse();

    // nullopt for "always", false for "never"
    std::variant<bool, std::optional<wxString>> pair;

    auto isTrue = []( const CONDITION& c )
    {
        return std::holds_alternative<bool>( c ) && std::get<bool>( c );
    };
    auto isFalse = []( const CONDITION& c )
    {
        return std::holds_alternative<bool>( c ) && !std::get<bool>( c );
    };

    CONDITION a = emit( first, wxT( "A" ) );

    if( trimmed( scope1 ).Lower() == trimmed( scope2 ).Lower() )
    {
        // the same scope on both sides: both objects must satisfy it
        if( isTrue( a ) )
        {
            pair = std::optional<wxString>();
        }
        else if( isFalse( a ) )
        {
            pair = false;
        }
        else
        {
            CONDITION b = emit( first, wxT( "B" ) );
            pair = std::optional<wxString>( std::get<wxString>( a ) + wxT( " && " ) + std::get<wxString>( b ) );
        }
    }
    else
    {
        CONDITION b = emit( second, wxT( "B" ) );

        if( isFalse( a ) || isFalse( b ) )
        {
            pair = false;
        }
        else if( isTrue( a ) && isTrue( b ) )
        {
            pair = std::optional<wxString>();
        }
        else if( isTrue( a ) )
        {
            // KiCad tests both orders, so a lone Second scope can sit on A
            CONDITION second_on_a = emit( second, wxT( "A" ) );
            pair = std::optional<wxString>( std::get<wxString>( second_on_a ) );
        }
        else if( isTrue( b ) )
        {
            pair = std::optional<wxString>( std::get<wxString>( a ) );
        }
        else
        {
            pair = std::optional<wxString>( std::get<wxString>( a ) + wxT( " && " ) + std::get<wxString>( b ) );
        }
    }

    if( std::holds_alternative<bool>( pair ) )
    {
        aApplies = false;

        for( size_t i = notesBefore; i < m_scopeNotes.size(); i++ )
            aWhy += ( i > notesBefore ? wxT( "; " ) : wxT( "" ) ) + m_scopeNotes[i];

        if( aWhy.IsEmpty() )
            aWhy = wxT( "scope can never match" );

        return std::nullopt;
    }

    aApplies = true;

    std::optional<wxString> condition = std::get<std::optional<wxString>>( pair );
    std::vector<wxString>   parts;

    if( !aExtra.IsEmpty() )
        parts.push_back( aExtra );

    if( condition && !condition->IsEmpty() )
        parts.push_back( *condition );

    if( parts.empty() )
        return std::nullopt;

    wxString joined;

    for( size_t i = 0; i < parts.size(); i++ )
        joined += ( i ? wxT( " && " ) : wxT( "" ) ) + parts[i];

    return joined;
}


wxString ALTIUM_DESIGN_RULES::uniqueName( const wxString& aKind, const wxString& aName )
{
    wxString base = ( !aName.IsEmpty() && aName != aKind ) ? wxT( "Altium " ) + aKind + wxT( " " ) + aName
                                                           : wxT( "Altium " ) + aKind;
    base.Replace( wxT( "\"" ), wxT( "'" ) );

    int count = ++m_names[base];
    return count == 1 ? base : wxString::Format( wxT( "%s (%d)" ), base, count );
}


void ALTIUM_DESIGN_RULES::note( const RULE& aRule, const wxString& aStatus, const wxString& aDetail )
{
    m_outcomes.push_back( { aRule.Kind(), aRule.Name(), aRule.Priority(), aRule.IsEnabled(), aStatus, aDetail } );
}


void ALTIUM_DESIGN_RULES::ported( const RULE& aRule, const wxString& aDetail )
{
    note( aRule, aRule.IsEnabled() ? wxT( "ported" ) : wxT( "ported-disabled" ), aDetail );
}


void ALTIUM_DESIGN_RULES::add( const RULE& aRule, const wxString& aKindLabel, const std::vector<wxString>& aConstraints,
                               const wxString& aCondition, const wxString& aLayer, int aGroup,
                               const wxString& aExtraName )
{
    EMITTED_RULE rule;
    rule.m_Name = uniqueName( aKindLabel, aRule.Name() + aExtraName );
    rule.m_Priority = aRule.Priority();
    rule.m_Index = m_emitted.size();
    rule.m_Constraints = aConstraints;
    rule.m_Condition = aCondition;
    rule.m_Layer = aLayer;
    rule.m_Comment = trimmed( aRule.Get( wxT( "COMMENT" ) ) );
    rule.m_Enabled = aRule.IsEnabled();
    rule.m_Group = aGroup;
    rule.m_AltiumName = aRule.Name();
    rule.m_AltiumKind = aRule.Kind();
    m_emitted.push_back( rule );
}


void ALTIUM_DESIGN_RULES::run()
{
    std::vector<const RULE*> byPriority;

    for( const RULE& rule : m_rules )
        byPriority.push_back( &rule );

    std::stable_sort( byPriority.begin(), byPriority.end(),
                      []( const RULE* a, const RULE* b )
                      {
                          return a->Priority() < b->Priority();
                      } );

    // the rule the importer reads into the board settings: the first enabled All/All by priority
    for( const RULE* rule : byPriority )
    {
        if( rule->IsEnabled() && rule->IsAllAll() && !m_defaults.count( rule->Kind() ) )
            m_defaults[rule->Kind()] = rule->m_Index;
    }

    // an All/All "short circuit not allowed" rule beats every lower priority allowance
    for( const RULE& rule : m_rules )
    {
        if( rule.Kind() == wxT( "ShortCircuit" ) && rule.IsEnabled() && rule.Get( wxT( "ALLOWED" ) ) != wxT( "TRUE" )
            && rule.Get( wxT( "SCOPE1EXPRESSION" ) ).Lower() == wxT( "all" )
            && rule.Get( wxT( "SCOPE2EXPRESSION" ) ).Lower() == wxT( "all" ) )
        {
            m_disallowShortPriority = std::min( m_disallowShortPriority.value_or( rule.Priority() ), rule.Priority() );
        }
    }

    for( const RULE& rule : m_rules )
    {
        try
        {
            translate( rule );
        }
        catch( const UNSUPPORTED& error )
        {
            note( rule, wxT( "omitted" ), fromUtf8( error.what() ) );
        }
        catch( const UNREADABLE& error )
        {
            note( rule, wxT( "omitted" ), wxT( "unreadable value: " ) + fromUtf8( error.what() ) );
        }
    }
}


void ALTIUM_DESIGN_RULES::zoneConnect( const RULE& aRule, const wxString& aStyleKey, const wxString& aWidthKey,
                                       const wxString& aGapKey, const wxString& aKindLabel,
                                       const std::vector<wxString>& aLayers )
{
    bool                    applies;
    wxString                why;
    std::optional<wxString> condition = pairCondition( aRule, wxEmptyString, applies, why );

    if( !applies )
        return note( aRule, wxT( "omitted" ), why );

    static const std::map<wxString, wxString> styles = { { wxT( "Relief" ), wxT( "thermal_reliefs" ) },
                                                         { wxT( "Direct" ), wxT( "solid" ) },
                                                         { wxT( "NoConnect" ), wxT( "none" ) } };

    wxString style = aRule.Get( aStyleKey, wxT( "Relief" ) );
    auto     styleIt = styles.find( style );

    if( styleIt == styles.end() )
        throw UNSUPPORTED( wxString::Format( wxT( "connect style '%s'" ), style ) );

    std::vector<wxString> constraints{ wxT( "zone_connection " ) + styleIt->second };
    wxString              detail = wxT( "zone connection " ) + styleIt->second;

    if( style == wxT( "Relief" ) )
    {
        double width = aRule.Length( aWidthKey );
        double gap = aRule.Length( aGapKey );

        if( gap > 0 )
            constraints.push_back( wxT( "thermal_relief_gap (min " ) + formatMillimetres( gap ) + wxT( ")" ) );

        if( width > 0 )
            constraints.push_back( wxT( "thermal_spoke_width (opt " ) + formatMillimetres( width ) + wxT( ")" ) );

        detail += wxT( ", gap " ) + formatMillimetres( gap ) + wxT( ", spoke width " ) + formatMillimetres( width )
                  + ( width > 0 ? wxT( "" ) : wxT( " (0 ignored)" ) );

        if( aRule.Has( wxT( "RELIEFENTRIES" ) ) && aRule.Get( wxT( "RELIEFENTRIES" ) ) != wxT( "4" ) )
        {
            detail += wxT( "; " ) + aRule.Get( wxT( "RELIEFENTRIES" ) )
                      + wxT( " conductors (KiCad always cuts 4 spokes)" );
        }

        if( aRule.Get( wxT( "POLYGONRELIEFANGLE" ), wxT( "90 Angle" ) ) != wxT( "90 Angle" ) )
            detail += wxT( "; relief angle " ) + aRule.Get( wxT( "POLYGONRELIEFANGLE" ) ) + wxT( " not ported" );
    }

    for( const wxString& layer : aLayers )
    {
        add( aRule, aKindLabel, constraints, condition.value_or( wxString() ), layer, layer.IsEmpty() ? 0 : 10,
             layer.IsEmpty() ? wxString() : wxT( " " ) + layer );
    }

    ported( aRule, detail );
}


void ALTIUM_DESIGN_RULES::translate( const RULE& aRule )
{
    const wxString kind = aRule.Kind();
    bool           applies;
    wxString       why;

    auto addFloor = [&]( const wxChar* aSetting, double aValue )
    {
        m_boardFloors[aSetting].push_back( aValue );
    };

    auto conditionFor = [&]( const wxString& aExtra = wxEmptyString ) -> wxString
    {
        return pairCondition( aRule, aExtra, applies, why ).value_or( wxString() );
    };

    auto mm = formatMillimetres;

    if( kind == wxT( "Clearance" ) )
    {
        double   gap = aRule.Length( wxT( "GAP" ) );
        wxString condition = conditionFor();

        if( !applies )
            return note( aRule, wxT( "omitted" ), why );

        wxString netScope = aRule.Get( wxT( "NETSCOPE" ), wxT( "DifferentNets" ) );
        wxString type = wxT( "clearance" );

        if( netScope == wxT( "AnyNet" ) )
        {
            type = wxT( "physical_clearance" );
        }
        else if( netScope == wxT( "SameNet" ) )
        {
            type = wxT( "physical_clearance" );
            condition = condition.IsEmpty() ? wxString( wxT( "A.NetName == B.NetName" ) )
                                            : condition + wxT( " && A.NetName == B.NetName" );
        }

        if( aRule.Has( wxT( "LAYERKIND" ) ) && aRule.Get( wxT( "LAYERKIND" ) ) != wxT( "SameLayer" ) )
        {
            throw UNSUPPORTED( wxString::Format( wxT( "clearance between different layers (%s)" ),
                                                 aRule.Get( wxT( "LAYERKIND" ) ) ) );
        }

        if( aRule.RequireEnabled() )
            addFloor( wxT( "min_clearance" ), gap );

        add( aRule, wxT( "Clearance" ), { type + wxT( " (min " ) + mm( gap ) + wxT( ")" ) }, condition, wxEmptyString,
             1 );

        wxString detail = wxT( "clearance " ) + mm( gap );

        if( !aRule.GetOr( wxT( "OBJECTCLEARANCES" ) ).IsEmpty() )
            detail += wxT( "; per-object-pair matrix (hole/arc/track...) not ported, generic value used" );

        return ported( aRule, detail );
    }

    if( kind == wxT( "PlaneClearance" ) )
    {
        std::vector<wxString> planes = planeLayers();

        if( planes.empty() )
            return note( aRule, wxT( "omitted" ), wxT( "board has no internal plane layers" ) );

        double   gap = aRule.Length( wxT( "CLEARANCE" ) );
        wxString condition = conditionFor();

        if( !applies )
            return note( aRule, wxT( "omitted" ), why );

        wxString layerList;

        for( const wxString& layer : planes )
        {
            add( aRule, wxT( "PlaneClearance" ), { wxT( "clearance (min " ) + mm( gap ) + wxT( ")" ) }, condition,
                 layer, 2, wxT( " " ) + layer );
            layerList += ( layerList.IsEmpty() ? wxT( "" ) : wxT( ", " ) ) + layer;
        }

        if( aRule.RequireEnabled() )
            addFloor( wxT( "min_clearance" ), gap );

        return ported( aRule, wxT( "clearance " ) + mm( gap ) + wxT( " on plane layers " ) + layerList );
    }

    if( kind == wxT( "ShortCircuit" ) )
    {
        if( aRule.Get( wxT( "ALLOWED" ) ) != wxT( "TRUE" ) )
        {
            return note( aRule, wxT( "ported-implicit" ),
                         wxT( "shorts are errors in KiCad by default (shorting_items)" ) );
        }

        wxString condition = conditionFor();

        if( !applies )
            return note( aRule, wxT( "omitted" ), why );

        if( m_disallowShortPriority && aRule.Priority() > *m_disallowShortPriority )
        {
            return note( aRule, wxT( "shadowed" ),
                         wxString::Format( wxT( "Altium priority %s is below the All/All \"short circuit not "
                                                "allowed\" rule at priority %d, which wins in Altium, so this "
                                                "allowance never applies" ),
                                           aRule.Get( wxT( "PRIORITY" ) ), *m_disallowShortPriority ) );
        }

        add( aRule, wxT( "ShortCircuit" ), { wxT( "clearance (min -1mm)" ) }, condition, wxEmptyString,
             GROUP_SHORT_CIRCUIT );

        return ported( aRule, wxT( "allowed short: clearance rule with negative minimum (overlap permitted)" ) );
    }

    if( kind == wxT( "Width" ) )
    {
        double   minimum = aRule.Length( wxT( "MINLIMIT" ) );
        double   maximum = aRule.Length( wxT( "MAXLIMIT" ) );
        double   preferred = aRule.Length( wxT( "PREFEREDWIDTH" ) );
        wxString condition = conditionFor();

        if( !applies )
            return note( aRule, wxT( "omitted" ), why );

        if( aRule.RequireEnabled() )
            addFloor( wxT( "min_track_width" ), minimum );

        add( aRule, wxT( "Width" ),
             { wxT( "track_width (min " ) + mm( minimum ) + wxT( ") (opt " ) + mm( preferred ) + wxT( ") (max " )
               + mm( maximum ) + wxT( ")" ) },
             condition, wxEmptyString, 3 );

        wxString detail = wxT( "track width min " ) + mm( minimum ) + wxT( " / pref " ) + mm( preferred )
                          + wxT( " / max " ) + mm( maximum );

        for( const auto& [key, value] : aRule.m_Props )
        {
            if( key.StartsWith( wxT( "MIDLAYER" ) ) || key.StartsWith( wxT( "TOPLAYER" ) )
                || key.StartsWith( wxT( "BOTTOMLAYER" ) ) )
            {
                detail += wxT( "; per-layer widths in the record not ported (same min/max applied on all "
                               "layers)" );
                break;
            }
        }

        if( aRule.Has( wxT( "LAYERKIND" ) ) && aRule.Get( wxT( "LAYERKIND" ) ) != wxT( "SameLayer" ) )
            detail += wxT( "; layer kind " ) + aRule.Get( wxT( "LAYERKIND" ) );

        return ported( aRule, detail );
    }

    if( kind == wxT( "RoutingVias" ) )
    {
        wxString condition = conditionFor( wxT( "A.Type == 'Via'" ) );

        if( !applies )
            return note( aRule, wxT( "omitted" ), why );

        double diameterMin = aRule.Length( wxT( "MINWIDTH" ) );
        double diameterOpt = aRule.Length( wxT( "WIDTH" ) );
        double diameterMax = aRule.Length( wxT( "MAXWIDTH" ) );
        double holeMin = aRule.Length( wxT( "MINHOLEWIDTH" ) );
        double holeOpt = aRule.Length( wxT( "HOLEWIDTH" ) );
        double holeMax = aRule.Length( wxT( "MAXHOLEWIDTH" ) );

        if( aRule.RequireEnabled() )
        {
            addFloor( wxT( "min_via_diameter" ), diameterMin );
            addFloor( wxT( "min_through_hole_diameter" ), holeMin );
        }

        add( aRule, wxT( "RoutingVias" ),
             { wxT( "via_diameter (min " ) + mm( diameterMin ) + wxT( ") (opt " ) + mm( diameterOpt ) + wxT( ") (max " )
               + mm( diameterMax ) + wxT( ")" ) },
             condition, wxEmptyString, 4 );

        // a via must satisfy the hole size rule as well in Altium
        if( auto it = m_defaults.find( wxT( "HoleSize" ) ); it != m_defaults.end() )
        {
            const RULE& holeSize = m_rules[it->second];

            if( holeSize.Get( wxT( "ABSOLUTEVALUES" ), wxT( "TRUE" ) ) == wxT( "TRUE" ) )
            {
                holeMin = std::max( holeMin, holeSize.Length( wxT( "MINLIMIT" ) ) );
                holeMax = std::min( holeMax, holeSize.Length( wxT( "MAXLIMIT" ) ) );
            }
        }

        add( aRule, wxT( "RoutingVias" ),
             { wxT( "hole_size (min " ) + mm( holeMin ) + wxT( ") (opt " ) + mm( holeOpt ) + wxT( ") (max " )
               + mm( holeMax ) + wxT( ")" ) },
             condition, wxEmptyString, 5, wxT( " hole" ) );

        wxString detail = wxT( "via diameter " ) + mm( diameterMin ) + wxT( "/" ) + mm( diameterOpt ) + wxT( "/" )
                          + mm( diameterMax ) + wxT( ", via hole " ) + mm( holeMin ) + wxT( "/" ) + mm( holeOpt )
                          + wxT( "/" ) + mm( holeMax );

        if( aRule.Get( wxT( "VIASTYLE" ), wxT( "Through Hole" ) ) != wxT( "Through Hole" ) )
            detail += wxT( "; via style " ) + aRule.Get( wxT( "VIASTYLE" ) ) + wxT( " not distinguished" );

        return ported( aRule, detail );
    }

    if( kind == wxT( "HoleSize" ) )
    {
        if( aRule.Get( wxT( "ABSOLUTEVALUES" ), wxT( "TRUE" ) ) != wxT( "TRUE" ) )
            throw UNSUPPORTED( wxT( "hole size given as percentage of pad size" ) );

        wxString condition = conditionFor();

        if( !applies )
            return note( aRule, wxT( "omitted" ), why );

        double minimum = aRule.Length( wxT( "MINLIMIT" ) );
        double maximum = aRule.Length( wxT( "MAXLIMIT" ) );

        if( aRule.RequireEnabled() )
            addFloor( wxT( "min_through_hole_diameter" ), minimum );

        add( aRule, wxT( "HoleSize" ),
             { wxT( "hole_size (min " ) + mm( minimum ) + wxT( ") (max " ) + mm( maximum ) + wxT( ")" ) }, condition,
             wxEmptyString, 5 );

        return ported( aRule, wxT( "pad and via hole " ) + mm( minimum ) + wxT( " .. " ) + mm( maximum ) );
    }

    if( kind == wxT( "HoleToHoleClearance" ) )
    {
        wxString condition = conditionFor();

        if( !applies )
            return note( aRule, wxT( "omitted" ), why );

        double gap = aRule.Length( wxT( "GAP" ) );

        if( aRule.RequireEnabled() )
            addFloor( wxT( "min_hole_to_hole" ), gap );

        add( aRule, wxT( "HoleToHole" ), { wxT( "hole_to_hole (min " ) + mm( gap ) + wxT( ")" ) }, condition,
             wxEmptyString, 6 );

        return ported( aRule, wxT( "hole to hole " ) + mm( gap ) );
    }

    if( kind == wxT( "MinimumAnnularRing" ) )
    {
        wxString condition = conditionFor();

        if( !applies )
            return note( aRule, wxT( "omitted" ), why );

        wxString value = aRule.GetOr( wxT( "MINIMUMANNULARRING" ) );

        if( value.IsEmpty() )
            value = aRule.GetOr( wxT( "MINLIMIT" ) );

        if( value.IsEmpty() )
            value = aRule.GetOr( wxT( "GAP" ) );

        double ring = toMillimetres( value );

        add( aRule, wxT( "AnnularRing" ), { wxT( "annular_width (min " ) + mm( ring ) + wxT( ")" ) }, condition,
             wxEmptyString, 6 );

        return ported( aRule, wxT( "annular ring " ) + mm( ring ) + wxT( " (record keys assumed: not in corpus)" ) );
    }

    if( kind == wxT( "BoardOutlineClearance" ) )
    {
        wxString condition = conditionFor();

        if( !applies )
            return note( aRule, wxT( "omitted" ), why );

        double gap = toMillimetres( aRule.GetOr( wxT( "GAP" ) ) );

        add( aRule, wxT( "BoardOutline" ), { wxT( "edge_clearance (min " ) + mm( gap ) + wxT( ")" ) }, condition,
             wxEmptyString, 6 );

        return ported( aRule, wxT( "board outline clearance " ) + mm( gap ) + wxT( " (not in corpus)" ) );
    }

    if( kind == wxT( "SolderMaskExpansion" ) || kind == wxT( "PasteMaskExpansion" ) )
    {
        const bool solder = kind == wxT( "SolderMaskExpansion" );
        wxString   condition = conditionFor();

        if( !applies )
            return note( aRule, wxT( "omitted" ), why );

        double   expansion = aRule.Length( wxT( "EXPANSION" ) );
        wxString detail = ( solder ? wxT( "mask expansion " ) : wxT( "paste expansion " ) ) + mm( expansion );

        if( solder
            && ( aRule.Get( wxT( "ISTENTINGTOP" ) ) == wxT( "FALSE" )
                 || aRule.Get( wxT( "ISTENTINGBOTTOM" ) ) == wxT( "FALSE" ) ) )
        {
            detail += wxT( "; via tenting flag (untented) is a board setting in KiCad, not a rule - not ported" );
        }

        auto defaultIt = m_defaults.find( kind );

        // the All/All rule is the board-wide margin, which the importer sets
        if( condition.IsEmpty() && defaultIt != m_defaults.end() && defaultIt->second == aRule.m_Index )
        {
            if( solder && !aRule.RequireEnabled() )
                return ported( aRule, detail );

            return note( aRule, wxT( "ported-importer" ), detail + wxT( " (importer sets the board-wide margin)" ) );
        }

        if( solder )
        {
            add( aRule, wxT( "SolderMask" ), { wxT( "solder_mask_expansion (opt " ) + mm( expansion ) + wxT( ")" ) },
                 condition, wxEmptyString, 7 );
        }
        else
        {
            add( aRule, wxT( "PasteMask" ), { wxT( "solder_paste_abs_margin (opt " ) + mm( expansion ) + wxT( ")" ) },
                 condition, wxEmptyString, 7 );
        }

        return ported( aRule, detail );
    }

    if( kind == wxT( "MinimumSolderMaskSliver" ) )
    {
        wxString condition = conditionFor();

        if( !applies )
            return note( aRule, wxT( "omitted" ), why );

        double width = aRule.Length( wxT( "MINSOLDERMASKWIDTH" ) );

        add( aRule, wxT( "MaskSliver" ), { wxT( "solder_mask_sliver (min " ) + mm( width ) + wxT( ")" ) }, condition,
             wxEmptyString, 7 );

        return ported( aRule, wxT( "minimum solder mask width " ) + mm( width ) );
    }

    if( kind == wxT( "SilkToSolderMaskClearance" ) )
    {
        conditionFor();

        if( !applies )
            return note( aRule, wxT( "omitted" ), why );

        double gap = aRule.Length( wxT( "MINSILKSCREENTOMASKGAP" ) );

        add( aRule, wxT( "SilkToMask" ), { wxT( "silk_clearance (min " ) + mm( gap ) + wxT( ")" ) },
             wxT( "(A.Layer == 'F.SilkS' || A.Layer == 'B.SilkS') && B.Type == 'Pad'" ), wxEmptyString, 8 );

        return ported( aRule, wxT( "silk to pad " ) + mm( gap )
                                      + wxT( " (approximation: KiCad measures to the pad copper, Altium to the mask "
                                             "opening)" ) );
    }

    if( kind == wxT( "SilkToSilkClearance" ) )
    {
        conditionFor();

        if( !applies )
            return note( aRule, wxT( "omitted" ), why );

        double gap = aRule.Length( wxT( "SILKTOSILKCLEARANCE" ) );

        add( aRule, wxT( "SilkToSilk" ), { wxT( "silk_clearance (min " ) + mm( gap ) + wxT( ")" ) },
             wxT( "(A.Layer == 'F.SilkS' || A.Layer == 'B.SilkS') && (B.Layer == 'F.SilkS' || B.Layer == "
                  "'B.SilkS')" ),
             wxEmptyString, 8 );

        return ported( aRule, wxT( "silk to silk " ) + mm( gap ) );
    }

    if( kind == wxT( "SilkToBoardRegionClearance" ) )
    {
        throw UNSUPPORTED( wxT( "record stores no distance (Altium default applies); KiCad has silk_clearance "
                                "only" ) );
    }

    if( kind == wxT( "ComponentClearance" ) )
    {
        wxString condition = conditionFor();

        if( !applies )
            return note( aRule, wxT( "omitted" ), why );

        double gap = aRule.Length( wxT( "GAP" ) );

        add( aRule, wxT( "ComponentClearance" ), { wxT( "courtyard_clearance (min " ) + mm( gap ) + wxT( ")" ) },
             condition, wxEmptyString, 8 );

        wxString detail = wxT( "courtyard clearance " ) + mm( gap );

        if( aRule.Get( wxT( "NETSCOPE" ) ) == wxT( "DifferentNets" ) )
            detail += wxT( "; Altium limits to different nets, KiCad applies to all footprints" );

        if( !aRule.GetOr( wxT( "VERTICALGAP" ) ).IsEmpty() && aRule.Length( wxT( "VERTICALGAP" ) ) < 1000 )
            detail += wxT( "; vertical (Z) gap not ported" );

        return ported( aRule, detail );
    }

    if( kind == wxT( "PolygonConnect" ) )
    {
        return zoneConnect( aRule, wxT( "CONNECTSTYLE" ), wxT( "RELIEFCONDUCTORWIDTH" ), wxT( "AIRGAPWIDTH" ),
                            wxT( "PolygonConnect" ), { wxEmptyString } );
    }

    if( kind == wxT( "PlaneConnect" ) )
    {
        std::vector<wxString> planes = planeLayers();

        if( planes.empty() )
            return note( aRule, wxT( "omitted" ), wxT( "board has no internal plane layers" ) );

        return zoneConnect( aRule, wxT( "PLANECONNECTSTYLE" ), wxT( "RELIEFCONDUCTORWIDTH" ), wxT( "RELIEFAIRGAP" ),
                            wxT( "PlaneConnect" ), planes );
    }

    if( kind == wxT( "DiffPairsRouting" ) )
    {
        bool anyUserPairs = std::any_of( m_diffPairClasses.begin(), m_diffPairClasses.end(),
                                         []( const auto& aEntry )
                                         {
                                             return !aEntry.second.m_IsSuperClass;
                                         } );

        if( !anyUserPairs )
        {
            throw UNSUPPORTED( wxT( "board defines no differential pairs; the default rule is not applied (KiCad "
                                    "would guess pairs from _P/_N and +/- net names)" ) );
        }

        wxString condition = conditionFor();

        if( !applies )
            return note( aRule, wxT( "omitted" ), why );

        wxString base = wxT( "A.inDiffPair('*')" );
        condition = condition.IsEmpty() ? base : base + wxT( " && " ) + condition;

        add( aRule, wxT( "DiffPair" ),
             { wxT( "diff_pair_gap (min " ) + mm( aRule.Length( wxT( "MINLIMIT" ) ) ) + wxT( ") (opt " )
                       + mm( aRule.Length( wxT( "MOSTFREQGAP" ) ) ) + wxT( ") (max " )
                       + mm( aRule.Length( wxT( "MAXLIMIT" ) ) ) + wxT( ")" ),
               wxT( "diff_pair_uncoupled (max " ) + mm( aRule.Length( wxT( "MAXUNCOUPLEDLENGTH" ) ) ) + wxT( ")" ) },
             condition, wxEmptyString, 11 );

        return ported( aRule, wxT( "diff pair gap and uncoupled length" ) );
    }

    if( kind == wxT( "RoutingLayers" ) )
    {
        throw UNSUPPORTED( wxT( "autorouter/router layer permission, not a design check in Altium: existing tracks "
                                "on unchecked layers are not violations there" ) );
    }

    if( kind == wxT( "UnRoutedNet" ) )
    {
        return note( aRule, wxT( "ported-implicit" ),
                     wxT( "KiCad always checks unconnected items (Board Setup > Violation Severity)" ) );
    }

    auto it = unsupportedKinds.find( kind );

    throw UNSUPPORTED( it != unsupportedKinds.end() ? it->second
                                                    : wxString( wxT( "no KiCad equivalent for rule "
                                                                     "kind" ) ) );
}


std::vector<std::pair<wxString, std::vector<wxString>>> ALTIUM_DESIGN_RULES::GetComponentClasses() const
{
    std::vector<std::pair<wxString, std::vector<wxString>>> classes;

    for( const wxString& key : m_componentClassOrder )
    {
        const CLASS_ENTRY& entry = m_componentClasses.at( key );

        if( entry.m_IsSuperClass || entry.m_Members.empty() )
            continue;

        if( builtinComponentClasses.count( entry.m_Name.Lower() ) )
            continue;

        // only the classes a rule refers to
        if( !m_usedComponentClasses.count( entry.m_Name ) )
            continue;

        std::set<wxString> members;

        for( const wxString& member : entry.m_Members )
        {
            if( m_board.m_References.count( member ) )
                members.insert( member );
        }

        if( !members.empty() )
            classes.emplace_back( entry.m_Name, std::vector<wxString>( members.begin(), members.end() ) );
    }

    return classes;
}


wxString ALTIUM_DESIGN_RULES::FormatRules( const wxString& aSourceName ) const
{
    if( m_emitted.empty() )
        return wxEmptyString;

    wxString out;

    out << wxT( "(version 1)\n" );
    out << wxT( "# Converted from " ) << aSourceName << wxT( " by the KiCad Altium importer\n" );
    out << wxT( "# Rules are evaluated last-to-first; written in descending Altium priority number.\n" );
    out << wxT( "\n" );

    // Short-circuit allowances last, so they override every clearance rule; otherwise descending
    // Altium priority number, and within one priority the earlier record last (it is checked first)
    std::vector<const EMITTED_RULE*> order;

    for( const EMITTED_RULE& rule : m_emitted )
        order.push_back( &rule );

    std::sort( order.begin(), order.end(),
               []( const EMITTED_RULE* a, const EMITTED_RULE* b )
               {
                   bool aShort = a->m_Group == GROUP_SHORT_CIRCUIT;
                   bool bShort = b->m_Group == GROUP_SHORT_CIRCUIT;

                   if( aShort != bShort )
                       return bShort;

                   if( a->m_Priority != b->m_Priority )
                       return a->m_Priority > b->m_Priority;

                   return a->m_Index > b->m_Index;
               } );

    for( const EMITTED_RULE* rule : order )
    {
        std::vector<wxString> body;

        body.push_back( wxT( "(rule \"" ) + rule->m_Name + wxT( "\"" ) );

        if( !rule->m_Layer.IsEmpty() )
            body.push_back( wxT( "    (layer \"" ) + rule->m_Layer + wxT( "\")" ) );

        for( const wxString& constraint : rule->m_Constraints )
            body.push_back( wxT( "    (constraint " ) + constraint + wxT( ")" ) );

        if( !rule->m_Condition.IsEmpty() )
        {
            wxString condition = rule->m_Condition;
            condition.Replace( wxT( "\"" ), wxT( "'" ) );
            body.push_back( wxT( "    (condition \"" ) + condition + wxT( "\")" ) );
        }

        body.back() += wxT( ")" );

        if( !rule->m_Comment.IsEmpty() )
        {
            wxString comment = rule->m_Comment;
            comment.Replace( wxT( "\n" ), wxT( " " ) );
            out << wxT( "# " ) << comment << wxT( "\n" );
        }

        out << wxString::Format( wxT( "# Altium %s rule \"%s\", priority %d%s\n" ), rule->m_AltiumKind,
                                 rule->m_AltiumName, rule->m_Priority,
                                 rule->m_Enabled ? wxT( "" ) : wxT( " (DISABLED in Altium)" ) );

        // a rule Altium has switched off is kept for reference, commented out
        for( const wxString& line : body )
            out << ( rule->m_Enabled ? wxT( "" ) : wxT( "# " ) ) << line << wxT( "\n" );

        out << wxT( "\n" );
    }

    // the text ends without the blank line that follows each rule
    out.RemoveLast();

    return out;
}


void ALTIUM_DESIGN_RULES::ApplyBoardMinimums( BOARD_DESIGN_SETTINGS& aSettings ) const
{
    // the board minimums the importer takes from the All/All rules, in millimetres
    std::map<wxString, double> importer;

    auto defaultRule = [&]( const wxChar* aKind ) -> const RULE*
    {
        auto it = m_defaults.find( aKind );
        return it == m_defaults.end() ? nullptr : &m_rules[it->second];
    };

    auto readInto = [&]( const wxChar* aSetting, const RULE* aRule, const wxChar* aKey )
    {
        if( !aRule )
            return;

        try
        {
            importer[aSetting] = aRule->Length( aKey );
        }
        catch( const UNREADABLE& )
        {
        }
    };

    readInto( wxT( "min_clearance" ), defaultRule( wxT( "Clearance" ) ), wxT( "GAP" ) );
    readInto( wxT( "min_track_width" ), defaultRule( wxT( "Width" ) ), wxT( "MINLIMIT" ) );
    readInto( wxT( "min_via_diameter" ), defaultRule( wxT( "RoutingVias" ) ), wxT( "MINWIDTH" ) );
    readInto( wxT( "min_through_hole_diameter" ), defaultRule( wxT( "RoutingVias" ) ), wxT( "MINHOLEWIDTH" ) );
    readInto( wxT( "min_hole_to_hole" ), defaultRule( wxT( "HoleToHoleClearance" ) ), wxT( "GAP" ) );

    const std::map<wxString, int*> settings = {
        { wxT( "min_clearance" ), &aSettings.m_MinClearance },
        { wxT( "min_track_width" ), &aSettings.m_TrackMinWidth },
        { wxT( "min_via_diameter" ), &aSettings.m_ViasMinSize },
        { wxT( "min_through_hole_diameter" ), &aSettings.m_MinThroughDrill },
        { wxT( "min_hole_to_hole" ), &aSettings.m_HoleToHoleMin },
    };

    for( const auto& [setting, values] : m_boardFloors )
    {
        double lowest = *std::min_element( values.begin(), values.end() );

        if( auto it = importer.find( setting ); it != importer.end() )
            lowest = std::min( lowest, it->second );

        if( auto it = settings.find( setting ); it != settings.end() )
            *it->second = KiROUND( roundMillimetres( std::max( lowest, 0.0 ) ) * pcbIUScale.IU_PER_MM );
    }
}
