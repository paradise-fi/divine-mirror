// -*- C++ -*-
#include <iostream>
#include <sstream>
#include <divine/version.h>
#include <wibble/commandline/parser.h>
#include <wibble/string.h>
#include <wibble/sys/fs.h>
#include <wibble/regexp.h>
#include <wibble/sys/pipe.h>
#include <wibble/sys/exec.h>

#include <divine/ltl2ba/ltl.hh>
#include <divine/ltl2ba/alt_graph.hh>
#include <divine/ltl2ba/formul.hh>
#include <divine/ltl2ba/support_dve.hh>
#include <divine/ltl2ba/DBA_graph.hh>

#include "compile.h"

#ifndef DIVINE_COMBINE_H
#define DIVINE_COMBINE_H

extern const char *combine_m4;

int ltl2buchi( int argc, char **argv );
#ifdef LTL2DSTAR
int main_ltl2dstar(int argc, const char **argv, std::istream& in, std::ostream& out);
#endif

using namespace wibble;
using namespace commandline;
using namespace sys;
using namespace divine;

// paste from ltl2ba.cc
static void copy_graph(const ALT_graph_t& aG, BA_opt_graph_t& oG)
{
	std::list<ALT_ba_node_t*> node_list, init_nodes, accept_nodes;
	std::list<ALT_ba_node_t*>::const_iterator n_b, n_e, n_i;
	std::list<ALT_ba_trans_t>::const_iterator t_b, t_e, t_i;
	KS_BA_node_t *p_N;

	node_list = aG.get_all_BA_nodes();
	init_nodes = aG.get_init_BA_nodes();
	accept_nodes = aG.get_accept_BA_nodes();

	oG.clear();

	n_b = node_list.begin(); n_e = node_list.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		p_N = oG.add_node((*n_i)->name);
		t_b = (*n_i)->adj.begin(); t_e = (*n_i)->adj.end();
		for (t_i = t_b; t_i != t_e; t_i++) {
			oG.add_trans(p_N, t_i->target->name,
				t_i->t_label);
		}
	}

	n_b = init_nodes.begin(); n_e = init_nodes.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		oG.set_initial((*n_i)->name);
	}

	n_b = accept_nodes.begin(); n_e = accept_nodes.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		oG.set_accept((*n_i)->name);
	}
}

std::string graph_to_cpp(const BA_opt_graph_t &g);

struct Combine {
    Engine *cmd_combine;
    IntOption *o_propId;
    BoolOption *o_stdout, *o_quiet, *o_help, *o_version, *o_det;
    StringOption *o_formula;
#ifdef LTL2DSTAR
    StringOption *o_condition;
#endif
    commandline::StandardParserWithMandatoryCommand &opts;

    bool have_ltl;
    bool probabilistic;
    bool compiled;

    std::string input, ltl, defs;
    std::string in_data, ltl_data, ltl_defs, system;
    std::string ext;
    // std::vector< std::string > defs;
    std::vector< std::string > ltl_formulae;

    void die_help( std::string bla )
    {
        opts.outputHelp( std::cerr );
        die( bla );
    }

    void die( std::string bla )
    {
        std::cerr << bla << std::endl;
        exit( 1 );
    }

    Combine( commandline::StandardParserWithMandatoryCommand &_opts,
             int argc, const char **argv )
        : opts( _opts )
    {
        cmd_combine = opts.addEngine(
            "combine",
            "input.[m][prob]dve [preprocessor defs]",
            "combine DVE models with LTL properties" );

        o_formula = cmd_combine->add< StringOption >(
            "formula", 'f', "formula", "",
            "a file with LTL properties" );
        o_propId = cmd_combine->add< IntOption >(
            "property", 'p', "property", "",
            "only process n-th property from the LTL file" );
        o_stdout = cmd_combine->add< BoolOption >(
            "stdout", 'o', "stdout", "",
            "print output to stdout; implies -q" );
        o_quiet = cmd_combine->add< BoolOption >(
            "quiet", 'q', "quiet", "",
            "suppress normal output" );
#ifdef LTL2DSTAR
        o_condition = cmd_combine->add< StringOption >(
            "condition", 'c', "condition", "",
            "acceptance condition type: NBA | DRA (default: NBA)" );
#endif
    }

