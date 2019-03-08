#include <rst/split.h>

namespace abstract::mstring {

    namespace {
        auto split( Segment & s, int idx ) -> std::pair< Segment, Segment >
        {
            return { Segment{ s.from, idx, s.value }, Segment{ idx + 1, s.to, s.value } };
        };

        void merge_neighbours( Section * sec, Segment * seg ) noexcept
        {
            auto next = seg->to;
            if ( next < sec->to() ) {
                auto * right = std::next( seg );
                if ( right->value == seg->value ) {
                    seg->to = right->to;
                    sec->erase( right );
                }
            }

            auto prev = seg->from - 1;
            if ( prev >= sec->from() ) {
                auto * left = std::prev( seg );
                if ( left->value == seg->value ) {
                    seg->from = left->from;
                    sec->erase( left );
                }
            }

        }

    } // anonymous namespace

    void Split::divide( Section * sec, Segment & seg, int idx ) noexcept
    {
        auto [left, right] = split( seg, idx );

        Section right_section;
        if ( !right.empty() )
            right_section.append( std::move( right ) );
        for ( auto rest = std::next( &seg ); rest != sec->segments().end(); ++rest )
            right_section.append( std::move( *rest ) );

        sec->erase( &seg, sec->segments().end() );
        if ( !left.empty() )
            sec->append( std::move( left ) );

        _sections.insert( std::next( sec ), right_section );
    }

    void Section::append( Segment && seg ) noexcept
    {
        _segments.emplace_back( std::move( seg ) );
        merge_neighbours( this, std::prev( _segments.end() ) );
    }

    void Section::prepend( Segment && seg ) noexcept
    {
        _segments.insert( _segments.begin(), std::move( seg ) );
        merge_neighbours( this, _segments.begin() );
    }

    Segment * Section::erase( Segment * seg ) noexcept
    {
        return _segments.erase( seg );
    }

    Segment * Section::erase( Segment * from, Segment * to ) noexcept
    {
        return _segments.erase( from, to );
    }

    const Section * Split::interest() const noexcept
    {
        assert( well_formed() );
        if ( empty() )
            return nullptr;

        auto * front = &_sections[ 0 ];
        if ( front->from() == 0 )
            return front;
        return nullptr;
    }

    bool Split::well_formed() const noexcept
    {
        return this->empty() || _sections.front().to() < _len;
    }

    void Split::strcpy( const Split * other ) noexcept
    {
        if ( this != other ) {
            //auto str = other->interest();
            // TODO check correct bounds
            //ASSERT( str.size() < _len && "copying mstring to smaller mstring" );
            // TODO copy segments
            _UNREACHABLE_F( "Not implemented." );
        }
    }

    void Split::strcat( const Split * /*other*/ ) noexcept
    {
        //int begin = interest().size();
        //int dist = other->interest().size() + 1;

        // TODO check correct bounds
        //ASSERT( _len >= begin + dist && "concating mstring into smaller mstring" );
        // TODO copy segments
        _UNREACHABLE_F( "Not implemented." );
    }

    Split * Split::strchr( char /*ch*/ ) const noexcept
    {
        /*auto str = interest();

        if ( !str.empty() ) {
            const auto &begin = str.segments().begin();
            const auto &end = str.segments().end();

            auto search = std::find_if( begin, end, [ch] ( const auto &seg ) {
                return seg.value() == ch;
            });

            if ( search != end ) {
                // TODO return subsplit
                // return __new< Split >( _VM_PT_Marked, const_cast< Split * >( this ), search->from() );
                _UNREACHABLE_F( "Not implemented." );
            } else {
                return nullptr;
            }
        } else {
            _UNREACHABLE_F("Error: there's no string of interest!");
        }*/
        _UNREACHABLE_F( "Not implemented." );
    }

    int Split::strlen() const noexcept
    {
        if ( empty() )
            return 0;
        if ( auto * str = interest() )
            return str->size();
        return 0;
    }

    int Split::strcmp( const Split * /*other*/ ) const noexcept
    {
        /*const auto & sq1 = interest();
        const auto & sq2 = other->interest();

        if ( !sq1.empty() && !sq2.empty() ) {
            for ( int i = 0; i < sq1.size(); i++ ) {
                const auto& sq1_seg = sq1.segment_of( i ); // TODO optimize segment_of
                const auto& sq2_seg = sq2.segment_of( i );

               // TODO optimize per segment comparison
                if ( sq1_seg.value() == sq2_seg.value() ) {
                    if ( sq1_seg.to() > sq2_seg.to() ) {
                        return sq1_seg.value() - sq2.segment_of( i + 1 ).value();
                    } else if (sq1_seg.to() < sq2_seg.to() ) {
                        return sq1.segment_of( i + 1 ).value() - sq2_seg.value();
                    }
                } else {
                    return sq1[ i ] - sq2[ i ];
                }
            }

            return 0;
        }

        _UNREACHABLE_F( "Error: there is no string of interest." );*/

        _UNREACHABLE_F( "Not implemented." );
    }

