/*
 * $Id: dbgentry.c,v 1.6 2005/09/27 05:31:13 paultucker Exp $
 */

/*
 * xHarbour Project source code:
 * Debugger entry routine
 *
 * Copyright 2005 Phil Krylov <phil a t newstar.rinet.ru>
 * www - http://www.xharbour.org
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
 * As a special exception, xHarbour license gives permission for
 * additional uses of the text contained in its release of xHarbour.
 *
 * The exception is that, if you link the xHarbour libraries with other
 * files to produce an executable, this does not by itself cause the
 * resulting executable to be covered by the GNU General Public License.
 * Your use of that executable is in no way restricted on account of
 * linking the xHarbour library code into it.
 *
 * This exception does not however invalidate any other reasons why
 * the executable file might be covered by the GNU General Public License.
 *
 * This exception applies only to the code released with this xHarbour
 * explicit exception.  If you add/copy code from other sources,
 * as the General Public License permits, the above exception does
 * not apply to the code that you add in this way.  To avoid misleading
 * anyone as to the status of such modified files, you must delete
 * this exception notice from them.
 *
 * If you write modifications of your own for xHarbour, it is your choice
 * whether to permit this exception to apply to your modifications.
 * If you do not wish that, delete this exception notice.
 *
 */

#include <ctype.h>

#include "hbvm.h"
#include "hbapierr.h"
#include "hbapiitm.h"
#include "hbdebug.ch"
#include "hbfast.h"
#include "classes.h"


#if defined( HB_OS_UNIX_COMPATIBLE )
#define FILENAME_EQUAL(s1, s2) ( !strcmp( (s1), (s2) ) )
#else
#define FILENAME_EQUAL(s1, s2) ( !hb_stricmp( (s1), (s2) ) )
#endif


#define ALLOC( size ) \
   hb_gcLock( hb_gcAlloc( size, NULL ) )

#define FREE( ptr ) \
   hb_gcFree( hb_gcUnlock( ptr ) )

#define STRDUP( source ) \
   strcpy( (char *) ALLOC( strlen( source ) + 1 ), source )

#define STRNDUP( dest, source, len ) \
   ( ( dest = strncpy( (char *) ALLOC( len + 1 ), source, len ) ), \
     ( dest[ len ] = '\0' ) )

#define ARRAY_ADD( type, array, length ) \
   ( (type *)( array_add( sizeof( type ), (void **)&array, &length ) ) )

#define ARRAY_DEL( type, array, length, index ) \
   if ( !--length ) \
      FREE( array ); \
   else \
      memmove( array + index, array + index + 1, sizeof( type ) * ( length - index ) );


typedef struct
{
   char *szModule;
   int nLine;
   char *szFunction;
} HB_BREAKPOINT;

typedef struct
{
   int nIndex;
   PHB_ITEM xValue;
} HB_TRACEPOINT;

typedef struct
{
   char *szName;
   char cType;
   int nFrame;
   int nIndex;
} HB_VARINFO;

typedef struct
{
   char *szExpr;
   HB_ITEM *pBlock;
   int nVars;
   char **aVars;
   HB_VARINFO *aScopes;
} HB_WATCHPOINT;

typedef struct
{
  char *szModule;
  char *szFunction;
  int nLine;
  int nProcLevel;
  int nLocals;
  HB_VARINFO *aLocals;
  int nStatics;
  HB_VARINFO *aStatics;
} HB_CALLSTACKINFO;

typedef struct
{
   char *szModule;
   int nVars;
   HB_VARINFO *aVars;
} HB_STATICMODULEINFO;

typedef struct
{
   BOOL bQuit;
   BOOL bGo;
   int nBreakPoints;
   HB_BREAKPOINT *aBreak;
   int nTracePoints;
   HB_TRACEPOINT *aTrace;
   int nWatchPoints;
   HB_WATCHPOINT *aWatch;
   BOOL bTraceOver;
   int nTraceLevel;
   BOOL bNextRoutine;
   BOOL bCodeBlock;
   BOOL bToCursor;
   int nToCursorLine;
   char *szToCursorModule;
   int nProcLevel;
   int nCallStackLen;
   HB_CALLSTACKINFO *aCallStack;
   int nStaticModules;
   HB_STATICMODULEINFO *aStaticModules;
   int bCBTrace;
   BOOL ( *pFunInvoke )( void );
} HB_DEBUGINFO;


static HB_DEBUGINFO s_Info = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static HB_DEBUGINFO *info = &s_Info;


