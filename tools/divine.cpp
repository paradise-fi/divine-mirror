// -*- C++ -*- (c) 2015, 2019 Petr Rockai <code@fixp.eu>

#include <divine/ui/cli.hpp>

int main( int argc, const char **argv )
{
    if ( argc > 1 && ( !strcmp( argv[ 1 ], "--help" ) || !strcmp( argv[ 1 ], "--version" ) ) )
            argv[ 1 ] += 2;

    auto ui = divine::ui::make_cli( argc, argv )->resolve();
    divine::ui::setup_death( ui );
    return ui->main();
}