    std::string m4( std::string in )
    {
        PipeThrough p( "m4" + defs );
        return p.run( in );
    }

    std::string cpp( std::string in )
    {
        PipeThrough p( "cpp -E -P" );
        return p.run( in );
    }

    std::string outFile( int i, std::string extension = "" )
    {
        if ( extension.empty() )
            extension = ext;

        std::ostringstream str;
        int a = input.rfind( '.' );
        if ( a == std::string::npos )
            throw wibble::exception::Generic( "Bla." );
        std::string base( input, 0, a );
        str << base;
        if ( i != -1 ) {
            str << ".prop";
            str << i;
        }
        str << extension;
        return str.str();
    }

    BA_opt_graph_t buchi( std::string ltl )
    {
        std::ostringstream str;
        LTL_parse_t L;
        LTL_formul_t F;
        ALT_graph_t G;
        BA_opt_graph_t oG, oG1;
        L.nacti( ltl );
        if ( !L.syntax_check( F ) )
            std::cerr << "Error: Syntax error in LTL formula." << std::endl;
        F = F.negace();
        G.create_graph( F );
        G.transform_vwaa();
        G.simplify_GBA();
        G.degeneralize();
        G.simplify_BA();
        copy_graph(G, oG);
        oG.to_one_initial();
        oG.optimize(oG1, 6);

        if ( oG1.is_empty() )
        {
            // XXX warning?
            oG1.clear();
            oG1.set_initial( 1 );
        }

        if ( probabilistic && !oG1.is_semideterministic() ) {
            DBA_graph_t DG;
            DG.semideterminization( oG1 );
            oG1 = DG.transform();
        }

        return oG1;
    }

#ifdef LTL2DSTAR
    /// Translates ltl formula to automaton specified by acceptanceConditionType using external translator
    std::string ltl2dstarTranslation( const std::string& acceptanceConditionType, std::string ltl ) {
        // ltl2dstar expects the ltl formula to be in the prefix form
        LTL_parse_t ltlParse( ltl );
        LTL_formul_t ltlFormula;
        if ( !ltlParse.syntax_check( ltlFormula ) ) {
            std::cerr << "Syntax error in LTL formula: " << ltl << endl;
            return "";
        }
        ltlFormula = ltlFormula.negace();
        std::string ltlInPrefixNotation = ltlFormula.prefixNotation();

        const std::string ac("--automata=" + acceptanceConditionType);
        const char *argv[] = { "ltl2dstar", "--ltl2nba=spinint:ltl2ba", ac.c_str(), 
          "--output=plugin:DVE_Process", "--plugin=DVE_Process", "-", "-" };
        std::stringstream ltlStream, automatonStream;
        ltlStream << ltlInPrefixNotation;
        main_ltl2dstar( 7, argv, ltlStream, automatonStream ); 
        return automatonStream.str();
    }
#endif

    void announce( int id, std::string descr ) {
        if ( !o_quiet->boolValue() && !o_stdout->boolValue() )
            std::cerr << outFile( id ) << ": " << descr << std::endl;
    }

    void output( int id, std::string data, std::string prop_descr ) {
        announce( id, prop_descr );
        if ( !o_stdout->boolValue() )
            fs::writeFile( outFile( id ), data );
        else
            std::cout << data;
    }

    template< typename F >
    void process_ltl( F each ) {
        wibble::Splitter lines( "\n", 0 );
        wibble::ERegexp prop( "^[ \t]*#property ([^\n]+)", 2 );
        wibble::ERegexp def( "^[ \t]*#define ([^\n]+)", 2 );

        std::vector< std::string >::iterator i;
        for ( Splitter::const_iterator ln = lines.begin( ltl_data ); ln != lines.end(); ++ln ) {
            if ( prop.match( *ln ) )
                ltl_formulae.push_back( prop[1] );
            if ( def.match( *ln ) )
                ltl_defs += "\n#define " + def[1];
        }

        int id = 1;
        for ( i = ltl_formulae.begin(); i != ltl_formulae.end(); ++i, ++id ) {
            if ( o_propId->intValue() && o_propId->intValue() != id )
                continue;

            (this->*each)( id, *i );
        }
    }

