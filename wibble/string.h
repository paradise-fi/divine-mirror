// -*- C++ -*-
#ifndef WIBBLE_STRING_H
#define WIBBLE_STRING_H

/*
 * Various string functions
 *
 * Copyright (C) 2007,2008  Enrico Zini <enrico@debian.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */

// modifications by DIVINE team:
// (c) 2014 Vladimír Štill

#include <wibble/operators.h>
#include <wibble/sfinae.h>

#include <cstdarg>
#include <cstdio>
#include <string>
#include <set>
#include <vector>
#include <deque>
#include <sstream>
#include <cctype>
#ifdef _WIN32
#include <cstring>
#include <cstdlib>
#endif

namespace wibble {
namespace str {

using namespace wibble::operators;

#ifdef _WIN32
static int vasprintf (char **, const char *, va_list);
#endif

std::string fmtf( const char* f, ... );
template< typename T > inline std::string fmt(const T& val);

// Formatting lists -- actually, we need to move list handling into wibble,
// really.
template< typename X >
inline typename TPair< std::ostream, typename X::Type >::First &operator<<(
    std::ostream &o, X list )
{
    if ( list.empty() )
        return o << "[]";

    o << "[ ";
    while( !list.empty() ) {
        o << fmt( list.head() );
        if ( !list.tail().empty() )
            o << ", ";
        list = list.tail();
    }
    return o << " ]";
}

/// Format any value into a string using a std::stringstream
template< typename T >
inline std::string fmt(const T& val)
{
    std::stringstream str;
    str << val;
    return str.str();
}

template<> inline std::string fmt<std::string>(const std::string& val) {
    return val;
}
template<> inline std::string fmt<char*>(char * const & val) { return val; }

template< typename C >
inline std::string fmt_container( const C &c, char f, char l )
{
    std::string s;
    s += f;
    if ( c.empty() )
        return s + l;

    s += ' ';
    for ( typename C::const_iterator i = c.begin(); i != c.end(); ++i ) {
        s += fmt( *i );
        if ( i != c.end() && i + 1 != c.end() )
            s += ", ";
    }
    s += ' ';
    s += l;
    return s;
}

// formatting sets using { ... } notation
template< typename X >
inline std::string fmt(const std::set< X >& val) {
    return fmt_container( val, '{', '}' );
}

// formatting vectors using [ ... ] notation
template< typename X >
inline std::string fmt(const std::vector< X > &val) {
    return fmt_container( val, '[', ']' );
}

// formatting vectors using [ ... ] notation
template< typename X >
inline std::string fmt(const std::deque< X > &val) {
    return fmt_container( val, '[', ']' );
}

/// Given a pathname, return the file name without its path
inline std::string basename(const std::string& pathname)
{
	size_t pos = pathname.rfind("/");
	if (pos == std::string::npos)
		return pathname;
	else
		return pathname.substr(pos+1);
}

/// Given a pathname, return the directory name without the file name
inline std::string dirname(const std::string& pathname)
{
	size_t pos = pathname.rfind("/");
	if (pos == std::string::npos)
		return std::string();
	else if (pos == 0)
		// Handle the case of '/foo'
		return std::string("/");
	else
		return pathname.substr(0, pos);
}

/**
 * Normalise a pathname.
 *
 * For example, A//B, A/./B and A/foo/../B all become A/B.
 */
std::string normpath( std::string pathname );

/// Check if a string starts with the given substring
inline bool startsWith(const std::string& str, const std::string& part)
{
	if (str.size() < part.size())
		return false;
	return str.substr(0, part.size()) == part;
}

/// Check if a string ends with the given substring
inline bool endsWith(const std::string& str, const std::string& part)
{
	if (str.size() < part.size())
		return false;
	return str.substr(str.size() - part.size()) == part;
}

inline std::string replace(const std::string& str, char from, char to)
{
	std::string res;
	res.reserve(str.size());
	for (std::string::const_iterator i = str.begin(); i != str.end(); ++i)
		if (*i == from)
			res.append(1, to);
		else
			res.append(1, *i);
	return res;
}

#if !__xlC__ && (! __GNUC__ || __GNUC__ >= 4)
/**
 * Return the substring of 'str' without all leading and trailing characters
 * for which 'classifier' returns true.
 */
template<typename FUN>
inline std::string trim(const std::string& str, const FUN& classifier)
{
	if (str.empty())
		return str;

	size_t beg = 0;
	size_t end = str.size() - 1;
	while (beg < end && classifier(str[beg]))
		++beg;
	while (end >= beg && classifier(str[end]))
		--end;

	return str.substr(beg, end-beg+1);
}

/**
 * Return the substring of 'str' without all leading and trailing spaces.
 */
inline std::string trim(const std::string& str)
{
    return trim(str, ::isspace);
}
#else
/// Workaround version for older gcc
inline std::string trim(const std::string& str)
{
	if (str.empty())
		return str;

	size_t beg = 0;
	size_t end = str.size() - 1;
	while (beg < end && ::isspace(str[beg]))
		++beg;
	while (end >= beg && ::isspace(str[end]))
		--end;

	return str.substr(beg, end-beg+1);
}
#endif

/// Convert a string to uppercase
inline std::string toupper(const std::string& str)
{
	std::string res;
	res.reserve(str.size());
	for (std::string::const_iterator i = str.begin(); i != str.end(); ++i)
		res += ::toupper(*i);
	return res;
}

/// Convert a string to lowercase
inline std::string tolower(const std::string& str)
{
	std::string res;
	res.reserve(str.size());
	for (std::string::const_iterator i = str.begin(); i != str.end(); ++i)
		res += ::tolower(*i);
	return res;
}

/// Return the same string, with the first character uppercased
inline std::string ucfirst(const std::string& str)
{
	if (str.empty()) return str;
	std::string res;
	res += ::toupper(str[0]);
	return res + tolower(str.substr(1));
}

// sorted by priority
#ifdef POSIX
const char pathSeparators[] = { '/' };
#endif
#ifdef WIN32
const char pathSeparators[] = { '\\', '/' };
#endif

inline bool isPathSeparator( char ch ) {
    for ( int i = 0; i < int( sizeof( pathSeparators ) ); ++i )
        if ( ch == pathSeparators[ i ] )
            return true;
    return false;
}

/// Join two paths, adding slashes when appropriate
inline std::string joinpath(const std::string& path1, const std::string& path2)
{
	if (path1.empty())
		return path2;
	if (path2.empty())
		return path1;

	if ( isPathSeparator( path1[path1.size() - 1] ) )
		if ( isPathSeparator( path2[0] ) )
			return path1 + path2.substr(1);
		else
			return path1 + path2;
	else
		if ( isPathSeparator( path2[0] ) )
			return path1 + path2;
		else
			return path1 + pathSeparators[ 0 ] + path2;
}
inline std::pair< std::string, std::string > absolutePrefix( const std::string &path ) {
#ifdef WIN32 /* this must go before general case, because \ is prefix of \\ */
    if ( path.size() >= 3 && path[ 1 ] == ':' && isPathSeparator( path[ 2 ] ) )
        return std::make_pair( path.substr( 0, 3 ), path.substr( 3 ) );
    if ( path.size() >= 2 && isPathSeparator( path[ 0 ] ) && isPathSeparator( path[ 1 ] ) )
        return std::make_pair( path.substr( 0, 2 ), path.substr( 2 ) );
#endif
    // this is absolute path in both windows and unix
    if ( path.size() >= 1 && isPathSeparator( path[ 0 ] ) )
        return std::make_pair( path.substr( 0, 1 ), path.substr( 1 ) );
    return std::make_pair( std::string(), std::string() );
}

inline bool isAbsolutePath( const std::string &path ) {
    return absolutePrefix( path ).first.size() != 0;
}

// append path2 to path1 if path2 is not absolute, otherwise return path2
inline std::string appendpath( const std::string &path1, const std::string &path2 ) {
    if ( isAbsolutePath( path2 ) )
        return path2;
    return joinpath( path1, path2 );
}

/// Urlencode a string
std::string urlencode(const std::string& str);

/// Decode an urlencoded string
std::string urldecode(const std::string& str);

/// Encode a string in Base64
std::string encodeBase64(const std::string& str);

/// Decode a string encoded in Base64
std::string decodeBase64(const std::string& str);

/**
 * Split a string where a given substring is found
 *
 * This does a similar work to the split functions of perl, python and ruby.
 *
 * Example code:
 * \code
 *   str::Split splitter("/", myString);
 *   vector<string> split;
 *   std::copy(splitter.begin(), splitter.end(), back_inserter(split));
 * \endcode
 */
class Split
{
	std::string sep;
	std::string str;

public:
	class const_iterator :
		public std::iterator<std::forward_iterator_tag, const std::string, void, const std::string*, const std::string&>
	{
		const std::string& sep;
		const std::string& str;
		std::string cur;
		size_t pos;

	public:
		const_iterator(const std::string& sep, const std::string& str) : sep(sep), str(str), pos(0)
		{
			++*this;
		}
		const_iterator(const std::string& sep, const std::string& str, bool) : sep(sep), str(str), pos(std::string::npos) {}

		const_iterator& operator++()
		{
			if (pos == str.size())
				pos = std::string::npos;
			else
			{
				size_t end;
				if (sep.empty())
					if (pos + 1 == str.size())
						end = std::string::npos;
					else
						end = pos + 1;
				else
					end = str.find(sep, pos);
				if (end == std::string::npos)
				{
					cur = str.substr(pos);
					pos = str.size();
				}
				else
				{
					cur = str.substr(pos, end-pos);
					pos = end + sep.size();
				}
			}
			return *this;
		}

		std::string remainder() const
		{
			if (pos == std::string::npos)
				return std::string();
			else
				return str.substr(pos);
		}

		const std::string& operator*() const
		{
			return cur;
		}
		const std::string* operator->() const
		{
			return &cur;
		}
		bool operator==(const const_iterator& ti) const
		{
			// Comparing iterators on different strings is not supported for
			// performance reasons
			return pos == ti.pos;
		}
		bool operator!=(const const_iterator& ti) const
		{
			// Comparing iterators on different strings is not supported for
			// performance reasons
			return pos != ti.pos;
		}
	};

