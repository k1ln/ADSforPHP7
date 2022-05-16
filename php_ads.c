/*
   +----------------------------------------------------------------------+
   | PHP version 4.0                                                      |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2014 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Stig S�ther Bakken <ssb@fast.no>                            |
   |          Andreas Karajannis <Andreas.Karajannis@gmd.de>              |
   |          Frank M. Kromann <frank@frontbase.com> Support for DB/2 CLI |
   |          Kevin N. Shallow <kshallow@tampabay.rr.com> Velocis Support |
   |          Daniel R. Kalowsky <kalowsky@php.net>                       |
   |          SAP AG or an SAP affiliate company <Advantage@sap.com>      |
   +----------------------------------------------------------------------+
   edited by Kilian Hertel to work with php7 on 2021/12/15 at Tobax Software GmbH Leverkusen
 */



/* $Id: php_ads.c,v 1.36 2008-12-22 10:54:09 AdsLinux Exp $ */

#include "php.h"
#include "php_globals.h"
#include <syslog.h>
#include "ext/standard/info.h"

#include "ext/standard/php_string.h"
#include "ext/standard/php_standard.h"

#include "php_ads.h"
#include "php_globals.h"

#include <stdbool.h>
#include <fcntl.h>
#include "ext/standard/head.h"
#include "php_ini.h"

#ifdef PHP_WIN32
   #include <winsock.h>
   #if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 3
      #include "zend_arg_defs.c"
   #endif
#else
   #include "build-defs.h"
#endif
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
/*
 * not defined elsewhere
 */

#if PHP_MAJOR_VERSION == 4
   #define OnUpdateLong OnUpdateInt
   static unsigned char a3_arg3_and_3_force_ref[] = { 3, BYREF_NONE, BYREF_FORCE, BYREF_FORCE};
#endif

#ifndef TRUE
   #define TRUE 1
   #define FALSE 0
#endif

void ads_do_connect(INTERNAL_FUNCTION_PARAMETERS, int persistent);

static int le_result, le_conn, le_pconn;

#define SAFE_SQL_NTS(n) ((SQLSMALLINT) ((n)?(SQL_NTS):0))

/* Utility definition for PHP 5.3, as this version changed some data structures. */
#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 3
   #define PHP_5_3_OR_GREATER 1
#elif PHP_MAJOR_VERSION > 5
   #define PHP_5_3_OR_GREATER 1
#else
   #undef PHP_5_3_OR_GREATER
#endif

/* {{{ arginfo */
ZEND_BEGIN_ARG_INFO(arginfo_ads_close_all, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_binmode, 0, 0, 2)
ZEND_ARG_INFO(0, result_id)
ZEND_ARG_INFO(0, mode)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_longreadlen, 0, 0, 2)
ZEND_ARG_INFO(0, result_id)
ZEND_ARG_INFO(0, length)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_prepare, 0, 0, 2)
ZEND_ARG_INFO(0, connection_id)
ZEND_ARG_INFO(0, query)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_execute, 0, 0, 1)
ZEND_ARG_INFO(0, result_id)
ZEND_ARG_INFO(0, parameters_array)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_cursor, 0, 0, 1)
ZEND_ARG_INFO(0, result_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_exec, 0, 0, 2)
ZEND_ARG_INFO(0, connection_id)
ZEND_ARG_INFO(0, query)
ZEND_ARG_INFO(0, flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_fetch_object, 0, 0, 1)
ZEND_ARG_INFO(0, result)
ZEND_ARG_INFO(0, rownumber)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_fetch_array, 0, 0, 1)
ZEND_ARG_INFO(0, result)
ZEND_ARG_INFO(0, rownumber)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_fetch_into, 0, 0, 2)
ZEND_ARG_INFO(0, result_id)
ZEND_ARG_INFO(0, rownumber)
ZEND_ARG_INFO(1, result_array)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_fetch_row, 0, 0, 1)
ZEND_ARG_INFO(0, result_id)
ZEND_ARG_INFO(0, row_number)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_result, 0, 0, 2)
ZEND_ARG_INFO(0, result_id)
ZEND_ARG_INFO(0, field)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_result_all, 0, 0, 1)
ZEND_ARG_INFO(0, result_id)
ZEND_ARG_INFO(0, format)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_free_result, 0, 0, 1)
ZEND_ARG_INFO(0, result_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_connect, 0, 0, 3)
ZEND_ARG_INFO(0, dsn)
ZEND_ARG_INFO(0, user)
ZEND_ARG_INFO(0, password)
ZEND_ARG_INFO(0, cursor_option)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_pconnect, 0, 0, 3)
ZEND_ARG_INFO(0, dsn)
ZEND_ARG_INFO(0, user)
ZEND_ARG_INFO(0, password)
ZEND_ARG_INFO(0, cursor_option)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_close, 0, 0, 1)
ZEND_ARG_INFO(0, connection_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_num_rows, 0, 0, 1)
ZEND_ARG_INFO(0, result_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_next_result, 0, 0, 1)
ZEND_ARG_INFO(0, result_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_num_fields, 0, 0, 1)
ZEND_ARG_INFO(0, result_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_field_name, 0, 0, 2)
ZEND_ARG_INFO(0, result_id)
ZEND_ARG_INFO(0, field_number)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_field_type, 0, 0, 2)
ZEND_ARG_INFO(0, result_id)
ZEND_ARG_INFO(0, field_number)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_field_len, 0, 0, 2)
ZEND_ARG_INFO(0, result_id)
ZEND_ARG_INFO(0, field_number)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_field_scale, 0, 0, 2)
ZEND_ARG_INFO(0, result_id)
ZEND_ARG_INFO(0, field_number)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_field_num, 0, 0, 2)
ZEND_ARG_INFO(0, result_id)
ZEND_ARG_INFO(0, field_name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_autocommit, 0, 0, 1)
ZEND_ARG_INFO(0, connection_id)
ZEND_ARG_INFO(0, onoff)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_commit, 0, 0, 1)
ZEND_ARG_INFO(0, connection_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_rollback, 0, 0, 1)
ZEND_ARG_INFO(0, connection_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_error, 0, 0, 0)
ZEND_ARG_INFO(0, connection_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_errormsg, 0, 0, 0)
ZEND_ARG_INFO(0, connection_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_setoption, 0, 0, 4)
ZEND_ARG_INFO(0, conn_id)
ZEND_ARG_INFO(0, which)
ZEND_ARG_INFO(0, option)
ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_tables, 0, 0, 1)
ZEND_ARG_INFO(0, connection_id)
ZEND_ARG_INFO(0, qualifier)
ZEND_ARG_INFO(0, owner)
ZEND_ARG_INFO(0, name)
ZEND_ARG_INFO(0, table_types)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_columns, 0, 0, 1)
ZEND_ARG_INFO(0, connection_id)
ZEND_ARG_INFO(0, qualifier)
ZEND_ARG_INFO(0, owner)
ZEND_ARG_INFO(0, table_name)
ZEND_ARG_INFO(0, column_name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_gettypeinfo, 0, 0, 1)
ZEND_ARG_INFO(0, connection_id)
ZEND_ARG_INFO(0, data_type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_primarykeys, 0, 0, 4)
ZEND_ARG_INFO(0, connection_id)
ZEND_ARG_INFO(0, qualifier)
ZEND_ARG_INFO(0, owner)
ZEND_ARG_INFO(0, table)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_procedurecolumns, 0, 0, 1)
ZEND_ARG_INFO(0, connection_id)
ZEND_ARG_INFO(0, qualifier)
ZEND_ARG_INFO(0, owner)
ZEND_ARG_INFO(0, proc)
ZEND_ARG_INFO(0, column)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_procedures, 0, 0, 1)
ZEND_ARG_INFO(0, connection_id)
ZEND_ARG_INFO(0, qualifier)
ZEND_ARG_INFO(0, owner)
ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_foreignkeys, 0, 0, 7)
ZEND_ARG_INFO(0, connection_id)
ZEND_ARG_INFO(0, pk_qualifier)
ZEND_ARG_INFO(0, pk_owner)
ZEND_ARG_INFO(0, pk_table)
ZEND_ARG_INFO(0, fk_qualifier)
ZEND_ARG_INFO(0, fk_owner)
ZEND_ARG_INFO(0, fk_table)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_specialcolumns, 0, 0, 7)
ZEND_ARG_INFO(0, connection_id)
ZEND_ARG_INFO(0, type)
ZEND_ARG_INFO(0, qualifier)
ZEND_ARG_INFO(0, owner)
ZEND_ARG_INFO(0, table)
ZEND_ARG_INFO(0, scope)
ZEND_ARG_INFO(0, nullable)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_statistics, 0, 0, 6)
ZEND_ARG_INFO(0, connection_id)
ZEND_ARG_INFO(0, qualifier)
ZEND_ARG_INFO(0, owner)
ZEND_ARG_INFO(0, name)
ZEND_ARG_INFO(0, unique)
ZEND_ARG_INFO(0, accuracy)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_tableprivileges, 0, 0, 4)
ZEND_ARG_INFO(0, connection_id)
ZEND_ARG_INFO(0, qualifier)
ZEND_ARG_INFO(0, owner)
ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_columnprivileges, 0, 0, 5)
ZEND_ARG_INFO(0, connection_id)
ZEND_ARG_INFO(0, catalog)
ZEND_ARG_INFO(0, schema)
ZEND_ARG_INFO(0, table)
ZEND_ARG_INFO(0, column)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_guid_to_string, 0)
	ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ ads_functions[]
 */
#if PHP_5_3_OR_GREATER
const
#endif
zend_function_entry advantage_functions[] = {
   PHP_FE(ads_error, arginfo_ads_error)
   PHP_FE(ads_errormsg, arginfo_ads_errormsg)
   PHP_FE(ads_setoption, arginfo_ads_setoption)
   PHP_FE(ads_autocommit, arginfo_ads_autocommit)
   PHP_FE(ads_close, arginfo_ads_close)
   PHP_FE(ads_close_all, arginfo_ads_close_all)
   PHP_FE(ads_commit, arginfo_ads_commit)
   PHP_FE(ads_connect, arginfo_ads_connect)
   PHP_FE(ads_pconnect, arginfo_ads_pconnect)
   PHP_FE(ads_cursor, arginfo_ads_cursor)
   PHP_FE(ads_fetch_array, arginfo_ads_fetch_array)
   PHP_FE(ads_fetch_object, arginfo_ads_fetch_object)
   PHP_FE(ads_exec, arginfo_ads_exec)
   PHP_FE(ads_prepare, arginfo_ads_prepare)
   PHP_FE(ads_execute, arginfo_ads_execute)
   PHP_FE(ads_fetch_row, arginfo_ads_fetch_row)
#if PHP_MAJOR_VERSION == 4
   PHP_FE(ads_fetch_into, a3_arg3_and_3_force_ref)
#else
   PHP_FE(ads_fetch_into, arginfo_ads_fetch_into)
#endif
// This is a problem
   PHP_FE(ads_field_len, arginfo_ads_field_len)
   PHP_FE(ads_field_scale, arginfo_ads_field_scale)
   PHP_FE(ads_field_name, arginfo_ads_field_name)
   PHP_FE(ads_field_type, arginfo_ads_field_type)
   PHP_FE(ads_field_num, arginfo_ads_field_num)
   PHP_FE(ads_free_result, arginfo_ads_free_result)
   PHP_FE(ads_next_result, arginfo_ads_next_result)
   PHP_FE(ads_num_fields, arginfo_ads_num_fields)
   PHP_FE(ads_num_rows, arginfo_ads_num_rows)
   PHP_FE(ads_result, arginfo_ads_result)
   PHP_FE(ads_result_all, arginfo_ads_result_all)
   PHP_FE(ads_rollback, arginfo_ads_rollback)
   PHP_FE(ads_binmode, arginfo_ads_binmode)
   PHP_FE(ads_longreadlen, arginfo_ads_longreadlen)
   PHP_FE(ads_tables, arginfo_ads_tables)
   PHP_FE(ads_columns, arginfo_ads_columns)
   PHP_FE(ads_gettypeinfo, arginfo_ads_gettypeinfo)
   PHP_FE(ads_primarykeys, arginfo_ads_primarykeys)
   PHP_FE(ads_columnprivileges, arginfo_ads_columnprivileges)
   PHP_FE(ads_tableprivileges, arginfo_ads_tableprivileges)
   PHP_FE(ads_foreignkeys, arginfo_ads_foreignkeys)
   PHP_FE(ads_procedures, arginfo_ads_procedures)
   PHP_FE(ads_procedurecolumns, arginfo_ads_procedurecolumns)
   PHP_FE(ads_specialcolumns, arginfo_ads_specialcolumns)
   PHP_FE(ads_statistics, arginfo_ads_statistics)
   //PHP_FE(ads_guid_to_string, arginfo_guid_to_string)
   PHP_FALIAS(ads_do, ads_exec, arginfo_ads_exec)
   PHP_FALIAS(ads_field_precision, ads_field_len, arginfo_ads_field_len)
      {
      NULL, NULL, NULL
      }
};
/* }}} */

ZEND_DECLARE_MODULE_GLOBALS(advantage);
static PHP_GINIT_FUNCTION(advantage);

/* {{{ advantage_module_entry
 */
