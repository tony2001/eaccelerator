/*
   +----------------------------------------------------------------------+
   | eAccelerator project                                                 |
   +----------------------------------------------------------------------+
   | Copyright (c) 2004 - 2006 eAccelerator                               |
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
   | A copy is availble at http://www.gnu.org/copyleft/gpl.txt            |
   +----------------------------------------------------------------------+
   $Id: cache.h 178 2006-03-06 09:08:40Z bart $
*/

#include "eaccelerator.h"

#ifndef EA_CACHE_H
#define EA_CACHE_H

#include "zend.h"
#include "zend_API.h"
#include "zend_extensions.h"

int eaccelerator_put (const char *key, int key_len, zval * val, time_t ttl TSRMLS_DC);
int eaccelerator_add (const char *key, int key_len, zval * val, time_t ttl TSRMLS_DC);
int eaccelerator_cas (unsigned long cas, const char *key, int key_len, zval * val, time_t ttl TSRMLS_DC);
int eaccelerator_get (const char *key, int key_len, zval * return_value, unsigned long *cas TSRMLS_DC);
int eaccelerator_rm (const char *key, int key_len TSRMLS_DC);
size_t eaccelerator_gc (eaccelerator_mm *instance TSRMLS_DC);

int eaccelerator_list_keys(zval *return_value TSRMLS_DC);

#endif /* EA_CACHE_H */