/*
 * $Id: cpbgwin.c,v 1.1 2005/11/19 15:52:04 likewolf Exp $
 */

/*
 * xHarbour Project source code:
 * National Collation Support Module (BGWIN)
 *
 * Copyright 2005 Rosen Vladimirov <kondor_ltd@dir.bg>
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

/* Language name: Bulgarian */
/* ISO language code (2 chars): BG */
/* Codepage: Windows-1251 */

#include <ctype.h>
#include "hbapi.h"
#include "hbapicdp.h"

static HB_CODEPAGE s_codepage = { "BG1251",
    CPID_1251,UNITB_1251,32,
    "��������������������������������",
    "��������������������������������",
    0,0,0,0,0,NULL,NULL,NULL,NULL,0,NULL };

HB_CODEPAGE_ANNOUNCE( BG1251 );

HB_CALL_ON_STARTUP_BEGIN( hb_codepage_Init_BG1251 )
   hb_cdpRegister( &s_codepage );
HB_CALL_ON_STARTUP_END( hb_codepage_Init_BG1251 )

#if defined(HB_PRAGMA_STARTUP)
   #pragma startup hb_codepage_Init_BG1251
#elif defined(HB_MSC_STARTUP)
   #if _MSC_VER >= 1010
      #pragma data_seg( ".CRT$XIY" )
      #pragma comment( linker, "/Merge:.CRT=.data" )
   #else
      #pragma data_seg( "XIY" )
   #endif
   static HB_$INITSYM hb_vm_auto_hb_codepage_Init_BG1251 = hb_codepage_Init_BG1251;
   #pragma data_seg()
#endif