    void ltl_to_dve( int id, std::string ltl ) {
        std::string automaton;
        std::stringstream automaton_str;
#ifdef LTL2DSTAR
        if ( !o_condition->boolValue() || o_condition->stringValue() == "NBA" )
#endif
        {
            divine::output( buchi( ltl ), automaton_str );
            automaton = automaton_str.str();
        }
#ifdef LTL2DSTAR
        // this can also handle NBA, Streett, etc.
        else if ( o_condition->stringValue() == "DRA" ) {
            automaton = ltl2dstarTranslation( "rabin", ltl );

            // something bad happened
            if (automaton.empty())
                return;
        }
#endif
        std::string prop = cpp( ltl_defs + "\n" + automaton );
        std::string dve = cpp( in_data + "\n" + prop + "\n" + system );

        output( id, dve, ltl );
    }

    void ltl_to_cpp( int id, std::string ltl ) {
        announce( id, ltl );
        std::string filename = outFile( id, ".cpp" );
        std::ofstream f( filename.c_str() );
        f << "#include <malloc.h>" << std::endl;
        f << "#include <algorithm>" << std::endl;
        f << divine::compile_defines_str << std::endl;
        f << divine::generator_custom_api_h_str << std::endl;
        f << divine::blob_h_str << std::endl;
        f << "using namespace divine;" << std::endl;
        f << graph_to_cpp( buchi( ltl ) );
        f.close();
        Compile::gplusplus( filename + " " + input, outFile( id ) );
    }

    void parseOptions() {
        try {
            if ( !opts.hasNext() )
                die_help( "FATAL: No input file specified." );
            input = opts.next();

            have_ltl = o_formula->boolValue();
            if ( have_ltl )
                ltl = o_formula->stringValue();

            while ( opts.hasNext() )
                defs += " -D" + opts.next(); // .push_back( opts.next() );

        } catch( wibble::exception::BadOption &e ) {
            die_help( e.fullInfo() );
        }

        if ( have_ltl && o_stdout->boolValue() && !o_propId->intValue() )
            die( "FATAL: cannot print to stdout more than single property. Use -p n." );
    }

    int main() {
        parseOptions();

        compiled = str::endsWith( input, ".o" );
        probabilistic = str::endsWith( input, "probdve" );
        if ( compiled )
            ext = ".so";
        else if ( probabilistic )
            ext = ".probdve";
        else if ( str::endsWith( input, "dve" ) )
            ext = ".dve";
        else
            die( "FATAL: Input file extension has to be one of "
                 ".o, .[m]dve or .[m]probdve." );

        if ( !fs::access( input, R_OK ) )
            die( "FATAL: Can't open '" + input + "' for reading." );

        if ( !compiled )
            in_data = fs::readFile( input ) + "\n";

        if ( have_ltl ) {
            if ( !fs::access( ltl, R_OK ) )
                die( "FATAL: Can't open '" + ltl + "' for reading." );
            ltl_data = fs::readFile( ltl ) + "\n";
        }

        if ( str::endsWith( ltl, ".mltl" ) ) {
            ltl_data = m4( combine_m4 + ltl_data );
        }

        if ( str::endsWith( input, ".mprobdve" )
             || str::endsWith( input, ".mdve" ) ) {
            in_data = m4( combine_m4 + in_data );
            if ( !have_ltl ) {
                output( -1, in_data, "(no property)" );
                return 0;
            }
        } else {
           if ( !have_ltl )
               die( "FATAL: Nothing to do." );
        }

        if ( compiled )
            combine_compiled();
        else
            combine_dve();

        return 0;
    }

    void combine_compiled() {
        process_ltl( &Combine::ltl_to_cpp );
    }

    void combine_dve() {
        int off = in_data.find( "system async" );
        if ( off != std::string::npos )
            system = "system async property LTL_property;\n";
        else {
            off = in_data.find( "system sync" );
            if ( off != std::string::npos ) {
                system = "system sync property LTL_property;\n";
                std::cerr << "Warning: LTL model checking of synchronous "
                             "systems is not supported yet." << std::endl;
            } else {
                std::cerr << "Failing preprocessed source: " << std::endl;
                std::cerr << in_data;
                die( "FATAL: DVE has to specify either 'system sync' or "
                     "'system async'." );
            }
        }
        in_data = std::string( in_data, 0, off );

        process_ltl( &Combine::ltl_to_dve );
    }

};

#endif
