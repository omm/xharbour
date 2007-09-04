/*
 * $Id: hbtoken.c 7114 2007-04-06 12:35:15Z druzus $
 */

/*
 * Harbour Project source code:
 * 
 *
 * Copyright 2007 Przemyslaw Czerpak <druzus / at / priv.onet.pl>
 * www - http://www.harbour-project.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307 USA (or visit the web site http://www.gnu.org/).
 *
 * As a special exception, the Harbour Project gives permission for
 * additional uses of the text contained in its release of Harbour.
 *
 * The exception is that, if you link the Harbour libraries with other
 * files to produce an executable, this does not by itself cause the
 * resulting executable to be covered by the GNU General Public License.
 * Your use of that executable is in no way restricted on account of
 * linking the Harbour library code into it.
 *
 * This exception does not however invalidate any other reasons why
 * the executable file might be covered by the GNU General Public License.
 *
 * This exception applies only to the code released by the Harbour
 * Project under the name Harbour.  If you copy code from other
 * Harbour Project or Free Software Foundation releases into a copy of
 * Harbour, as the General Public License permits, the exception does
 * not apply to the code that you add in this way.  To avoid misleading
 * anyone as to the status of such modified files, you must delete
 * this exception notice from them.
 *
 * If you write modifications of your own for Harbour, it is your choice
 * whether to permit this exception to apply to your modifications.
 * If you do not wish that, delete this exception notice.
 *
 */

#include "hbapi.h"
#include "hbapiitm.h"
#include "hbapierr.h"

static ULONG hb_tokenCount( char * szLine, ULONG ulLen,
                            char * szDelim, ULONG ulDelim,
                            BOOL fSkipStrings, BOOL fDoubleQuoteOnly )
{
   ULONG ul = 0, ulTokens = 1;
   char cQuote = 0;

   while( ul < ulLen )
   {
      if( cQuote )
      {
         if( szLine[ ul ] == cQuote )
            cQuote = 0;
      }
      else if( fSkipStrings && ( szLine[ ul ] == '"' ||
               ( !fDoubleQuoteOnly && szLine[ ul ] == '\'' ) ) )
         cQuote = szLine[ ul ];
      else if( szLine[ ul ] == szDelim[ 0 ] &&
               ( ulDelim == 1 || !memcmp( szLine + ul, szDelim, ulDelim ) ) )
      {
         ++ulTokens;
         if( ulDelim == 1 && *szDelim == ' ' )
         {
            while( ++ul < ulLen && szLine[ ul ] == ' ' );
         }
         ul += ulDelim - 1;
      }
      ++ul;
   }

   return ulTokens;
}

static char * hb_tokenGet( char * szLine, ULONG ulLen,
                           char * szDelim, ULONG ulDelim,
                           BOOL fSkipStrings, BOOL fDoubleQuoteOnly,
                           ULONG ulToken, ULONG * pulLen )
{
   ULONG ul, ulStart;
   char cQuote = 0;
   
   for( ul = ulStart = 0; ul < ulLen; ++ul )
   {
      if( cQuote )
      {
         if( szLine[ ul ] == cQuote )
            cQuote = 0;
      }
      else if( fSkipStrings && ( szLine[ ul ] == '"' ||
               ( !fDoubleQuoteOnly && szLine[ ul ] == '\'' ) ) )
         cQuote = szLine[ ul ];
      else if( szLine[ ul ] == szDelim[ 0 ] &&
               ( ulDelim == 1 || !memcmp( szLine + ul, szDelim, ulDelim ) ) )
      {
         if( --ulToken == 0 )
         {
            * pulLen = ul - ulStart;
            return szLine + ulStart;
         }
         if( ulDelim == 1 && *szDelim == ' ' )
         {
            while( ul < ulLen && szLine[ ul + 1 ] == ' ' )
               ++ul;
         }
         ulStart = ul + ulDelim;
      }
   }
   if( --ulToken == 0 )
   {
      * pulLen = ul - ulStart;
      return szLine + ulStart;
   }
   * pulLen = 0;
   return NULL;
}