    Section * Split::section_of( int idx ) noexcept
    {
        for ( auto& sec : _sections ) {
            if ( idx >= sec.from() && idx < sec.to() )
                return &sec;
            if ( idx < sec.from() )
                return nullptr;
        }

        return nullptr;
    }

    const Section * Split::section_of( int idx ) const noexcept
    {
        for ( const auto& sec : _sections ) {
            if ( idx >= sec.from() && idx < sec.to() )
                return &sec;
            if ( idx < sec.from() )
                return nullptr;
        }

        return nullptr;
    }

    void Split::shrink_correction( Section * sec, Segment * bound ) noexcept
    {
        if ( sec->empty() ) {
            _sections.erase( sec );
        } else {
            if ( bound->empty() )
                sec->erase( bound );
        }
    }

    void Split::shrink_left( Section * sec, Segment * bound ) noexcept
    {
        bound->from++;
        shrink_correction( sec, bound );
    }

    void Split::shrink_right( Section * sec, Segment * bound ) noexcept
    {
        bound->to--;
        shrink_correction( sec, bound );
    }

    void Split::write_zero( int idx ) noexcept
    {
        auto sec = section_of( idx );
        if ( !sec )
            return; // position already contains zero

        auto & seg = sec->segment_of( idx );

        if ( idx == sec->from() ) {
            shrink_left( sec, &seg );
        } else if ( idx == sec->to() - 1 ) {
            shrink_right( sec, &seg );
        } else {
            divide( sec, seg, idx );
        }
    }

    void Split::overwrite_char( Section * sec, Segment & seg, int idx, char val ) noexcept
    {
        if ( seg.size() == 1 ) {
            seg.value = val;
            merge_neighbours( sec, &seg );
        } else {
            auto [left, right] = split( seg, idx );
            auto & segs = sec->segments();

            seg.from = idx;
            seg.to = idx + 1;
            seg.value = val;

            auto place = &seg;
            if ( !left.empty() ) {
                auto it = segs.insert( place, left );
                place = std::next( it );
            }

            if ( !right.empty() ) {
                auto it = segs.insert( std::next( place ), right );
                place = std::prev( it );
            }

            if ( left.empty() || right.empty() )
                merge_neighbours( sec, place );
        }
    }

    void Split::overwrite_zero( int idx, char val ) noexcept
    {
        auto left = section_of( idx - 1 );
        auto right = section_of( idx + 1 );

        Segment seg{ idx, idx + 1, val };

        if ( left && right ) { // merge sections
            auto & segs = left->segments();
            auto mid = segs.size();
            segs.push_back( seg );

            auto & rsegs = right->segments();
            // TODO use move
            segs.append( rsegs.size(), rsegs.begin(), rsegs.end() );
            merge_neighbours( left, &segs[ mid ] );

            _sections.erase( right );
        } else if ( left ) {
            left->append( std::move( seg ) );
        } else if ( right ) { // extend right section
            right->prepend( std::move( seg ) );
        } else { // create a new section
            Section new_section;
            new_section.append( std::move( seg ) );

            if ( _sections.empty() ) {
                _sections.push_back( new_section );
            } else {
                auto point = _sections.begin();
                while ( point->to() < idx ) ++point;
                _sections.insert( point, new_section );
            }
        }
    }

    void Split::write_char( int idx, char val ) noexcept
    {
        if ( auto sec = section_of( idx ) ) {
            auto & seg = sec->segment_of( idx );
            if ( seg.value == val )
                return;
            overwrite_char( sec, seg, idx, val );
        } else {
            overwrite_zero( idx, val );
        }
    }

    void Split::write( int idx, char val ) noexcept
    {
        assert( idx < _len );
        if ( val == '\0' )
            write_zero( idx );
        else
            write_char( idx, val );
    }


    char Split::read( int idx ) noexcept
    {
        assert( idx < _len );
        if ( auto sec = section_of( idx ) )
            return sec->segment_of( idx ).value;
        return '\0';
    }

    void split_release( Split * str ) noexcept
    {
        str->refcount_decrement();
    }

    void split_cleanup( Split * str ) noexcept
    {
        str->~Split();
        __dios_safe_free( str );
    }

    void split_cleanup_check( Split * str ) noexcept
    {
        cleanup_check( str, split_cleanup );
    }

} // namespace abstract::mstring
