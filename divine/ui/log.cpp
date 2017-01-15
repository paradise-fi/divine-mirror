// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017 Petr Ročkai <code@fixp.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <divine/ui/log.hpp>
#include <divine/ui/version.hpp>
#include <divine/ui/sysinfo.hpp>

namespace divine::ui
{

struct CompositeSink : LogSink
{
    std::vector< SinkPtr > _slaves;

    template< typename L >
    void each( L l )
    {
        for ( auto sink: _slaves )
            l( sink );
    }

    void progress( int x, int y, bool l ) override
    { each( [&]( auto s ) { s->progress( x, y, l ); } ); }

    void memory( const mc::Job::PoolStats &st, bool l ) override
    { each( [&]( auto s ) { s->memory( st, l ); } ); }

    void info( std::string i ) override
    { each( [&]( auto s ) { s->info( i ); } ); }

    void result( bool res, const mc::Trace &tr ) override
    { each( [&]( auto s ) { s->result( res, tr ); } ); }

    void start() override
    { each( [&]( auto s ) { s->start(); } ); }
};


struct TimedSink : LogSink
{
    using Clock = std::chrono::steady_clock;
    using MSecs = std::chrono::milliseconds;

    Clock::time_point _start;
    MSecs _interval;

    std::string interval_str()
    {
        std::stringstream t;
        t << int( _interval.count() / 60000 ) << ":"
          << std::setw( 2 ) << std::setfill( '0' ) << int( _interval.count() / 1000 ) % 60;
        return t.str();
    }

    double timeavg( double val )
    {
        return 1000 * val / _interval.count();
    }

    std::string timeavg_str( double val )
    {
        std::stringstream s;
        s << std::fixed << std::setprecision( 1 ) << timeavg( val );
        return s.str();
    }

    void update_interval()
    {
        _interval = std::chrono::duration_cast< MSecs >( Clock::now() - _start );
    }

    void start() override { _start = Clock::now(); }
};

template< typename S >
std::string printitem( S s )
{
    std::stringstream str;
    str << "{ items: " << s.count.used << ", used: " << s.bytes.used
        << ", held: " << s.bytes.held << " }";
    return str.str();
}

void printpool( std::string name, const brick::mem::Stats &s )
{
    std::cout << name << ":" << std::endl;
    std::cout << "  total: " << printitem( s.total ) << std::endl;
    for ( auto i : s )
        if ( i.count.held )
            std::cout << "  " << i.size << ": " << printitem( i ) << std::endl;
}

/* format a yaml report */
struct YamlSink : TimedSink
{
    bool _detailed;
    mc::Job::PoolStats latest;
    SysInfo _sysinfo;

    YamlSink( bool detailed ) : _detailed( detailed ) {}

    void progress( int states, int, bool last ) override
    {
        if ( !last )
            return;

        update_interval();
        std::cout << "search time: " << std::setprecision( 3 )
                  << double( _interval.count() ) / 1000
                  << std::endl << "state count: " << states
                  << std::endl << "states per second: " << timeavg( states )
                  << std::endl << "version: " << version()
                  << std::endl << std::endl;
        _sysinfo.report( []( auto k, auto v )
                         { std::cout << k << ": " << v << std::endl; } );
    }

    void memory( const mc::Job::PoolStats &st, bool last ) override
    {
        if ( !last || !_detailed )
            return;
        for ( auto p : st )
            printpool( p.first, p.second );
    }

    void result( bool, const mc::Trace & ) override {}
};

/* print progress updates to stderr */
struct InteractiveSink : TimedSink
{
    virtual void progress( int states, int queued, bool last ) override
    {
        update_interval();
        if ( last )
            std::cerr << "\rfound " << states
                      << " states in " << interval_str()
                      << ", averaging " << timeavg_str( states )
                      << "                             " << std::endl << std::endl;
        else
            std::cerr << "\rsearching: " << states
                      << " states found in " << interval_str()
                      << ", averaging " << timeavg_str( states )
                      << ", queued: " << queued << "      ";
    }

    virtual void info( std::string ) override {}
    virtual void start() override { TimedSink::start(); }
};

struct NullSink : LogSink {};

SinkPtr nullsink()
{
    static SinkPtr global;
    if ( !global )
        global = std::make_shared< NullSink >();
    return global;
}

SinkPtr make_yaml( bool d ) { return std::make_shared< YamlSink >( d ); }
SinkPtr make_interactive() { return std::make_shared< InteractiveSink >(); }

SinkPtr make_composite( std::vector< SinkPtr > s )
{
    auto rv = std::make_shared< CompositeSink >();
    rv->_slaves = s;
    return rv;
}

}
