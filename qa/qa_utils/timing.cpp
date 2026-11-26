/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <qa_utils/timing.h>

#if defined( _WIN32 )
#include <windows.h>

#if defined( _M_X64 ) || defined( _M_IX86 )
#include <intrin.h>
#define QA_TIMING_PROCESS_CYCLES
#endif
#else
#include <time.h>
#endif


#ifdef QA_TIMING_PROCESS_CYCLES
/**
 * The rate of the time-stamp counter, which QueryProcessCycleTime() counts in, measured once against the
 * performance counter.
 *
 * Both clocks keep running while this thread sleeps, so a busy machine cannot bias the ratio.
 */
static double timeStampCounterHz()
{
    static const double hz = []()
    {
        LARGE_INTEGER frequency, start, end;
        QueryPerformanceFrequency( &frequency );

        QueryPerformanceCounter( &start );
        const unsigned long long tscStart = __rdtsc();
        Sleep( 20 );
        QueryPerformanceCounter( &end );
        const unsigned long long tscEnd = __rdtsc();

        return static_cast<double>( tscEnd - tscStart ) * static_cast<double>( frequency.QuadPart )
               / static_cast<double>( end.QuadPart - start.QuadPart );
    }();

    return hz;
}
#endif


std::chrono::nanoseconds KI_TEST::ProcessCpuTime()
{
#if defined( QA_TIMING_PROCESS_CYCLES )
    // GetProcessTimes() only advances on the 15.6 ms scheduler tick, far too coarse for the bounds, but the cycle
    // count of every thread in the process is exact
    ULONG64 cycles = 0;
    QueryProcessCycleTime( GetCurrentProcess(), &cycles );

    return std::chrono::nanoseconds(
            static_cast<long long>( static_cast<double>( cycles ) * 1e9 / timeStampCounterHz() ) );
#elif defined( _WIN32 )
    // Without a time-stamp counter to convert the cycle count, fall back to wall time
    return std::chrono::steady_clock::now().time_since_epoch();
#else
    timespec ts;
    clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &ts );

    return std::chrono::seconds( ts.tv_sec ) + std::chrono::nanoseconds( ts.tv_nsec );
#endif
}