static HB_ITEM *
hb_dbgActivateBreakArray( HB_DEBUGINFO *info );
static HB_ITEM *
hb_dbgActivateStaticModuleArray( HB_DEBUGINFO *info );
static HB_ITEM *
hb_dbgActivateVarArray( int nVars, HB_VARINFO *aVars );
static void
hb_dbgAddLocal( HB_DEBUGINFO *info, char *szName, int nIndex, int nFrame );
static void
hb_dbgAddStack( HB_DEBUGINFO *info, char *szName, int nProcLevel );
static void
hb_dbgAddStatic( HB_DEBUGINFO *info, char *szName, int nIndex, int nFrame );
static void
hb_dbgAddStaticModule( HB_DEBUGINFO *info, char *szName );
static void
hb_dbgAddVar( int *nVars, HB_VARINFO **aVars, char *szName, char cType, int nIndex, int nFrame );
static void
hb_dbgEndProc( HB_DEBUGINFO *info );
static HB_ITEM *
hb_dbgEval( HB_DEBUGINFO *info, HB_WATCHPOINT *watch );
static PHB_ITEM
hb_dbgEvalMacro( char *szExpr, PHB_ITEM pItem );
static PHB_ITEM
hb_dbgEvalMakeBlock( HB_WATCHPOINT *watch );
static PHB_ITEM
hb_dbgEvalResolve( HB_DEBUGINFO *info, HB_WATCHPOINT *watch );
static BOOL
hb_dbgIsAltD( void );
static BOOL
hb_dbgIsBreakPoint( HB_DEBUGINFO *info, char *szModule, int nLine );
static BOOL
hb_dbgIsInitStatics( void );
static BOOL
hb_dbgEqual( HB_ITEM *pItem1, HB_ITEM *pItem2 );
static PHB_ITEM
hb_dbgVarGet( HB_VARINFO *scope );
static void
hb_dbgVarSet( HB_VARINFO *scope, HB_ITEM *xNewValue );


static void *
array_add( int size, void **array, int *length )
{
   if ( ++*length == 1 )
   {
      *array = ALLOC( size );
   }
   else
   {
      void *new_array = ALLOC( ( *length ) * size );

      memmove( new_array, *array, ( *length - 1 ) * size );
      FREE( *array );
      *array = new_array;
   }
   return (void *)( (char *)( *array ) + size * ( *length - 1 ) );
}


static void
hb_dbgActivate( HB_DEBUGINFO *info )
{
   PHB_DYNS pDynSym = hb_dynsymFind( "__DBGENTRY" );

   if( pDynSym && pDynSym->pSymbol->value.pFunPtr )
   {
      int i;
      PHB_ITEM aCallStack = hb_itemArrayNew( info->nCallStackLen );
      PHB_ITEM aStaticModules;
      PHB_ITEM aBreak;

      for ( i = 0; i < info->nCallStackLen; i++ )
      {
         HB_CALLSTACKINFO *pEntry = &info->aCallStack[ i ];
         PHB_ITEM aEntry = hb_itemArrayNew( 6 );
         HB_ITEM *item;

         item = hb_itemPutC( NULL, pEntry->szModule );
         hb_arraySet( aEntry, 1, item );
         hb_itemRelease( item );

         item = hb_itemPutC( NULL, pEntry->szFunction );
         hb_arraySet( aEntry, 2, item );
         hb_itemRelease( item );

         item = hb_itemPutNL( NULL, pEntry->nLine );
         hb_arraySet( aEntry, 3, item );
         hb_itemRelease( item );

         item = hb_itemPutNL( NULL, pEntry->nProcLevel );
         hb_arraySet( aEntry, 4, item );
         hb_itemRelease( item );

         item = hb_dbgActivateVarArray( pEntry->nLocals, pEntry->aLocals );
         hb_arraySet( aEntry, 5, item );
         hb_itemRelease( item );

         item = hb_dbgActivateVarArray( pEntry->nStatics, pEntry->aStatics );
         hb_arraySet( aEntry, 6, item );
         hb_itemRelease( item );

         hb_arraySet( aCallStack, info->nCallStackLen - i, aEntry );
         hb_itemRelease( aEntry );
      }

      aStaticModules = hb_dbgActivateStaticModuleArray( info );
      aBreak = hb_dbgActivateBreakArray( info );

      hb_vmPushSymbol( pDynSym->pSymbol );
      hb_vmPushNil();
      hb_vmPushLong( HB_DBG_ACTIVATE );
      hb_vmPushPointer( info );
      hb_vmPushLong( info->nProcLevel );
      hb_vmPush( aCallStack );
      hb_vmPush( aStaticModules );
      hb_vmPush( aBreak );

      hb_itemRelease( aCallStack );
      hb_itemRelease( aStaticModules );
      hb_itemRelease( aBreak );

      hb_vmDo( 6 );
   }
}


static HB_ITEM *
hb_dbgActivateBreakArray( HB_DEBUGINFO *info )
{
   int i;
   PHB_ITEM pArray = hb_itemArrayNew( info->nBreakPoints );

   for ( i = 0; i < info->nBreakPoints; i++ )
   {
      PHB_ITEM pBreak = hb_itemArrayNew( 3 );
      PHB_ITEM item;

      if ( !info->aBreak[ i ].szFunction )
      {
         item = hb_itemPutNI( NULL, info->aBreak[ i ].nLine );
         hb_arraySet( pBreak, 1, item );
         hb_itemRelease( item );

         item = hb_itemPutC( NULL, info->aBreak[ i ].szModule );
         hb_arraySet( pBreak, 2, item );
         hb_itemRelease( item );
      }
      else
      {
         item = hb_itemPutC( NULL, info->aBreak[ i ].szFunction );
         hb_arraySet( pBreak, 3, item );
         hb_itemRelease( item );
      }

      hb_arraySet( pArray, i + 1, pBreak );
      hb_itemRelease( pBreak );
   }
   return pArray;
}


