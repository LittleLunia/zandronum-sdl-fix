//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2007 Brad Carney, Benjamin Berkels
// Copyright (C) 2007-2012 Skulltag Development Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the Skulltag Development Team nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 4. Redistributions in any form must be accompanied by information on how to
//    obtain complete source code for the software and any accompanying
//    software that uses the software. The source code must either be included
//    in the distribution or be available for no more than the cost of
//    distribution plus a nominal fee, and must be freely redistributable
//    under reasonable conditions. For an executable file, complete source
//    code means the source code for all modules it contains. It does not
//    include source code for modules or files that typically accompany the
//    major components of the operating system on which the executable file
//    runs.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//
//
// Filename: networkshared.cpp
//
// Description: Contains network related code shared between
// Skulltag and the master server.
//
//-----------------------------------------------------------------------------

#include "networkheaders.h"
#include "networkshared.h"
#include <sstream>
#include <errno.h>

//*****************************************************************************
//
bool NETWORK_StringToAddress( char *s, NETADDRESS_s *a )
{
     struct hostent  *h;
     struct sockaddr_in sadr;
     char    *colon;
     char    copy[128];

     memset (&sadr, 0, sizeof(sadr));
     sadr.sin_family = AF_INET;

     sadr.sin_port = 0;

     strcpy (copy, s);
     // strip off a trailing :port if present
     for (colon = copy ; *colon ; colon++)
          if (*colon == ':')
          {
             *colon = 0;
             sadr.sin_port = htons(atoi(colon+1));
          }

	{
		LONG	lRet;

		lRet = inet_addr( copy );

		// If our return value is INADDR_NONE, the IP specified is not a valid IPv4 string.
		if ( lRet == INADDR_NONE )
		{
			// If the string cannot be resolved to a valid IP address, return false.
          if (( h = gethostbyname( copy )) == NULL )
                return ( false );
          *(int *)&sadr.sin_addr = *(int *)h->h_addr_list[0];
		}
		else
			*(int *)&sadr.sin_addr = lRet;
	}

	NETWORK_SocketAddressToNetAddress (&sadr, a);

     return true;
}

//*****************************************************************************
//
void NETWORK_SocketAddressToNetAddress( struct sockaddr_in *s, NETADDRESS_s *a )
{
     *(int *)&a->abIP = *(int *)&s->sin_addr;
     a->usPort = s->sin_port;
}

//*****************************************************************************
//
bool NETWORK_StringToIP( char *pszAddress, char *pszIP0, char *pszIP1, char *pszIP2, char *pszIP3 )
{
	char	szCopy[16];
	char	*pszCopy;
	char	szTemp[4];
	char	*pszTemp;
	ULONG	ulIdx;
	ULONG	ulNumPeriods;

	// Too long.
	if ( strlen( pszAddress ) > 15 )
		return ( false );

	// First, get rid of anything after the colon (if it exists).
	strcpy( szCopy, pszAddress );
	for ( pszCopy = szCopy; *pszCopy; pszCopy++ )
	{
		if ( *pszCopy == ':' )
		{
			*pszCopy = 0;
			break;
		}
	}

	// Next, make sure there's at least 3 periods.
	ulNumPeriods = 0;
	for ( pszCopy = szCopy; *pszCopy; pszCopy++ )
	{
		if ( *pszCopy == '.' )
			ulNumPeriods++;
	}

	// If there weren't 3 periods, then it's not a valid ban string.
	if ( ulNumPeriods != 3 )
		return ( false );

	ulIdx = 0;
	pszTemp = szTemp;
	*pszTemp = 0;
	for ( pszCopy = szCopy; *pszCopy; pszCopy++ )
	{
		if ( *pszCopy == '.' )
		{
			// Shouldn't happen.
			if ( ulIdx > 3 )
				return ( false );

			switch ( ulIdx )
			{
			case 0:

				strcpy( pszIP0, szTemp );
				break;
			case 1:

				strcpy( pszIP1, szTemp );
				break;
			case 2:

				strcpy( pszIP2, szTemp );
				break;
			case 3:

				strcpy( pszIP3, szTemp );
				break;
			}
			ulIdx++;
//			strcpy( szBan[ulIdx++], szTemp );
			pszTemp = szTemp;
		}
		else
		{
			*pszTemp++ = *pszCopy;
			*pszTemp = 0;
			if ( strlen( szTemp ) > 3 )
				return ( false );
		}
	}

	strcpy( pszIP3, szTemp );

	// Finally, make sure each entry of our string is valid.
	if ((( atoi( pszIP0 ) < 0 ) || ( atoi( pszIP0 ) > 255 )) && ( _stricmp( "*", pszIP0 ) != 0 ))
		return ( false );
	if ((( atoi( pszIP1 ) < 0 ) || ( atoi( pszIP1 ) > 255 )) && ( _stricmp( "*", pszIP1 ) != 0 ))
		return ( false );
	if ((( atoi( pszIP2 ) < 0 ) || ( atoi( pszIP2 ) > 255 )) && ( _stricmp( "*", pszIP2 ) != 0 ))
		return ( false );
	if ((( atoi( pszIP3 ) < 0 ) || ( atoi( pszIP3 ) > 255 )) && ( _stricmp( "*", pszIP3 ) != 0 ))
		return ( false );

    return ( true );
}

