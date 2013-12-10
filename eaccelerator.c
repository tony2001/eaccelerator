/*
   +----------------------------------------------------------------------+
   | eAccelerator project                                                 |
   +----------------------------------------------------------------------+
   | Copyright (c) 2004 - 2010 eAccelerator                               |
   | http://eaccelerator.net                                              |
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

#ifdef HAVE_EACCELERATOR

#include "opcodes.h"

#include "zend.h"
#include "zend_API.h"
#include "zend_extensions.h"

#include "debug.h"
#include "ea_store.h"
#include "ea_restore.h"
#include "ea_info.h"
#include "ea_atomic.h"
#include "ea_cache.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifdef ZEND_WIN32
#  include "win32/time.h"
#  include <time.h>
#  include <sys/utime.h>
#else
#  include <sys/file.h>
#  include <sys/time.h>
#  include <utime.h>
#endif
#include <fcntl.h>

#ifndef O_BINARY
#  define O_BINARY 0
#endif

#include "php.h"
#include "php_ini.h"
#include "main/fopen_wrappers.h"
#include "ext/standard/info.h"
#include "ext/standard/php_incomplete_class.h"
#include "ext/standard/md5.h"
#include "ext/date/php_date.h"

#include "SAPI.h"

#define MAX_DUP_STR_LEN 256

/* Globals (different for each process/thread) */
ZEND_DECLARE_MODULE_GLOBALS(eaccelerator)

/* Globals (common for each process/thread) */
static long ea_user_shm_size = 0;

eaccelerator_mm* ea_mm_user_instance = NULL;
static void (*ea_saved_on_timeout)(int seconds TSRMLS_DC);
ZEND_DLEXPORT void eaccelerator_on_timeout(int seconds TSRMLS_DC);

/* Initialise the shared memory */
static int init_mm(TSRMLS_D) /* {{{ */
{
	pid_t  owner = getpid();
	MM     *mm_user;
	size_t total;
	char   mm_user_path[MAXPATHLEN];

	snprintf(mm_user_path, MAXPATHLEN, "user_data_%s.%s%d", EACCELERATOR_MM_FILE, sapi_module.name, owner);
	ea_mm_user_instance = (eaccelerator_mm*)mm_attach(ea_user_shm_size*1024*1024, mm_user_path);
	if (ea_mm_user_instance != NULL) {
		return SUCCESS;
	}

	mm_user = mm_create(ea_user_shm_size*1024*1024, mm_user_path);
	if (!mm_user) {
		return FAILURE;
	}

	total = mm_available(mm_user);
	ea_mm_user_instance = mm_malloc_lock(mm_user, sizeof(*ea_mm_user_instance));
	if (!ea_mm_user_instance) {
		return FAILURE;
	}
	mm_set_attach(mm_user, ea_mm_user_instance);
	memset(ea_mm_user_instance, 0, sizeof(*ea_mm_user_instance));
	ea_mm_user_instance->owner = owner;
	ea_mm_user_instance->mm    = mm_user;
	ea_mm_user_instance->total = total;
	ea_mm_user_instance->user_hash_cnt = 0;
	ea_mm_user_instance->rem_cnt  = 0;
	ea_mm_user_instance->enabled = 1;
	ea_mm_user_instance->cas = 0;
	EACCELERATOR_PROTECT(ea_mm_user_instance);
	return SUCCESS;
}
/* }}} */

/* Clean up the shared memory */
static void shutdown_mm(TSRMLS_D) /* {{{ */
{
	if (ea_mm_user_instance) {
		if (getpgrp() == getpid()) {
			MM *mm = ea_mm_user_instance->mm;
			if (mm) {
				mm_destroy(mm);
			}
			ea_mm_user_instance = NULL;
		}
	}
}
/* }}} */

ZEND_DLEXPORT void eaccelerator_on_timeout(int seconds TSRMLS_DC) /* {{{ */
{
	ea_debug_error("eaccelerator_on_timeout(%d sec)", seconds);
	ea_saved_on_timeout(seconds TSRMLS_CC);
}
/* }}} */