zend_module_entry advantage_module_entry = {
   STANDARD_MODULE_HEADER,
   "advantage",
   advantage_functions,
   PHP_MINIT(advantage),
   PHP_MSHUTDOWN(advantage),
   PHP_RINIT(advantage),
   PHP_RSHUTDOWN(advantage),
   PHP_MINFO(advantage),
   "12.00.0.02",
   PHP_MODULE_GLOBALS(advantage),
   PHP_GINIT(advantage),
   NULL,
   NULL,
   STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

#if PHP_MAJOR_VERSION == 4

   #ifdef COMPILE_DL_ADS
      ZEND_GET_MODULE(advantage)
   #endif
#else
   #ifdef COMPILE_DL_ADVANTAGE
      ZEND_GET_MODULE(advantage)
   #endif
#endif

/* {{{ _free_ads_result
 */
static void _free_ads_result(zend_resource *rsrc TSRMLS_DC)
   {
   ads_result *res = (ads_result *)rsrc->ptr;
   int i;

   if ( res )
      {
      if ( res->values )
         {
         for ( i = 0; i < res->numcols; i++ )
            {
            if ( res->values[i].value )
               efree(res->values[i].value);
            }
         efree(res->values);
         res->values = NULL;
         }
      if ( res->stmt )
         {
         //zend_resource *pConnResource;

         SQLFreeStmt(res->stmt,SQL_DROP);

         res->stmt = 0;

         /* Decrement the reference count on this connection, so the connection can be closed before the script finishes. */
         if (( res->conn_ptr != NULL ) &&
             ( res->conn_ptr->res->gc.refcount > 1 ))
            {
            res->conn_ptr->res->gc.refcount--;
            }

         /* We don't want the connection to be closed after the last statment has been closed
          * Connections will be closed on shutdown
          * zend_list_delete(res->conn_ptr->id);
          */
         }
      res->conn_ptr = NULL;

      efree(res);
      }
   }
/* }}} */

/* {{{ safe_ads_disconnect
 * disconnect, and if it fails, then issue a rollback for any pending transaction (lurcher)
 */
static void safe_ads_disconnect( void *handle )
   {
   int ret;

   ret = SQLDisconnect( handle );
   if ( ret == SQL_ERROR )
      {
      SQLTransact( NULL, handle, SQL_ROLLBACK );
      SQLDisconnect( handle );
      }
   }
/* }}} */

/* {{{ _close_ads_conn
 */
static void _close_ads_conn(zend_resource *rsrc TSRMLS_DC)
   {
   ads_connection *conn = (ads_connection *)rsrc->ptr;

   safe_ads_disconnect(conn->hdbc);
   SQLFreeConnect(conn->hdbc);
   SQLFreeEnv(conn->henv);

   conn->hdbc = 0;
   conn->henv = 0;

   efree(conn);
   ADSG(num_links)--;
   }
/* }}} */

static void _close_ads_pconn(zend_resource *rsrc TSRMLS_DC)
   {
   ads_connection *conn = (ads_connection *)rsrc->ptr;

   safe_ads_disconnect(conn->hdbc);
   SQLFreeConnect(conn->hdbc);
   SQLFreeEnv(conn->henv);

   conn->hdbc = 0;
   conn->henv = 0;

   free(conn);

   ADSG(num_links)--;
   ADSG(num_persistent)--;
   }

static PHP_INI_DISP(display_link_nums)
   {
   zend_string *value;
   TSRMLS_FETCH();

   if ( type == PHP_INI_DISPLAY_ORIG && ini_entry->modified )
      {
      value = ini_entry->orig_value;
      }
   else if ( ini_entry->value )
      {
      value = ini_entry->value;
      }
   else
      {
      value = NULL;
      }

   if ( value )
      {
      if ( atoi(value->val) == -1 )
         {
         PUTS("Unlimited");
         }
      else
         {
         php_printf("%s", value->val);
         }
      }
   }

static PHP_INI_DISP(display_defPW)
   {
   zend_string *value;
   TSRMLS_FETCH();

   if ( type == PHP_INI_DISPLAY_ORIG && ini_entry->modified )
      {
      value = ini_entry->orig_value;
      }
   else if ( ini_entry->value )
      {
      value = ini_entry->value;
      }
   else
      {
      value = NULL;
      }

   if ( value )
      {
#if PHP_DEBUG
      php_printf("%s", value->val);
#else
      PUTS("********");
#endif
      }
   else
      {
      PUTS("<i>no value</i>");
      }
   }

static PHP_INI_DISP(display_binmode)
   {
   zend_string *value;
   TSRMLS_FETCH();

   if ( type == PHP_INI_DISPLAY_ORIG && ini_entry->modified )
      {
      value = ini_entry->orig_value;
      }
   else if ( ini_entry->value )
      {
      value = ini_entry->value;
      }
   else
      {
      value = NULL;
      }

   if ( value )
      {
      switch ( atoi(value->val) )
         {
         case 0:
            PUTS("passthru");
            break;
         case 1:
            PUTS("return as is");
            break;
         case 2:
            PUTS("return as char");
            break;
         }
      }
   }

static PHP_INI_DISP(display_lrl)
   {
   zend_string *value;
   TSRMLS_FETCH();

   if ( type == PHP_INI_DISPLAY_ORIG && ini_entry->modified )
      {
      value = ini_entry->orig_value;
      }
   else if ( ini_entry->value )
      {
      value = ini_entry->value;
      }
   else
      {
      value = NULL;
      }

   if ( value )
      {
      if ( atoi(value->val) <= 0 )
         {
         PUTS("Passthru");
         }
      else
         {
         php_printf("return up to %s bytes", value->val);
         }
      }
   }

PHP_INI_BEGIN()
STD_PHP_INI_BOOLEAN("advantage.allow_persistent", "1", PHP_INI_SYSTEM, OnUpdateLong,
                    allow_persistent, zend_advantage_globals, advantage_globals)
STD_PHP_INI_ENTRY_EX("advantage.max_persistent",  "-1", PHP_INI_SYSTEM, OnUpdateLong,
                     max_persistent, zend_advantage_globals, advantage_globals, display_link_nums)
STD_PHP_INI_ENTRY_EX("advantage.max_links", "-1", PHP_INI_SYSTEM, OnUpdateLong,
                     max_links, zend_advantage_globals, advantage_globals, display_link_nums)
STD_PHP_INI_ENTRY("advantage.default_db", NULL, PHP_INI_ALL, OnUpdateString,
                  defDB, zend_advantage_globals, advantage_globals)
STD_PHP_INI_ENTRY("advantage.default_user", NULL, PHP_INI_ALL, OnUpdateString,
                  defUser, zend_advantage_globals, advantage_globals)
STD_PHP_INI_ENTRY_EX("advantage.default_pw", NULL, PHP_INI_ALL, OnUpdateString,
                     defPW, zend_advantage_globals, advantage_globals, display_defPW)
STD_PHP_INI_ENTRY_EX("advantage.defaultlrl", "4096", PHP_INI_ALL, OnUpdateLong,
                     defaultlrl, zend_advantage_globals, advantage_globals, display_lrl)
STD_PHP_INI_ENTRY_EX("advantage.defaultbinmode", "1", PHP_INI_ALL, OnUpdateLong,
                     defaultbinmode, zend_advantage_globals, advantage_globals, display_binmode)
STD_PHP_INI_BOOLEAN("advantage.check_persistent", "1", PHP_INI_SYSTEM, OnUpdateLong,
                    check_persistent, zend_advantage_globals, advantage_globals)
PHP_INI_END()


static PHP_GINIT_FUNCTION(advantage)
   {
   advantage_globals->num_persistent = 0;
   }


PHP_MINIT_FUNCTION(advantage)
   {

   REGISTER_INI_ENTRIES();
   le_result = zend_register_list_destructors_ex(_free_ads_result, NULL, "advantage result", module_number);
   le_conn = zend_register_list_destructors_ex(_close_ads_conn, NULL, "advantage link", module_number);
   le_pconn = zend_register_list_destructors_ex(NULL, _close_ads_pconn, "advantage link persistent", module_number);
   advantage_module_entry.type = type;

   REGISTER_LONG_CONSTANT("ODBC_BINMODE_PASSTHRU", 0, CONST_CS | CONST_PERSISTENT);
   REGISTER_LONG_CONSTANT("ODBC_BINMODE_RETURN", 1, CONST_CS | CONST_PERSISTENT);
   REGISTER_LONG_CONSTANT("ODBC_BINMODE_CONVERT", 2, CONST_CS | CONST_PERSISTENT);

   REGISTER_LONG_CONSTANT("ADS_BINMODE_PASSTHRU", 0, CONST_CS | CONST_PERSISTENT);
   REGISTER_LONG_CONSTANT("ADS_BINMODE_RETURN", 1, CONST_CS | CONST_PERSISTENT);
   REGISTER_LONG_CONSTANT("ADS_BINMODE_CONVERT", 2, CONST_CS | CONST_PERSISTENT);
   /* Define Constants for options
      these Constants are defined in <sqlext.h>
   */
   REGISTER_LONG_CONSTANT("SQL_ODBC_CURSORS", SQL_ODBC_CURSORS, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_CUR_USE_DRIVER", SQL_CUR_USE_DRIVER, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_CUR_USE_IF_NEEDED", SQL_CUR_USE_IF_NEEDED, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_CUR_USE_ODBC", SQL_CUR_USE_ODBC, CONST_PERSISTENT | CONST_CS);


   REGISTER_LONG_CONSTANT("SQL_CONCURRENCY", SQL_CONCURRENCY, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_CONCUR_READ_ONLY", SQL_CONCUR_READ_ONLY, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_CONCUR_LOCK", SQL_CONCUR_LOCK, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_CONCUR_ROWVER", SQL_CONCUR_ROWVER, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_CONCUR_VALUES", SQL_CONCUR_VALUES, CONST_PERSISTENT | CONST_CS);

   REGISTER_LONG_CONSTANT("SQL_CURSOR_TYPE", SQL_CURSOR_TYPE, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_CURSOR_FORWARD_ONLY", SQL_CURSOR_FORWARD_ONLY, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_CURSOR_KEYSET_DRIVEN", SQL_CURSOR_KEYSET_DRIVEN, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_CURSOR_DYNAMIC", SQL_CURSOR_DYNAMIC, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_CURSOR_STATIC", SQL_CURSOR_STATIC, CONST_PERSISTENT | CONST_CS);

   REGISTER_LONG_CONSTANT("SQL_KEYSET_SIZE", SQL_KEYSET_SIZE, CONST_PERSISTENT | CONST_CS);

   /*
    * register the standard data types
    */
   REGISTER_LONG_CONSTANT("SQL_CHAR", SQL_CHAR, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_VARCHAR", SQL_VARCHAR, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_LONGVARCHAR", SQL_LONGVARCHAR, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_DECIMAL", SQL_DECIMAL, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_NUMERIC", SQL_NUMERIC, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_BIT", SQL_BIT, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_TINYINT", SQL_TINYINT, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_SMALLINT", SQL_SMALLINT, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_INTEGER", SQL_INTEGER, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_BIGINT", SQL_BIGINT, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_REAL", SQL_REAL, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_FLOAT", SQL_FLOAT, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_DOUBLE", SQL_DOUBLE, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_BINARY", SQL_BINARY, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_GUID", SQL_GUID, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_VARBINARY", SQL_VARBINARY, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_LONGVARBINARY", SQL_LONGVARBINARY, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_DATE", SQL_DATE, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_TIME", SQL_TIME, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_TIMESTAMP", SQL_TIMESTAMP, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_WCHAR", SQL_WCHAR, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_WVARCHAR", SQL_WVARCHAR, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_WLONGVARCHAR", SQL_WLONGVARCHAR, CONST_PERSISTENT | CONST_CS);
#if defined(ODBCVER) && (ODBCVER >= 0x0300)
   REGISTER_LONG_CONSTANT("SQL_TYPE_DATE", SQL_TYPE_DATE, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_TYPE_TIME", SQL_TYPE_TIME, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_TYPE_TIMESTAMP", SQL_TYPE_TIMESTAMP, CONST_PERSISTENT | CONST_CS);

   /*
    * SQLSpecialColumns values
    */
   REGISTER_LONG_CONSTANT("SQL_BEST_ROWID", SQL_BEST_ROWID, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_ROWVER", SQL_ROWVER, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_SCOPE_CURROW", SQL_SCOPE_CURROW, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_SCOPE_TRANSACTION", SQL_SCOPE_TRANSACTION, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_SCOPE_SESSION", SQL_SCOPE_SESSION, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_NO_NULLS", SQL_NO_NULLS, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_NULLABLE", SQL_NULLABLE, CONST_PERSISTENT | CONST_CS);

   /*
    * SQLStatistics values
    */
   REGISTER_LONG_CONSTANT("SQL_INDEX_UNIQUE", SQL_INDEX_UNIQUE, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_INDEX_ALL", SQL_INDEX_ALL, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_ENSURE", SQL_ENSURE, CONST_PERSISTENT | CONST_CS);
   REGISTER_LONG_CONSTANT("SQL_QUICK", SQL_QUICK, CONST_PERSISTENT | CONST_CS);
#endif

   return SUCCESS;
   }


PHP_RINIT_FUNCTION( advantage )
   {
   ADSG(defConn) = -1;
   ADSG(num_links) = ADSG(num_persistent);
   memset(ADSG(laststate), '\0', 6);
   memset(ADSG(lasterrormsg), '\0', SQL_MAX_MESSAGE_LENGTH);
   return SUCCESS;
   }

PHP_RSHUTDOWN_FUNCTION( advantage )
   {
   return SUCCESS;
   }

PHP_MSHUTDOWN_FUNCTION( advantage )
   {
   UNREGISTER_INI_ENTRIES();
   return SUCCESS;
   }

PHP_MINFO_FUNCTION( advantage )
   {
   char buf[32];

   php_info_print_table_start();
   php_info_print_table_row( 2, "Advantage Version", advantage_module_entry.version );
   php_info_print_table_header(2, "Advantage Support", "enabled");
   sprintf(buf, "%ld", ADSG(num_persistent));
   php_info_print_table_row(2, "Active Persistent Links", buf);
   sprintf(buf, "%ld", ADSG(num_links));
   php_info_print_table_row(2, "Active Links", buf);
/*   php_info_print_table_row(2, "ADVANTAGE_INCLUDE", PHP_ADVANTAGE_INCLUDE);
   php_info_print_table_row(2, "ADVANTAGE_LFLAGS", PHP_ADVANTAGE_LFLAGS);
   php_info_print_table_row(2, "ADVANTAGE_LIBS", PHP_ADVANTAGE_LIBS);*/
   php_info_print_table_end();

   DISPLAY_INI_ENTRIES();

   }

void ads_sql_error(ADS_SQL_ERROR_PARAMS)
   {
   char  state[6];
   SDWORD   error;        /* Not used */
   char  errormsg[SQL_MAX_MESSAGE_LENGTH];
   SQLSMALLINT errormsgsize; /* Not used */
   RETCODE rc;
   ADS_SQL_ENV_T henv;
   ADS_SQL_CONN_T conn;
   TSRMLS_FETCH();

   if ( conn_resource )
      {
      henv = conn_resource->henv;
      conn = conn_resource->hdbc;
      }
   else
      {
      henv = SQL_NULL_HENV;
      conn = SQL_NULL_HDBC;
      }

   /* This leads to an endless loop in many drivers!
    *
      while(henv != SQL_NULL_HENV){
      do {
    */
   rc = SQLError(henv, conn, stmt, (unsigned char*)state,
                 &error, (unsigned char*)errormsg, sizeof(errormsg)-1, &errormsgsize);
   if ( conn_resource )
      {
      memcpy(conn_resource->laststate, state, sizeof(state));
      memcpy(conn_resource->lasterrormsg, errormsg, sizeof(errormsg));
      }
   memcpy(ADSG(laststate), state, sizeof(state));
   memcpy(ADSG(lasterrormsg), errormsg, sizeof(errormsg));
   if ( func )
      {
      php_error(E_WARNING, "SQL error: %s, SQL state %s in %s",
                errormsg, state, func);
      }
   else
      {
      php_error(E_WARNING, "SQL error: %s, SQL state %s",
                errormsg, state);
      }
   /*
      } while (SQL_SUCCEEDED(rc));
   }
   */
   }

void php_ads_fetch_attribs(INTERNAL_FUNCTION_PARAMETERS, int mode)
   {
   ads_result *result;
   zval *pv_res;
   long  flag;

   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rl", &pv_res, &flag) == FAILURE )
      return;

   if ( Z_LVAL_P( pv_res ) )
      {
      if ((result = (ads_result *)zend_fetch_resource_ex(pv_res, "Advantage result", le_result)) == NULL) {
         RETURN_FALSE;
      }
      //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);
      if ( mode )
         result->longreadlen = flag;
      else
         result->binmode = flag;
      }
   else
      {
      if ( mode )
         ADSG(defaultlrl) = flag;
      else
         ADSG(defaultbinmode) = flag;
      }
   RETURN_TRUE;
   }

static char hexconvtab[] = "0123456789ABCDEF";

static char *ads_guid_as_string(const unsigned char *old, const size_t oldlen, size_t *newlen)
{
	register unsigned char *result = NULL;
	size_t i, j;
	char temp;
	
	result = (unsigned char *) safe_emalloc(oldlen * 2 + 7, sizeof(char), 1);

	if (oldlen != 16) {
		efree(result);
		return NULL;
	}

	result[0] = '{';
	for (i = 0, j = 8; i < 4; i++) {
		result[j--] = hexconvtab[old[i] >> 4];
		result[j--] = hexconvtab[old[i] & 15];
		temp = result[j+1];
		result[j+1] = result[j+2];
		result[j+2] = temp;
	}
	result[9] = '-';

	for (j = 13; i < 6; i++) {
		result[j--] = hexconvtab[old[i] >> 4];
		result[j--] = hexconvtab[old[i] & 15];
		temp = result[j+1];
		result[j+1] = result[j+2];
		result[j+2] = temp;
	}
	result[14] = '-';

	for (j = 18; i < 8; i++) {
		result[j--] = hexconvtab[old[i] >> 4];
		result[j--] = hexconvtab[old[i] & 15];
		temp = result[j+1];
		result[j+1] = result[j+2];
		result[j+2] = temp;
	}
	result[19] = '-';
	
	for (j = 20; i < 10; i++) {
		result[j++] = hexconvtab[old[i] >> 4];
		result[j++] = hexconvtab[old[i] & 15];
	}
	result[24] = '-';
	for (j = 25; i < 16; i++) {
		result[j++] = hexconvtab[old[i] >> 4];
		result[j++] = hexconvtab[old[i] & 15];
	}
	result[37] = '}';
	result[38] = '\0';

	if (newlen)
		*newlen = (oldlen * 2 + 7) * sizeof(char);

	return (char *)result;
}

int ads_bindcols(ads_result *result TSRMLS_DC)
   {
   RETCODE rc;
   int i;
   SQLSMALLINT colnamelen; /* Not used */
   SDWORD      displaysize;

   result->values = (ads_result_value *)
                    emalloc(sizeof(ads_result_value)*result->numcols);

   if ( result->values == NULL )
      {
      php_error(E_WARNING, "Out of memory");
      SQLFreeStmt(result->stmt, SQL_DROP);
      return 0;
      }

   result->longreadlen = ADSG(defaultlrl);
   result->binmode = ADSG(defaultbinmode);

   for ( i = 0; i < result->numcols; i++ )
      {
      rc = SQLColAttributes(result->stmt, (SQLUSMALLINT)(i+1), SQL_COLUMN_NAME,
                            result->values[i].name,
                            sizeof(result->values[i].name),
                            &colnamelen,
                            0);
      rc = SQLColAttributes(result->stmt, (SQLUSMALLINT)(i+1), SQL_COLUMN_TYPE,
                            NULL, 0, NULL, &result->values[i].coltype);

      /* Don't bind LONG / BINARY columns, so that fetch behaviour can
           be controlled by ads_binmode() / ads_longreadlen()
       */

      switch ( result->values[i].coltype )
         {
         case SQL_BINARY:
         case SQL_GUID:
         case SQL_VARBINARY:
         case SQL_LONGVARBINARY:
         case SQL_LONGVARCHAR:
         case SQL_WLONGVARCHAR:
            result->values[i].value = NULL;
            break;
         case SQL_WCHAR:
         case SQL_WVARCHAR:
            rc = SQLColAttributes(result->stmt, (SQLUSMALLINT)(i+1), SQL_COLUMN_DISPLAY_SIZE,
                                  NULL, 0, NULL, &displaysize);
            result->values[i].value = (char *)emalloc(displaysize * 3);
            rc = SQLBindCol(result->stmt, (SQLUSMALLINT)(i+1), SQL_C_UTF8, result->values[i].value,
                            displaysize * 3, &result->values[i].vallen);
            break;
         default:
            rc = SQLColAttributes(result->stmt, (SQLUSMALLINT)(i+1), SQL_COLUMN_DISPLAY_SIZE,
                                  NULL, 0, NULL, &displaysize);
            result->values[i].value = (char *)emalloc(displaysize + 1);
            rc = SQLBindCol(result->stmt, (SQLUSMALLINT)(i+1), SQL_C_CHAR, result->values[i].value,
                            displaysize + 1, &result->values[i].vallen);
            break;
         }
      }
   return 1;
   }

void ads_transact(INTERNAL_FUNCTION_PARAMETERS, int type)
   {
   ads_connection *conn;
   RETCODE rc;
   zval *pv_conn;

   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &pv_conn) == FAILURE )
      {
      WRONG_PARAM_COUNT;
      }

   //- ZEND_FETCH_RESOURCE2(ib_link, ibase_db_link *, &link_arg, link_id, LE_LINK, le_link, le_plink);

      //if you are sure that link_arg is a IS_RESOURCE type, then use :
   if ((conn = (ads_connection *)zend_fetch_resource2(Z_RES_P(pv_conn), "Advantage-Link", le_conn, le_pconn)) == NULL) {
      RETURN_FALSE;
   }

   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);

   rc = SQLTransact(conn->henv, conn->hdbc, (SQLUSMALLINT)((type)?SQL_COMMIT:SQL_ROLLBACK));
   if ( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLTransact");
      RETURN_FALSE;
      }

   RETURN_TRUE;
   }

