
/*
* SQLRDD connection
* Sample applications connect routine
* Copyright (c) 2005 - Marcelo Lombardo  <lombardo@uol.com.br>
* All Rights Reserved
*/

#include "pgs.ch"          // Needed if you plan to use native connection to Postgres
#include "mysql.ch"        // Needed if you plan to use native connection to MySQL
#include "oracle.ch"       // Needed if you plan to use native connection to Oracle
#include "firebird.ch"     // Needed if you plan to use native connection to Firebird

REQUEST SQLRDD             // SQLRDD should be linked in
REQUEST SR_ODBC            // Needed if you plan to connect with ODBC
REQUEST SR_PGS             // Needed if you plan to use native connection to Postgres
REQUEST SR_MYSQL           // Needed if you plan to use native connection to MySQL
REQUEST SR_ORACLE          // Needed if you plan to use native connection to Oracle
REQUEST SR_FIREBIRD        // Needed if you plan to use native connection to Firebird

REQUEST DBFNTX
REQUEST DBFCDX
REQUEST DBFFPT
REQUEST DBFDBT

/*------------------------------------------------------------------------*/

Function Connect( cDSN )

   local nCnn, nDrv, cDriver, nOpt, nDetected, hIniFile, aKeys, nKey, cConnString
   local oldScreen, hDsn

   hIniFile := HB_ReadIni( "sqlrdd.ini", .F.,,.F. )     // Read ini file in a hash table

   If hIniFile == NIL
      ? "Could not read from sqlrdd.ini"
      Quit
   EndIf

   If cDSN == NIL
      aKeys := HGetKeys( hIniFile )
      If len(aKeys) == 0
         ? "No connections available in sqlrdd.ini"
         Quit
      ElseIf len(aKeys) == 1
         nKey := 1
      Else
         clear screen
         @5,1 say PadC( "Choose connection option", 80 )
         nKey := achoice( 7, 20, 20, 60, aKeys )
         clear screen

         If nKey == 0
            ? "No connection selected"
            Quit
         EndIf
      EndIf

      hDsn := HGetValueAt( hIniFile, nKey )

      If !"CONNSTRING" IN hDsn
         ? "ConnString not found in " + aKeys[nKey]
         Quit
      EndIf
   Else
      If ! cDsn IN hIniFile
         ? "Connection [" + cDsn + "] not found in sqlrdd.ini"
         Quit
      EndIf

      hDsn := hIniFile[ cDsn ]

      If !"CONNSTRING" IN hDsn
         ? "ConnString not found in " + cDsn
         Quit
      EndIf

   EndIf

   cConnString := hDsn[ "CONNSTRING" ]
   nDetected   := DetectDBFromDSN( cConnString )

   If nDetected > SYSTEMID_UNKNOW
      ? "Connecting to", cConnString
      nCnn := SR_AddConnection( nDetected, cConnString )
   Else
      clear screen
      nOpt := Alert( "Please, select connection type", { "ODBC", "Postgres", "MySQL", "Oracle", "Firebird" } )
      If nOpt > 0
         nCnn := SR_AddConnection( If( nOpt = 1, CONNECT_ODBC, if( nOpt = 2, CONNECT_POSTGRES, if( nOpt = 3, CONNECT_MYSQL, if( nOpt = 4, CONNECT_ORACLE, CONNECT_FIREBIRD ) ) ) ), cConnString )
      Else
         ? "No connection type selected"
         Quit
      EndIf
   EndIf

   /* returns the connection handle or -1 if it fails */
   If nCnn < 0
      ? "Connection error. See sqlerror.log for details."
      Quit
   EndIf

Return .T.

/*------------------------------------------------------------------------*/
