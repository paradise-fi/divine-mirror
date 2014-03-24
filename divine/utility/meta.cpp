// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>
#include <divine/utility/meta.h>

using namespace divine;

namespace divine {
using Rep = std::vector< ReportLine >;

namespace meta {

template< typename Trace >
std::string showTrace( Trace ntrace ) {
    if ( ntrace.size() == 1 && ntrace.front() == -1 )
        return "-";
    if ( ntrace.empty() )
        return "";

    std::stringstream ss;

    for ( auto next : ntrace )
        ss << next << ",";

    auto str = ss.str();
    // drop trailing comma
    return std::string( str, 0, str.length() - 1 );
}

std::string tostr( Result::R v ) {
    return v == Result::R::Unknown
        ? "Unknown"
        : (v == Result::R::Yes ? "Yes" : "No");
}

std::string tostr( Result::CET t ) {
    switch (t) {
        case Result::CET::NoCE: return "none";
        case Result::CET::Goal: return "goal";
        case Result::CET::Cycle: return "cycle";
        case Result::CET::Deadlock: return "deadlock";
        default: assert_unreachable( "unknown CE type" );
    }
}

std::string tostr( graph::PropertyType t ) {
    switch (t) {
        case graph::PT_Deadlock: return "deadlock";
        case graph::PT_Goal: return "goal";
        case graph::PT_Buchi: return "neverclaim";
        default: assert_unreachable( "unknown property" );
    }
}

std::string tostr( Algorithm::Compression c ) {
    switch ( c ) {
        case Algorithm::Compression::None: return "None";
        case Algorithm::Compression::Tree: return "Tree";
        default: assert_unreachable( "unknown compression" );
    }
}

std::string tostr( bool b ) {
    return b ? "Yes" : "No";
}

template< typename T >
std::string tostr( T t ) {
    return std::to_string( t );
}

Rep Input::report() const {
    return { { "Model", model },
             { "Model-Type", modelType },
             { "Property-Type", tostr( propertyType ) },
             { "Properties", properties.empty() ? "-" : wibble::str::fmt( properties ) },
             { "Property", propertyDetails.empty() ? "-" : propertyDetails }
           };
}

Rep Result::report() const {
    return { { "Property-Holds", tostr( propertyHolds ) },
             { "Full-State-Space", tostr( fullyExplored ) },
             { "CE-Type", tostr( ceType ) },
             { "CE-Init", showTrace( iniTrail ) },
             { "CE-Cycle",showTrace( cycleTrail ) }
           };
}

Rep Output::report() const {
    return { };
}

Rep Statistics::report() const {
    return { { "States-Visited", tostr( visited ) },
             { "States-Accepting", tostr( accepting ) },
             { "States-Expanded", tostr( expanded ) },
             { "Transition-Count", transitions >= 0 ? tostr( transitions ) : "-" },
             { "Deadlock-Count", deadlocks >= 0 ? tostr( deadlocks ) : "-" }
           };
}

Rep Algorithm::report() const {
    std::vector< std::string > txt;

    for ( auto r = reduce.begin(); r != reduce.end(); ++r )
        switch ( *r ) {
            case graph::R_POR: txt.push_back( "POR" ); break;
            case graph::R_TauPlus: txt.push_back( "tau+" ); break;
            case graph::R_TauStores: txt.push_back( "taustores" ); break;
            case graph::R_Tau: txt.push_back( "tau" ); break;
            case graph::R_Heap: txt.push_back( "heap" ); break;
            case graph::R_LU: txt.push_back( "LU" ); break;
        }

    if ( fairness )
        txt.push_back( "fairness" );

    return { { "Algorithm", name },
             { "Transformations", wibble::str::fmt( txt ) },
             { "Compression", tostr( compression ) },
             { "Shared", tostr( sharedVisitor ) }
           };
}

Rep Execution::report() const {
    return { { "Threads", tostr( threads ) },
             { "MPI-Nodes", tostr( nodes ) }
           };
}
}

Rep Meta::report() const {
    ReportLine empty = { "", "" };
    return WithReport::merge( input, empty, algorithm, empty, execution,
            empty, result, empty, statistics );
}

}