static int _close_pconn_with_id(zend_resource *le, int *id TSRMLS_DC)
   {
      if ( le->type == le_pconn && (((ads_connection *)(le->ptr))->id == *id) )
      {
         return 1;
      }
      else
      {
         return 0;
      }
   }

void ads_column_lengths(INTERNAL_FUNCTION_PARAMETERS, int type)
   {
   ads_result *result;
   SQLINTEGER len;
   zval *pv_res;
   long lnum;

   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rl", &pv_res, &lnum) == FAILURE )
      {
      WRONG_PARAM_COUNT;
      }
   if ((result = (ads_result *)zend_fetch_resource_ex(pv_res, "Advantage result", le_result)) == NULL) {
         RETURN_FALSE;
      }
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);

   if ( result->numcols == 0 )
      {
      php_error(E_WARNING, "No tuples available at this result index");
      RETURN_FALSE;
      }

   if ( lnum > result->numcols )
      {
      php_error(E_WARNING, "Field index larger than number of fields");
      RETURN_FALSE;
      }

   if ( lnum < 1 )
      {
      php_error(E_WARNING, "Field numbering starts at 1");
      RETURN_FALSE;
      }

   SQLColAttributes(result->stmt, (SQLUSMALLINT)lnum,
                    (SQLUSMALLINT) (type?SQL_COLUMN_SCALE:SQL_COLUMN_PRECISION),
                    NULL, 0, NULL, &len);

   RETURN_LONG(len);
   }

/* Main User Functions */

/* {{{ proto void ads_close_all(void)
   Closes all connections */
PHP_FUNCTION(ads_close_all)
   {
   void *ptr;
   int type;
   int i;
   int nument;

#if PHP_5_3_OR_GREATER
   if ( zend_parse_parameters_none() == FAILURE )
      {
      return;
      }
#else
   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE )
      {
      return;
      }
#endif
   /*
    uint32_t i;
   for (i = 0; i < EG(regular_list)->nNumUsed; ++i) {
	   Bucket *b = &EG(regular_list)->arData[i];
	   if (Z_ISUNDEF(b->val)) continue;
         if(b->val && (b->val.type == le_result))
         {
            res = (ads_result *)b->val;
            if ( res->conn_ptr == conn )
               {
                  zend_list_delete(*b);
               }
            }
         }
	      // do stuff with bucket
   }*/
   /*nument = zend_hash_next_free_element(&EG(regular_list));

   /* Loop through list and close all statements 
   for ( i = 1; i < nument; i++ )
      {
      ptr = zend_list_find(i, &type);
      if ( ptr && (type == le_result) )
         {
         zend_list_delete(i);
         }
      }
   */
   /* Second loop through list, now close all connections */

   uint32_t ii;
   for (ii = 0; ii < EG(regular_list).nNumUsed; ++ii) {
	   Bucket *b = &EG(regular_list).arData[ii];
	   if (Z_ISUNDEF(b->val)) continue;
         if((Z_ISUNDEF(b->val) == false) && (b->val.u1.v.type == le_result))
         {
            zend_list_delete(Z_RES(b->val));
         }
	      // do stuff with bucket
   }


   for (ii = 0; ii < EG(regular_list).nNumUsed; ++ii) {
	   Bucket *b = &EG(regular_list).arData[ii];
	   if (Z_ISUNDEF(b->val)) continue;
         if(Z_ISUNDEF(b->val)==false)
         {
            if (b->val.u1.v.type == le_conn )
            {
               zend_list_delete(Z_RES(b->val));
            }
            else if (b->val.u1.v.type == le_pconn )
            {
               zend_list_delete(Z_RES(b->val));
               /* Delete the persistent connection */
               zend_hash_apply_with_argument(&EG(persistent_list),(apply_func_arg_t) _close_pconn_with_id, (void *) &ii TSRMLS_CC);
            }
         }
	      // do stuff with bucket
   }

   /*

   nument = zend_hash_next_free_element(&EG(regular_list));

   for ( i = 1; i < nument; i++ )
      {
      ptr = zend_list_find(i, &type);
      if ( ptr )
         {
         if ( type == le_conn )
            {
            zend_list_delete(ptr);
            }
         else if ( type == le_pconn )
            {
            zend_list_delete(ptr);
            /* Delete the persistent connection 
            zend_hash_apply_with_argument(&EG(persistent_list),
                                          (apply_func_arg_t) _close_pconn_with_id, (void *) &i TSRMLS_CC);
            }
         }
      }
      */
   }
   
/* }}} */

/* {{{ proto int ads_binmode(int result_id, int mode)
   Handle binary column data */
PHP_FUNCTION(ads_binmode)
   {
   php_ads_fetch_attribs(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
   }
/* }}} */

/* {{{ proto int ads_longreadlen(int result_id, int length)
   Handle LONG columns */
PHP_FUNCTION(ads_longreadlen)
   {
   php_ads_fetch_attribs(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
   }
/* }}} */


/* {{{ proto int ads_prepare(int connection_id, string query)
   Prepares a statement for execution */
PHP_FUNCTION(ads_prepare)
   {
   zval *pv_conn;
   char *query;
   int64_t query_len;
   ads_result *result = NULL;
   ads_connection *conn;
   RETCODE rc;
#ifdef HAVE_SQL_EXTENDED_FETCH
   SQLUINTEGER      scrollopts;
#endif

   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &pv_conn, &query, &query_len) == FAILURE )
      {
      return;
      }

   if ((conn = (ads_connection *)zend_fetch_resource2(Z_RES_P(pv_conn), "Advantage-Link", le_conn, le_pconn)) == NULL) {
      RETURN_FALSE;
   }

   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);


   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);

   result = (ads_result *)emalloc(sizeof(ads_result));
   if ( result == NULL )
      {
      php_error(E_WARNING, "Out of memory");
      RETURN_FALSE;
      }

   result->numparams = 0;

   rc = SQLAllocStmt(conn->hdbc, &(result->stmt));
   if ( rc == SQL_INVALID_HANDLE )
      {
      efree(result);
      php_error(E_WARNING, "SQLAllocStmt error 'Invalid Handle' in ads_prepare");
      RETURN_FALSE;
      }

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLAllocStmt");
      efree(result);
      RETURN_FALSE;
      }

#ifdef HAVE_SQL_EXTENDED_FETCH
   rc = SQLGetInfo(conn->hdbc, SQL_FETCH_DIRECTION, (void *) &scrollopts, sizeof(scrollopts), NULL);
   if ( rc == SQL_SUCCESS )
      {
      if ( (result->fetch_abs = (scrollopts & SQL_FD_FETCH_ABSOLUTE)) )
         {
         /*
          * Set the Cursor to forward scrolling only because there is a major permformance hit
          * for setting it to SQL_CURSOR_KEYSET
          *
          * Acutally don't set it to forward scrolling set it to a dynamic cursor as of
          * 6.2 this should be optomized and be as fast as forward scrolling cursors.
          */
         if ( SQLSetStmtOption(result->stmt, SQL_CURSOR_TYPE, SQL_CURSOR_DYNAMIC )
              == SQL_ERROR )
            {
            ads_sql_error(conn, result->stmt, " SQLSetStmtOption");
            SQLFreeStmt(result->stmt, SQL_DROP);
            result->stmt = 0;
            efree(result);
            RETURN_FALSE;
            }
         }
      }
   else
      {
      result->fetch_abs = 0;
      }
#endif

   rc = SQLPrepare(result->stmt, (unsigned char*)query, SQL_NTS);
   switch ( rc )
      {
      case SQL_SUCCESS:
         break;
      case SQL_SUCCESS_WITH_INFO:
         ads_sql_error(conn, result->stmt, "SQLPrepare");
         break;
      default:
         ads_sql_error(conn, result->stmt, "SQLPrepare");
         RETURN_FALSE;
      }

   SQLNumParams(result->stmt, &(result->numparams));
   SQLNumResultCols(result->stmt, &(result->numcols));

   if ( result->numcols > 0 )
      {
      if ( !ads_bindcols(result TSRMLS_CC) )
         {
         efree(result);
         RETURN_FALSE;
         }
      }
   else
      {
      result->values = NULL;
      }
   conn->res->gc.refcount++;
   //Z_ADDREF(conn->id)
   //Z_RES(conn->id)->gc.refcount++;
   //zend_list_addref();
   //zval *le;
	//le = zend_hash_index_find(&EG(regular_list), conn->id);
	//if (le) {
/*		printf("add(%d): %d->%d\n", id, le->refcount, le->refcount+1); */
	//	le->refcount++;
   //}
   result->conn_ptr = conn;
   result->fetched = 0;
   RETURN_RES(zend_register_resource(result, le_result));
   }
/* }}} */

/*
 * Execute prepared SQL statement. Supports only input parameters.
 */
/* {{{ proto int ads_execute(int result_id [[, array parameters_array], array parameters_type_array])
   Execute a prepared statement */
PHP_FUNCTION(ads_execute)
   {
   zval *pv_res, *pv_param_arr, *tmp;
   zval *pv_param_types;
   zval *pv_param_type;
   HashPosition pos;
   typedef struct params_t
      {
      SDWORD vallen;
#ifdef PHP_WIN32
      int fp;
#else
      long int fp;
#endif
      } params_t;
   params_t *params = NULL;
   char *filename;
   SQLSMALLINT sqltype, ctype, scale;
   SQLSMALLINT nullable;
   UDWORD precision;
   ads_result   *result;
   int numArgs, i, ne;
   RETCODE rc;
   UCHAR ucType;

   numArgs = ZEND_NUM_ARGS();

   if ( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "r|aa", &pv_res, &pv_param_arr, &pv_param_types ) == FAILURE )
      {
      return;
      }

    if ((result = (ads_result *)zend_fetch_resource_ex(pv_res, "Advantage result", le_result)) == NULL) {
         RETURN_FALSE;
      }
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);

   /* XXX check for already bound parameters*/
   if ( result->numparams > 0 && numArgs == 1 )
   {
      php_error(E_WARNING, "No parameters to SQL statement given");
      RETURN_FALSE;
   }

   if ( result->numparams > 0 )
      {
      if ( (ne = zend_array_count(pv_param_arr->value.arr)) < result->numparams )
         {
         php_error(E_WARNING,"Not enough parameters given (%d should be %d)",
                   ne, result->numparams);
         RETURN_FALSE;
         }

      /* Check that enough types were passed in */
      if ( numArgs == 3 )
         {
         if ( ne =  zend_array_count(pv_param_arr->value.arr) < result->numparams )
            {
            php_error(E_WARNING,"Not enough parameter types given (%d should be %d)",
                     ne, result->numparams);
            RETURN_FALSE;
            }
         zend_hash_internal_pointer_reset( pv_param_types->value.arr);
         }

      zend_hash_internal_pointer_reset(pv_param_arr->value.arr);
      params = (params_t *)emalloc(sizeof(params_t) * result->numparams);

      for ( i = 1; i <= result->numparams; i++ )
      {
         bool success = false;
          //ZEND_HASH_FOREACH_KEY_VAL(ht, num_key, key, val) 
          //zend_hash_get_current_data_ex(&ht, (void**)&ppzval, &pos) == SUCCESS)
          //zend_hash_get_current_data( pv_param_types->value.ht, (void **) &pv_param_type ) == FAILURE

          //zend_hash_get_current_data_ex(&pv_param_arr->value.arr,&pos) == FAILURE 
          
          //HashPosition hp = 0;
          tmp = zend_hash_get_current_data( pv_param_arr->value.arr);

         if ( tmp == NULL )
            {
            php_error(E_WARNING,"Error getting parameter");
            SQLFreeStmt(result->stmt,SQL_RESET_PARAMS);
            efree(params);
            RETURN_FALSE;
            }         

            /*
          for (zend_hash_internal_pointer_reset_ex((HashTable *) &pv_param_arr->value.arr, &pos);
                   zend_hash_get_current_data_ex( (HashTable *) &pv_param_arr->value.arr,&pos) != NULL;
                  zend_hash_move_forward_ex((HashTable *) &pv_param_arr->value.arr, &pos)
         ) {
            success = true;
            break;
           
         }
         if ( success == false)
         {
            php_error(E_WARNING,"Error getting parameter");
            SQLFreeStmt(result->stmt,SQL_RESET_PARAMS);
            efree(params);
            RETURN_FALSE;
         }*/

         /* Save the type of the parameter in case it is NULL */
         ucType = tmp->u1.v.type;

         /*
          * Do the conversion of boolean parameters ourselves for bug, 3683.  A
          * NULL value rather than false is being inserted because a length of zero
          * and an empty string is the conversion of boolean false to a string in PHP.
          */
         if ( tmp->u1.v.type == IS_TRUE || tmp->u1.v.type == IS_FALSE)
            {
            if ( tmp->value.lval)
               {
                  tmp->value.str->val[0] = '1';
                  tmp->value.str->len = 1;
               }
            else
               {
                  tmp->value.str->val[0] = '0';
                  tmp->value.str->len = 1;
               }
            tmp->u1.v.type = IS_STRING;
            }
         else
            {

            convert_to_string(tmp);
            if ( tmp->u1.v.type != IS_STRING )
               {
               php_error(E_WARNING,"Error converting parameter");
               SQLFreeStmt(result->stmt, SQL_RESET_PARAMS);
               efree(params);
               RETURN_FALSE;
               }
            }

         SQLDescribeParam(result->stmt, (SQLUSMALLINT)i, &sqltype, &precision,
                          &scale, &nullable);
         params[i-1].vallen = tmp->value.str->len;
         params[i-1].fp = -1;

         /*
          * Our ODBC driver really doesn't describe the parameters. It believes everything is
          * a char parameter.  This is fine for everything but binary data, so if the user passed
          * in an array of types use them to better describe the parameters now.
          */
         if ( numArgs == 3 )
            {
               //zend_hash_get_current_data_ex(&pv_param_arr->value.arr, (void **) &pv_param_type) == FAILURE 

            bool success = false;
            //ZEND_HASH_FOREACH_KEY_VAL(ht, num_key, key, val) 
            //zend_hash_get_current_data_ex(&ht, (void**)&ppzval, &pos) == SUCCESS)
            //zend_hash_get_current_data( pv_param_types->value.ht, (void **) &pv_param_type ) == FAILURE

            //zend_hash_get_current_data_ex(&pv_param_arr->value.arr,&pos) == FAILURE 
            //HashPosition hp = 0;
            for (zend_hash_internal_pointer_reset_ex((HashTable *) &pv_param_arr->value.arr, &pos);
                     zend_hash_get_current_data_ex((HashTable *) &pv_param_arr->value.arr,&pos) != NULL;
                     zend_hash_move_forward_ex((HashTable *) &pv_param_arr->value.arr, &pos)
            ) {
               php_error(E_WARNING,"Error getting parameter types");
               SQLFreeStmt(result->stmt,SQL_RESET_PARAMS);
               efree(params);
               RETURN_FALSE;
               }
            convert_to_long( pv_param_type );
            if ( pv_param_type->u1.v.type != IS_LONG )
               {
               php_error(E_WARNING,"Error getting parameter types");
               SQLFreeStmt(result->stmt,SQL_RESET_PARAMS);
               efree(params);
               RETURN_FALSE;
               }

            sqltype = (short)(pv_param_type->value.lval);

            }


         if ( IS_SQL_BINARY(sqltype) )
            ctype = SQL_C_BINARY;
         else if ( IS_SQL_UNICODE(sqltype) )
            ctype = SQL_C_UTF8;
         else
            ctype = SQL_C_CHAR;

         if ( tmp->value.str->val[0] == '\'' &&
              tmp->value.str->val[tmp->value.str->len - 1] == '\'' )
            {
            filename = &tmp->value.str->val[1];
            filename[tmp->value.str->len - 2] = '\0';

            if ( (params[i-1].fp = open(filename,O_RDONLY)) == -1 )
               {
               php_error(E_WARNING,"Can't open file %s", filename);
               SQLFreeStmt(result->stmt, SQL_RESET_PARAMS);
               for ( i = 0; i < result->numparams; i++ )
                  {
                  if ( params[i].fp != -1 )
                     {
                     close(params[i].fp);
                     }
                  }
               efree(params);
               RETURN_FALSE;
               }

            params[i-1].vallen = SQL_LEN_DATA_AT_EXEC(0);

            rc = SQLBindParameter(result->stmt, (SQLUSMALLINT)i, SQL_PARAM_INPUT,
                                  ctype, sqltype, precision, scale,
                                  (void *)params[i-1].fp, 0,
                                  &params[i-1].vallen);
            if (( rc != SQL_SUCCESS ) && ( rc != SQL_SUCCESS_WITH_INFO ))
               {
               ads_sql_error( result->conn_ptr, result->stmt, "SQLBindParameter" );
               efree(params);
               RETVAL_FALSE;
               }
            }
         else
            {

            precision = params[i-1].vallen;

            /*
             * If the parameter is NULL set the length to the special
             * signifier used by ODBC.
             */
            if ( ucType == IS_NULL )
               {
               params[i-1].vallen = SQL_NULL_DATA;
               }

            rc = SQLBindParameter(result->stmt, (SQLSMALLINT)i, SQL_PARAM_INPUT,
                                  ctype, sqltype, precision, scale,
                                  tmp->value.str->val, 0,
                                  &params[i-1].vallen );
            if (( rc != SQL_SUCCESS ) && ( rc != SQL_SUCCESS_WITH_INFO ))
               {
               ads_sql_error( result->conn_ptr, result->stmt, "SQLBindParameter" );
               efree(params);
               RETVAL_FALSE;
               }

            }
         zend_hash_move_forward(pv_param_arr->value.arr);

         /* Increment the position of the types array */
         if ( numArgs == 3 )
            zend_hash_move_forward(pv_param_types->value.arr);

         }
      }
   /* Close cursor, needed for doing multiple selects */
   rc = SQLFreeStmt(result->stmt, SQL_CLOSE);
   if ( rc == SQL_ERROR )
   {
      ads_sql_error(result->conn_ptr, result->stmt, "SQLFreeStmt");
   }
   //php_printf("Before SQL Execute Result");
   rc = SQLExecute( result->stmt );
   result->fetched = 0;

   if ( rc == SQL_NEED_DATA )
      {
         //php_printf("SQL NEED DATA");
      char buf[4096];
      int fp, nbytes;
      while ( rc == SQL_NEED_DATA )
         {
         rc = SQLParamData(result->stmt, (void*)&fp);
         switch ( rc )
            {
            case SQL_SUCCESS:
               break;
            case SQL_NO_DATA_FOUND:
            case SQL_SUCCESS_WITH_INFO:
            case SQL_ERROR:
               ads_sql_error(result->conn_ptr, result->stmt, "SQLExecute");
               break;
            case SQL_NEED_DATA:
               while ( (nbytes = read(fp, &buf, 4096)) > 0 )
                  {
                  if ( SQLPutData( result->stmt, (void*)&buf, nbytes ) != SQL_SUCCESS )
                     ads_sql_error( result->conn_ptr, result->stmt, "SQLExecute" );
                  }
               break;
            default:
               efree(params);
               RETVAL_FALSE;
            }
         }
      }
   else
      {
         //php_printf("SQL SUCCESS");
      switch ( rc )
         {
         case SQL_SUCCESS:
            break;
         case SQL_NO_DATA_FOUND:
         case SQL_SUCCESS_WITH_INFO:
         case SQL_ERROR:
            ads_sql_error(result->conn_ptr, result->stmt, "SQLExecute");
            break;
         default:
            RETVAL_FALSE;
         }
      }

   if ( result->numparams > 0 )
      {
      SQLFreeStmt(result->stmt, SQL_RESET_PARAMS);

      for ( i = 0; i < result->numparams; i++ )
         {
         if ( params[i].fp != -1 )
            close(params[i].fp);
         }
      efree(params);
      }

   if ( rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO
        || rc == SQL_NO_DATA_FOUND )
      {
      RETVAL_TRUE;
      }

   if ( result->numcols == 0 )
      {
      SQLNumResultCols(result->stmt, &(result->numcols));

      if ( result->numcols > 0 )
         {
         if ( !ads_bindcols(result TSRMLS_CC) )
            {
            efree(result);
            RETVAL_FALSE;
            }
         }
      else
         {
         result->values = NULL;
         }
      }
   }