static PHB_ITEM hb_tokenArray( char * szLine, ULONG ulLen,
                               char * szDelim, ULONG ulDelim,
                               BOOL fSkipStrings, BOOL fDoubleQuoteOnly )
{
   ULONG ulTokens = hb_tokenCount( szLine, ulLen, szDelim, ulDelim,
                                   fSkipStrings, fDoubleQuoteOnly );
   PHB_ITEM pArray = hb_itemArrayNew( ulTokens );

   if( ulTokens )
   {
      ULONG ul, ulStart, ulToken;
      char cQuote = 0;
      
      for( ul = ulStart = ulToken = 0; ul < ulLen; ++ul )
      {
         if( cQuote )
         {
            if( szLine[ ul ] == cQuote )
               cQuote = 0;
         }
         else if( fSkipStrings && ( szLine[ ul ] == '"' ||
                  ( !fDoubleQuoteOnly && szLine[ ul ] == '\'' ) ) )
            cQuote = szLine[ ul ];
         else if( szLine[ ul ] == szDelim[ 0 ] &&
                  ( ulDelim == 1 || !memcmp( szLine + ul, szDelim, ulDelim ) ) )
         {
            hb_itemPutCL( hb_arrayGetItemPtr( pArray, ++ulToken ), szLine + ulStart, ul - ulStart );
            if( ulDelim == 1 && *szDelim == ' ' )
            {
               while( ul < ulLen && szLine[ ul + 1 ] == ' ' )
                  ++ul;
            }
            ulStart = ul + ulDelim;
         }
      }
      hb_itemPutCL( hb_arrayGetItemPtr( pArray, ++ulToken ), szLine + ulStart, ul - ulStart );
   }

   return pArray;
}

static void hb_tokenParam( int iDelim, ULONG ulSkip,
                           char ** pszLine, ULONG * pulLen,
                           char ** pszDelim, ULONG * pulDelim )
{
   char * szLine = hb_parc( 1 ), * szDelim = NULL;
   ULONG ulLen = hb_parclen( 1 ), ulDelim = 0;

   if( ulLen )
   {
      if( ulSkip )
      {
         szLine += ulSkip;
         if( ulLen <= ulSkip )
            ulLen = 0;
         else
            ulLen -= ulSkip;
      }

      ulDelim = hb_parclen( iDelim );
      if( ulDelim )
         szDelim = hb_parc( iDelim );
      else
      {
         szDelim = " ";
         ulDelim = 1;
      }

      if( ulDelim == 1 && *szDelim == ' ' )
      {
         while( ulLen && * szLine == ' ' )
         {
            ++szLine;
            --ulLen;
         }
         while( ulLen && szLine[ ulLen - 1 ] == ' ' )
            --ulLen;
      }
   }

   *pulLen = ulLen;
   *pulDelim = ulDelim;
   *pszLine = szLine;
   *pszDelim = szDelim;
}

HB_FUNC( HB_TOKENCOUNT )
{
   char * szLine, * szDelim;
   ULONG ulLen, ulDelim;

   hb_tokenParam( 2, 0, &szLine, &ulLen, &szDelim, &ulDelim );

   if( szLine )
      hb_retnint( hb_tokenCount( szLine, ulLen, szDelim, ulDelim,
                                 hb_parl( 3 ), hb_parl( 4 ) ) );
   else
      hb_retni( 0 );
}

HB_FUNC( HB_TOKENGET )
{
   char * szLine, * szDelim;
   ULONG ulLen, ulDelim;

   hb_tokenParam( 3, 0, &szLine, &ulLen, &szDelim, &ulDelim );

   if( szLine )
   {
      szLine = hb_tokenGet( szLine, ulLen, szDelim, ulDelim,
                            hb_parl( 4 ), hb_parl( 5 ),
                            hb_parnl( 2 ), &ulLen );
      hb_retclen( szLine, ulLen );
   }
   else
      hb_retc( NULL );
}