static HB_ITEM *
hb_dbgActivateStaticModuleArray( HB_DEBUGINFO *info )
{
   int i;
   PHB_ITEM pArray = hb_itemArrayNew( info->nStaticModules );

   for ( i = 0; i < info->nStaticModules; i++ )
   {
      PHB_ITEM pStaticModule = hb_itemArrayNew( 2 );
      PHB_ITEM item;

      item = hb_itemPutC( NULL, info->aStaticModules[ i ].szModule );
      hb_arraySet( pStaticModule, 1, item );
      hb_itemRelease( item );

      item = hb_dbgActivateVarArray( info->aStaticModules[ i ].nVars,
                                     info->aStaticModules[ i ].aVars );
      hb_arraySet( pStaticModule, 2, item );
      hb_itemRelease( item );

      hb_arraySet( pArray, i + 1, pStaticModule );
      hb_itemRelease( pStaticModule );
   }
   return pArray;
}


static HB_ITEM *
hb_dbgActivateVarArray( int nVars, HB_VARINFO *aVars )
{
   int i;
   PHB_ITEM pArray = hb_itemArrayNew( nVars );

   for ( i = 0; i < nVars; i++ )
   {
      PHB_ITEM aVar = hb_itemArrayNew( 4 );
      PHB_ITEM item;

      item = hb_itemPutC( NULL, aVars[ i ].szName );
      hb_arraySet( aVar, 1, item );
      hb_itemRelease( item );

      item = hb_itemPutNL( NULL, aVars[ i ].nIndex );
      hb_arraySet( aVar, 2, item );
      hb_itemRelease( item );

      item = hb_itemPutCL( NULL, &aVars[ i ].cType, 1 );
      hb_arraySet( aVar, 3, item );
      hb_itemRelease( item );

      item = hb_itemPutNL( NULL, aVars[ i ].nFrame );
      hb_arraySet( aVar, 4, item );
      hb_itemRelease( item );

      hb_arraySet( pArray, i + 1, aVar );
      hb_itemRelease( aVar );
   }
   return pArray;
}


HB_EXPORT void
hb_dbgEntry( int nMode, int nLine, char *szName, int nIndex, int nFrame )
{
   int i;
   ULONG nProcLevel;
   char szProcName[ HB_SYMBOL_NAME_LEN + HB_SYMBOL_NAME_LEN + 5 ];

   if ( info->bQuit )
      return;

   switch ( nMode )
   {
      case HB_DBG_MODULENAME:
         HB_TRACE( HB_TR_DEBUG, ( "MODULENAME %s", szName ) );
         
         hb_procinfo( 0, szProcName, NULL, NULL );
         if ( !strcmp( szProcName, "(_INITSTATICS)" ) )
         {
            hb_dbgAddStaticModule( info, szName );
         }
         else if ( !strncmp( szProcName, "(b)", 3 ) )
            info->bCodeBlock = TRUE;
         else if ( info->bNextRoutine )
            info->bNextRoutine = FALSE;
         hb_dbgAddStack( info, szName, hb_dbg_ProcLevel() );
         for ( i = 0; i < info->nBreakPoints; i++ )
         {
            if ( info->aBreak[ i ].szFunction
                 && !strcmp( info->aBreak[ i ].szFunction, szProcName ) )
            {
               hb_dbg_InvokeDebug( TRUE );
               break;
            }
         }
         return;

      case HB_DBG_LOCALNAME:
         HB_TRACE( HB_TR_DEBUG, ( "LOCALNAME %s index %d", szName, nIndex ) );
         
         hb_dbgAddLocal( info, szName, nIndex, hb_dbg_ProcLevel() );
         return;

      case HB_DBG_STATICNAME:
         HB_TRACE( HB_TR_DEBUG, ( "STATICNAME %s index %d frame %d", szName, nIndex, nFrame ) );
         
         hb_dbgAddStatic( info, szName, nIndex, nFrame );
         return;

      case HB_DBG_SHOWLINE:
      {
         HB_CALLSTACKINFO *pTop = &info->aCallStack[ info->nCallStackLen - 1 ];
         BOOL bOldClsScope;
         
         HB_TRACE( HB_TR_DEBUG, ( "SHOWLINE %d", nLine ) );

         nProcLevel = hb_dbg_ProcLevel();

         /* Check if we've hit a tracepoint */
         bOldClsScope = hb_clsSetScope( FALSE );
         for ( i = 0; i < info->nTracePoints; i++ )
         {
            HB_TRACEPOINT *tp = &info->aTrace[ i ];
            HB_ITEM *xValue;

            xValue = hb_dbgEval( info, &info->aWatch[ tp->nIndex ] );
            if ( !xValue )
               xValue = hb_itemNew( NULL );

            if ( xValue->type != tp->xValue->type || !hb_dbgEqual( xValue, tp->xValue ) )
            {
               hb_itemCopy( tp->xValue, xValue );
               hb_itemRelease( xValue );

               pTop->nLine = nLine;
               info->nProcLevel = nProcLevel - ( hb_dbgIsAltD() ? 2 : 0 );
               info->bTraceOver = FALSE;
               info->bCodeBlock = FALSE;
               info->bGo = FALSE;
               if ( info->bToCursor )
               {
                  info->bToCursor = FALSE;
                  FREE( info->szToCursorModule );
               }
               info->bNextRoutine = FALSE;

               hb_dbgActivate( info );
               return;
            }
            hb_itemRelease( xValue );
         }
         hb_clsSetScope( bOldClsScope );

         if ( hb_dbgIsBreakPoint( info, pTop->szModule, nLine )
              || hb_dbg_InvokeDebug( FALSE )
              || ( info->pFunInvoke && info->pFunInvoke() ) )
         {
            info->bTraceOver = FALSE;
            if ( info->bToCursor )
            {
               info->bToCursor = FALSE;
               FREE( info->szToCursorModule );
            }
            info->bNextRoutine = FALSE;
            info->bGo = FALSE;
         }

         /* Check if we must skip every level above info->nTraceLevel */
         if ( info->bTraceOver )
         {
            if ( info->nTraceLevel < info->nCallStackLen )
               return;
            info->bTraceOver = FALSE;
         }

         /* Check if we're skipping to a specific line of source */
         if ( info->bToCursor )
         {
            if ( nLine == info->nToCursorLine
                 && FILENAME_EQUAL( pTop->szModule, info->szToCursorModule ) )
            {
               FREE( info->szToCursorModule );
               info->bToCursor = FALSE;
            }
            else
            {
               return;
            }
         }

         /* Check if'we skipping to the end of current routine */
         if ( info->bNextRoutine )
            return;

         if ( info->bCodeBlock )
         {
            info->bCodeBlock = FALSE;
            if ( !info->bCBTrace )
               return;
         }

         pTop->nLine = nLine;
         if ( !info->bGo )
         {
            info->nProcLevel = nProcLevel - ( hb_dbgIsAltD() ? 2 : 0 );
            hb_dbgActivate( info );
         }
         return;
      }

      case HB_DBG_ENDPROC:
         if ( info->bQuit )
            return;
         
         HB_TRACE( HB_TR_DEBUG, ( "ENDPROC", nLine ) );

         info->bCodeBlock = FALSE;
         hb_dbgEndProc( info );
         return;
   }
}