//*****************************************************************************
//
std::string GenerateCouldNotOpenFileErrorString( const char *pszFunctionHeader, const char *pszFileName, LONG lErrorCode )
{
	std::stringstream errorMessage;
	errorMessage << pszFunctionHeader << ": Couldn't open file: " << pszFileName << "!\nREASON: ";
	switch ( lErrorCode )
	{
	case EACCES:

		errorMessage << "EACCES: Search permission is denied on a component of the path prefix, or the file exists and the permissions specified by mode are denied, or the file does not exist and write permission is denied for the parent directory of the file to be created.\n";
		break;
	case EINTR:

		errorMessage << "EINTR: A signal was caught during fopen().\n";
		break;
	case EISDIR:

		errorMessage << "EISDIR: The named file is a directory and mode requires write access.\n";
		break;
	case EMFILE:

		errorMessage << "EMFILE: {OPEN_MAX} file descriptors are currently open in the calling process.\n";
		break;
	case ENAMETOOLONG:

		errorMessage << "ENAMETOOLONG: Pathname resolution of a symbolic link produced an intermediate result whose length exceeds {PATH_MAX}.\n";
		break;
	case ENFILE:

		errorMessage << "ENFILE: The maximum allowable number of files is currently open in the system.\n";
		break;
	case ENOENT:

		errorMessage << "ENOENT: A component of filename does not name an existing file or filename is an empty string.\n";
		break;
	case ENOSPC:

		errorMessage << "ENOSPC: The directory or file system that would contain the new file cannot be expanded, the file does not exist, and it was to be created.\n";
		break;
	case ENOTDIR:

		errorMessage << "ENOTDIR: A component of the path prefix is not a directory.\n";
		break;
	case ENXIO:

		errorMessage << "ENXIO: The named file is a character special or block special file, and the device associated with this special file does not exist.\n";
		break;
	case EROFS:

		errorMessage << "EROFS: The named file resides on a read-only file system and mode requires write access.\n";
		break;
	case EINVAL:

		errorMessage << "EINVAL: The value of the mode argument is not valid.\n";
		break;
	case ENOMEM:

		errorMessage << "ENOMEM: Insufficient storage space is available.\n";
		break;
//	case EOVERFLOW:
//	case ETXTBSY:
//	case ELOOP:
	default:

		errorMessage << "UNKNOWN\n";
		break;
	}
	return errorMessage.str();
}

//*****************************************************************************
//
bool IPFileParser::parseIPList( const char* FileName, std::vector<IPADDRESSBAN_s> &IPArray )
{
	FILE			*pFile;

	IPArray.clear();

	char curChar = 0;
	_numberOfEntries = 0;
	if (( pFile = fopen( FileName, "r" )) != NULL )
	{
		while ( true )
		{
			IPADDRESSBAN_s IP;
			ULONG oldNumberOfEntries = _numberOfEntries;
			bool parsingDone = !parseNextLine( pFile, IP, _numberOfEntries );

			if ( _errorMessage[0] != '\0' )
			{
				fclose( pFile );
				return false;
			}
			else if ( oldNumberOfEntries < _numberOfEntries )
				IPArray.push_back( IP );

			if ( parsingDone == true )
				break;
		}
	}
	else
	{
		sprintf( _errorMessage, "%s", GenerateCouldNotOpenFileErrorString( "IPFileParser::parseIPList", FileName, errno ).c_str() );
		return false;
	}

	fclose( pFile );
	return true;
}

//*****************************************************************************
//
char IPFileParser::skipWhitespace( FILE *pFile )
{
	char curChar = fgetc( pFile );
	while (( curChar == ' ' ) && ( curChar != -1 ))
		curChar = fgetc( pFile );

	return ( curChar );
}

//*****************************************************************************
//
char IPFileParser::skipComment( FILE *pFile )
{
	char curChar = fgetc( pFile );
	while (( curChar != '\r' ) && ( curChar != '\n' ) && ( curChar != -1 ))
		curChar = fgetc( pFile );

	return ( curChar );
}