	/**
	 * Create a splitter that uses the given regular expression to find tokens.
	 */
	Split(const std::string& sep, const std::string& str) : sep(sep), str(str) {}

	/**
	 * Split the string and iterate the resulting tokens
	 */
	const_iterator begin() const { return const_iterator(sep, str); }
	const_iterator end() const { return const_iterator(sep, str, false); }
};

template<typename ITER>
std::string join(const ITER& begin, const ITER& end, const std::string& sep = ", ")
{
	std::stringstream res;
	bool first = true;
	for (ITER i = begin; i != end; ++i)
	{
		if (first)
			first = false;
		else
			res << sep;
		res << *i;
	}
	return res.str();
}

/**
 * Parse a record of Yaml-style field: value couples.
 *
 * Parsing stops either at end of record (one empty line) or at end of file.
 *
 * The value is deindented properly.
 *
 * Example code:
 * \code
 *	utils::YamlStream stream;
 *	map<string, string> record;
 *	std::copy(stream.begin(inputstream), stream.end(), inserter(record));
 * \endcode
 */
class YamlStream
{
public:
	// TODO: add iterator_traits
	class const_iterator
	{
		std::istream* in;
		std::pair<std::string, std::string> value;
		std::string line;

	public:
		const_iterator(std::istream& in);
		const_iterator() : in(0) {}

