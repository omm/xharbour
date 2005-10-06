
/*
 * $Id: ocgi.prg,v 1.1 2003/02/23 23:15:17 lculik Exp $
 */

/*
 * Harbour Project source code:
 * Cgi Class
 *
 * Copyright 2000 Manos Aspradakis <maspr@otenet.gr>
 * www - http://www.harbour-project.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version, with one exception:
 *
 * The exception is that if you link the Harbour Runtime Library (HRL)
 * and/or the Harbour Virtual Machine (HVM) with other files to produce
 * an executable, this does not by itself cause the resulting executable
 * to be covered by the GNU General Public License. Your use of that
 * executable is in no way restricted on account of linking the HRL
 * and/or HVM code into it.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA (or visit
 * their web site at http://www.gnu.org/).
 *
 */
/*
 * The following parts are Copyright of the individual authors.
 * www - http://www.harbour-project.org
 *
 * Copyright 2000 Luiz Rafael Culik <culik@sl.conex.net>
 *    Porting this library to Harbour
 *
 * See doc/license.txt for licensing terms.
 *
 */

#include "hbclass.ch"
#include "default.ch"
#include "html.ch"

CLASS oCgi FROM HTML

   DATA nH
   DATA Server_Software
   DATA Server_Name
   DATA Gateway_Interface
   DATA Server_Protocol
   DATA Server_Port
   DATA Request_Method
   DATA Http_Accept
   DATA Http_User_agent
   DATA Http_Referer
   DATA Path_Info
   DATA Path_Translated
   DATA Script_Name
   DATA Query_String
   DATA Remote_Host
   DATA Remote_Addr
   DATA ipAddress
   DATA Remote_User
   DATA Auth_Type
   DATA Auth_User
   DATA Auth_Pass
   DATA Content_Type
   DATA Content_Length
   DATA Annotation_Server

   DATA aQueryFields INIT {}

   METHOD New( c )

   METHOD CGIField( c )

   METHOD toObject()

ENDCLASS

   /****
*
*     oCgi():new()
*
*
*
*/

METHOD New( cInBuffer ) Class oCgi

   LOCAL cBuff
   LOCAL i
   LOCAL nBuff
   LOCAL aTemp := {}
   LOCAL aVar  := {}

   // function in oHtm.prg
   ::nH := PageHandle()

   ::Server_Software   := Getenv( "SERVER_SOFTWARE" )
   ::Server_Name       := Getenv( "SERVER_NAME" )
   ::Gateway_Interface := Getenv( "GATEWAY_INTERFACE" )
   ::Server_Protocol   := Getenv( "SERVER_PROTOCOL" )
   ::Server_Port       := Getenv( "SERVER_PORT" )
   ::Request_Method    := Getenv( "REQUEST_METHOD" )
   ::Http_Accept       := Getenv( "HTTP_ACCEPT" )
   ::Http_User_agent   := Getenv( "HTTP_USER_AGENT" )
   ::Http_Referer      := Getenv( "HTTP_REFERER" )
   ::Path_Info         := Getenv( "PATH_INFO" )
   ::Path_Translated   := Getenv( "PATH_TRANSLATED" )
   ::Script_Name       := Getenv( "SCRIPT_NAME" )
   ::Query_String      := Getenv( "QUERY_STRING" )
   ::Remote_Host       := Getenv( "REMOTE_HOST" )
   ::Remote_Addr       := Getenv( "REMOTE_ADDR" )
   ::ipAddress         := Getenv( "REMOTE_ADDR" )
   ::Remote_User       := Getenv( "REMOTE_USER" )
   ::Auth_Type         := Getenv( "AUTH_TYPE" )
   ::Auth_User         := Getenv( "AUTH_USER" )
   ::Auth_Pass         := Getenv( "AUTH_PASS" )
   ::Content_Type      := Getenv( "CONTENT_TYPE" )
   ::Content_Length    := Getenv( "CONTENT_LENGTH" )
   ::Annotation_Server := Getenv( "ANNOTATION_SERVER" )

   IF cInBuffer != NIL
      ::Query_String := Rtrim( cInBuffer )
   ELSE
      IF "POST" $ Upper( ::Request_Method )
         ::Query_String := Rtrim( Freadstr( STD_IN, Val( ::CONTENT_LENGTH ) ) )
      ENDIF
   ENDIF

   IF !Empty( ::Query_String )

      ::aQueryFields := {}

      aTemp := Listasarray( ::Query_String, "&" )           // separate fields

      IF Len( aTemp ) != 0
         FOR i := 1 TO Len( aTemp )
            aVar := Listasarray( aTemp[ i ], "=" )
            IF Len( aVar ) == 2
               Aadd( ::aQueryFields, { aVar[ 1 ], decodeURL( aVar[ 2 ] ) } )
            ENDIF
         NEXT
      ENDIF

   ENDIF


RETURN ::ToObject()                     //Self
//#ifdef _CLASS_CH

