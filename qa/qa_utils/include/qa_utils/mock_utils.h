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

#pragma once

#include <turtle/mock.hpp>

/**
 * Like MOCK_METHOD and MOCK_CONST_METHOD, but declare the mocked method with 'override' so that
 * the compiler checks it against the base class.
 *
 * Unlike MOCK_METHOD, only the one given constness is generated, and the signature is required.
 */
#define KI_MOCK_METHOD_OVERRIDE( M, n, S )                                                         \
    static_assert( n == mock::detail::function_arity_t<S>::value, "Arity mismatch" );              \
    MOCK_DECL( M, n, S, ) override                                                                 \
    {                                                                                              \
        return MOCK_ANONYMOUS_HELPER( M )( MOCK_FORWARD_PARAMS( n, S ) );                          \
    }                                                                                              \
    MOCK_METHOD_HELPER( S, M )

#define KI_MOCK_CONST_METHOD_OVERRIDE( M, n, S )                                                   \
    static_assert( n == mock::detail::function_arity_t<S>::value, "Arity mismatch" );              \
    MOCK_DECL( M, n, S, const ) override                                                           \
    {                                                                                              \
        return MOCK_ANONYMOUS_HELPER( M )( MOCK_FORWARD_PARAMS( n, S ) );                          \
    }                                                                                              \
    MOCK_METHOD_HELPER( S, M )


namespace KI_TEST
{
void SetMockConfigDir();
} // namespace KI_TEST
