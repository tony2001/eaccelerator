/*
   +----------------------------------------------------------------------+
   | eAccelerator project                                                 |
   +----------------------------------------------------------------------+
   | Copyright (c) 2004 - 2007 eAccelerator                               |
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
   $Id$
*/

#ifndef INCLUDED_CONTENT_H
#define INCLUDED_CONTENT_H

#include "php_ini.h"

#ifdef HAVE_EACCELERATOR
#ifdef WITH_EACCELERATOR_CONTENT_CACHING
void eaccelerator_content_cache_startup();
void eaccelerator_content_cache_shutdown();

PHP_FUNCTION(_eaccelerator_output_handler);
PHP_FUNCTION(eaccelerator_cache_page);
PHP_FUNCTION(eaccelerator_rm_page);
PHP_FUNCTION(eaccelerator_cache_output);
PHP_FUNCTION(eaccelerator_cache_result);
PHP_INI_MH(eaccelerator_OnUpdateContentCachePlace);
#endif
#endif
#endif /* INCLUDED_CONTENT_H */