/* }}} */

/* {{{ proto string ads_cursor(int result_id)
   Get cursor name */
PHP_FUNCTION(ads_cursor)
   {
   zval *pv_res;
   SQLSMALLINT len, max_len;
   char *cursorname;
   ads_result *result;
   RETCODE rc;

   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &pv_res) == FAILURE )
      {
      return;
      }
    if ((result = (ads_result *)zend_fetch_resource_ex(pv_res, "Advantage result", le_result)) == NULL) {
         RETURN_FALSE;
      }
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);

   rc = SQLGetInfo(result->conn_ptr->hdbc,SQL_MAX_CURSOR_NAME_LEN,
                   (void *)&max_len,0,&len);
   if ( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO )
      {
      RETURN_FALSE;
      }

   if ( max_len > 0 )
      {
      cursorname = emalloc(max_len + 1);
      if ( cursorname == NULL )
         {
         php_error(E_WARNING,"Out of memory");
         RETURN_FALSE;
         }
      rc = SQLGetCursorName(result->stmt,(unsigned char*)cursorname,(SQLSMALLINT)max_len,&len);
      if ( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO )
         {
         char    state[6];     /* Not used */
         SDWORD  error;        /* Not used */
         char    errormsg[255];
         SQLSMALLINT   errormsgsize; /* Not used */

         SQLError( result->conn_ptr->henv, result->conn_ptr->hdbc,
                   result->stmt, (unsigned char*)state, &error, (unsigned char*)errormsg,
                   sizeof(errormsg)-1, &errormsgsize);
         if ( !strncmp((char*)state,"S1015",5) )
            {
#ifdef PHP_WIN32
            sprintf((char*)cursorname,"php_curs_%d", (int)result->stmt);
#else
            sprintf((char*)cursorname,"php_curs_%ld", (long int)result->stmt);
#endif
            if ( SQLSetCursorName(result->stmt,(unsigned char*)cursorname,SQL_NTS) != SQL_SUCCESS )
               {
               ads_sql_error(result->conn_ptr, result->stmt, "SQLSetCursorName");
               RETVAL_FALSE;
               }
            else
               {
               RETVAL_STRING((char*)cursorname);
               }
            }
         else
            {
            php_error(E_WARNING, "SQL error: %s, SQL state %s", errormsg, state);
            RETVAL_FALSE;
            }
         }
      else
         {
         RETVAL_STRING((char*)cursorname);
         }
      efree(cursorname);
      }
   else
      {
      RETVAL_FALSE;
      }
   }
/* }}} */

/* {{{ proto int ads_exec(int connection_id, string query [, int flags])
   Prepare and execute an SQL statement */
/* XXX Use flags */
PHP_FUNCTION(ads_exec)
   {
   zval *pv_conn; //, **pv_query, **pv_flags;
   zend_long flags;
   int numArgs;
   int64_t query_len;
   char *query;
   ads_result *result = NULL;
   ads_connection *conn;
   RETCODE rc;
#ifdef HAVE_SQL_EXTENDED_FETCH
   SQLUINTEGER      scrollopts;
#endif

   numArgs = ZEND_NUM_ARGS();

   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs|l", &pv_conn, &query, &query_len, &flags) == FAILURE )
      {
      return;
      }
   if ((conn = (ads_connection *)zend_fetch_resource2(Z_RES_P(pv_conn), "Advantage-Link", le_conn, le_pconn)) == NULL) {
      RETURN_FALSE;
   }

   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);

   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);

   result = (ads_result *)emalloc(sizeof(ads_result));
   if ( result == NULL )
      {
      php_error(E_WARNING, "Out of memory");
      RETURN_FALSE;
      }

   rc = SQLAllocStmt(conn->hdbc, &(result->stmt));
   if ( rc == SQL_INVALID_HANDLE )
      {
      php_error(E_WARNING, "SQLAllocStmt error 'Invalid Handle'");
      efree(result);
      RETURN_FALSE;
      }

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLAllocStmt");
      efree(result);
      RETURN_FALSE;
      }

#ifdef HAVE_SQL_EXTENDED_FETCH
   /* Solid doesn't have ExtendedFetch, if DriverManager is used, get Info,
      whether Driver supports ExtendedFetch */
   rc = SQLGetInfo(conn->hdbc, SQL_FETCH_DIRECTION, (void *) &scrollopts, sizeof(scrollopts), NULL);
   if ( rc == SQL_SUCCESS )
      {
      if ( (result->fetch_abs = (scrollopts & SQL_FD_FETCH_ABSOLUTE)) )
         {
         /*
          * Set the Cursor to forward scrolling only because there is a major permformance hit
          * for setting it to SQL_CURSOR_KEYSET
          */
         if ( SQLSetStmtOption(result->stmt, SQL_CURSOR_TYPE, SQL_CURSOR_DYNAMIC )
              == SQL_ERROR )
            {
            ads_sql_error(conn, result->stmt, " SQLSetStmtOption");
            SQLFreeStmt(result->stmt, SQL_DROP);
            result->stmt = 0;
            efree(result);
            RETURN_FALSE;
            }
         }
      }
   else
      {
      result->fetch_abs = 0;
      }
#endif

   rc = SQLExecDirect(result->stmt, (unsigned char*)query, SQL_NTS);
   if ( rc != SQL_SUCCESS
        && rc != SQL_SUCCESS_WITH_INFO
        && rc != SQL_NO_DATA_FOUND )
      {
      /* XXX FIXME we should really check out SQLSTATE with SQLError
       * in case rc is SQL_SUCCESS_WITH_INFO here.
       */
      ads_sql_error(conn, result->stmt, "SQLExecDirect");
      SQLFreeStmt(result->stmt, SQL_DROP);
      result->stmt = 0;
      efree(result);
      RETURN_FALSE;
      }

   SQLNumResultCols(result->stmt, &(result->numcols));

   /* For insert, update etc. cols == 0 */
   if ( result->numcols > 0 )
      {
      if ( !ads_bindcols(result TSRMLS_CC) )
         {
         efree(result);
         RETURN_FALSE;
         }
      }
   else
      {
      result->values = NULL;
      }

   Z_ADDREF_P(pv_conn);
   //Z_RES_P(conn->id)->gc.refcount++;
   //zend_list_addref(conn->id);
   result->conn_ptr = conn;
   result->fetched = 0;
   //ZEND_REGISTER_RESOURCE(return_value, result, le_result);
   RETURN_RES(zend_register_resource(result, le_result));
   }
/* }}} */

#define ADS_NUM  1
#define ADS_OBJECT  2

static void php_ads_fetch_hash(INTERNAL_FUNCTION_PARAMETERS, int result_type)
   {
   int i;
   ads_result *result;
   RETCODE rc;
   SQLSMALLINT sql_c_type;
   char *buf = NULL;
   zend_string *zstrrr ;
#ifdef HAVE_SQL_EXTENDED_FETCH
   UDWORD crow;
   SQLUSMALLINT  RowStatus[1];
   SDWORD rownum = -1;
   zval *pv_res, *tmp;
   long  row = -1;

   if (zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "r|l", &pv_res, &row ) == FAILURE)
      {
      return;
      }

   rownum = row;

#else
   zval *pv_res, *tmp;
   if (zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "r", &pv_res ) == FAILURE)
   {
      return;
   }
#endif
   if ((result = (ads_result *)zend_fetch_resource_ex(pv_res, "Advantage result", le_result)) == NULL) {
         RETURN_FALSE;
   }
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);

   if ( result->numcols == 0 )
      {
      php_error(E_WARNING, "No tuples available at this result index");
      RETURN_FALSE;
      }
      
   /* 
   Hier noch eine Alternative für array_init
   if ( array_init(return_value)==FAILURE )
      {
      RETURN_FALSE;
      }
   */
#ifdef HAVE_SQL_EXTENDED_FETCH
   if ( result->fetch_abs )
      {
      if ( rownum > 0 )
         rc = SQLExtendedFetch(result->stmt,SQL_FETCH_ABSOLUTE,rownum,&crow,RowStatus);
      else
         rc = SQLExtendedFetch(result->stmt,SQL_FETCH_NEXT,1,&crow,RowStatus);
      }
   else
#endif
      rc = SQLFetch(result->stmt);

   if ( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO )
      RETURN_FALSE;

#ifdef HAVE_SQL_EXTENDED_FETCH
   if ( rownum > 0 && result->fetch_abs )
      result->fetched = rownum;
   else
#endif
      result->fetched++;

   for ( i = 0; i < result->numcols; i++ )
      {
      //ALLOC_INIT_ZVAL(tmp);
#ifdef PHP_5_3_OR_GREATER
      tmp->value.counted->gc.refcount = 1;
#else
      tmp->refcount = 1;
#endif
      tmp->u1.v.type = IS_STRING;
      tmp->value.str->len = 0;

      if ( IS_SQL_UNICODE( result->values[i].coltype ) )
         sql_c_type = SQL_C_UTF8;
      else
         sql_c_type = SQL_C_CHAR;

      switch ( result->values[i].coltype )
         {
         case SQL_BINARY:
         case SQL_GUID:
         case SQL_VARBINARY:
         case SQL_LONGVARBINARY:
            if ( result->binmode <= 0 )
               {
               #if PHP_MAJOR_VERSION == 4
                  Z_STRVAL_P(tmp) = empty_string;
               #else
                  //strcpy(char* destination, const char* source);
                  memcpy(tmp->value.str->val,    STR_EMPTY_ALLOC(), 0);
               #endif
               break;
               }
            if ( result->binmode == 1 ) sql_c_type = SQL_C_BINARY;
         case SQL_LONGVARCHAR:
         case SQL_WLONGVARCHAR:
            if ( IS_SQL_LONG(result->values[i].coltype) &&
                 result->longreadlen <= 0 )
               {
               #if PHP_MAJOR_VERSION == 4
                  Z_STRVAL_P(tmp) = empty_string;
               #else
                  memcpy(tmp->value.str->val,    STR_EMPTY_ALLOC(), 0);
                  //Z_STRVAL_P(tmp) = STR_EMPTY_ALLOC();
               #endif

               break;
               }

            if ( buf == NULL ) buf = emalloc(result->longreadlen + 1);
            rc = SQLGetData(result->stmt, (SQLUSMALLINT)(i + 1),sql_c_type,
                            buf, result->longreadlen + 1, &result->values[i].vallen);

            if ( rc == SQL_ERROR )
               {
               ads_sql_error(result->conn_ptr, result->stmt, "SQLGetData");
               efree(buf);
               RETURN_FALSE;
               }
            if ( rc == SQL_SUCCESS_WITH_INFO )
               {
               tmp->value.str->len = result->longreadlen;
               }
            else if ( result->values[i].vallen == SQL_NULL_DATA )
               {
               #if PHP_MAJOR_VERSION == 4
                  Z_STRVAL_P(tmp) = empty_string;
               #else
                  memcpy(tmp->value.str->val,    STR_EMPTY_ALLOC(), 0);
                  //Z_STRVAL_P(tmp) = STR_EMPTY_ALLOC();
               #endif

               break;
               }
            else
               {
               tmp->value.str->len = result->values[i].vallen;
               }
            memcpy(tmp->value.str->val, estrndup(buf, tmp->value.str->len), tmp->value.str->len);
            break;

         default:
            if ( result->values[i].vallen == SQL_NULL_DATA )
               {
               #if PHP_MAJOR_VERSION == 4
                  Z_STRVAL_P(tmp) = empty_string;
               #else
                  memcpy(tmp->value.str->val,    STR_EMPTY_ALLOC(), 0);
               #endif

               break;
               }
            tmp->value.str->len = result->values[i].vallen;
             memcpy(tmp->value.str->val, estrndup(result->values[i].value,tmp->value.str->len), tmp->value.str->len);
            break;
         }
      if ( result_type & ADS_NUM )
         {
         zend_hash_index_update(return_value->value.arr, i, tmp);
         }
      else
         {
            memcpy(zstrrr->val, result->values[i].name, strlen(result->values[i].name));

            zend_hash_update(return_value->value.arr, zstrrr, tmp);
         }
      }
   if ( buf ) efree(buf);
   }


/* {{{ proto object ads_fetch_object(int result_id [, int rownumber])
   Fetch a result row as an object */
PHP_FUNCTION(ads_fetch_object)
   {
   php_ads_fetch_hash(INTERNAL_FUNCTION_PARAM_PASSTHRU, ADS_OBJECT);
   if ( Z_TYPE_P(return_value) == IS_ARRAY )
      {
      object_and_properties_init(return_value, ZEND_STANDARD_CLASS_DEF_PTR, Z_ARRVAL_P(return_value));
      }
   }
/* }}} */

/* {{{ proto array ads_fetch_array(int result_id [, int rownumber])
   Fetch a result row as an associative array */
PHP_FUNCTION(ads_fetch_array)
   {
   php_ads_fetch_hash(INTERNAL_FUNCTION_PARAM_PASSTHRU, ADS_OBJECT);
   }
/* }}} */


/* {{{ proto int ads_fetch_into(int result_id [, int rownumber], array result_array)
   Fetch one result row into an array */
PHP_FUNCTION(ads_fetch_into)
   {
   int i;
   ads_result *result;
   RETCODE rc;
   SQLSMALLINT sql_c_type;
   char *buf = NULL;
#ifdef HAVE_SQL_EXTENDED_FETCH
   UDWORD crow;
   SQLUSMALLINT  RowStatus[1];
   SDWORD rownum = -1;
   zval *pv_res, **pv_res_arr, *tmp;
   long row = 0;

   if (zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "r|lZ", &pv_res, &row, &pv_res_arr) == FAILURE)
      {
      return;
      }

   rownum = row;
#else
   zval *pv_res, **pv_res_arr, *tmp;

   numArgs = ZEND_NUM_ARGS();

   if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rZ", &pv_res, &pv_res_arr) == FAILURE)
      {
      return;
      }
#endif
   if ((result = (ads_result *)zend_fetch_resource_ex(pv_res, "Advantage result", le_result)) == NULL) {
      RETURN_FALSE;
   }
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);

   if ( result->numcols == 0 )
      {
      php_error(E_WARNING, "No tuples available at this result index");
      RETURN_FALSE;
      }

   if ( (*pv_res_arr)->u1.v.type != IS_ARRAY )
      {
      /*if ( array_init(*pv_res_arr) == FAILURE )
         {
         php_error(E_WARNING, "Can't convert to type Array");
         RETURN_FALSE;
         }*/
      }

#ifdef HAVE_SQL_EXTENDED_FETCH
   if ( result->fetch_abs )
      {
      if ( rownum > 0 )
         rc = SQLExtendedFetch(result->stmt,SQL_FETCH_ABSOLUTE,rownum,&crow,RowStatus);
      else
         rc = SQLExtendedFetch(result->stmt,SQL_FETCH_NEXT,1,&crow,RowStatus);
      }
   else