HB_EXPORT void
hb_dbgAddBreak( void *handle, char *cModule, int nLine, char *szFunction )
{
   HB_DEBUGINFO *info = (HB_DEBUGINFO *)handle;
   HB_BREAKPOINT *pBreak;

   pBreak = ARRAY_ADD( HB_BREAKPOINT, info->aBreak, info->nBreakPoints );
   pBreak->szModule = STRDUP( cModule );
   pBreak->nLine = nLine;
   if ( szFunction )
   {
      pBreak->szFunction = STRDUP( szFunction );
   }
   else
   {
      pBreak->szFunction = NULL;
   }
}


static void
hb_dbgAddLocal( HB_DEBUGINFO *info, char *szName, int nIndex, int nFrame )
{
   HB_CALLSTACKINFO *top = &info->aCallStack[ info->nCallStackLen - 1 ];

   hb_dbgAddVar( &top->nLocals, &top->aLocals, szName, 'L', nIndex, nFrame );
}


static void
hb_dbgAddStack( HB_DEBUGINFO *info, char *szName, int nProcLevel )
{
   HB_CALLSTACKINFO *top;
   char *szFunction = strrchr( szName, ':' );
   char *tmp;

   if ( szFunction )
   {
      szFunction++;
   }
   top = ARRAY_ADD( HB_CALLSTACKINFO, info->aCallStack, info->nCallStackLen );
   if ( info->bCodeBlock )
   {
      char tmp[ HB_SYMBOL_NAME_LEN + HB_SYMBOL_NAME_LEN + 5 ] = "(b)";

      strcpy( tmp + 3, szFunction );
      top->szFunction = STRDUP( tmp );
   }
   else
   {
      top->szFunction = szFunction ? STRDUP( szFunction ) : STRDUP( "(_INITSTATICS)" );
   }
   tmp = strrchr( szName, '/' );
   if ( tmp )
      szName = tmp + 1;
   tmp = strrchr( szName, '\\' );
   if ( tmp )
      szName = tmp + 1;
   if ( szFunction )
   {
      STRNDUP( top->szModule, szName, szFunction - szName - 1 );
   }
   else
   {
      top->szModule = STRDUP( szName );
   }
   top->nProcLevel = nProcLevel;
   top->nLine = 0;
   top->nLocals = 0;
   top->nStatics = 0;
}


static void
hb_dbgAddStatic( HB_DEBUGINFO *info, char *szName, int nIndex, int nFrame )
{
   HB_STATICMODULEINFO *st;

   if ( hb_dbgIsInitStatics() )
   {
      st = &info->aStaticModules[ info->nStaticModules - 1 ];
      hb_dbgAddVar( &st->nVars, &st->aVars, szName, 'S', nIndex, nFrame );
   }
   else
   {
      HB_CALLSTACKINFO *top = &info->aCallStack[ info->nCallStackLen - 1 ];

      hb_dbgAddVar( &top->nStatics, &top->aStatics, szName, 'S', nIndex, nFrame );
   }
}


static void
hb_dbgAddStaticModule( HB_DEBUGINFO *info, char *szName )
{
   HB_STATICMODULEINFO *pStaticModule;

   pStaticModule = ARRAY_ADD( HB_STATICMODULEINFO, info->aStaticModules, info->nStaticModules );
   pStaticModule->szModule = szName;
   pStaticModule->nVars = 0;
}


static void
hb_dbgAddVar( int *nVars, HB_VARINFO **aVars, char *szName, char cType, int nIndex, int nFrame )
{
   HB_VARINFO *var;

   var = ARRAY_ADD( HB_VARINFO, *aVars, *nVars );
   var->szName = szName;
   var->cType = cType;
   var->nIndex = nIndex;
   var->nFrame = nFrame;
}


