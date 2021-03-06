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
#include <algorithm>
#include <signal.h>
#include <dios.h>
#include <sys/fault.h>
#include <sys/trace.h>

#include <dios/sys/memory.hpp>
#include <dios/sys/kernel.hpp> /* get_debug */

#include <dios/vfs/inode.hpp>
#include <dios/vfs/fd.hpp>
#include <dios/vfs/stream.hpp>

namespace __dios::fs
{

struct SymLink : INode
{
    SymLink( std::string_view t = "" ) { target( t ); }

    size_t size() const override { return _target.size(); }
    std::string_view target() const { return { _target.begin(), _target.size() }; }
    void target( std::string_view s ) { _target.assign( s.size(), s.begin(), s.end() ); }

private:
    Array< char > _target;
};

struct RegularFile : INode
{
    RegularFile() = default;

    RegularFile( const RegularFile &other ) = default;
    RegularFile( RegularFile &&other ) = default;
    RegularFile &operator=( RegularFile ) = delete;

    size_t size() const override { return _content.size(); }
    bool canRead() const override { return true; }
    bool canWrite( int, Node ) const override { return true; }

    bool read( char *buffer, size_t offset, size_t &length ) override
    {
        if ( offset >= size() )
        {
            length = 0;
            return true;
        }

        auto begin = _content.begin() + offset;
        if ( offset + length > size() )
            length = size() - offset;
        std::copy( begin, begin + length, buffer );
        return true;
    }

    bool write( const char *buffer, size_t offset, size_t &length, Node ) override
    {
        if ( _content.size() < offset + length )
            resize( offset + length );

        std::copy( buffer, buffer + length, _content.begin() + offset );
        return true;
    }

    void resize( size_t length )
    {
        _content.resize( length );
    }

    void content( std::string_view s )
    {
        resize( s.size() );
        std::copy( s.begin(), s.end(), _content.begin() );
    }

private:
    Array< char > _content;
};

/* Each write is propagated to the trace/counterexample. */
struct VmTraceFile : INode
{
    bool canWrite( int, Node ) const override { return true; }

    __debugfn void do_write( const char *buffer, size_t &length ) noexcept
    {
        if ( buffer[ length - 1 ] == 0 )
            __dios_trace_internal( 0, "%s", buffer );
        else
        {
            char buf[ length + 1 ];
            std::copy( buffer, buffer + length, buf );
            buf[ length ] = 0;
            __dios_trace_internal( 0, "%s", buf );
        }
    }

    bool write( const char *buffer, size_t, size_t &length, Node ) override
    {
        do_write( buffer, length );
        return true;
    }
};

/* Each written line is propagated to the trace/counterexample. */
struct VmBuffTraceFile : INode
{
    bool canWrite( int, Node ) const override { return true; }

    __debugfn void do_write( const char *data, size_t &length ) noexcept
    {
        auto &buf = get_debug().trace_buf[ __dios_this_task() ];
        buf.append( length, data, data + length );
        void *mem = buf.begin();
        auto nl = std::find( buf.rbegin(), buf.rend(), '\n' );
        if ( nl != buf.rend() )
        {
            __dios_trace_internal( 0, "%.*s", int( buf.rend() - nl - 1 ), buf.begin() );
            buf.erase( buf.begin(), nl.base() );
        }
        get_debug().persist();
        get_debug().persist_buffers();
        __vm_trace( _VM_T_DebugPersist, mem );
    }

    __debugfn void do_flush() noexcept
    {
        for ( auto &b : get_debug().trace_buf )
        {
            if ( !b.second.empty() )
                __dios_trace_internal( 0, "%.*s", b.second.size(), b.second.begin() );
            b.second.clear();
        }
        get_debug().persist();
        get_debug().persist_buffers();
    }

    bool write( const char *data, size_t, size_t & length, Node ) override
    {
        do_write( data, length );
        return true;
    }

    ~VmBuffTraceFile() { do_flush(); }
};

/* For simulation of stdin from a file (passed to the verifier) */
struct StandardInput : INode
{
    StandardInput() :
        _content( nullptr ),
        _size( 0 )
    {}

    StandardInput( const char *content, size_t size ) :
        _content( content ),
        _size( size )
    {}

    size_t size() const override { return _size; }

    bool canRead() const override
    {
        // simulate user drinking coffee
        if ( _size )
            return __vm_choose( 2 ) == 0;
        return false;
    }

    bool read( char *buffer, size_t offset, size_t &length ) override
    {
        if ( offset >= _size ) {
            length = 0;
            return true;
        }
        const char *source = _content + offset;
        if ( offset + length > _size )
            length = _size - offset;
        std::copy( source, source + length, buffer );
        return true;
    }

private:
    const char *_content;
    size_t _size;
};

struct Pipe : INode
{
    Pipe() :
        _stream( PIPE_SIZE_LIMIT ),
        _reader( false ),
        _writer( false )
    {}

    Pipe( bool r, bool w ) :
        _stream( PIPE_SIZE_LIMIT ),
        _reader( r ),
        _writer( w )
    {}

    size_t size() const override { return _stream.size(); }
    bool canRead() const override { return size() > 0; }
    bool canWrite( int, Node ) const override { return size() < PIPE_SIZE_LIMIT; }

    void open( FileDescriptor &fd ) override
    {
        __dios_assert( fd.inode() == this );

        if ( fd.flags().read() && fd.flags().write() )
            __dios_fault( _VM_Fault::_VM_F_Assert, "Pipe is opened both for reading and writing" );

        if ( fd.flags().read() )
        {
            if ( !fd.flags().has( O_NONBLOCK ) && !writer() )
               __vm_cancel();
            assignReader();
        }

        if ( fd.flags().write() )
        {
            if ( !fd.flags().has( O_NONBLOCK ) && !reader() )
               __vm_cancel();
            assignWriter();
        }

        fd.flags().clear( O_NONBLOCK );
        INode::open( fd );
    }

    void close( FileDescriptor &fd ) override
    {
        __dios_assert( fd.inode() == this );
        if ( fd.flags().read() )
            releaseReader();
        INode::close( fd );
    }

    bool read( char *buffer, size_t, size_t &length ) override
    {
        if ( length == 0 )
            return true;

        // progress or deadlock
        while ( ( length = _stream.pop( buffer, length ) ) == 0 )
            __vm_cancel();

        return true;
    }

    bool write( const char *buffer, size_t, size_t &length, Node ) override
    {
        if ( !_reader )
        {
            raise( SIGPIPE );
            return error( EPIPE ), false;
        }

        // progress or deadlock
        while ( ( length = _stream.push( buffer, length ) ) == 0 )
            __vm_cancel();

        return true;
    }

    void releaseReader() { _reader = false; }
    bool reader() const { return _reader; }
    bool writer() const { return _writer; }

    void assignReader()
    {
        if ( _reader )
            __dios_fault( _VM_Fault::_VM_F_Assert, "Pipe is opened for reading again." );
        _reader = true;
    }

    void assignWriter()
    {
        if ( _writer )
            __dios_fault( _VM_Fault::_VM_F_Assert, "Pipe is opened for writing again." );
        _writer = true;
    }

private:
    Stream _stream;
    bool _reader;
    bool _writer;
};

}