#endif
      rc = SQLFetch(result->stmt);

   if ( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO )
      RETURN_FALSE;

#ifdef HAVE_SQL_EXTENDED_FETCH
   if ( rownum > 0 && result->fetch_abs )
      result->fetched = rownum;
   else
#endif
      result->fetched++;

   for ( i = 0; i < result->numcols; i++ )
      {
      zval *tmp;
#ifdef PHP_5_3_OR_GREATER
     tmp->value.counted->gc.refcount = 1;
#else
      tmp->refcount = 1;
#endif
      tmp->u1.v.type = IS_STRING;
      tmp->value.str->len = 0;

      if ( IS_SQL_UNICODE( result->values[i].coltype ) )
         sql_c_type = SQL_C_UTF8;
      else
         sql_c_type = SQL_C_CHAR;

      switch ( result->values[i].coltype )
         {
         case SQL_BINARY:
         case SQL_GUID:
         case SQL_VARBINARY:
         case SQL_LONGVARBINARY:
            if ( result->binmode <= 0 )
               {
               #if PHP_MAJOR_VERSION == 4
                  Z_STRVAL_P(tmp) = empty_string;
               #else
                  memcpy(tmp->value.str->val,    STR_EMPTY_ALLOC(), 0);
               #endif

               break;
               }
            if ( result->binmode == 1 ) sql_c_type = SQL_C_BINARY;
         case SQL_LONGVARCHAR:
         case SQL_WLONGVARCHAR:
            if ( IS_SQL_LONG(result->values[i].coltype) &&
                 result->longreadlen <= 0 )
               {
               #if PHP_MAJOR_VERSION == 4
                  Z_STRVAL_P(tmp) = empty_string;
               #else
                  memcpy(tmp->value.str->val,    STR_EMPTY_ALLOC(), 0);
                  //Z_STRVAL_P(tmp) = STR_EMPTY_ALLOC();
               #endif

               break;
               }

            if ( buf == NULL ) buf = emalloc(result->longreadlen + 1);
            rc = SQLGetData(result->stmt, (SQLUSMALLINT)(i + 1),sql_c_type,
                            buf, result->longreadlen + 1, &result->values[i].vallen);

            if ( rc == SQL_ERROR )
               {
               ads_sql_error(result->conn_ptr, result->stmt, "SQLGetData");
               efree(buf);
               RETURN_FALSE;
               }
            if ( rc == SQL_SUCCESS_WITH_INFO )
               {
               tmp->value.str->len = result->longreadlen;
               }
            else if ( result->values[i].vallen == SQL_NULL_DATA )
               {
               #if PHP_MAJOR_VERSION == 4
                  Z_STRVAL_P(tmp) = empty_string;
               #else
                  memcpy(tmp->value.str->val,    STR_EMPTY_ALLOC(), 0);
                  //Z_STRVAL_P(tmp) = STR_EMPTY_ALLOC();
               #endif

               break;
               }
            else
               {
               tmp->value.str->len = result->values[i].vallen;
               }
            memcpy(tmp->value.str->val, estrndup(result->values[i].value,tmp->value.str->len), tmp->value.str->len);
            //tmp->value.str->val = estrndup(buf, tmp->value.str->len);
            break;

         default:
            if ( result->values[i].vallen == SQL_NULL_DATA )
               {
               #if PHP_MAJOR_VERSION == 4
                  Z_STRVAL_P(tmp) = empty_string;
               #else
                  memcpy(tmp->value.str->val,    STR_EMPTY_ALLOC(), 0);
                  //Z_STRVAL_P(tmp) = STR_EMPTY_ALLOC();
               #endif

               break;
               }
            tmp->value.str->len = result->values[i].vallen;
            memcpy(tmp->value.str->val, estrndup(result->values[i].value,tmp->value.str->len), tmp->value.str->len);
            //tmp->value.str->val = estrndup(result->values[i].value,tmp->value.str->len);
            break;
         }
      zend_hash_index_update((*pv_res_arr)->value.arr, i, tmp);
      }
   if ( buf ) efree(buf);
   RETURN_LONG(result->numcols);
   }
/* }}} */

/* {{{ proto int ads_fetch_row(int result_id [, int row)number])
   Fetch a row */
PHP_FUNCTION(ads_fetch_row)
   {
   SDWORD rownum = 1;
   ads_result *result;
   RETCODE rc;
   zval *pv_res;
   zend_long row = 1;
#ifdef HAVE_SQL_EXTENDED_FETCH
   UDWORD crow;
   SQLUSMALLINT RowStatus[1];
#endif

   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r|l", &pv_res, &row) == FAILURE )
      {
      return;
      }

   rownum = row;
   if ((result = (ads_result *)zend_fetch_resource_ex(pv_res, "Advantage result", le_result)) == NULL) {
      RETURN_FALSE;
   }
   //php_printf("RESULT NOT FALSE");
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);

   if ( result->numcols == 0 )
      {
      php_error(E_WARNING, "No tuples available at this result index");
      RETURN_FALSE;
      }

#ifdef HAVE_SQL_EXTENDED_FETCH
   if ( result->fetch_abs )
      {
      if ( ZEND_NUM_ARGS() > 1 )
         rc = SQLExtendedFetch(result->stmt,SQL_FETCH_ABSOLUTE,rownum,&crow,RowStatus);
      else
         rc = SQLExtendedFetch(result->stmt,SQL_FETCH_NEXT,1,&crow,RowStatus);
      }
   else
#endif
    rc = SQLFetch(result->stmt);

   if ( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO )
      {
      RETURN_FALSE;
      }

   if ( ZEND_NUM_ARGS() > 1 )
      {
      result->fetched = rownum;
      }
   else
      {
      result->fetched++;
      }

   RETURN_TRUE;
   }
/* }}} */

/* {{{ proto string ads_result(int result_id, mixed field)
   Get result data */
PHP_FUNCTION(ads_result)
   {
   char *field;
   int field_ind;
   SQLSMALLINT sql_c_type = SQL_C_CHAR;
   ads_result *result;
   int i = 0;
   RETCODE rc;
   SDWORD   fieldsize;
   zval *pv_res, *pv_field;
#ifdef HAVE_SQL_EXTENDED_FETCH
   UDWORD crow;
   SQLUSMALLINT RowStatus[1];
#endif

   field_ind = -1;
   field = NULL;

   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rz", &pv_res, &pv_field) == FAILURE )
      {
      return;
      }

   if ( pv_field->u1.v.type == IS_STRING )
      {
      field = pv_field->value.str->val;
      }
   else
      {
      convert_to_long_ex(pv_field);
      field_ind = pv_field->value.lval - 1;
      }
    if ((result = (ads_result *)zend_fetch_resource_ex(pv_res, "Advantage result", le_result)) == NULL) {
         RETURN_FALSE;
    }
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);

   if ( (result->numcols == 0) )
      {
      php_error(E_WARNING, "No tuples available at this result index");
      RETURN_FALSE;
      }

   /* get field index if the field parameter was a string */
   if ( field != NULL )
      {
      for ( i = 0; i < result->numcols; i++ )
         {
         if ( !strcasecmp(result->values[i].name, field) )
            {
            field_ind = i;
            break;
            }
         }

      if ( field_ind < 0 )
         {
         php_error(E_WARNING, "Field %s not found", field);
         RETURN_FALSE;
         }
      }
   else
      {
      /* check for limits of field_ind if the field parameter was an int */
      if ( field_ind >= result->numcols || field_ind < 0 )
         {
         php_error(E_WARNING, "Field index is larger than the number of fields");
         RETURN_FALSE;
         }
      }

   if ( result->fetched == 0 )
      {
      /* User forgot to call ads_fetchrow(), let's do it here */
#ifdef HAVE_SQL_EXTENDED_FETCH
      if ( result->fetch_abs )
         rc = SQLExtendedFetch(result->stmt, SQL_FETCH_NEXT, 1, &crow,RowStatus);
      else
#endif
         rc = SQLFetch(result->stmt);

      if ( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO )
         RETURN_FALSE;

      result->fetched++;
      }

   if ( IS_SQL_UNICODE( result->values[field_ind].coltype ) )
      sql_c_type = SQL_C_UTF8;
   else
      sql_c_type = SQL_C_CHAR;

   switch ( result->values[field_ind].coltype )
      {
      case SQL_BINARY:
      case SQL_GUID:
      case SQL_VARBINARY:
      case SQL_LONGVARBINARY:
         if ( result->binmode <= 1 ) sql_c_type = SQL_C_BINARY;
         if ( result->binmode <= 0 ) break;
      case SQL_LONGVARCHAR:
      case SQL_WLONGVARCHAR:
         if ( IS_SQL_LONG(result->values[field_ind].coltype) )
            {
            if ( result->longreadlen <= 0 )
               break;
            else
               fieldsize = result->longreadlen;
            }
         else
            {
            SQLColAttributes(result->stmt, (SQLUSMALLINT)(field_ind + 1),
                             (SQLUSMALLINT)((sql_c_type == SQL_C_BINARY) ? SQL_COLUMN_LENGTH :
                                     SQL_COLUMN_DISPLAY_SIZE),
                             NULL, 0, NULL, &fieldsize);
            }
         /* For char data, the length of the returned string will be longreadlen - 1 */
         fieldsize = (result->longreadlen <= 0) ? 4096 : result->longreadlen;
         field = emalloc(fieldsize);
         if ( !field )
            {
            php_error(E_WARNING, "Out of memory");
            RETURN_FALSE;
            }
         /* SQLGetData will truncate CHAR data to fieldsize - 1 bytes and append \0.
            For binary data it is truncated to fieldsize bytes.
          */
         rc = SQLGetData(result->stmt, (SQLUSMALLINT)(field_ind + 1), sql_c_type,
                         field, fieldsize, &result->values[field_ind].vallen);

         if ( rc == SQL_ERROR )
            {
            ads_sql_error(result->conn_ptr, result->stmt, "SQLGetData");
            efree(field);
            RETURN_FALSE;
            }

         if ( result->values[field_ind].vallen == SQL_NULL_DATA || rc == SQL_NO_DATA_FOUND )
            {
            efree(field);
            RETURN_FALSE;
            }
         /* Reduce fieldlen by 1 if we have char data. One day we might
            have binary strings... */
         if ( result->values[field_ind].coltype == SQL_LONGVARCHAR ) fieldsize -= 1;
         /* Don't duplicate result, saves one emalloc.
         For SQL_SUCCESS, the length is in vallen.
       */
         RETURN_STRINGL(field, (rc == SQL_SUCCESS_WITH_INFO) ? fieldsize :
                        result->values[field_ind].vallen);
         break;

      default:
         if ( result->values[field_ind].vallen == SQL_NULL_DATA )
            {
            RETURN_FALSE;
            }
         else
            {
            RETURN_STRINGL(result->values[field_ind].value, result->values[field_ind].vallen);
            }
         break;
      }

/* If we come here, output unbound LONG and/or BINARY column data to the client */

   /* We emalloc 1 byte more for SQL_C_CHAR (trailing \0) */
   fieldsize = (sql_c_type == SQL_C_CHAR || sql_c_type == SQL_C_UTF8) ? 4096 : 4095;
   if ( (field = emalloc(fieldsize)) == NULL )
      {
      php_error(E_WARNING,"Out of memory");
      RETURN_FALSE;
      }

   /* Call SQLGetData() until SQL_SUCCESS is returned */
   while ( 1 )
      {
      rc = SQLGetData(result->stmt, (SQLUSMALLINT)(field_ind + 1),sql_c_type,
                      field, fieldsize, &result->values[field_ind].vallen);

      if ( rc == SQL_ERROR )
         {
         ads_sql_error(result->conn_ptr, result->stmt, "SQLGetData");
         efree(field);
         RETURN_FALSE;
         }

      if ( result->values[field_ind].vallen == SQL_NULL_DATA )
         {
         efree(field);
         RETURN_FALSE;
         }
      /* chop the trailing \0 by outputing only 4095 bytes */
      PHPWRITE(field,(rc == SQL_SUCCESS_WITH_INFO) ? 4095 :
               result->values[field_ind].vallen);

      if ( rc == SQL_SUCCESS )
         { /* no more data avail */
         efree(field);
         RETURN_TRUE;
         }
      }
   RETURN_TRUE;
   }
/* }}} */

/* {{{ proto int ads_result_all(int result_id [, string format])
   Print result as HTML table */
PHP_FUNCTION(ads_result_all)
   {
   char *buf = NULL;
   int i;
   ads_result *result;
   RETCODE rc;
   zval *pv_res;
   char *pv_format = NULL;
   int pv_format_len = 0;
   SQLSMALLINT sql_c_type;
#ifdef HAVE_SQL_EXTENDED_FETCH
   UDWORD crow;
   SQLUSMALLINT RowStatus[1];
#endif

   if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r|s", &pv_res, &pv_format, &pv_format_len) == FAILURE)
      {
      return;
      }
   if ((result = (ads_result *)zend_fetch_resource_ex(pv_res, "Advantage result", le_result)) == NULL) {
      RETURN_FALSE;
   }
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);

   if ( result->numcols == 0 )
      {
      php_error(E_NOTICE, "No tuples available at this result index");
      RETURN_FALSE;
      }
#ifdef HAVE_SQL_EXTENDED_FETCH
   if ( result->fetch_abs )
      rc = SQLExtendedFetch(result->stmt,SQL_FETCH_NEXT,1,&crow,RowStatus);
   else
#endif
      rc = SQLFetch(result->stmt);

   if ( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO )
      {
      php_printf("<h2>No rows found</h2>\n");
      RETURN_LONG(0);
      }

   /* Start table tag */
   if ( ZEND_NUM_ARGS() == 1 )
      {
      php_printf("<table><tr>");
      }
   else
      {
      php_printf("<table %s ><tr>", pv_format);
      }

   for ( i = 0; i < result->numcols; i++ )
      php_printf("<th>%s</th>", result->values[i].name);

   php_printf("</tr>\n");

   while ( rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO )
      {
      result->fetched++;
      php_printf("<tr>");
      for ( i = 0; i < result->numcols; i++ )
         {
         if ( IS_SQL_UNICODE( result->values[i].coltype ) )
            sql_c_type = SQL_C_UTF8;
         else
            sql_c_type = SQL_C_CHAR;

         switch ( result->values[i].coltype )
            {
            case SQL_BINARY:
            case SQL_GUID:
            case SQL_VARBINARY:
            case SQL_LONGVARBINARY:
               if ( result->binmode <= 0 )
                  {
                  php_printf("<td>Not printable</td>");
                  break;
                  }
               if ( result->binmode <= 1 ) sql_c_type = SQL_C_BINARY;
            case SQL_LONGVARCHAR:
            case SQL_WLONGVARCHAR:
               if ( IS_SQL_LONG(result->values[i].coltype) &&
                    result->longreadlen <= 0 )
                  {
                  php_printf("<td>Not printable</td>");
                  break;
                  }

               if ( buf == NULL )
                  {
                  buf = emalloc(result->longreadlen);
                  }

               rc = SQLGetData(result->stmt, (SQLUSMALLINT)(i + 1),sql_c_type,
                               buf, result->longreadlen, &result->values[i].vallen);

               php_printf("<td>");

               if ( rc == SQL_ERROR )
                  {
                  ads_sql_error(result->conn_ptr, result->stmt, "SQLGetData");
                  php_printf("</td></tr></table>");
                  efree(buf);
                  RETURN_FALSE;
                  }
               if ( rc == SQL_SUCCESS_WITH_INFO )
                  {
                  PHPWRITE(buf,result->longreadlen);
                  }
               else if ( result->values[i].vallen == SQL_NULL_DATA )
                  {
                  php_printf("&nbsp;</td>");
                  break;
                  }
               else
                  {
                  PHPWRITE(buf, result->values[i].vallen);
                  }
               php_printf("</td>");
               break;
            default:
               if ( result->values[i].vallen == SQL_NULL_DATA )
                  {
                  php_printf("<td>&nbsp;</td>");
                  }
               else
                  {
                  php_printf("<td>%s</td>", result->values[i].value);
                  }
               break;
            }
         }
      php_printf("</tr>\n");

#ifdef HAVE_SQL_EXTENDED_FETCH
      if ( result->fetch_abs )
         rc = SQLExtendedFetch(result->stmt,SQL_FETCH_NEXT,1,&crow,RowStatus);
      else
#endif
         rc = SQLFetch(result->stmt);
      }
   php_printf("</table>\n");
   if ( buf ) efree(buf);
   RETURN_LONG(result->fetched);
   }
/* }}} */

/* {{{ proto int ads_free_result(int result_id)
   Free resources associated with a result */
PHP_FUNCTION(ads_free_result)
   {
   zval *pv_res;
   ads_result *result;
   int i;

   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &pv_res) == FAILURE )
      {
      return;
      }
   if ((result = (ads_result *)zend_fetch_resource_ex(pv_res, "Advantage result", le_result)) == NULL) {
      RETURN_FALSE;
   }
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);
   if ( result->values )
      {
      for ( i = 0; i < result->numcols; i++ )
         {
         if ( result->values[i].value )
            {
            efree(result->values[i].value);
            }
         }
      efree(result->values);
      result->values = NULL;
      }

   zend_list_delete(Z_RES_P(pv_res));

   RETURN_TRUE;
   }
/* }}} */

/* {{{ proto int ads_connect(string DSN, string user, string password [, int cursor_option])
   Connect to a datasource */
PHP_FUNCTION(ads_connect)
   {
   ads_do_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
   }
/* }}} */

/* {{{ proto int ads_pconnect(string DSN, string user, string password [, int cursor_option])
   Establish a persistent connection to a datasource */
PHP_FUNCTION(ads_pconnect)
   {
   ads_do_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
   }
/* }}} */