/* like HB_TOKENGET() but returns next token starting from passed position
 * (0 based) inside string, f.e.:
 *    HB_TOKENPTR( cString, @nTokPos, Chr( 9 ) ) -> cToken
 */
HB_FUNC( HB_TOKENPTR )
{
   char * szLine, * szDelim, * szToken;
   ULONG ulLen, ulDelim, ulSkip, ulToken;

   hb_tokenParam( 3, hb_parnl( 2 ), &szLine, &ulLen, &szDelim, &ulDelim );

   if( szLine )
   {
      szToken = hb_tokenGet( szLine, ulLen, szDelim, ulDelim,
                             hb_parl( 4 ), hb_parl( 5 ),
                             1, &ulToken );
      if( szToken && ulLen > ulToken )
         ulSkip = szToken - hb_parc( 1 ) + ulToken + ulDelim;
      else
         ulSkip = hb_parclen( 1 ) + 1;

      /* return position to start next search from */
      hb_stornl( ulSkip, 2 );
      /* return token */
      hb_retclen( szToken, ulToken );
   }
   else
      hb_retc( NULL );
}

HB_FUNC( HB_ATOKENS )
{
   char * szLine, * szDelim;
   ULONG ulLen, ulDelim;

   hb_tokenParam( 2, 0, &szLine, &ulLen, &szDelim, &ulDelim );

   if( szLine )
      hb_itemRelease( hb_itemReturnForward(
                         hb_tokenArray( szLine, ulLen, szDelim, ulDelim,
                                        hb_parl( 3 ), hb_parl( 4 ) ) ) );
   else
      hb_errRT_BASE_SubstR( EG_ARG, 1123, NULL, &hb_errFuncName, HB_ERR_ARGS_BASEPARAMS );
}
/*
HB_FUNC( HB_ATOKENS )
{
   PHB_ITEM pLine  = hb_param( 1, HB_IT_STRING );
   PHB_ITEM pDelim = hb_param( 2, HB_IT_STRING );

   if( pLine )
   {
      HB_ITEM_NEW ( Token );
      char cDelimiter = pDelim ? pDelim->item.asString.value[0] : 32;
      size_t i, iOffset = 0;
      BOOL bSkipStrings = hb_parl( 3 );
      BOOL bDoubleQuoteOnly = hb_parl( 4 );

      hb_arrayNew( &(HB_VM_STACK.Return), 0 );

      for( i = 0; i < pLine->item.asString.length; i++ )
      {
         if( bSkipStrings && ( pLine->item.asString.value[i] == '"'
                               || ( bDoubleQuoteOnly == FALSE && pLine->item.asString.value[i] == '\'' ) ) )
         {
            char cTerminator = pLine->item.asString.value[i];

            while( ++i < pLine->item.asString.length && pLine->item.asString.value[i] != cTerminator )
            {
            }
         }
         else if( pLine->item.asString.value[i] == cDelimiter )
         {
            hb_arrayAddForward( &(HB_VM_STACK.Return), hb_itemPutCL( &Token, pLine->item.asString.value + iOffset, i - iOffset ) );

            iOffset = i + 1;
         }
      }

      if( iOffset < pLine->item.asString.length )
      {
         hb_arrayAddForward( &(HB_VM_STACK.Return), hb_itemPutCL( &Token, pLine->item.asString.value + iOffset, pLine->item.asString.length - iOffset ) );
      }

   }
   else
   {
      hb_errRT_BASE_SubstR( EG_ARG, 1123, NULL, "HB_ATOKENS", 3, hb_paramError(1), hb_paramError(2), hb_paramError(3) );
      return;
   }
}
*/
HB_FUNC( __STRTOKEN )
{
   HB_FUNC_EXEC( HB_TOKENGET );
}

HB_FUNC( __STRTKPTR )
{
   HB_FUNC_EXEC( HB_TOKENPTR );
}

HB_FUNC( __STRTOKENCOUNT )
{
   HB_FUNC_EXEC( HB_TOKENCOUNT );
}