HB_EXPORT void
hb_dbgAddWatch( void *handle, char *szExpr, BOOL bTrace )
{
   HB_DEBUGINFO *info = (HB_DEBUGINFO *)handle;
   HB_WATCHPOINT *pWatch;

   pWatch = ARRAY_ADD( HB_WATCHPOINT, info->aWatch, info->nWatchPoints );
   pWatch->szExpr = STRDUP( szExpr );
   pWatch->pBlock = NULL;

   if ( bTrace )
   {
     HB_TRACEPOINT *pTrace = ARRAY_ADD( HB_TRACEPOINT, info->aTrace, info->nTracePoints );

     pTrace->nIndex = info->nWatchPoints - 1;
     pTrace->xValue = hb_dbgEval( info, pWatch );
   }
}


static void
hb_dbgClearWatch( HB_WATCHPOINT *pWatch )
{
   FREE( pWatch->szExpr );
   if ( pWatch->pBlock )
   {
      hb_itemRelease( pWatch->pBlock );
   }
   if ( pWatch->nVars )
   {
      FREE( pWatch->aVars );
   }
}


HB_EXPORT void
hb_dbgDelBreak( void *handle, int nBreak )
{
   HB_DEBUGINFO *info = (HB_DEBUGINFO *)handle;
   HB_BREAKPOINT *pBreak = &info->aBreak[ nBreak ];

   FREE( pBreak->szModule );
   if ( pBreak->szFunction )
   {
      FREE( pBreak->szFunction );
   }
   ARRAY_DEL( HB_BREAKPOINT, info->aBreak, info->nBreakPoints, nBreak );
}


HB_EXPORT void
hb_dbgDelWatch( void *handle, int nWatch )
{
   HB_DEBUGINFO *info = (HB_DEBUGINFO *)handle;
   HB_WATCHPOINT *pWatch = &info->aWatch[ nWatch ];
   int i;

   hb_dbgClearWatch( pWatch );
   ARRAY_DEL( HB_WATCHPOINT, info->aWatch, info->nWatchPoints, nWatch );

   for ( i = 0; i < info->nTracePoints; i++ )
   {
      HB_TRACEPOINT *pTrace = &info->aTrace[ i ];

      if ( pTrace->nIndex == nWatch )
      {
         if ( pTrace->xValue )
         {
            hb_itemRelease( pTrace->xValue );
         }
         ARRAY_DEL( HB_TRACEPOINT, info->aTrace, info->nTracePoints, i );
         i--;
      }
      else if ( pTrace->nIndex > nWatch )
      {
         pTrace->nIndex--;
      }
   }
}


static void
hb_dbgEndProc( HB_DEBUGINFO *info )
{
   HB_CALLSTACKINFO *top;

   if ( !info->nCallStackLen )
      return;

   top = &info->aCallStack[ info->nCallStackLen - 1 ];
   FREE( top->szFunction );
   FREE( top->szModule );
   if ( top->nLocals )
   {
      FREE( top->aLocals );
   }
   if ( top->nStatics )
   {
      FREE( top->aStatics );
   }
   info->nCallStackLen--;
   if ( !info->nCallStackLen )
   {
      FREE( info->aCallStack );
   }
}


static BOOL
hb_dbgEqual( HB_ITEM *pItem1, HB_ITEM *pItem2 )
{
   if ( pItem1->type != pItem2->type )
      return FALSE;
   if ( HB_IS_NIL( pItem1 ) )
      return HB_IS_NIL( pItem2 );
   if ( HB_IS_LOGICAL( pItem1 ) )
      return ( pItem1->item.asLogical.value == pItem2->item.asLogical.value );
   if ( HB_IS_POINTER( pItem1 ) )
      return ( pItem1->item.asPointer.value == pItem2->item.asPointer.value );
   if ( HB_IS_STRING( pItem1 ) )
      return !hb_itemStrCmp( pItem1, pItem2, TRUE );
   if ( HB_IS_NUMINT( pItem1 ) )
      return ( HB_ITEM_GET_NUMINTRAW( pItem1 ) == HB_ITEM_GET_NUMINTRAW( pItem2 ) );
   if ( HB_IS_NUMERIC( pItem1 ) )
      return ( hb_itemGetND( pItem1 ) == hb_itemGetND( pItem2 ) );
   if ( HB_IS_ARRAY( pItem1 ) )
      return ( pItem1->item.asArray.value == pItem2->item.asArray.value );
   if ( HB_IS_HASH( pItem1 ) )
      return ( pItem1->item.asHash.value == pItem2->item.asHash.value );
   return FALSE;
}