int ads_sqlconnect(ads_connection **conn, char *db, char *uid, char *pwd, int cur_opt, int persistent TSRMLS_DC)
   {
   RETCODE rc;
   char    acDSNBuf[2048];
   short   sDSNBufLen;
   char    *pcDB = 0;
   int     iDBLen = 0;
   //php_printf("connecting_SQLConnect");
   *conn = (ads_connection *)pemalloc(sizeof(ads_connection), persistent);
   (*conn)->persistent = persistent;
   SQLAllocEnv(&((*conn)->henv));
   SQLAllocConnect((*conn)->henv, &((*conn)->hdbc));

   if ( cur_opt != SQL_CUR_DEFAULT )
      {
      rc = SQLSetConnectOption((*conn)->hdbc, SQL_ODBC_CURSORS, cur_opt);
      if ( rc != SQL_SUCCESS )
         {  /* && rc != SQL_SUCCESS_WITH_INFO ? */
         ads_sql_error(*conn, SQL_NULL_HSTMT, "SQLSetConnectOption");
         SQLFreeConnect((*conn)->hdbc);
         pefree(*conn, persistent);
         return FALSE;
         }
      }

   /*
    * Default to Dynamic Cursors, they are fast, effecient, and allow
    * bi directional cursors.
    */
   rc = SQLSetConnectOption((*conn)->hdbc, SQL_CURSOR_TYPE, SQL_CURSOR_DYNAMIC );
   if ( rc != SQL_SUCCESS )
      {
      ads_sql_error(*conn, SQL_NULL_HSTMT, "SQLSetConnectOption");
      SQLFreeConnect((*conn)->hdbc);
      pefree(*conn, persistent);
      return FALSE;
      }

   /*
    * SQLDriverConnect needs to be inserted here along with appending Driver=Advantage; on to the
    * the connection string so it will not prompt for information.
    */

   if ( uid && !strstr ((char*)db, "uid") && !strstr((char*)db, "UID") )
      {
      iDBLen = strlen(db) + strlen(uid) + strlen( pwd ) + strlen( "UID=;PWD=;driver=Advantage StreamlineSQL ODBC;" ) + 1;
      pcDB = (char*) emalloc( iDBLen );
      sprintf(pcDB, "driver=Advantage StreamlineSQL ODBC;UID=%s;PWD=%s;%s", uid, pwd, db );
      }
   else
      {
      iDBLen = strlen( db )+ strlen( ";driver=Advantage StreamlineSQL ODBC;" ) + 1;
      pcDB = (char*) emalloc( iDBLen );
      sprintf(pcDB, "driver=Advantage StreamlineSQL ODBC;%s", db);
      }

   rc = SQLDriverConnect((*conn)->hdbc, NULL, (unsigned char*)pcDB, (SQLSMALLINT)strlen( pcDB ),
                          (unsigned char*)acDSNBuf, sizeof( acDSNBuf ), &sDSNBufLen, SQL_DRIVER_NOPROMPT);

   if ( pcDB )
      efree( pcDB );


   if ( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO )
      {
      ads_sql_error(*conn, SQL_NULL_HSTMT, "SQLDriverConnect");
      SQLFreeConnect((*conn)->hdbc);
      pefree((*conn), persistent);
      return FALSE;
      }

   /* (*conn)->open = 1;*/
   //php_printf("SQL CONNECT RETURN TRUE");
   return TRUE;
   }


/* Persistent connections: two list-types le_pconn, le_conn and a plist
 * where hashed connection info is stored together with index pointer to
 * the actual link of type le_pconn in the list. Only persistent
 * connections get hashed up. Normal connections use existing pconnections.
 * Maybe this has to change with regard to transactions on pconnections?
 * Possibly set autocommit to on on request shutdown.
 *
 * We do have to hash non-persistent connections, and reuse connections.
 * In the case where two connects were being made, without closing the first
 * connect, access violations were occuring.  This is because some of the
 * "globals" in this module should actualy be per-connection variables.  I
 * simply fixed things to get them working for now.  Shane
 *
 * I have not encountered any problems with not hashing connections.  It
 * should not be a problem with Advantage.  DDS 5/29/2002
 *
 */
void ads_do_connect(INTERNAL_FUNCTION_PARAMETERS, int persistent)
   {
   char    *db = NULL;
   char    *uid = NULL;
   char    *pwd = NULL;
   int64_t db_len;
   int64_t uid_len;
   int64_t pwd_len;
   zend_long opt = SQL_CUR_DEFAULT;
   ads_connection *db_conn;
   char *hashed_details;
   int64_t hashed_len, len, cur_opt;


   int someInt;
   char str[12];
   sprintf(str, "%d", zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "sss|l", &db, &db_len, &uid, &uid_len, &pwd, &pwd_len, &opt));

   /*openlog("Logs", LOG_PID, LOG_USER);
   syslog(LOG_INFO, str);
   closelog();
   */

   /*  Now an optional 4th parameter specifying the cursor type
    *  defaulting to the cursors default
    */
   if (zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "sss|l", &db, &db_len, &uid, &uid_len, &pwd, &pwd_len, &opt) == FAILURE)
   {
      return;
   }

   //openlog("Logs", LOG_PID, LOG_USER);
   //syslog(LOG_INFO, itoa(zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "sss|l", &db, &db_len, &uid, &uid_len, &pwd, &pwd_len, &opt)));
   //closelog();

   /*openlog("Logs", LOG_PID, LOG_USER);
   syslog(LOG_INFO, "After Parsing");
   closelog();
   */

  


   /*
   openlog("Logs", LOG_PID, LOG_USER);
   syslog(LOG_INFO, db);
   closelog();
   */

   cur_opt = opt;

   if(ZEND_NUM_ARGS() > 3)
      {
      /* confirm the cur_opt range */
      if (! (cur_opt == SQL_CUR_USE_IF_NEEDED ||
             cur_opt == SQL_CUR_USE_ODBC ||
             cur_opt == SQL_CUR_USE_DRIVER ||
             cur_opt == SQL_CUR_DEFAULT) )
         {
         php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid Cursor type (%d)",  cur_opt );
         RETURN_FALSE;
         }
      }

   if ( ADSG(allow_persistent) <= 0 )
      {
      persistent = 0;
      }

   /*
    * Make sure to get the length right.
    *   4 for the four underscores.
    *   1 for the null.
    *   11 for the option. maximum of -2147483647
    *
    */
    //printf( "TestthisCrap!" );
   /*
   openlog("Logs", LOG_PID, LOG_USER);
    syslog(LOG_INFO, "BLA:");
   closelog();
    openlog("Logs", LOG_PID, LOG_USER);
   syslog(LOG_INFO,  db);
   syslog(LOG_INFO,  uid);
   syslog(LOG_INFO,  pwd);
   closelog();
   */
    //__asm__("int3");
   //printf( db );
   //rintf( uid );
   //printf( pwd );
   len = strlen(db) + strlen(uid) + strlen(pwd) + strlen("Advantage") + 16;
   hashed_details = emalloc(len);

   if ( hashed_details == NULL )
      {
      php_error(E_WARNING, "Out of memory");
      RETURN_FALSE;
      }

   hashed_len = sprintf(hashed_details, "%s_%s_%s_%s_%d", "Advantage", db, uid, pwd, cur_opt);

   /* FIXME the idea of checking to see if our connection is already persistent
      is good, but it adds a lot of overhead to non-persistent connections.  We
      should look and see if we can fix that somehow */
   /* try to find if we already have this link in our persistent list,
    * no matter if it is to be persistent or not
    */
   // __asm__("int3");
   try_and_get_another_connection:

   if ( persistent )
      {
      zend_resource *le;

      /* the link is not in the persistent list */
      zend_string zs_details; 
      //zend_string *zs_ptr

      memcpy(zs_details.val, hashed_details, sizeof(hashed_details));  
      //zs_ptr 
      if (zend_hash_find(&EG(persistent_list), &zs_details))
         {
            zend_resource new_le;
            //list_entry new_le; // Existing definition.

            if ( ADSG(max_links) != -1 && ADSG(num_links) >= ADSG(max_links) )
               {
               php_error(E_WARNING, "Advantage: Too many open links (%d)", ADSG(num_links));
               efree(hashed_details);
               RETURN_FALSE;
               }
            if ( ADSG(max_persistent) != -1 && ADSG(num_persistent) >= ADSG(max_persistent) )
               {
               php_error(E_WARNING,"Advantage: Too many open persistent links (%d)", ADSG(num_persistent));
               efree(hashed_details);
               RETURN_FALSE;
               }

            if ( !ads_sqlconnect(&db_conn, db, uid, pwd, cur_opt, 1 TSRMLS_CC) )
               {
               efree(hashed_details);
               RETURN_FALSE;
               }

            new_le.type = le_pconn;
            new_le.ptr = db_conn;
            if ( zend_hash_update(&EG(persistent_list), hashed_details, &new_le) == NULL )
               {
               free(db_conn);
               efree(hashed_details);
               RETURN_FALSE;
               }
            ADSG(num_persistent)++;
            ADSG(num_links)++;

            db_conn->res = zend_register_resource(db_conn, le_pconn);//ZEND_REGISTER_RESOURCE(return_value, db_conn, le_pconn);
         }
      else
         { /* found connection */
         if ( Z_TYPE_P(le ) != le_pconn )
            {
            RETURN_FALSE;
            }
         /*
          * check to see if the connection is still valid
          */
         db_conn = (ads_connection *)le->ptr;

         /*
          * check to see if the connection is still in place (lurcher)
          */
         if ( ADSG(check_persistent) )
            {
            RETCODE ret;
            UCHAR d_name[32];
            SQLSMALLINT len;

            ret = SQLGetInfo(db_conn->hdbc,
                             SQL_DATA_SOURCE_READ_ONLY,
                             d_name, sizeof(d_name), &len);

            if ( ret != SQL_SUCCESS || len == 0 )
               {
               zend_hash_del(&EG(persistent_list), hashed_details);
               safe_ads_disconnect(db_conn->hdbc);
               SQLFreeConnect(db_conn->hdbc);
               goto try_and_get_another_connection;
               }
            }
         }
      db_conn->res = zend_register_resource(db_conn, le_pconn);//ZEND_REGISTER_RESOURCE(return_value, db_conn, le_pconn);
      }
   else
      { /* non persistent */
      if ( ADSG(max_links) != -1 && ADSG(num_links) >= ADSG(max_links) )
         {
         php_error(E_WARNING,"Advantage:  Too many open connections (%d)",ADSG(num_links));
         efree(hashed_details);
         RETURN_FALSE;
         }

      if ( !ads_sqlconnect(&db_conn, db, uid, pwd, cur_opt, 0 TSRMLS_CC) )
         {
         efree(hashed_details);
         RETURN_FALSE;
         }
         // __asm__("int3");
      db_conn->res = zend_register_resource(db_conn, le_conn);//ZEND_REGISTER_RESOURCE(return_value, db_conn, le_conn);
       RETURN_RES(db_conn->res);
       //__asm__("int3");
      ADSG(num_links)++;
      }
   efree(hashed_details);
   }

/* {{{ proto void ads_close(int connection_id)
   Closes a connection */
PHP_FUNCTION(ads_close)
{
   zval *pv_conn;
   void *ptr;
   ads_connection *conn;
   ads_result *res;
   int nument;
  
   int type;
   int is_pconn = 0;
   int found_resource_type = le_conn;

   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &pv_conn) == FAILURE )
      {
      return;
      }

      if ((conn = (ads_connection *)zend_fetch_resource2(Z_RES_P(pv_conn), "Advantage-Link", le_conn, le_pconn)) == NULL) {
         RETURN_FALSE;
      }

   if ( found_resource_type==le_pconn )
   {
      is_pconn = 1;
   }

   uint32_t i;
   for (i = 0; i < EG(regular_list).nNumUsed; ++i) {
	   Bucket *b = &EG(regular_list).arData[i];

	   if (Z_ISUNDEF(b->val)) continue;
         if(Z_ISUNDEF(b->val)!=false && (b->val.u1.v.type == le_result))
         {
            res = (ads_result *)&b->val;
            if ( res->conn_ptr == conn )
            {
               zend_list_delete(b->val.value.res);
             }
         }      
	      // do stuff with bucket
   }

   /*nument = zend_hash_next_free_element(&EG(regular_list));

   for ( i = 1; i < nument; i++ )
   {
      ptr = zend_list_find(i, &type);
      if ( ptr && (type == le_result) )
         {
         res = (ads_result *)ptr;
         if ( res->conn_ptr == conn )
            {
            zend_list_delete(i);
            }
         }
   }*/

   zend_list_delete(pv_conn);

   if ( is_pconn == 1)
   {
      zend_hash_apply_with_argument(&EG(persistent_list),(apply_func_arg_t) _close_pconn_with_id, (void *) &(Z_LVAL_P(pv_conn)) TSRMLS_CC);
   }
}
/* }}} */

/* {{{ proto int ads_num_rows(int result_id)
   Get number of rows in a result */
PHP_FUNCTION(ads_num_rows)
   {
   ads_result *result;
   SDWORD rows;
   zval *pv_res;

   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &pv_res) == FAILURE )
      {
      return;
      }
   if ((result = (ads_result *)zend_fetch_resource_ex(pv_res, "Advantage result", le_result)) == NULL) {
      RETURN_FALSE;
   }
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);
   SQLRowCount(result->stmt, &rows);
   RETURN_LONG(rows);
   }
/* }}} */



/* {{{ proto bool ads_next_result(int result_id)
   Checks if multiple results are avaiable */
PHP_FUNCTION(ads_next_result)
{
   ads_result   *result;
   zval         *pv_res;
   int rc, i;

   if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &pv_res) == FAILURE)
   {
     return;
   }
   if ((result = (ads_result *)zend_fetch_resource_ex(Z_RES_P(pv_res), "Advantage result", le_result)) == NULL) {
      RETURN_FALSE;
   }
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);

   if (result->values)
      {
      for(i = 0; i < result->numcols; i++)
         {
         if (result->values[i].value)
            efree(result->values[i].value);
         }
      efree(result->values);
      result->values = NULL;
      }

   result->fetched = 0;
   rc = SQLMoreResults(result->stmt);
   if (rc == SQL_SUCCESS)
      {
      RETURN_TRUE;
      }
   else if (rc == SQL_SUCCESS_WITH_INFO)
      {
      rc = SQLFreeStmt(result->stmt, SQL_UNBIND);
      SQLNumParams(result->stmt, &(result->numparams));
      SQLNumResultCols(result->stmt, &(result->numcols));

      if (result->numcols > 0)
         {
         if (!ads_bindcols(result TSRMLS_CC))
            {
            efree(result);
            RETVAL_FALSE;
            }
         }
      else
         {
         result->values = NULL;
         }
      RETURN_TRUE;
      }
   else
      {
      RETURN_FALSE;
      }
}
/* }}} */


/* {{{ proto int ads_num_fields(int result_id)
   Get number of columns in a result */
PHP_FUNCTION(ads_num_fields)
   {
   ads_result   *result;
   zval     *pv_res;

   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &pv_res) == FAILURE )
      {
      return;
      }
   if ((result = (ads_result *)zend_fetch_resource_ex(pv_res, "Advantage result", le_result)) == NULL) {
      RETURN_FALSE;
   }
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);
   RETURN_LONG(result->numcols);
   }
/* }}} */

/* {{{ proto string ads_field_name(int result_id, int field_number)
   Get a column name */
PHP_FUNCTION(ads_field_name)
   {
   ads_result  *result;
   zval        *pv_res;
   long        num;

   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rl", &pv_res, &num) == FAILURE )
      {
      return;
      }
   if ((result = (ads_result *)zend_fetch_resource_ex(pv_res, "Advantage result", le_result)) == NULL) {
      RETURN_FALSE;
   }
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);

   if ( result->numcols == 0 )
      {
      php_error(E_WARNING, "No tuples available at this result index");
      RETURN_FALSE;
      }

   if ( num > result->numcols )
      {
      php_error(E_WARNING, "Field index larger than number of fields");
      RETURN_FALSE;
      }

   if ( num < 1 )
      {
      php_error(E_WARNING, "Field numbering starts at 1");
      RETURN_FALSE;
      }

   RETURN_STRING(result->values[num - 1].name);
   }
/* }}} */

/* {{{ proto string ads_field_type(int result_id, int field_number)
   Get the datatype of a column */
PHP_FUNCTION(ads_field_type)
   {
   ads_result *result;
   char        tmp[32];
   SQLSMALLINT tmplen;
   zval        *pv_res;
   long        num;

   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rl", &pv_res, &num) == FAILURE )
      {
      return;
      }
   if ((result = (ads_result *)zend_fetch_resource_ex(pv_res, "Advantage result", le_result)) == NULL) {
      RETURN_FALSE;
   }
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advatnage result", le_result);

   if ( result->numcols == 0 )
      {
      php_error(E_WARNING, "No tuples available at this result index");
      RETURN_FALSE;
      }

   if ( num > result->numcols )
      {
      php_error(E_WARNING, "Field index larger than number of fields");
      RETURN_FALSE;
      }

   if ( num < 1 )
      {
      php_error(E_WARNING, "Field numbering starts at 1");
      RETURN_FALSE;
      }

   SQLColAttributes(result->stmt, (SQLUSMALLINT)(num),
                    SQL_COLUMN_TYPE_NAME, tmp, 31, &tmplen, NULL);
   RETURN_STRING(tmp)
   }
/* }}} */

/* {{{ proto int ads_field_len(int result_id, int field_number)
   Get the length (precision) of a column */
