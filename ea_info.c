/*
   +----------------------------------------------------------------------+
   | eAccelerator project                                                 |
   +----------------------------------------------------------------------+
   | Copyright (c) 2004 - 2010 eAccelerator                               |
   | http://eaccelerator.net                                  			  |
   +----------------------------------------------------------------------+
   | This program is free software; you can redistribute it and/or        |
   | modify it under the terms of the GNU General Public License          |
   | as published by the Free Software Foundation; either version 2       |
   | of the License, or (at your option) any later version.               |
   |                                                                      |
   | This program is distributed in the hope that it will be useful,      |
   | but WITHOUT ANY WARRANTY; without even the implied warranty of       |
   | MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        |
   | GNU General Public License for more details.                         |
   |                                                                      |
   | You should have received a copy of the GNU General Public License    |
   | along with this program; if not, write to the Free Software          |
   | Foundation, Inc., 59 Temple Place - Suite 330, Boston,               |
   | MA  02111-1307, USA.                                                 |
   |                                                                      |
   | A copy is available at http://www.gnu.org/copyleft/gpl.txt           |
   +----------------------------------------------------------------------+
   $Id$
*/

#include "eaccelerator.h"
#include "eaccelerator_version.h"
#include "ea_info.h"
#include "ea_cache.h"
#include "mm.h"
#include "zend.h"
#include "fopen_wrappers.h"
#include "debug.h"
#include <fcntl.h>

#ifndef O_BINARY
#  define O_BINARY 0
#endif

#define NOT_ADMIN_WARNING "This script isn't in the allowed_admin_path setting!"

extern eaccelerator_mm *ea_mm_user_instance;

PHP_FUNCTION(eaccelerator_put) /* {{{ */
{
	char *key;
	int key_len;
	zval *val;
	time_t ttl = 0;

	if (zend_parse_parameters (ZEND_NUM_ARGS() TSRMLS_CC, "sz|l", &key, &key_len, &val, &ttl) == FAILURE) {
		return;
	}

	if (eaccelerator_put (key, key_len, val, ttl TSRMLS_CC)) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

PHP_FUNCTION(eaccelerator_add) /* {{{ */
{
	char *key;
	int key_len;
	zval *val;
	time_t ttl = 0;

	if (zend_parse_parameters (ZEND_NUM_ARGS() TSRMLS_CC, "sz|l", &key, &key_len, &val, &ttl) == FAILURE) {
		return;
	}

	if (eaccelerator_add (key, key_len, val, ttl TSRMLS_CC)) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

PHP_FUNCTION(eaccelerator_cas) /* {{{ */
{
	char *key;
	int key_len;
	zval *val;
	time_t ttl = 0;
	long cas = 0;

	if (zend_parse_parameters (ZEND_NUM_ARGS() TSRMLS_CC, "lsz|l", &cas, &key, &key_len, &val, &ttl) == FAILURE) {
		return;
	}

	if (eaccelerator_cas (cas, key, key_len, val, ttl TSRMLS_CC)) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

PHP_FUNCTION(eaccelerator_get) /* {{{ */
{
	char *key;
	int key_len;
	zval *cas = NULL;
	unsigned long ui_cas = 0;

	if (zend_parse_parameters (ZEND_NUM_ARGS() TSRMLS_CC, "s|z", &key, &key_len, &cas) == FAILURE) {
		return;
	}

	if (eaccelerator_get (key, key_len, return_value, &ui_cas TSRMLS_CC)) {
		if (cas) {
			zval_dtor(cas);
			ZVAL_LONG(cas, ui_cas);
		}
		return;
	}
	RETURN_NULL();
}
/* }}} */

PHP_FUNCTION(eaccelerator_rm) /* {{{ */
{
	char *key;
	int key_len;

	if (zend_parse_parameters (ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len) == FAILURE) {
		return;
	}

	if (eaccelerator_rm (key, key_len TSRMLS_CC)) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

PHP_FUNCTION(eaccelerator_gc) /* {{{ */
{
	eaccelerator_gc(ea_mm_user_instance TSRMLS_C);
	RETURN_TRUE;
}
/* }}} */

/* {{{ PHP_FUNCTION(eaccelerator_list_keys): returns list of keys in shared memory that matches actual hostname or namespace */
PHP_FUNCTION(eaccelerator_list_keys)
{
	if (ea_mm_user_instance != NULL && eaccelerator_list_keys(return_value TSRMLS_CC)) {
		return;
	}
	RETURN_NULL ();
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