static HB_ITEM *
hb_dbgEval( HB_DEBUGINFO *info, HB_WATCHPOINT *watch )
{
   HB_ITEM *xResult = NULL;

   HB_TRACE( HB_TR_DEBUG, ( "expr %s", watch->szExpr ) );
   
   /* Check if we have a cached pBlock */
   if ( !watch->pBlock )
   {
      watch->pBlock = hb_dbgEvalMakeBlock( watch );
   }

   if ( watch->pBlock )
   {
      PHB_ITEM aVars = hb_dbgEvalResolve( info, watch );
      HB_ITEM *aNewVars = hb_itemArrayNew( watch->nVars );
      int i;

      hb_arrayCopy( aVars, aNewVars, NULL, NULL, NULL );

      xResult = hb_itemDo( watch->pBlock, 1, aNewVars );

      for ( i = 0; i < watch->nVars; i++ )
      {
         HB_ITEM *xOldValue = hb_itemArrayGet( aVars, i + 1 );
         HB_ITEM *xNewValue = hb_itemArrayGet( aNewVars, i + 1 );

         if ( !hb_dbgEqual( xOldValue, xNewValue ) )
         {
            hb_dbgVarSet( &watch->aScopes[ i ], xNewValue );
         }
      }

      hb_itemRelease( aVars );
      hb_itemRelease( aNewVars );
      for ( i = 0; i < watch->nVars; i++ )
      {
         if ( watch->aScopes[ i ].cType == 'M' )
         {
            FREE( watch->aScopes[ i ].szName );
         }
      }
      if ( watch->nVars )
      {
         FREE( watch->aScopes );
      }
   }
   return xResult;
}


static PHB_ITEM
hb_dbgEvalMacro( char *szExpr, PHB_ITEM pItem )
{
   PHB_ITEM pStr;
   const char *type;

   pStr = hb_itemPutC( NULL, szExpr );
   type = hb_macroGetType( pStr, 0 );
   hb_itemRelease( pStr );
   if ( ( !strcmp( type, "U" ) || !strcmp( type, "UE" ) ) )
   {
      return NULL;
   }
   hb_vmPushString( szExpr, strlen( szExpr ) );
   hb_macroGetValue( hb_stackItemFromTop( -1 ), 0, 0 );
   hb_itemForwardValue( pItem, hb_stackItemFromTop( -1 ) );
   hb_stackPop();
   return pItem;
}


#define IS_IDENT_START( c ) ( isalpha( (c) ) || (c) == '_' )
#define IS_IDENT_CHAR( c ) ( IS_IDENT_START( (c) ) || isdigit( (c) ) )

static PHB_ITEM
hb_dbgEvalMakeBlock( HB_WATCHPOINT *watch )
{
   int i = 0;
   PHB_ITEM pBlock;
   BOOL bAfterId = FALSE;

   watch->nVars = 0;
   while ( watch->szExpr[ i ] )
   {
      char c = watch->szExpr[ i ];

      if ( IS_IDENT_START( c ) )
      {
         int nStart = i, nLen;
         int j = i;
         char *szWord;

         while ( c && IS_IDENT_CHAR( c ) )
         {
            j++;
            c = watch->szExpr[ j ];
         }
         nLen = j - i;
         STRNDUP( szWord, watch->szExpr + i, nLen );
         i = j;
         if ( c )
         {
            while ( watch->szExpr[ i ] && watch->szExpr[ i ] == ' ' )
            {
               i++;
            }
            if ( watch->szExpr[ i ] == '(' )
            {
               FREE( szWord );
               continue;
            }
            if ( watch->szExpr[ i ] == '-' && watch->szExpr[ i + 1 ] == '>' )
            {
               i += 2;
               while ( ( c = watch->szExpr[ i ] ) != '\0' && IS_IDENT_CHAR( c ) )
               {
                  i++;
               }
               FREE( szWord );
               continue;
            }
         }
         hb_strupr( szWord );
         for ( j = 0; j < watch->nVars; j++ )
         {
            if ( !strcmp( szWord, watch->aVars[ j ] ) )
            {
               break;
            }
         }
         if ( j == watch->nVars )
         {
            *ARRAY_ADD( char *, watch->aVars, watch->nVars ) = szWord;
         }
         else
         {
            FREE( szWord );
         }
         {
            char *t = (char *) ALLOC( strlen( watch->szExpr ) - nLen + 9 + 1 );

            memmove( t, watch->szExpr, nStart );
            memmove( t + nStart, "__dbg[", 6 );
            t[ nStart + 6 ] = '0' + ( ( j + 1 ) / 10 );
            t[ nStart + 7 ] = '0' + ( ( j + 1 ) % 10 );
            t[ nStart + 8 ] = ']';
            strcpy( t + nStart + 9, watch->szExpr + nStart + nLen );
            FREE( watch->szExpr );
            watch->szExpr = t;
            i = nStart + 9;
         }
         bAfterId = TRUE;
         continue;
      }
      if ( c == '.' )
      {
         if ( watch->szExpr[ i + 1 ]
              && !strchr( "TtFf", watch->szExpr[ i + 1 ] )
              && watch->szExpr[ i + 2 ] == '.' )
         {
            i += 3;
         }
         else if ( !hb_strnicmp( watch->szExpr + i + 1, "OR.", 3 ) )
         {
            i += 4;
         }
         else if ( !hb_strnicmp( watch->szExpr + i + 1, "AND.", 4 )
                   && !hb_strnicmp( watch->szExpr + i + 1, "NOT.", 4 ) )
         {
            i += 5;
         }
         else
         {
            i++;
         }
         bAfterId = FALSE;
         continue;
      }
      if ( c == ':'
           || ( c == '-' && watch->szExpr[ i + 1 ] == '>'
                && IS_IDENT_START( watch->szExpr[ i + 2 ] ) ) )
      {
         if ( c == '-' )
         {
            i++;
         }
         i++;
         while ( watch->szExpr[ i ] && IS_IDENT_CHAR( watch->szExpr[ i ] ) )
         {
            i++;
         }
         bAfterId = TRUE;
         continue;
      }
      if ( strchr( " !#$=<>(+-*/%^|,{&", c ) )
      {
         i++;
         bAfterId = FALSE;
         continue;
      }
      if ( c == '\'' || c == '\"' )
      {
         i++;
         while ( watch->szExpr[ i ] && watch->szExpr[ i ] != c )
         {
            i++;
         }
         if ( watch->szExpr[ i ] )
         {
            i++;
         }
         bAfterId = TRUE;
         continue;
      }
      if ( c == '[' )
      {
         i++;
         if ( bAfterId )
         {
            bAfterId = FALSE;
         }
         else
         {
            while ( watch->szExpr[ i ] && watch->szExpr[ i ] != ']' )
            {
               i++;
            }
            if ( watch->szExpr[ i ] )
            {
               i++;
            }
            bAfterId = TRUE;
         }
         continue;
      }
      i++;
   }
   {
      HB_ITEM block;
      char *s = (char *) ALLOC( 8 + strlen( watch->szExpr ) + 1 + 1 );

      strcpy( s, "{|__dbg|" );
      strcat( s, watch->szExpr );
      strcat( s, "}" );
      block.type = 0;
      pBlock = hb_dbgEvalMacro( s, &block );
      if ( pBlock )
      {
         pBlock = hb_itemNew( pBlock );
         hb_itemClear( &block );
      }
      FREE( s );
   }
   return pBlock;
}