//*****************************************************************************
//
bool IPFileParser::parseNextLine( FILE *pFile, IPADDRESSBAN_s &IP, ULONG &BanIdx )
{
	NETADDRESS_s	IPAddress;
	char			szIP[257];
	int				lPosition;

	lPosition = 0;
	szIP[0] = 0;

	char curChar = fgetc( pFile );

	// Skip whitespace.
	if ( curChar == ' ' )
	{
		curChar = skipWhitespace( pFile );

		if ( feof( pFile ))
		{
			fclose( pFile );
			return ( false );
		}
	}

	while ( 1 )
	{
		if ( curChar == '\r' || curChar == '\n' || curChar == ':' || curChar == '/' || curChar == -1 )
		{
			if ( lPosition > 0 )
			{
				if ( NETWORK_StringToIP( szIP, IP.szIP[0], IP.szIP[1], IP.szIP[2], IP.szIP[3] ))
				{
					if ( BanIdx == _listLength )
					{
						sprintf( _errorMessage, "parseNextLine: WARNING! Maximum number of IPs (%d) exceeded!\n", _listLength );
						return ( false );
					}

					BanIdx++;
					// [BB] If there is a reason given why the IP is on the list, read it now.
					if ( curChar == ':' )
						readReason( pFile, IP.szComment, 128 );
					else
						IP.szComment[0] = 0;
					return ( true );
				}
				else if ( NETWORK_StringToAddress( szIP, &IPAddress ))
				{
					if ( BanIdx == _listLength )
					{
						sprintf( _errorMessage, "parseNextLine: WARNING! Maximum number of IPs (%d) exceeded!\n", _listLength );
						return ( false );
					}

					_itoa( IPAddress.abIP[0], IP.szIP[0], 10 );
					_itoa( IPAddress.abIP[1], IP.szIP[1], 10 );
					_itoa( IPAddress.abIP[2], IP.szIP[2], 10 );
					_itoa( IPAddress.abIP[3], IP.szIP[3], 10 );
					BanIdx++;
					// [BB] If there is a reason given why the IP is on the list, read it now.
					if ( curChar == ':' )
						readReason( pFile, IP.szComment, 128 );
					return ( true );
				}
				else
				{
					IP.szIP[0][0] = 0;
					IP.szIP[1][0] = 0;
					IP.szIP[2][0] = 0;
					IP.szIP[3][0] = 0;
				}
			}

			if ( feof( pFile ))
			{
				fclose( pFile );
				return ( false );
			}
			// If we've hit a comment, skip until the end of the line (or the end of the file) and get out.
			else if ( curChar == ':' || curChar == '/' )
			{
				skipComment( pFile );
				return ( true );
			}
			else
				return ( true );
		}

		szIP[lPosition++] = curChar;
		szIP[lPosition] = 0;

		if ( lPosition == 256 )
		{
			fclose( pFile );
			return ( false );
		}

		curChar = fgetc( pFile );
	}
}

//*****************************************************************************
//
void IPFileParser::readReason( FILE *pFile, char *Reason, const int MaxReasonLength )
{
	char curChar = fgetc( pFile );
	int i = 0;
	while (( curChar != '\r' ) && ( curChar != '\n' ) && !feof( pFile ) && i < MaxReasonLength-1 )
	{
		Reason[i] = curChar;
		curChar = fgetc( pFile );
		i++;
	}
	Reason[i] = 0;
	// [BB] Check if we reached the end of the comment, if not skip the rest.
	if( ( curChar != '\r' ) && ( curChar != '\n' ) && ( curChar != -1 ) )
		skipComment( pFile );
}

//*****************************************************************************
//
bool IPList::clearAndLoadFromFile( const char *Filename )
{
	bool success = false;
	_filename = Filename;
	_error = "";

	IPFileParser parser( 65536 );

	success = parser.parseIPList( Filename, _ipVector );
	if ( !success )
		_error = parser.getErrorMessage();

	return success;
}

//*****************************************************************************
//
bool IPList::isIPInList( const char *pszIP0, const char *pszIP1, const char *pszIP2, const char *pszIP3 ) const
{
	for ( ULONG ulIdx = 0; ulIdx < _ipVector.size(); ulIdx++ )
	{
		if ((( _ipVector[ulIdx].szIP[0][0] == '*' ) || ( stricmp( pszIP0, _ipVector[ulIdx].szIP[0] ) == 0 )) &&
			(( _ipVector[ulIdx].szIP[1][0] == '*' ) || ( stricmp( pszIP1, _ipVector[ulIdx].szIP[1] ) == 0 )) &&
			(( _ipVector[ulIdx].szIP[2][0] == '*' ) || ( stricmp( pszIP2, _ipVector[ulIdx].szIP[2] ) == 0 )) &&
			(( _ipVector[ulIdx].szIP[3][0] == '*' ) || ( stricmp( pszIP3, _ipVector[ulIdx].szIP[3] ) == 0 )))
		{
			return ( true );
		}
	}

	return ( false );
}