/****
*
*        oCgi():ToObject()
*
*        Creates instance variables out of CGI FORM return values
*        or URL encoded content.
*
*        It subclasses the oCGI class to a *new* class
*/

METHOD ToObject() CLAss ocgi

   LOCAL i
   LOCAL bBlock
   LOCAL cFldName
   LOCAL nScope    := 1
   LOCAL aDb
   LOCAL oDb
   LOCAL hNewClass
   LOCAL oNew
   LOCAL n
   LOCAL hNew
   STATIC sn       := 0

   // --> create new oObject class from this one...
   sn ++
   //aDB := ClassNew( "NewCgi"+STRZERO(sn,3), {|| oCGI() }, "cgi" )
   aDb := hbClass():New( "NewCgi" + Strzero( sn, 3 ), __CLS_PARAM( "oCgi" ) )

   FOR i := 1 TO Len( ::aQueryFields )

      IF ::aQueryFields[ i, 2 ] == NIL .or. Empty( ::aQueryFields[ i, 2 ] )
         ::aQueryFields[ i, 2 ] := ""
      ENDIF

      adb:AddData( ::aQueryFields[ i, 1 ], ::aQueryFields[ i, 2 ],, nScope )
   NEXT

   adb:Create()
   oNew := adb:Instance()
   oNew:aQueryFields      := ::aQueryFields
   oNew:Server_Software   := ::Server_Software
   oNew:Server_Name       := ::Server_Name
   oNew:Gateway_Interface := ::Gateway_Interface
   oNew:Server_Protocol   := ::Server_Protocol
   oNew:Server_Port       := ::Server_Port
   oNew:Request_Method    := ::Request_Method
   oNew:Http_Accept       := ::Http_Accept
   oNew:Http_User_agent   := ::Http_User_agent
   oNew:Http_Referer      := ::Http_Referer
   oNew:Path_Info         := ::Path_Info
   oNew:Path_Translated   := ::Path_Translated
   oNew:Script_Name       := ::Script_Name
   oNew:Query_String      := ::Query_String
   oNew:Remote_Host       := ::Remote_Host
   oNew:Remote_Addr       := ::Remote_Addr
   oNew:ipAddress         := ::ipAddress
   oNew:Remote_User       := ::Remote_User
   oNew:Auth_Type         := ::Auth_Type
   oNew:Content_Type      := ::Content_Type
   oNew:Content_Length    := ::Content_Length
   oNew:Annotation_Server := ::Annotation_Server
   oNew:nH                := IIF( PageHandle() == NIL, STD_OUT, PageHandle() )

RETURN oNew

//#endif

// ripped from HARBOUR

METHOD CGIField( cQueryName ) Class oCgi

   LOCAL cRet := ""
   LOCAL nRet

   DEFAULT cQueryName := ""

   nRet := Ascan( ::aQueryFields, ;
                  { | x | Upper( x[ 1 ] ) = Upper( cQueryName ) } )

   IF nRet > 0
      cRet := ::aQueryFields[ nRet, 2 ]
   ENDIF

RETURN ( cRet )

// ripped from HARBOUR

FUNCTION ParseString( cString, cDelim, nRet )

   LOCAL cBuf
   LOCAL aElem
   LOCAL nPosFim
   LOCAL nSize
   LOCAL i

   nSize := Len( cString ) - Len( Strtran( cString, cDelim, '' ) ) + 1
   aElem := Array( nSize )

   cBuf := cString
   i    := 1
   FOR i := 1 TO nSize
      nPosFim := At( cDelim, cBuf )

      IF nPosFim > 0
         aElem[ i ] := Substr( cBuf, 1, nPosFim - 1 )
      ELSE
         aElem[ i ] := cBuf
      ENDIF

      cBuf := Substr( cBuf, nPosFim + 1, Len( cBuf ) )

   NEXT i

RETURN ( aElem[ nRet ] )

// ripped from HARBOUR

FUNCTION Hex2Dec( cHex )

   LOCAL aHex := { { "0", 00 }, ;
                   { "1", 01 }, ;
                   { "2", 02 }, ;
                   { "3", 03 }, ;
                   { "4", 04 }, ;
                   { "5", 05 }, ;
                   { "6", 06 }, ;
                   { "7", 07 }, ;
                   { "8", 08 }, ;
                   { "9", 09 }, ;
                   { "A", 10 }, ;
                   { "B", 11 }, ;
                   { "C", 12 }, ;
                   { "D", 13 }, ;
                   { "E", 14 }, ;
                   { "F", 15 } }
   LOCAL nRet
   LOCAL nRes

   nRet := Ascan( aHex, { | x | Upper( x[ 1 ] ) = Upper( Left( cHex, 1 ) ) } )
   nRes := aHex[ nRet, 2 ] * 16
   nRet := Ascan( aHex, { | x | Upper( x[ 1 ] ) = Upper( Right( cHex, 1 ) ) } )
   nRes += aHex[ nRet, 2 ]

RETURN ( nRes )