PHP_FUNCTION(ads_field_len)
   {
   ads_column_lengths(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
   }
/* }}} */

/* {{{ proto int ads_field_scale(int result_id, int field_number)
   Get the scale of a column */
PHP_FUNCTION(ads_field_scale)
   {
   ads_column_lengths(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
   }
/* }}} */

/* {{{ proto int ads_field_num(int result_id, string field_name)
   Return column number */
PHP_FUNCTION(ads_field_num)
   {
   int field_ind;
   char *fname;
   ads_result *result;
   int i;
   int fname_len;
   zval *pv_res;

   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &pv_res, &fname, &fname_len) == FAILURE )
      {
      return;
      }
   if ((result = (ads_result *)zend_fetch_resource_ex(pv_res, "Advantage result", le_result)) == NULL) {
      RETURN_FALSE;
   }
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);
   //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);

   if ( result->numcols == 0 )
      {
      php_error(E_WARNING, "No tuples available at this result index");
      RETURN_FALSE;
      }

   field_ind = -1;
   for ( i = 0; i < result->numcols; i++ )
      {
      if ( strcasecmp(result->values[i].name, fname) == 0 )
         field_ind = i + 1;
      }

   if ( field_ind == -1 )
      RETURN_FALSE;
   RETURN_LONG(field_ind);
   }
/* }}} */

/* {{{ proto int ads_autocommit(int connection_id [, int OnOff])
   Toggle autocommit mode or get status */
/* There can be problems with pconnections!*/
PHP_FUNCTION(ads_autocommit)
   {
   ads_connection *conn;
   RETCODE rc;
   zval *pv_conn;
   long  onoff = -1;  //initialize so we can tell if it's passed in.

   if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r|l", &pv_conn, &onoff ) == FAILURE)
      {
      return;
      }

   if ((conn = (ads_connection *)zend_fetch_resource2(Z_RES_P(pv_conn), "Advantage-Link", le_conn, le_pconn)) == NULL) {
      RETURN_FALSE;
   }

   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);

   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);

   // Check if onoff was modified.  If it was, then Set the connect option.
   if (onoff != -1)
      {
      rc = SQLSetConnectOption(conn->hdbc, SQL_AUTOCOMMIT,
                               (onoff) ? SQL_AUTOCOMMIT_ON : SQL_AUTOCOMMIT_OFF);
      if ( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO )
         {
         ads_sql_error(conn, SQL_NULL_HSTMT, "Set autocommit");
         RETURN_FALSE;
         }
      RETVAL_TRUE;
      }
   else
      {
      SDWORD status;

      rc = SQLGetConnectOption(conn->hdbc, SQL_AUTOCOMMIT, (PTR)&status);
      if ( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO )
         {
         ads_sql_error(conn, SQL_NULL_HSTMT, "Get commit status");
         RETURN_FALSE;
         }
      RETVAL_LONG((long)status);
      }
   }
/* }}} */

/* {{{ proto int ads_commit(int connection_id)
   Commit an ads transaction */
PHP_FUNCTION(ads_commit)
   {
   ads_transact(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
   }
/* }}} */

/* {{{ proto int ads_rollback(int connection_id)
   Rollback a transaction */
PHP_FUNCTION(ads_rollback)
   {
   ads_transact(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
   }
/* }}} */

static void php_ads_lasterror(INTERNAL_FUNCTION_PARAMETERS, int mode)
   {
   ads_connection *conn;
   zval *pv_handle;
   char *ptr;
   int len;

   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|r", &pv_handle) )
      {
      return;
      }

   if ( mode == 0 )
      {  /* last state */
      len = 6;
      }
   else
      { /* last error message */
      len = SQL_MAX_MESSAGE_LENGTH;
      }
   ptr = emalloc( len + 1 );
   memset( ptr, '\0', len+1 );
   if ( ZEND_NUM_ARGS() == 1 )
      {
      if ((conn = (ads_connection *)zend_fetch_resource2(Z_RES_P(pv_handle), "Advantage-Link", le_conn, le_pconn)) == NULL) {
         RETURN_FALSE;
      }

      //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);
   
      //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_handle, -1, "Advantage-Link", le_conn, le_pconn);
      if ( mode == 0 )
         {
         strlcpy(ptr, conn->laststate, len+1);
         }
      else
         {
         strlcpy(ptr, conn->lasterrormsg, len+1);
         }
      }
   else
      {
      if ( mode == 0 )
         {
         strlcpy(ptr, ADSG(laststate), len+1);
         }
      else
         {
         strlcpy(ptr, ADSG(lasterrormsg), len+1);
         }
      }
   RETVAL_STRING(ptr);
   }

/* {{{ proto string ads_error([int connection_id])
   Get the last error code */
PHP_FUNCTION(ads_error)
   {
   php_ads_lasterror(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
   }
/* }}}    */

/* {{{ proto string ads_errormsg([int connection_id])
   Get the last error message */
PHP_FUNCTION(ads_errormsg)
   {
   php_ads_lasterror(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
   }
/* }}}    */

/* {{{ proto int ads_setoption(int conn_id|result_id, int which, int option, int value)
   Sets connection or statement options */
/* This one has to be used carefully. We can't allow to set connection options for
   persistent connections. I think that SetStmtOption is of little use, since most
   of those can only be specified before preparing/executing statements.
   On the other hand, they can be made connection wide default through SetConnectOption
   - but will be overidden by calls to SetStmtOption() in ads_prepare/ads_do
*/
PHP_FUNCTION(ads_setoption)
   {
   ads_connection *conn;
   ads_result *result;
   RETCODE rc;
   zval *pv_handle;
   long which, opt, val;

   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rlll", &pv_handle, &which, &opt, &val) == FAILURE )
      {
      return;
      }

   switch ( which )
      {
      case 1:     /* SQLSetConnectOption */
          if ((conn = (ads_connection *)zend_fetch_resource2(Z_RES_P(pv_handle), "Advantage-Link", le_conn, le_pconn)) == NULL) {
            RETURN_FALSE;
         }
         //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);
         //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_handle, -1, "Advantage-Link", le_conn, le_pconn);
         if ( conn->persistent )
            {
            php_error(E_WARNING, "Can't set option for persistent connection");
            RETURN_FALSE;
            }
         rc = SQLSetConnectOption(conn->hdbc, (unsigned short)(opt), val);
         if ( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO )
            {
            ads_sql_error(conn, SQL_NULL_HSTMT, "SetConnectOption");
            RETURN_FALSE;
            }
         break;
      case 2:     /* SQLSetStmtOption */
         if ((result = (ads_result *)zend_fetch_resource_ex(pv_handle, "Advantage result", le_result)) == NULL) {
            RETURN_FALSE;
         }
         //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_res, -1, "Advantage result", le_result);
         //ZEND_FETCH_RESOURCE(result, ads_result *, &pv_handle, -1, "Advantage result", le_result);

         rc = SQLSetStmtOption(result->stmt, (unsigned short)(opt), val);

         if ( rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO )
            {
            ads_sql_error(result->conn_ptr, result->stmt, "SetStmtOption");
            RETURN_FALSE;
            }
         break;
      default:
         php_error(E_WARNING, "Unknown option type");
         RETURN_FALSE;
         break;
      }

   RETURN_TRUE;
   }
/* }}} */

/*
 * metadata functions
 */

/* {{{ proto int ads_tables(int connection_id [, string qualifier, string owner, string name, string table_types])
   Call the SQLTables function */
PHP_FUNCTION(ads_tables)
   {
   zval *pv_conn;
   ads_result   *result = NULL;
   ads_connection *conn;
   char *cat = NULL, *schema = NULL, *table = NULL, *type = NULL;
   int cat_len = 0, schema_len = 0, table_len = 0, type_len = 0;
   RETCODE rc;

   if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r|s!sss", &pv_conn, &cat, &cat_len, &schema,
                             &schema_len, &table, &table_len, &type, &type_len) == FAILURE)
   {
      return;
   }
   if ((conn = (ads_connection *)zend_fetch_resource2(Z_RES_P(pv_conn), "Advantage-Link", le_conn, le_pconn)) == NULL) {
       RETURN_FALSE;
   }
   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);
   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);

   result = (ads_result *)emalloc(sizeof(ads_result));
   if ( result == NULL )
      {
      php_error(E_WARNING, "Out of memory");
      RETURN_FALSE;
      }

   rc = SQLAllocStmt(conn->hdbc, &(result->stmt));
   if ( rc == SQL_INVALID_HANDLE )
      {
      efree(result);
      php_error(E_WARNING, "SQLAllocStmt error 'Invalid Handle' in ads_tables");
      RETURN_FALSE;
      }

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLAllocStmt");
      efree(result);
      RETURN_FALSE;
      }

   /* This hack is needed to access table information in Access databases (fmk) */
   if ( table && table_len && schema && schema_len == 0 )
      {
      schema = NULL;
      }

   rc = SQLTables(result->stmt,
                  (unsigned char*)cat, SAFE_SQL_NTS(cat),
                  (unsigned char*)schema, SAFE_SQL_NTS(schema),
                  (unsigned char*)table, SAFE_SQL_NTS(table),
                  (unsigned char*)type, SAFE_SQL_NTS(type));

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLTables");
      efree(result);
      RETURN_FALSE;
      }

   result->numparams = 0;
   SQLNumResultCols(result->stmt, &(result->numcols));

   if ( result->numcols > 0 )
      {
      if ( !ads_bindcols(result TSRMLS_CC) )
         {
         efree(result);
         RETURN_FALSE;
         }
      }
   else
      {
      result->values = NULL;
      }
   result->conn_ptr = conn;
   result->fetched = 0;
   RETURN_RES(zend_register_resource(result, le_result));
   //ZEND_REGISTER_RESOURCE(return_value, result, le_result);
   }
/* }}} */

/* {{{ proto int ads_columns(int connection_id, string qualifier, string owner, string table_name, string column_name)
   Returns a result identifier that can be used to fetch a list of column names in specified tables */
PHP_FUNCTION(ads_columns)
   {
   zval *pv_conn;
   ads_result   *result = NULL;
   ads_connection *conn;
   char *cat = NULL, *schema = NULL, *table = NULL, *column = NULL;
   int  cat_len = 0, schema_len = 0, table_len = 0, column_len = 0;
   RETCODE rc;

   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r|s!sss", &pv_conn, &cat, &cat_len, &schema,
                              &schema_len, &table, &table_len, &column, &column_len) == FAILURE )
      {
      return;
      }

   if ((conn = (ads_connection *)zend_fetch_resource2(Z_RES_P(pv_conn), "Advantage-Link", le_conn, le_pconn)) == NULL) {
       RETURN_FALSE;
   }
   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);
   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);

   result = (ads_result *)emalloc(sizeof(ads_result));
   if ( result == NULL )
      {
      php_error(E_WARNING, "Out of memory");
      RETURN_FALSE;
      }

   rc = SQLAllocStmt(conn->hdbc, &(result->stmt));
   if ( rc == SQL_INVALID_HANDLE )
      {
      efree(result);
      php_error(E_WARNING, "SQLAllocStmt error 'Invalid Handle' in ads_columns");
      RETURN_FALSE;
      }

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLAllocStmt");
      efree(result);
      RETURN_FALSE;
      }

   rc = SQLColumns(result->stmt,
                   (unsigned char*)cat, (SQLSMALLINT)cat_len,
                   (unsigned char*)schema, (SQLSMALLINT)schema_len,
                   (unsigned char*)table, (SQLSMALLINT)table_len,
                   (unsigned char*)column, (SQLSMALLINT)column_len);

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLColumns");
      efree(result);
      RETURN_FALSE;
      }

   result->numparams = 0;
   SQLNumResultCols(result->stmt, &(result->numcols));

   if ( result->numcols > 0 )
      {
      if ( !ads_bindcols(result TSRMLS_CC) )
         {
         efree(result);
         RETURN_FALSE;
         }
      }
   else
      {
      result->values = NULL;
      }
   result->conn_ptr = conn;
   result->fetched = 0;
   RETURN_RES(zend_register_resource(result, le_result));
   //ZEND_REGISTER_RESOURCE(return_value, result, le_result);
   }
/* }}} */


/* {{{ proto int ads_columnprivileges(int connection_id, string catalog, string schema, string table, string column)
   Returns a result identifier that can be used to fetch a list of columns and associated privileges for the specified table */

/*
 * This currently returns an not supported; however, if we support this at some time in the future
 * then the port is already handled
 */
PHP_FUNCTION(ads_columnprivileges)
   {
   zval *pv_conn;
   ads_result   *result = NULL;
   ads_connection *conn;
   char *cat = NULL, *schema = NULL, *table = NULL, *column = NULL;
   int cat_len = 0, schema_len = 0, table_len = 0, column_len = 0;
   RETCODE rc;

   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs!sss", &pv_conn, &cat, &cat_len, &schema,
                              &schema_len, &table, &table_len, &column, &column_len) == FAILURE )
   {
      return;
   }
   if ((conn = (ads_connection *)zend_fetch_resource2(Z_RES_P(pv_conn), "Advantage-Link", le_conn, le_pconn)) == NULL) {
       RETURN_FALSE;
   }
   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);
   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);

   result = (ads_result *)emalloc(sizeof(ads_result));
   if ( result == NULL )
      {
      php_error(E_WARNING, "Out of memory");
      RETURN_FALSE;
      }

   rc = SQLAllocStmt(conn->hdbc, &(result->stmt));
   if ( rc == SQL_INVALID_HANDLE )
      {
      efree(result);
      php_error(E_WARNING, "SQLAllocStmt error 'Invalid Handle' in ads_columnprivileges");
      RETURN_FALSE;
      }

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLAllocStmt");
      efree(result);
      RETURN_FALSE;
      }

   rc = SQLColumnPrivileges(result->stmt,
                            (unsigned char*)cat, SAFE_SQL_NTS(cat),
                            (unsigned char*)schema, SAFE_SQL_NTS(schema),
                            (unsigned char*)table, SAFE_SQL_NTS(table),
                            (unsigned char*)column, SAFE_SQL_NTS(column));

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLColumnPrivileges");
      efree(result);
      RETURN_FALSE;
      }

   result->numparams = 0;
   SQLNumResultCols(result->stmt, &(result->numcols));

   if ( result->numcols > 0 )
      {
      if ( !ads_bindcols(result TSRMLS_CC) )
         {
         efree(result);
         RETURN_FALSE;
         }
      }
   else
      {
      result->values = NULL;
      }
   result->conn_ptr = conn;
   result->fetched = 0;
   RETURN_RES(zend_register_resource(result, le_result));
   //ZEND_REGISTER_RESOURCE(return_value, result, le_result);
   }
/* }}} */


/* {{{ proto int ads_foreignkeys(int connection_id, string pk_qualifier, string pk_owner, string pk_table, string fk_qualifier, string fk_owner, string fk_table)
   Returns a result identifier to either a list of foreign keys in the specified table or a list of foreign keys in other tables that refer to the primary key in the specified table */
PHP_FUNCTION(ads_foreignkeys)
   {
   zval *pv_conn;
   ads_result   *result = NULL;
   ads_connection *conn;
   char *pcat = NULL, *pschema = NULL, *ptable = NULL;
   char *fcat = NULL, *fschema = NULL, *ftable = NULL;
   int pcat_len = 0, pschema_len, ptable_len, fcat_len, fschema_len, ftable_len;
   RETCODE rc;

   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rssssss", &pv_conn, &pcat, &pcat_len,
                              &pschema, &pschema_len, &ptable, &ptable_len, &fcat, &fcat_len,
                              &fschema, &fschema_len, &ftable, &ftable_len ) == FAILURE )
   {
     return;
   }

   if ((conn = (ads_connection *)zend_fetch_resource2(Z_RES_P(pv_conn), "Advantage-Link", le_conn, le_pconn)) == NULL) {
       RETURN_FALSE;
   }
   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);
   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);

   result = (ads_result *)emalloc(sizeof(ads_result));
   if ( result == NULL )
      {
      php_error(E_WARNING, "Out of memory");
      RETURN_FALSE;
      }

   rc = SQLAllocStmt(conn->hdbc, &(result->stmt));
   if ( rc == SQL_INVALID_HANDLE )
      {
      efree(result);
      php_error(E_WARNING, "SQLAllocStmt error 'Invalid Handle' in ads_foreignkeys");
      RETURN_FALSE;
      }

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLAllocStmt");
      efree(result);
      RETURN_FALSE;
      }

   rc = SQLForeignKeys(result->stmt,
                       (unsigned char*)pcat, SAFE_SQL_NTS(pcat),
                       (unsigned char*)pschema, SAFE_SQL_NTS(pschema),
                       (unsigned char*)ptable, SAFE_SQL_NTS(ptable),
                       (unsigned char*)fcat, SAFE_SQL_NTS(fcat),
                       (unsigned char*)fschema, SAFE_SQL_NTS(fschema),
                       (unsigned char*)ftable, SAFE_SQL_NTS(ftable) );

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLForeignKeys");
      efree(result);
      RETURN_FALSE;
      }

   result->numparams = 0;
   SQLNumResultCols(result->stmt, &(result->numcols));

   if ( result->numcols > 0 )
      {
      if ( !ads_bindcols(result TSRMLS_CC) )
         {
         efree(result);
         RETURN_FALSE;
         }
      }
   else
      {
      result->values = NULL;
      }
   result->conn_ptr = conn;
   result->fetched = 0;
   RETURN_RES(zend_register_resource(result, le_result));
   //ZEND_REGISTER_RESOURCE(return_value, result, le_result);
   }
/* }}} */

/* {{{ proto int ads_gettypeinfo(int connection_id [, int data_type])
   Returns a result identifier containing information about data types supported by the data source */