static PHB_ITEM
hb_dbgEvalResolve( HB_DEBUGINFO *info, HB_WATCHPOINT *watch )
{
   int i;
   HB_CALLSTACKINFO *top = &info->aCallStack[ info->nCallStackLen - 1 ];
   HB_ITEM *aVars = hb_itemArrayNew( watch->nVars );
   HB_VARINFO *scopes;
   HB_STATICMODULEINFO *static_module = NULL;
   int nProcLevel;

   if ( !watch->nVars )
   {
      return aVars;
   }
   
   scopes = (HB_VARINFO *) ALLOC( watch->nVars * sizeof( HB_VARINFO ) );
   nProcLevel = hb_dbg_ProcLevel();
   
   for ( i = 0; i < info->nStaticModules; i++ )
   {
      if ( !strcmp( info->aStaticModules[ i ].szModule, top->szModule ) )
      {
         static_module = &info->aStaticModules[ i ];
         break;
      }
   }

   for ( i = 0; i < watch->nVars; i++ )
   {
      char *name = watch->aVars[ i ];
      HB_VARINFO *var;
      int j;
      PHB_ITEM pItem;

      for ( j = 0; j < top->nLocals; j++ )
      {
         var = &top->aLocals[ j ];
         if ( !strcmp( name, var->szName ) )
         {
            scopes[ i ].cType = 'L';
            scopes[ i ].nFrame = nProcLevel - var->nFrame;
            scopes[ i ].nIndex = var->nIndex;
            hb_itemArrayPut( aVars, i + 1, hb_dbgVarGet( &scopes[ i ] ) );
            break;
         }
      }
      if ( j < top->nLocals )
         continue;

      for ( j = 0; j < top->nStatics; j++ )
      {
         var = &top->aStatics[ j ];
         if ( !strcmp( name, var->szName ) )
         {
            scopes[ i ].cType = 'S';
            scopes[ i ].nFrame = var->nFrame;
            scopes[ i ].nIndex = var->nIndex;
            hb_itemArrayPut( aVars, i + 1, hb_dbgVarGet( &scopes[ i ] ) );
            break;
         }
      }
      if ( j < top->nStatics )
         continue;

      if ( static_module )
      {
         for ( j = 0; j < static_module->nVars; j++ )
         {
            var = &static_module->aVars[ j ];
            if ( !strcmp( name, var->szName ) )
            {
               scopes[ i ].cType = 'S';
               scopes[ i ].nFrame = var->nFrame;
               scopes[ i ].nIndex = var->nIndex;
               hb_itemArrayPut( aVars, i + 1, hb_dbgVarGet( &scopes[ i ] ) );
               break;
            }
         }
         if ( j < static_module->nVars )
            continue;
      }
      scopes[ i ].cType = 'M';
      scopes[ i ].szName = STRDUP( name );
      pItem = hb_dbgVarGet( &scopes[ i ] );
      if ( pItem )
      {
         hb_itemArrayPut( aVars, i + 1, pItem );
      }
   }
   watch->aScopes = scopes;
   return aVars;
}


HB_EXPORT PHB_ITEM
hb_dbgGetExpressionValue( void *handle, char *expression )
{
   HB_DEBUGINFO *info = (HB_DEBUGINFO *)handle;
   PHB_ITEM result;
   HB_WATCHPOINT point;

   point.szExpr = STRDUP( expression );
   point.pBlock = NULL;
   point.nVars = 0;
   
   result = hb_dbgEval( info, &point );

   hb_dbgClearWatch( &point );
   
   return result;
}


HB_EXPORT PHB_ITEM
hb_dbgGetWatchValue( void *handle, int nWatch )
{
   HB_DEBUGINFO *info = (HB_DEBUGINFO *)handle;
   
   return hb_dbgEval( info, &( info->aWatch[ nWatch ] ) );
}


static BOOL
hb_dbgIsAltD( void )
{
   char szName[ HB_SYMBOL_NAME_LEN + HB_SYMBOL_NAME_LEN + 5];

   hb_procinfo( 1, szName, NULL, NULL );
   return !strcmp( szName, "ALTD" );
}