		const_iterator& operator++();

		const std::pair<std::string, std::string>& operator*() const
		{
			return value;
		}
		const std::pair<std::string, std::string>* operator->() const
		{
			return &value;
		}
		bool operator==(const const_iterator& ti) const
		{
			return in == ti.in;
		}
		bool operator!=(const const_iterator& ti) const
		{
			return in != ti.in;
		}
	};

	const_iterator begin(std::istream& in) { return const_iterator(in); }
	const_iterator end() { return const_iterator(); }
};

/**
 * Escape the string so it can safely used as a C string inside double quotes
 */
std::string c_escape(const std::string& str);

/**
 * Unescape a C string, stopping at the first double quotes or at the end of
 * the string.
 *
 * lenParsed is set to the number of characters that were pased (which can be
 * greather than the size of the resulting string in case escapes were found)
 */
std::string c_unescape(const std::string& str, size_t& lenParsed);


}
}

// _WIN32 std::to_string hack
#if defined( _WIN32 ) \
    && __GNUC__ == 4 && __GNUC_MINOR__ == 7 \
    && __cplusplus >= 201103L

#warning WIBBLE string defines std::to_string (which is missing on GCC < 4.8 on win)
#include <stdexcept>

namespace std {
    static inline std::string to_string( int value ) { return wibble::str::fmt( value ); }
    static inline std::string to_string( long value ) { return wibble::str::fmt( value ); }
    static inline std::string to_string( long long value ) { return wibble::str::fmt( value ); }
    static inline std::string to_string( unsigned value ) { return wibble::str::fmt( value ); }
    static inline std::string to_string( unsigned long value ) { return wibble::str::fmt( value ); }
    static inline std::string to_string( unsigned long long value ) { return wibble::str::fmt( value ); }
    static inline std::string to_string( float value ) { return wibble::str::fmt( value ); }
    static inline std::string to_string( double value ) { return wibble::str::fmt( value ); }
    static inline std::string to_string( long double value ) { return wibble::str::fmt( value ); }

    static inline int       stoi( const std::string& str, size_t *pos = 0, int base = 10 ) {
        char *end;
        long ret = std::strtol( str.c_str(), &end, base );
        if ( end == str.c_str() )
            throw  std::invalid_argument( "stoi" );
        if ( errno == ERANGE || long( int( ret ) ) != ret  )
            throw std::out_of_range( "stoi" );
        if ( pos )
            *pos = end - str.c_str();
        return ret;
    }/*
    static inline long      stol( const std::string& str, size_t *pos = 0, int base = 10 );
    static inline long long stoll( const std::string& str, size_t *pos = 0, int base = 10 );
    */
}
#endif // _WIN32 std::to_string hack

// vim:set ts=4 sw=4:
#endif