PHP_FUNCTION(ads_gettypeinfo)
   {
   zval *pv_conn;
   long pv_data_type = SQL_ALL_TYPES;
   ads_result   *result = NULL;
   ads_connection *conn;
   RETCODE rc;
   SQLSMALLINT data_type = SQL_ALL_TYPES;

   if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r|l", &pv_conn, &pv_data_type) == FAILURE)
      {
      return;
      }
   if ((conn = (ads_connection *)zend_fetch_resource2(Z_RES_P(pv_conn), "Advantage-Link", le_conn, le_pconn)) == NULL) {
       RETURN_FALSE;
   }
   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);
   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);

   result = (ads_result *)emalloc(sizeof(ads_result));
   if ( result == NULL )
      {
      php_error(E_WARNING, "Out of memory");
      RETURN_FALSE;
      }

   rc = SQLAllocStmt(conn->hdbc, &(result->stmt));
   if ( rc == SQL_INVALID_HANDLE )
      {
      efree(result);
      php_error(E_WARNING, "SQLAllocStmt error 'Invalid Handle' in ads_gettypeinfo");
      RETURN_FALSE;
      }

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLAllocStmt");
      efree(result);
      RETURN_FALSE;
      }

   rc = SQLGetTypeInfo(result->stmt, data_type );

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLGetTypeInfo");
      efree(result);
      RETURN_FALSE;
      }

   result->numparams = 0;
   SQLNumResultCols(result->stmt, &(result->numcols));

   if ( result->numcols > 0 )
      {
      if ( !ads_bindcols(result TSRMLS_CC) )
         {
         efree(result);
         RETURN_FALSE;
         }
      }
   else
      {
      result->values = NULL;
      }
   result->conn_ptr = conn;
   result->fetched = 0;
   RETURN_RES(zend_register_resource(result, le_result));
   //ZEND_REGISTER_RESOURCE(return_value, result, le_result);
   }
/* }}} */

/* {{{ proto int ads_primarykeys(int connection_id, string qualifier, string owner, string table)
   Returns a result identifier listing the column names that comprise the primary key for a table */
PHP_FUNCTION(ads_primarykeys)
   {
   zval *pv_conn;
   ads_result   *result = NULL;
   ads_connection *conn;
   char *cat = NULL, *schema = NULL, *table = NULL;
   int cat_len = 0, schema_len, table_len;
   RETCODE rc;

   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rsss", &pv_conn, &cat, &cat_len, &schema, &schema_len,
                              &table, &table_len) == FAILURE )
      {
      return;
      }
   if ((conn = (ads_connection *)zend_fetch_resource2(Z_RES_P(pv_conn), "Advantage-Link", le_conn, le_pconn)) == NULL) {
       RETURN_FALSE;
   }
   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);
   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);

   result = (ads_result *)emalloc(sizeof(ads_result));
   if ( result == NULL )
      {
      php_error(E_WARNING, "Out of memory");
      RETURN_FALSE;
      }

   rc = SQLAllocStmt(conn->hdbc, &(result->stmt));
   if ( rc == SQL_INVALID_HANDLE )
      {
      efree(result);
      php_error(E_WARNING, "SQLAllocStmt error 'Invalid Handle' in ads_primarykeys");
      RETURN_FALSE;
      }

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLAllocStmt");
      efree(result);
      RETURN_FALSE;
      }

   rc = SQLPrimaryKeys(result->stmt,
                       (unsigned char*)cat, SAFE_SQL_NTS(cat),
                       (unsigned char*)schema, SAFE_SQL_NTS(schema),
                       (unsigned char*)table, SAFE_SQL_NTS(table) );

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLPrimaryKeys");
      efree(result);
      RETURN_FALSE;
      }

   result->numparams = 0;
   SQLNumResultCols(result->stmt, &(result->numcols));

   if ( result->numcols > 0 )
      {
      if ( !ads_bindcols(result TSRMLS_CC) )
         {
         efree(result);
         RETURN_FALSE;
         }
      }
   else
      {
      result->values = NULL;
      }
   result->conn_ptr = conn;
   result->fetched = 0;
   RETURN_RES(zend_register_resource(result, le_result));
   //ZEND_REGISTER_RESOURCE(return_value, result, le_result);
   }
/* }}} */

/* {{{ proto int ads_procedurecolumns(int connection_id [, string qualifier, string owner, string proc, string column])
   Returns a result identifier containing the list of input and output parameters, as well as the columns that make up the result set for the specified procedures */
PHP_FUNCTION(ads_procedurecolumns)
   {
   zval *pv_conn;
   ads_result   *result = NULL;
   ads_connection *conn;
   char *cat = NULL, *schema = NULL, *proc = NULL, *col = NULL;
   int cat_len = 0, schema_len = 0, proc_len = 0, col_len = 0;
   RETCODE rc;

   if ( ZEND_NUM_ARGS() != 1 && ZEND_NUM_ARGS() != 5 )
      {
      WRONG_PARAM_COUNT;
      }

   if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r|s!sss", &pv_conn, &cat, &cat_len, &schema, &schema_len,
                              &proc, &proc_len, &col, &col_len) == FAILURE)
   {
      return;
   }

   if ((conn = (ads_connection *)zend_fetch_resource2(Z_RES_P(pv_conn), "Advantage-Link", le_conn, le_pconn)) == NULL) {
       RETURN_FALSE;
   }
   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);
   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);

   result = (ads_result *)emalloc(sizeof(ads_result));
   if ( result == NULL )
      {
      php_error(E_WARNING, "Out of memory");
      RETURN_FALSE;
      }

   rc = SQLAllocStmt(conn->hdbc, &(result->stmt));
   if ( rc == SQL_INVALID_HANDLE )
      {
      efree(result);
      php_error(E_WARNING, "SQLAllocStmt error 'Invalid Handle' in ads_procedurecolumns");
      RETURN_FALSE;
      }

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLAllocStmt");
      efree(result);
      RETURN_FALSE;
      }

   rc = SQLProcedureColumns(result->stmt,
                            (unsigned char*)cat, SAFE_SQL_NTS(cat),
                            (unsigned char*)schema, SAFE_SQL_NTS(schema),
                            (unsigned char*)proc, SAFE_SQL_NTS(proc),
                            (unsigned char*)col, SAFE_SQL_NTS(col) );

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLProcedureColumns");
      efree(result);
      RETURN_FALSE;
      }

   result->numparams = 0;
   SQLNumResultCols(result->stmt, &(result->numcols));

   if ( result->numcols > 0 )
      {
      if ( !ads_bindcols(result TSRMLS_CC) )
         {
         efree(result);
         RETURN_FALSE;
         }
      }
   else
      {
      result->values = NULL;
      }
   result->conn_ptr = conn;
   result->fetched = 0;
   RETURN_RES(zend_register_resource(result, le_result));
   //ZEND_REGISTER_RESOURCE(return_value, result, le_result);
   }
/* }}} */

/* {{{ proto int ads_procedures(int connection_id [, string qualifier, string owner, string name])
   Returns a result identifier containg the list of procedure names in a datasource */
PHP_FUNCTION(ads_procedures)
   {
   zval *pv_conn;
   ads_result   *result = NULL;
   ads_connection *conn;
   char *cat = NULL, *schema = NULL, *proc = NULL;
   int cat_len = 0, schema_len = 0, proc_len = 0;
   RETCODE rc;

   if ( ZEND_NUM_ARGS() != 1 && ZEND_NUM_ARGS() != 4 )
      {
      WRONG_PARAM_COUNT;
      }

   if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r|s!ss", &pv_conn, &cat, &cat_len, &schema, &schema_len,
                              &proc, &proc_len) == FAILURE)
   {
     return;
   }
   if ((conn = (ads_connection *)zend_fetch_resource2(Z_RES_P(pv_conn), "Advantage-Link", le_conn, le_pconn)) == NULL) {
       RETURN_FALSE;
   }
   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);
   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);

   result = (ads_result *)emalloc(sizeof(ads_result));
   if ( result == NULL )
      {
      php_error(E_WARNING, "Out of memory");
      RETURN_FALSE;
      }

   rc = SQLAllocStmt(conn->hdbc, &(result->stmt));
   if ( rc == SQL_INVALID_HANDLE )
      {
      efree(result);
      php_error(E_WARNING, "SQLAllocStmt error 'Invalid Handle' in ads_procedures");
      RETURN_FALSE;
      }

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLAllocStmt");
      efree(result);
      RETURN_FALSE;
      }

   rc = SQLProcedures(result->stmt,
                      (unsigned char*)cat, SAFE_SQL_NTS(cat),
                      (unsigned char*)schema, SAFE_SQL_NTS(schema),
                      (unsigned char*)proc, SAFE_SQL_NTS(proc) );

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLProcedures");
      efree(result);
      RETURN_FALSE;
      }

   result->numparams = 0;
   SQLNumResultCols(result->stmt, &(result->numcols));

   if ( result->numcols > 0 )
      {
      if ( !ads_bindcols(result TSRMLS_CC) )
         {
         efree(result);
         RETURN_FALSE;
         }
      }
   else
      {
      result->values = NULL;
      }
   result->conn_ptr = conn;
   result->fetched = 0;
   RETURN_RES(zend_register_resource(result, le_result));
   //ZEND_REGISTER_RESOURCE(return_value, result, le_result);
   }
/* }}} */

/* {{{ proto int ads_specialcolumns(int connection_id, int type, string qualifier, string owner, string table, int scope, int nullable)
   Returns a result identifier containing either the optimal set of columns that uniquely identifies a row in the table or columns that are automatically updated when any value in the row is updated by a transaction */
PHP_FUNCTION(ads_specialcolumns)
   {
   zval *pv_conn;
   long vtype, vscope, vnullable;
   ads_result   *result = NULL;
   ads_connection *conn;
   char *cat = NULL, *schema = NULL, *name = NULL;
   int cat_len = 0, schema_len = 0, name_len = 0;
   SQLUSMALLINT type;
   SQLUSMALLINT scope, nullable;
   RETCODE rc;

   if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rls!ssll", &pv_conn, &vtype, &cat, &cat_len, &schema,
                               &schema_len, &name, &name_len, &vscope, &vnullable) == FAILURE)
      {
      return;
      }

   type = (SQLUSMALLINT) vtype;
   scope = (SQLUSMALLINT) vscope;
   nullable = (SQLUSMALLINT) vnullable;

   if ((conn = (ads_connection *)zend_fetch_resource2(Z_RES_P(pv_conn), "Advantage-Link", le_conn, le_pconn)) == NULL) {
       RETURN_FALSE;
   }
   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);
   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);

   result = (ads_result *)emalloc(sizeof(ads_result));
   if ( result == NULL )
      {
      php_error(E_WARNING, "Out of memory");
      RETURN_FALSE;
      }

   rc = SQLAllocStmt(conn->hdbc, &(result->stmt));
   if ( rc == SQL_INVALID_HANDLE )
      {
      efree(result);
      php_error(E_WARNING, "SQLAllocStmt error 'Invalid Handle' in ads_specialcolumns");
      RETURN_FALSE;
      }

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLAllocStmt");
      efree(result);
      RETURN_FALSE;
      }

   rc = SQLSpecialColumns(result->stmt,
                          type,
                          (unsigned char*)cat, SAFE_SQL_NTS(cat),
                          (unsigned char*)schema, SAFE_SQL_NTS(schema),
                          (unsigned char*)name, SAFE_SQL_NTS(name),
                          scope,
                          nullable);

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLSpecialColumns");
      efree(result);
      RETURN_FALSE;
      }

   result->numparams = 0;
   SQLNumResultCols(result->stmt, &(result->numcols));

   if ( result->numcols > 0 )
      {
      if ( !ads_bindcols(result TSRMLS_CC) )
         {
         efree(result);
         RETURN_FALSE;
         }
      }
   else
      {
      result->values = NULL;
      }
   result->conn_ptr = conn;
   result->fetched = 0;
   RETURN_RES(zend_register_resource(result, le_result));
   //ZEND_REGISTER_RESOURCE(return_value, result, le_result);
   }
/* }}} */

/* {{{ proto int ads_statistics(int connection_id, string qualifier, string owner, string name, int unique, int accuracy)
   Returns a result identifier that contains statistics about a single table and the indexes associated with the table */
PHP_FUNCTION(ads_statistics)
   {
   zval *pv_conn;
   long vunique, vreserved;
   ads_result   *result = NULL;
   ads_connection *conn;
   char *cat = NULL, *schema = NULL, *name = NULL;
   int cat_len = 0, schema_len = 0, name_len = 0;
   SQLUSMALLINT unique, reserved;
   RETCODE rc;

   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs!ssll", &pv_conn, &cat, &cat_len, &schema, &schema_len,
                               &name, &name_len, &vunique, &vreserved) == FAILURE )
      {
      return;
      }

   unique = (SQLUSMALLINT) vunique;
   reserved = (SQLUSMALLINT) vreserved;

   if ((conn = (ads_connection *)zend_fetch_resource2(Z_RES_P(pv_conn), "Advantage-Link", le_conn, le_pconn)) == NULL) {
       RETURN_FALSE;
   }
   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);
   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);

   result = (ads_result *)emalloc(sizeof(ads_result));
   if ( result == NULL )
      {
      php_error(E_WARNING, "Out of memory");
      RETURN_FALSE;
      }

   rc = SQLAllocStmt(conn->hdbc, &(result->stmt));
   if ( rc == SQL_INVALID_HANDLE )
      {
      efree(result);
      php_error(E_WARNING, "SQLAllocStmt error 'Invalid Handle' in ads_statistics");
      RETURN_FALSE;
      }

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLAllocStmt");
      efree(result);
      RETURN_FALSE;
      }

   rc = SQLStatistics(result->stmt,
                      (unsigned char*)cat, SAFE_SQL_NTS(cat),
                      (unsigned char*)schema, SAFE_SQL_NTS(schema),
                      (unsigned char*)name, SAFE_SQL_NTS(name),
                      unique,
                      reserved);

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLStatistics");
      efree(result);
      RETURN_FALSE;
      }

   result->numparams = 0;
   SQLNumResultCols(result->stmt, &(result->numcols));

   if ( result->numcols > 0 )
      {
      if ( !ads_bindcols(result TSRMLS_CC) )
         {
         efree(result);
         RETURN_FALSE;
         }
      }
   else
      {
      result->values = NULL;
      }
   result->conn_ptr = conn;
   result->fetched = 0;
   RETURN_RES(zend_register_resource(result, le_result));
   //ZEND_REGISTER_RESOURCE(return_value, result, le_result);
   }
/* }}} */


/* {{{ proto int ads_tableprivileges(int connection_id, string qualifier, string owner, string name)
   Returns a result identifier containing a list of tables and the privileges associated with each table */
PHP_FUNCTION(ads_tableprivileges)
   {
   zval *pv_conn;
   ads_result   *result = NULL;
   ads_connection *conn;
   char *cat = NULL, *schema = NULL, *table = NULL;
   int cat_len = 0, schema_len = 0, table_len = 0;
   RETCODE rc;

   if ( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs!ss", &pv_conn, &cat, &cat_len, &schema, &schema_len,
                               &table, &table_len) == FAILURE )
      {
      return;
      }
   if ((conn = (ads_connection *)zend_fetch_resource2(Z_RES_P(pv_conn), "Advantage-Link", le_conn, le_pconn)) == NULL) {
       RETURN_FALSE;
   }
   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);
   //ZEND_FETCH_RESOURCE2(conn, ads_connection *, &pv_conn, -1, "Advantage-Link", le_conn, le_pconn);

   result = (ads_result *)emalloc(sizeof(ads_result));
   if ( result == NULL )
      {
      php_error(E_WARNING, "Out of memory");
      RETURN_FALSE;
      }

   rc = SQLAllocStmt(conn->hdbc, &(result->stmt));
   if ( rc == SQL_INVALID_HANDLE )
      {
      efree(result);
      php_error(E_WARNING, "SQLAllocStmt error 'Invalid Handle' in ads_tableprivileges");
      RETURN_FALSE;
      }

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLAllocStmt");
      efree(result);
      RETURN_FALSE;
      }

   rc = SQLTablePrivileges(result->stmt,
                           (unsigned char*)cat, SAFE_SQL_NTS(cat),
                           (unsigned char*)schema, SAFE_SQL_NTS(schema),
                           (unsigned char*)table, SAFE_SQL_NTS(table));

   if ( rc == SQL_ERROR )
      {
      ads_sql_error(conn, SQL_NULL_HSTMT, "SQLTablePrivileges");
      efree(result);
      RETURN_FALSE;
      }

   result->numparams = 0;
   SQLNumResultCols(result->stmt, &(result->numcols));

   if ( result->numcols > 0 )
      {
      if ( !ads_bindcols(result TSRMLS_CC) )
         {
         efree(result);
         RETURN_FALSE;
         }
      }
   else
      {
      result->values = NULL;
      }
   result->conn_ptr = conn;
   result->fetched = 0;
   RETURN_RES(zend_register_resource(result, le_result));
   //ZEND_REGISTER_RESOURCE(return_value, result, le_result);
   }
/* }}} */


/* {{{ proto string ads_guid_to_string(string data)
   Converts the guid representation to string */
/*PHP_FUNCTION(ads_guid_to_string)
{
	zval **data;
	char *result;
	size_t newlen;

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &data) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string_ex(data);

	result = ads_guid_as_string(Z_STRVAL_PP(data), Z_STRLEN_PP(data), &newlen);
	
	if (!result) {
		RETURN_FALSE;
	}

	RETURN_STRINGL(result, newlen);
}*/

/* }}} */



/*
 * Local variables:
 * tab-width: 3
 * c-basic-offset: 3
 * End:
 * vim600: sw=3 ts=3 tw=78 fdm=marker
 * vim<600: sw=3 ts=3 tw=78
 */
