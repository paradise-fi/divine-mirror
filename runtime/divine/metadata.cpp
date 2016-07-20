#include <cstring>
#include <divine/metadata.h>
#include <limits>

extern const _MD_Function __md_functions[];
extern const int __md_functions_count;

const _MD_Function *__md_get_function_meta( const char *name ) {
    long from = 0;
    long to = __md_functions_count;
    while ( from < to ) {
        long half = (from + to) / 2;
        int c = std::strcmp( name, __md_functions[ half ].name );
        if ( c < 0 ) // name < half
            to = half;
        else if ( c > 0 ) // name > half
            from = half + 1;
        else
            return __md_functions + half;
    }
    return nullptr;
}

const _MD_Function *__md_get_pc_meta( uintptr_t _pc ) {
    uintptr_t pc = uintptr_t(_pc );
    uintptr_t dist = std::numeric_limits< intptr_t >::max();
    const _MD_Function *it = __md_functions,
                       *ptr = nullptr;
    for ( int i = 0; i < __md_functions_count; ++i, ++it ) {
        if ( uintptr_t( it->entry_point ) <= pc ) {
            uintptr_t d = pc - uintptr_t( it->entry_point );
            if ( dist > d ) {
                dist = d;
                ptr = it;
            }
        }
    }
    return ptr;
}

_MD_RegInfo __md_get_register_info( _VM_Frame *frame, uintptr_t pc, _MD_Function *funMeta )
{
    if ( !frame || !funMeta || !pc )
        return { nullptr, 0 };
    uintptr_t entry = uintptr_t( funMeta->entry_point );
    intptr_t offset = pc - entry;
    if ( offset < 0 || offset > funMeta->inst_table_size )
        return { nullptr, 0 };

    char *base = reinterpret_cast< char * >( frame + 1 ); // skip frame header
    auto &imeta = funMeta->inst_table[ offset ];
    return { base + imeta.val_offset, imeta.val_width };
}