static BOOL
hb_dbgIsBreakPoint( HB_DEBUGINFO *info, char *szModule, int nLine )
{
   int i;

   for ( i = 0; i < info->nBreakPoints; i++ )
   {
      HB_BREAKPOINT *point = &info->aBreak[ i ];
      
      if ( point->nLine == nLine
           && FILENAME_EQUAL( szModule, point->szModule ) )
         return TRUE;
   }
   return FALSE;
}


static BOOL
hb_dbgIsInitStatics( void )
{
   char szName[ HB_SYMBOL_NAME_LEN + HB_SYMBOL_NAME_LEN + 5];

   hb_procinfo( 0, szName, NULL, NULL );
   return !strcmp( szName, "(_INITSTATICS)" );
}


HB_EXPORT void
hb_dbgSetCBTrace( void *handle, BOOL bCBTrace )
{
   HB_DEBUGINFO *info = (HB_DEBUGINFO *)handle;
   
   info->bCBTrace = bCBTrace;
}


HB_EXPORT void
hb_dbgSetGo( void *handle )
{
   HB_DEBUGINFO *info = (HB_DEBUGINFO *)handle;
   
   info->bGo = TRUE;
}


HB_EXPORT void
hb_dbgSetInvoke( void *handle, BOOL ( *pFunInvoke )( void ) )
{
   HB_DEBUGINFO *info = (HB_DEBUGINFO *)handle;

   info->pFunInvoke = pFunInvoke;
}


HB_EXPORT void
hb_dbgSetNextRoutine( void *handle )
{
   HB_DEBUGINFO *info = (HB_DEBUGINFO *)handle;
   
   info->bNextRoutine = TRUE;
}


HB_EXPORT void
hb_dbgSetQuit( void *handle )
{
   HB_DEBUGINFO *info = (HB_DEBUGINFO *)handle;
   
   info->bQuit = TRUE;
   while ( info->nWatchPoints )
   {
      hb_dbgDelWatch( info, info->nWatchPoints - 1 );
   }
   while ( info->nBreakPoints )
   {
      hb_dbgDelBreak( info, info->nBreakPoints - 1 );
   }
   while ( info->nCallStackLen )
   {
      hb_dbgEndProc( info );
   }
}


HB_EXPORT void
hb_dbgSetToCursor( void *handle, char *szModule, int nLine )
{
   HB_DEBUGINFO *info = (HB_DEBUGINFO *)handle;
   
   info->bToCursor = TRUE;
   info->szToCursorModule = STRDUP( szModule );
   info->nToCursorLine = nLine;
}


HB_EXPORT void
hb_dbgSetTrace( void *handle )
{
   HB_DEBUGINFO *info = (HB_DEBUGINFO *)handle;
   
   info->bTraceOver = TRUE;
   info->nTraceLevel = info->nCallStackLen;
}


HB_EXPORT void
hb_dbgSetWatch( void *handle, int nWatch, char *szExpr, BOOL bTrace )
{
   HB_DEBUGINFO *info = (HB_DEBUGINFO *)handle;
   HB_WATCHPOINT *pWatch = &info->aWatch[ nWatch ];
   int i;
   
   hb_dbgClearWatch( pWatch );
   pWatch->szExpr = STRDUP( szExpr );
   pWatch->pBlock = NULL;
   for ( i = 0; i < info->nTracePoints; i++ )
   {
      HB_TRACEPOINT *pTrace = &info->aTrace[ i ];
      
      if ( pTrace->nIndex == nWatch )
      {
         if ( pTrace->xValue )
         {
            hb_itemRelease( pTrace->xValue );
         }
         ARRAY_DEL( HB_TRACEPOINT, info->aTrace, info->nTracePoints, i );
         break;
      }
   }
   if ( bTrace )
   {
      HB_TRACEPOINT *pTrace = ARRAY_ADD( HB_TRACEPOINT, info->aTrace, info->nTracePoints );

      pTrace->nIndex = nWatch;
      pTrace->xValue = hb_dbgEval( info, pWatch );
   }
}


static PHB_ITEM
hb_dbgVarGet( HB_VARINFO *scope )
{
   switch ( scope->cType )
   {
      case 'L':
         return hb_dbg_vmVarLGet( scope->nFrame, scope->nIndex );
      case 'S':
         return hb_dbg_vmVarSGet( scope->nFrame, scope->nIndex );
      case 'M':
      {
         HB_HANDLE hMemVar = hb_memvarGetVarHandle( scope->szName );

         return hb_memvarGetValueByHandle( hMemVar );
      }
      default:
         return NULL;
   }
}


static void
hb_dbgVarSet( HB_VARINFO *scope, HB_ITEM *xNewValue )
{
   switch ( scope->cType )
   {
      case 'L':
      case 'S':
         hb_itemCopy( hb_dbgVarGet( scope ), xNewValue );
         break;
      case 'M':
      {
         PHB_DYNS pDynSym = hb_dynsymFind( "__MVPUT" );
         
         if ( pDynSym && pDynSym->pSymbol->value.pFunPtr )
         {
            hb_vmPushSymbol( pDynSym->pSymbol );
            hb_vmPushNil();
            hb_vmPushString( scope->szName, strlen( scope->szName ) );
            hb_vmPush( xNewValue );
            hb_vmDo( 2 );
         }
         break;
      }
   }
}