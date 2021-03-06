// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2015 Jiří Weiser
 * (c) 2018 Petr Ročkai <code@fixp.eu>
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

#pragma once
#include <string_view>
#include <memory>
#include <sys/types.h>

#include <dios/sys/kobject.hpp>
#include <dios/vfs/flags.hpp>

namespace __dios::fs
{

/* A file mode and type saved in the INode. Please note that S_* are now
   integral macros defined in sys/stat.h (through #include
   <dios/vfs/flags.hpp>), but they will be later replaced by corresponding
   instances of this struct. */
struct Mode : FlagOps< Mode, uint16_t >
{
    using FlagOps< Mode, uint16_t >::FlagOps;

    bool is_socket() const { return _is( S_IFSOCK ); }
    bool is_link()   const { return _is( S_IFLNK ); }
    bool is_file()   const { return _is( S_IFREG ); }
    bool is_block()  const { return _is( S_IFBLK ); }
    bool is_char()   const { return _is( S_IFCHR ); }
    bool is_dir()    const { return _is( S_IFDIR ); }
    bool is_fifo ()  const { return _is( S_IFIFO ); }

    bool has_suid()   const { return _has( S_ISUID ); }
    bool has_sgid()   const { return _has( S_ISGID ); }
    bool has_sticky() const { return _has( S_ISVTX ); }

    bool user_read()  const { return _has( S_IRUSR ); }
    bool user_write() const { return _has( S_IWUSR ); }
    bool user_exec()  const { return _has( S_IXUSR ); }

    bool group_read()  const { return _has( S_IRGRP ); }
    bool group_write() const { return _has( S_IWGRP ); }
    bool group_exec()  const { return _has( S_IXGRP ); }

    bool other_read()  const { return _has( S_IROTH ); }
    bool other_write() const { return _has( S_IWOTH ); }
    bool other_exec()  const { return _has( S_IXOTH ); }

private:
    bool _is( unsigned p )  const { return _check( p, S_IFMT ); }
    bool _has( unsigned p ) const { return _check( p, p ); }
    bool _check( unsigned p, unsigned mask ) const
    {
        return ( this->to_i() & mask ) == p;
    }
};

#undef S_ISUID
#undef S_ISGID

#undef S_IRWXU
#undef S_IRUSR
#undef S_IWUSR
#undef S_IXUSR

#undef S_IRWXG
#undef S_IRGRP
#undef S_IWGRP
#undef S_IXGRP

#undef S_IRWXO
#undef S_IROTH
#undef S_IWOTH
#undef S_IXOTH

#undef S_IFMT
#undef S_IFIFO
#undef S_IFCHR
#undef S_IFDIR
#undef S_IFBLK
#undef S_IFREG
#undef S_IFLNK
#undef S_IFSOCK
#undef S_ISVTX

#undef ACCESSPERMS
#undef ALLPERMS

constexpr Mode S_ISUID = _HOST_S_ISUID;
constexpr Mode S_ISGID = _HOST_S_ISGID;

constexpr Mode S_IRWXU = _HOST_S_IRWXU;
constexpr Mode S_IRUSR = _HOST_S_IRUSR;
constexpr Mode S_IWUSR = _HOST_S_IWUSR;
constexpr Mode S_IXUSR = _HOST_S_IXUSR;

constexpr Mode S_IRWXG = _HOST_S_IRWXG;
constexpr Mode S_IRGRP = _HOST_S_IRGRP;
constexpr Mode S_IWGRP = _HOST_S_IWGRP;
constexpr Mode S_IXGRP = _HOST_S_IXGRP;

constexpr Mode S_IRWXO = _HOST_S_IRWXO;
constexpr Mode S_IROTH = _HOST_S_IROTH;
constexpr Mode S_IWOTH = _HOST_S_IWOTH;
constexpr Mode S_IXOTH = _HOST_S_IXOTH;

constexpr Mode S_IFMT   = _HOST_S_IFMT;
constexpr Mode S_IFIFO  = _HOST_S_IFIFO;
constexpr Mode S_IFCHR  = _HOST_S_IFCHR;
constexpr Mode S_IFDIR  = _HOST_S_IFDIR;
constexpr Mode S_IFBLK  = _HOST_S_IFBLK;
constexpr Mode S_IFREG  = _HOST_S_IFREG;
constexpr Mode S_IFLNK  = _HOST_S_IFLNK;
constexpr Mode S_IFSOCK = _HOST_S_IFSOCK;
constexpr Mode S_ISVTX  = _HOST_S_ISVTX;

constexpr Mode ACCESSPERMS = S_IRWXU | S_IRWXG | S_IRWXO;
constexpr Mode ALLPERMS = ACCESSPERMS | S_ISUID | S_ISGID | S_ISVTX;

struct FileDescriptor;
struct INode;

using Node = INode *;

/* An object in the virtual file system. This is a base type for all types of
   filesystem objects, each of them can override their operations and own its
   data. INode itself defines operations for many file types, but mostly in a
   way which reports an error.
   See file.hpp for these objects.
   An INode contains a refference count and self-destructs if it reaches zero.
   For this reason, an INode must be always allocated dynamically. */
struct INode : KObject
{
    INode() :
        _mode( 0 ),
        _ino( getIno() ),
        _uid( 0 ),
        _gid( 0 ),
        _nlink( 0 ),
        _nopen( 0 )
    {}