//*****************************************************************************
//
ULONG IPList::doesEntryExist( const char *pszIP0, const char *pszIP1, const char *pszIP2, const char *pszIP3 ) const
{
	for ( ULONG ulIdx = 0; ulIdx < _ipVector.size(); ulIdx++ )
	{
		if (( stricmp( pszIP0, _ipVector[ulIdx].szIP[0] ) == 0 ) &&
			( stricmp( pszIP1, _ipVector[ulIdx].szIP[1] ) == 0 ) &&
			( stricmp( pszIP2, _ipVector[ulIdx].szIP[2] ) == 0 ) &&
			( stricmp( pszIP3, _ipVector[ulIdx].szIP[3] ) == 0 ))
		{
			return ( ulIdx );
		}
	}

	return ( _ipVector.size() );
}

//*****************************************************************************
//
IPADDRESSBAN_s IPList::getEntry( const ULONG ulIdx ) const
{
	if ( ulIdx >= _ipVector.size() )
	{
		IPADDRESSBAN_s	ZeroBan;

		sprintf( ZeroBan.szIP[0], "0" );
		sprintf( ZeroBan.szIP[1], "0" );
		sprintf( ZeroBan.szIP[2], "0" );
		sprintf( ZeroBan.szIP[3], "0" );

		ZeroBan.szComment[0] = 0;

		return ( ZeroBan );
	}

	return ( _ipVector[ulIdx] );
}

//*****************************************************************************
//
std::string IPList::getEntryAsString( const ULONG ulIdx ) const
{
	std::stringstream entryStream;

	if( ulIdx < _ipVector.size() )
	{
		entryStream << _ipVector[ulIdx].szIP[0] << "."
					<< _ipVector[ulIdx].szIP[1] << "."
					<< _ipVector[ulIdx].szIP[2] << "."
					<< _ipVector[ulIdx].szIP[3];
		if ( _ipVector[ulIdx].szComment[0] )
			entryStream << ":" << _ipVector[ulIdx].szComment;
		entryStream << std::endl;
	}
	return entryStream.str();
}

//*****************************************************************************
//
void IPList::addEntry( const char *pszIP0, const char *pszIP1, const char *pszIP2, const char *pszIP3, const char *pszPlayerName, const char *pszComment, std::string &Message )
{
	FILE		*pFile;
	char		szOutString[512];
	ULONG		ulIdx;
	std::stringstream messageStream;

	// Address is already in the list.
	ulIdx = doesEntryExist( pszIP0, pszIP1, pszIP2, pszIP3 );
	if ( ulIdx != _ipVector.size() )
	{
		messageStream << pszIP0 << "." << pszIP1 << "."	<< pszIP2 << "." << pszIP3 << " already exists in list.\n";
		Message = messageStream.str();
		return;
	}

	szOutString[0] = 0;
	if ( pszPlayerName )
	{
		sprintf( szOutString, "%s", szOutString, pszPlayerName );
		if ( pszComment )
			sprintf( szOutString, "%s:", szOutString );
	}
	if ( pszComment )
		sprintf( szOutString, "%s%s", szOutString, pszComment );

	// Add the entry and comment into memory.
	IPADDRESSBAN_s newIPEntry;
	sprintf( newIPEntry.szIP[0], pszIP0 );
	sprintf( newIPEntry.szIP[1], pszIP1 );
	sprintf( newIPEntry.szIP[2], pszIP2 );
	sprintf( newIPEntry.szIP[3], pszIP3 );
	sprintf( newIPEntry.szComment, "%s", szOutString );
	_ipVector.push_back( newIPEntry );

	// Finally, append the IP to the file.
	if ( (pFile = fopen( _filename.c_str(), "a" )) )
	{
		sprintf( szOutString, "\n%s.%s.%s.%s", pszIP0, pszIP1, pszIP2, pszIP3 );
		if ( pszPlayerName )
			sprintf( szOutString, "%s:%s", szOutString, pszPlayerName );
		if ( pszComment )
			sprintf( szOutString, "%s:%s", szOutString, pszComment );
		fputs( szOutString, pFile );
		fclose( pFile );

		messageStream << pszIP0 << "." << pszIP1 << "."	<< pszIP2 << "." << pszIP3 << " added to list.\n";
		Message = messageStream.str();
	}
	else
	{
		Message = GenerateCouldNotOpenFileErrorString( "IPList::addEntry", _filename.c_str(), errno );
	}
}