/* Allocate a new cache chunk */
void* eaccelerator_malloc2(eaccelerator_mm *mm_instance, size_t size TSRMLS_DC)  /* {{{ */
{
	void *p = NULL;

	if (eaccelerator_gc(mm_instance TSRMLS_CC) > 0) {
		p = eaccelerator_malloc(mm_instance, size);
		if (p != NULL) {
			return p;
		}
	}

	return p;
}
/* }}} */

/* Format Bytes */
void format_size(char* s, unsigned int size, int legend) /* {{{ */
{
	unsigned int i = 0;
	unsigned int n = 0;
	char ch;
	do {
		if ((n != 0) && (n % 3 == 0)) {
			s[i++] = ',';
		}
		s[i++] = (char)((int)'0' + (size % 10));
		n++;
		size = size / 10;
	} while (size != 0);
	s[i] = '\0';
	n = 0; i--;
	while (n < i) {
		ch = s[n];
		s[n] = s[i];
		s[i] = ch;
		n++, i--;
	}
	if (legend) {
		strcat(s, " Bytes");
	}
}
/* }}} */

static PHP_MINFO_FUNCTION(eaccelerator)  /* {{{ */
{
	char s[32];

	php_info_print_table_start();
	php_info_print_table_header(2, "eAccelerator support", "enabled");
	php_info_print_table_row(2, "Version", EACCELERATOR_VERSION);
	php_info_print_table_row(2, "Shared memory type", EAC_SHM_TYPE);
	php_info_print_table_row(2, "Semaphores type", EAC_SEM_TYPE);
	if (ea_mm_user_instance != NULL) {
		size_t available;

		EACCELERATOR_UNPROTECT(ea_mm_user_instance);
		available = mm_available(ea_mm_user_instance->mm);
		EACCELERATOR_LOCK_RD(ea_mm_user_instance);
		EACCELERATOR_PROTECT(ea_mm_user_instance);
		php_info_print_table_header(1, "User data segment");
		format_size(s, ea_mm_user_instance->total, 1);
		php_info_print_table_row(2, "Memory Size", s);
		format_size(s, available, 1);
		php_info_print_table_row(2, "Memory Available", s);
		format_size(s, ea_mm_user_instance->total - available, 1);
		php_info_print_table_row(2, "Memory Allocated", s);
		snprintf(s, 32, "%u", ea_mm_user_instance->user_hash_cnt);
		php_info_print_table_row(2, "Cached User Variables", s);
		EACCELERATOR_UNPROTECT(ea_mm_user_instance);
		EACCELERATOR_UNLOCK_RD(ea_mm_user_instance);
		EACCELERATOR_PROTECT(ea_mm_user_instance);
	}
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */

static PHP_INI_MH(eaccelerator_OnUpdateLong) /* {{{ */
{
	long *p = (long*)mh_arg1;
	*p = zend_atoi(new_value, new_value_length);
	return SUCCESS;
}
/* }}} */

PHP_INI_BEGIN()
STD_PHP_INI_ENTRY("eaccelerator.enable",         "1", PHP_INI_ALL, OnUpdateBool, enabled, zend_eaccelerator_globals, eaccelerator_globals)
STD_PHP_INI_ENTRY("eaccelerator.name_space",     "", PHP_INI_ALL, OnUpdateString, name_space, zend_eaccelerator_globals, eaccelerator_globals)
ZEND_INI_ENTRY1("eaccelerator.user_shm_size",    "32", PHP_INI_SYSTEM, eaccelerator_OnUpdateLong, &ea_user_shm_size)
PHP_INI_END()

/* signal handlers */
#ifdef WITH_EACCELERATOR_CRASH_DETECTION
static void eaccelerator_crash_handler(int dummy)  /* {{{ */
{
	struct tm *loctime;

	TSRMLS_FETCH();
	fflush(stdout);
	fflush(stderr);
#ifdef SIGSEGV
	if (EAG(original_sigsegv_handler) != eaccelerator_crash_handler) {
		signal(SIGSEGV, EAG(original_sigsegv_handler));
	} else {
		signal(SIGSEGV, SIG_DFL);
	}
#endif
#ifdef SIGFPE
	if (EAG(original_sigfpe_handler) != eaccelerator_crash_handler) {
		signal(SIGFPE, EAG(original_sigfpe_handler));
	} else {
		signal(SIGFPE, SIG_DFL);
	}
#endif
#ifdef SIGBUS
	if (EAG(original_sigbus_handler) != eaccelerator_crash_handler) {
		signal(SIGBUS, EAG(original_sigbus_handler));
	} else {
		signal(SIGBUS, SIG_DFL);
	}
#endif
#ifdef SIGILL
	if (EAG(original_sigill_handler) != eaccelerator_crash_handler) {
		signal(SIGILL, EAG(original_sigill_handler));
	} else {
		signal(SIGILL, SIG_DFL);
	}
#endif
#ifdef SIGABRT
	if (EAG(original_sigabrt_handler) != eaccelerator_crash_handler) {
		signal(SIGABRT, EAG(original_sigabrt_handler));
	} else {
		signal(SIGABRT, SIG_DFL);
	}
#endif

	loctime = localtime(&EAG(req_start));

	if (EG(active_op_array)) {
		fprintf(stderr, "[%s] [notice] EACCELERATOR(%d): PHP crashed on opline %ld of %s() at %s:%u\n\n",
				asctime(loctime),
				getpid(),
				(long)(active_opline-EG(active_op_array)->opcodes),
				get_active_function_name(TSRMLS_C),
				zend_get_executed_filename(TSRMLS_C),
				zend_get_executed_lineno(TSRMLS_C));
	} else {
		fprintf(stderr, "[%s] [notice] EACCELERATOR(%d): PHP crashed\n\n", asctime(loctime), getpid());
	}
#if !defined(WIN32) && !defined(NETWARE)
	kill(getpid(), dummy);
#else
	raise(dummy);
#endif
}
/* }}} */
#endif

static void eaccelerator_init_globals(zend_eaccelerator_globals *eag) /* {{{ */
{
	eag->enabled = 1;
	eag->in_request = 0;
}
/* }}} */

static PHP_MINIT_FUNCTION(eaccelerator)  /* {{{ */
{
	ZEND_INIT_MODULE_GLOBALS(eaccelerator, eaccelerator_init_globals, NULL);

	REGISTER_INI_ENTRIES();
	REGISTER_STRING_CONSTANT("EACCELERATOR_VERSION", EACCELERATOR_VERSION, CONST_CS | CONST_PERSISTENT);

	if (type == MODULE_PERSISTENT) {
		if (init_mm(TSRMLS_C) == FAILURE) {
			zend_error(E_CORE_WARNING,"[%s] Can not create shared memory area", EACCELERATOR_EXTENSION_NAME);
			return FAILURE;
		}

		ea_saved_on_timeout = zend_on_timeout;
		zend_on_timeout = eaccelerator_on_timeout;
	}

	return SUCCESS;
}
/* }}} */

static PHP_MSHUTDOWN_FUNCTION(eaccelerator)  /* {{{ */
{
	if (ea_mm_user_instance) {
		shutdown_mm(TSRMLS_C);
	}
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}
/* }}} */

static PHP_RINIT_FUNCTION(eaccelerator) /* {{{ */
{
	EAG(in_request) = 1;

#ifdef WITH_EACCELERATOR_CRASH_DETECTION
#ifdef SIGSEGV
	EAG(original_sigsegv_handler) = signal(SIGSEGV, eaccelerator_crash_handler);
#endif
#ifdef SIGFPE
	EAG(original_sigfpe_handler) = signal(SIGFPE, eaccelerator_crash_handler);
#endif
#ifdef SIGBUS
	EAG(original_sigbus_handler) = signal(SIGBUS, eaccelerator_crash_handler);
#endif
#ifdef SIGILL
	EAG(original_sigill_handler) = signal(SIGILL, eaccelerator_crash_handler);
#endif
#ifdef SIGABRT
	EAG(original_sigabrt_handler) = signal(SIGABRT, eaccelerator_crash_handler);
#endif
#endif

	return SUCCESS;
}
/* }}} */

static PHP_RSHUTDOWN_FUNCTION(eaccelerator) /* {{{ */
{
#ifdef WITH_EACCELERATOR_CRASH_DETECTION
#ifdef SIGSEGV
	if (EAG(original_sigsegv_handler) != eaccelerator_crash_handler) {
		signal(SIGSEGV, EAG(original_sigsegv_handler));
	} else {
		signal(SIGSEGV, SIG_DFL);
	}
#endif
#ifdef SIGFPE
	if (EAG(original_sigfpe_handler) != eaccelerator_crash_handler) {
		signal(SIGFPE, EAG(original_sigfpe_handler));
	} else {
		signal(SIGFPE, SIG_DFL);
	}
#endif
#ifdef SIGBUS
	if (EAG(original_sigbus_handler) != eaccelerator_crash_handler) {
		signal(SIGBUS, EAG(original_sigbus_handler));
	} else {
		signal(SIGBUS, SIG_DFL);
	}
#endif
#ifdef SIGILL
	if (EAG(original_sigill_handler) != eaccelerator_crash_handler) {
		signal(SIGILL, EAG(original_sigill_handler));
	} else {
		signal(SIGILL, SIG_DFL);
	}
#endif
#ifdef SIGABRT
	if (EAG(original_sigabrt_handler) != eaccelerator_crash_handler) {
		signal(SIGABRT, EAG(original_sigabrt_handler));
	} else {
		signal(SIGABRT, SIG_DFL);
	}
#endif
#endif

	return SUCCESS;
}
/* }}} */

ZEND_BEGIN_ARG_INFO(eaccelerator_second_arg_force_ref, 0)
  ZEND_ARG_PASS_INFO(0)
  ZEND_ARG_PASS_INFO(1)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(arginfo_eaccelerator_get, 0, 0, 1)
  ZEND_ARG_INFO(0, key)
  ZEND_ARG_INFO(1, cas)
ZEND_END_ARG_INFO()

zend_function_entry eaccelerator_functions[] = {
  PHP_FE(eaccelerator_put, NULL)
  PHP_FE(eaccelerator_add, NULL)
  PHP_FE(eaccelerator_cas, NULL)
  PHP_FE(eaccelerator_get, arginfo_eaccelerator_get)
  PHP_FE(eaccelerator_rm, NULL)
  PHP_FE(eaccelerator_gc, NULL)
  PHP_FE(eaccelerator_list_keys, NULL)
  {NULL, NULL, NULL, 0U, 0U}
};

zend_module_entry eaccelerator_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
  STANDARD_MODULE_HEADER,
#endif
  EACCELERATOR_EXTENSION_NAME,
  eaccelerator_functions,
  PHP_MINIT(eaccelerator),
  PHP_MSHUTDOWN(eaccelerator),
  PHP_RINIT(eaccelerator),
  PHP_RSHUTDOWN(eaccelerator),
  PHP_MINFO(eaccelerator),
#if ZEND_MODULE_API_NO >= 20010901
  EACCELERATOR_VERSION,          /* extension version number (string) */
#endif
  NO_MODULE_GLOBALS,
#if 0 && defined(PHP_VERSION_ID) && PHP_VERSION_ID < 50307
  ZEND_MODULE_POST_ZEND_DEACTIVATE_N(eaccelerator),
#else
  NULL,
#endif
  STANDARD_MODULE_PROPERTIES_EX
};

#if defined(COMPILE_DL_EACCELERATOR)
ZEND_GET_MODULE(eaccelerator)
#endif

#endif  /* #ifdef HAVE_EACCELERATOR */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: noet sw=4 ts=4 fdm=marker
 */