    virtual ~INode() {}

    template< typename T >
    T *as() {
        return static_cast< T * >( this );
    }

    template< typename T >
    const T *as() const {
        return static_cast< const T * >( this );
    }

    virtual size_t size() const { return 0; }
    virtual bool read( char *, size_t, size_t & ) { return false; }
    virtual bool write( const char *, size_t, size_t &, Node ) { return false; }
    virtual bool canRead() const { return false; }
    virtual bool canWrite( int, Node ) const { return false; }

    void open() { ++ _nopen; }
    void close() { -- _nopen; checkref(); }

    virtual void open( FileDescriptor & ) { open(); }
    virtual void close( FileDescriptor & ) { close(); }

    virtual bool listen( int ) { return error( ENOTSOCK ), false; }
    virtual bool connect( Node ) { return error( ENOTSOCK ), false; }
    virtual bool bind( std::string_view ) { return error( ENOTSOCK ), false; }
    virtual Node receive( char *, size_t &, MFlags ) { return error( ENOTSOCK ), nullptr; }
    virtual Node accept() { return error( ENOTSOCK ), nullptr; }
    virtual Node peer() const { return error( ENOTSOCK ), nullptr; }
    virtual std::string_view address() { return ""; }

    Mode mode() const { return _mode; }
    void mode( Mode mode ) { _mode = mode; }

    unsigned ino() const { return _ino; }
    unsigned uid() const { return _uid; }
    unsigned gid() const { return _gid; }

    void link() { ++ _nlink; }
    void unlink() { -- _nlink; checkref(); }

    void checkref()
    {
        if ( !_nlink && !_nopen )
            delete this;
    }

    void stat( struct stat &buf )
    {
        buf.st_dev = 0;
        buf.st_rdev = 0;
        buf.st_atime = 0;
        buf.st_mtime = 0;
        buf.st_ctime = 0;
        buf.st_ino = ino();
        buf.st_mode = mode().to_i();
        buf.st_nlink = _nlink;
        buf.st_size = size();
        buf.st_uid = uid();
        buf.st_gid = gid();
        buf.st_blksize = 512;
        buf.st_blocks = ( buf.st_size + 1 ) / buf.st_blksize;
    }

    template< typename stat >
    void set_stat( const stat &s )
    {
        _ino  = s.st_ino;
        _mode = s.st_mode;
        _uid  = s.st_uid;
        _gid  = s.st_gid;
    }

private:
    Mode _mode;
    unsigned _ino;
    uint16_t _uid;
    uint16_t _gid;
    uint16_t _nlink;
    uint16_t _nopen;

    static unsigned getIno()
    {
        static unsigned ino = 0;
        return ++ino;
    }
};

}
