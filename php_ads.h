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
   | Authors: Stig S�ther Bakken <ssb@guardian.no>                        |
   |          Andreas Karajannis <Andreas.Karajannis@gmd.de>              |
   |          Kevin N. Shallow <kshallow@tampabay.rr.com> Velocis Support |
   |          SAP AG or an SAP affiliate company (Advantage@sap.com)      |
   +----------------------------------------------------------------------+
*/

/* $Id: php_ads.h,v 1.7 2008-01-31 18:33:30 LanceSc Exp $ */

#ifndef PHP_ADS_H
#define PHP_ADS_H


#define ODBCVER 0x0300

#ifdef ZTS
   #include "TSRM.h"
#endif


#include "sql.h"
#include "sqlext.h"
#define HAVE_SQL_EXTENDED_FETCH 1

extern zend_module_entry advantage_module_entry;
#define advantage_module_ptr &advantage_module_entry


/* user functions */
PHP_MINIT_FUNCTION(advantage);
PHP_MSHUTDOWN_FUNCTION(advantage);
PHP_RINIT_FUNCTION(advantage);
PHP_RSHUTDOWN_FUNCTION(advantage);
PHP_MINFO_FUNCTION(advantage);

PHP_FUNCTION(ads_error);
PHP_FUNCTION(ads_errormsg);
PHP_FUNCTION(ads_setoption);
PHP_FUNCTION(ads_autocommit);
PHP_FUNCTION(ads_close);
PHP_FUNCTION(ads_close_all);
PHP_FUNCTION(ads_commit);
PHP_FUNCTION(ads_connect);
PHP_FUNCTION(ads_pconnect);
PHP_FUNCTION(ads_cursor);
PHP_FUNCTION(ads_exec);
PHP_FUNCTION(ads_do);
PHP_FUNCTION(ads_execute);
PHP_FUNCTION(ads_fetch_array);
PHP_FUNCTION(ads_fetch_object);
PHP_FUNCTION(ads_fetch_into);
PHP_FUNCTION(ads_fetch_row);
PHP_FUNCTION(ads_field_len);
PHP_FUNCTION(ads_field_scale);
PHP_FUNCTION(ads_field_name);
PHP_FUNCTION(ads_field_type);
PHP_FUNCTION(ads_field_num);
PHP_FUNCTION(ads_free_result);
PHP_FUNCTION(ads_next_result);
PHP_FUNCTION(ads_num_fields);
PHP_FUNCTION(ads_num_rows);
PHP_FUNCTION(ads_prepare);
PHP_FUNCTION(ads_result);
PHP_FUNCTION(ads_result_all);
PHP_FUNCTION(ads_rollback);
PHP_FUNCTION(ads_binmode);
PHP_FUNCTION(ads_longreadlen);
PHP_FUNCTION(ads_tables);
PHP_FUNCTION(ads_columns);
PHP_FUNCTION(ads_columnprivileges);
PHP_FUNCTION(ads_tableprivileges);
PHP_FUNCTION(ads_foreignkeys);
PHP_FUNCTION(ads_procedures);
PHP_FUNCTION(ads_procedurecolumns);
PHP_FUNCTION(ads_gettypeinfo);
PHP_FUNCTION(ads_primarykeys);
PHP_FUNCTION(ads_specialcolumns);
PHP_FUNCTION(ads_statistics);
PHP_FUNCTION(ads_guid_to_string);

#define ADS_SQL_ENV_T HENV
#define ADS_SQL_CONN_T HDBC
#define ADS_SQL_STMT_T HSTMT


typedef struct ads_connection
   {
   ADS_SQL_ENV_T henv;
   ADS_SQL_CONN_T hdbc;
   char laststate[6];
   char lasterrormsg[SQL_MAX_MESSAGE_LENGTH];
   zend_resource *res;
   int id;
   int persistent;
   } ads_connection;

typedef struct ads_result_value
   {
   char name[32];
   char *value;
   long int vallen;
   SDWORD coltype;
   } ads_result_value;

typedef struct ads_result
   {
   ADS_SQL_STMT_T stmt;
   int id;
   ads_result_value *values;
   SWORD numcols;
   SWORD numparams;
#if HAVE_SQL_EXTENDED_FETCH
   int fetch_abs;
#endif
   long longreadlen;
   int binmode;
   int fetched;
   ads_connection *conn_ptr;
   } ads_result;

ZEND_BEGIN_MODULE_GLOBALS(advantage)
   char *defDB;
   char *defUser;
   char *defPW;
   long allow_persistent;
   long check_persistent;
   long max_persistent;
   long max_links;
   long num_persistent;
   long num_links;
   int  defConn;
   long defaultlrl;
   long defaultbinmode;
   char laststate[6];
   char lasterrormsg[SQL_MAX_MESSAGE_LENGTH];
   HashTable *resource_list;
   HashTable *resource_plist;
ZEND_END_MODULE_GLOBALS(advantage)

int ads_add_result(HashTable *list, ads_result *result);
ads_result *ads_get_result(HashTable *list, int count);
void ads_del_result(HashTable *list, int count);
int ads_add_conn(HashTable *list, HDBC conn);
ads_connection *ads_get_conn(HashTable *list, int count);
void ads_del_conn(HashTable *list, int ind);
int ads_bindcols(ads_result *result TSRMLS_DC);

#define ADS_SQL_ERROR_PARAMS ads_connection *conn_resource, ADS_SQL_STMT_T stmt, char *func

void ads_sql_error(ADS_SQL_ERROR_PARAMS);

#define IS_SQL_LONG(x) (x == SQL_LONGVARBINARY || x == SQL_LONGVARCHAR || x == SQL_WLONGVARCHAR )
#define IS_SQL_BINARY(x) (x == SQL_BINARY || x == SQL_VARBINARY || x == SQL_LONGVARBINARY || x == SQL_GUID )
#define IS_SQL_UNICODE(x) ( x == SQL_WCHAR || x == SQL_WVARCHAR || x == SQL_WLONGVARCHAR )

#ifdef ZTS
   #define ADSG(v) TSRMG(advantage_globals_id, zend_advantage_globals *, v)
#else
   #define ADSG(v) (advantage_globals.v)
#endif

#define phpext_advantage_ptr advantage_module_ptr

/*
 * private define for ADS PHP component.  PHP for Win32 uses Microsoft
 * headers, so this define must be in this file.
 */
#define SQL_C_UTF8  (-1000)

#endif /* PHP_ADS_H */

/*
 * Local variables:
 * tab-width: 3
 * c-basic-offset: 3
 * End:
 */
