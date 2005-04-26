/*
   +----------------------------------------------------------------------+
   | eAccelerator project                                                 |
   +----------------------------------------------------------------------+
   | Copyright (c) 2004 - 2005 eAccelerator                               |
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
   | Author(s): Dmitry Stogov <dstogov@users.sourceforge.net>             |
   |            Everaldo Canuto <everaldo_canuto@yahoo.com.br>            |
   +----------------------------------------------------------------------+
   $Id$
*/

#ifndef INCLUDED_EACCELERATOR_H
#define INCLUDED_EACCELERATOR_H

#include "php.h"
#include "zend.h"
#include "zend_API.h"
#include "zend_extensions.h"
#include "mm.h"

/* Handle __attribute__ for nongcc compilers */
#if (__GNUC__ >= 3)  || ((__GNUC__ == 2) && (__GNUC_MINOR__ >= 95))
#  define HAS_ATTRIBUTE
#else
#  define __attribute__(x)
#endif

#ifndef ZEND_WIN32
#  if ZEND_MODULE_API_NO >= 20001222
#    include "config.h"
#  else
#    include "php_config.h"
#  endif
#endif

// TODO: add as configure switch
#ifndef ZEND_WIN32
/* UnDefine if your filesystem doesn't support inodes */
#  define EACCELERATOR_USE_INODE
#endif

// TODO: add as configure switch
/* Define some of the following macros if you like to debug eAccelerator */
//#define DEBUG
//#define TEST_PERFORMANCE
//#define PROFILE_OPCODES

#ifdef WITH_EACCELERATOR_CRASH_DETECTION
#  include <signal.h>
#endif

#if defined(DEBUG) || defined(TEST_PERFORMANCE)  || defined(PROFILE_OPCODES)
/* Here you can chage debuging log filename */
#define DEBUG_LOGFILE     "/var/log/httpd/eaccelerator_log"
#define DEBUG_LOGFILE_CGI "/tmp/eaccelerator_log"
#endif

#define EACCELERATOR_MM_FILE "/tmp/eaccelerator"

#ifdef HAVE_EACCELERATOR

#ifdef EACCELERATOR_WITHOUT_FILE_LOCKING
#  ifndef LOCK_SH
#    define LOCK_SH 1
#    define LOCK_EX 2
#    define LOCK_UN 8
#  endif
#  define EACCELERATOR_FLOCK(FILE,OP)
#else
#  ifndef ZEND_WIN32
#    ifdef HAVE_FLOCK
#      define EACCELERATOR_FLOCK(FILE,OP) flock((FILE),(OP))
#    else
#      ifndef LOCK_SH
#        define LOCK_SH 1
#        define LOCK_EX 2
#        define LOCK_UN 8
#      endif
#      define EACCELERATOR_FLOCK(FILE,OP)
#    endif
#  else
#    define LOCK_SH 0
#    define LOCK_EX 1
#    define LOCK_UN 2
#    define EACCELERATOR_FLOCK(FILE,OP) {OVERLAPPED offset = {0,0,0,0,NULL};\
                                   if ((OP) == LOCK_EX) {\
                                     LockFileEx((HANDLE)_get_osfhandle(FILE), \
                                       LOCKFILE_EXCLUSIVE_LOCK, 0,\
                                       1, 0, &offset);\
                                   } else if ((OP) == LOCK_SH) {\
                                     LockFileEx((HANDLE)_get_osfhandle(FILE), \
                                       0, 0,\
                                       1, 0, &offset);\
                                   } else if ((OP) == LOCK_UN) {\
                                     UnlockFileEx((HANDLE)_get_osfhandle(FILE), \
                                       0,\
                                       1, 0, &offset);\
                                   }}
#  endif
#endif

#ifdef ZTS
#  define ZTS_LOCK()    tsrm_mutex_lock(mm_mutex)
#  define ZTS_UNLOCK()  tsrm_mutex_unlock(mm_mutex)
#else
#  define ZTS_LOCK()
#  define ZTS_UNLOCK()
#endif

#if defined(EACCELERATOR_PROTECT_SHM)
#  define EACCELERATOR_PROTECT()    do {mm_protect(eaccelerator_mm_instance->mm, MM_PROT_READ);} while(0)
#  define EACCELERATOR_UNPROTECT()  do {mm_protect(eaccelerator_mm_instance->mm, MM_PROT_READ|MM_PROT_WRITE);} while(0)
#else
#  define EACCELERATOR_PROTECT()
#  define EACCELERATOR_UNPROTECT()
#endif

#define EACCELERATOR_LOCK_RW()    do {ZTS_LOCK(); mm_lock(eaccelerator_mm_instance->mm, MM_LOCK_RW);} while(0)
#define EACCELERATOR_LOCK_RD()    do {ZTS_LOCK(); mm_lock(eaccelerator_mm_instance->mm, MM_LOCK_RD);} while(0)
#define EACCELERATOR_UNLOCK()     do {mm_unlock(eaccelerator_mm_instance->mm); ZTS_UNLOCK();} while(0)
#define EACCELERATOR_UNLOCK_RW()  EACCELERATOR_UNLOCK()
#define EACCELERATOR_UNLOCK_RD()  EACCELERATOR_UNLOCK()

#define EACCELERATOR_BLOCK_INTERRUPTIONS()   HANDLE_BLOCK_INTERRUPTIONS()
#define EACCELERATOR_UNBLOCK_INTERRUPTIONS() HANDLE_UNBLOCK_INTERRUPTIONS()

#define MM_HASH_SIZE      256
#define MM_USER_HASH_SIZE 256
#define MM_HASH_MAX       (MM_HASH_SIZE-1)
#define MM_USER_HASH_MAX  (MM_USER_HASH_SIZE-1)

#define eaccelerator_malloc(size)        mm_malloc_lock(eaccelerator_mm_instance->mm, size)
#define eaccelerator_free(x)             mm_free_lock(eaccelerator_mm_instance->mm, x)
#define eaccelerator_malloc_nolock(size) mm_malloc_nolock(eaccelerator_mm_instance->mm, size)
#define eaccelerator_free_nolock(x)      mm_free_nolock(eaccelerator_mm_instance->mm, x)

#if (defined (__GNUC__) && __GNUC__ >= 2)
#define EACCELERATOR_PLATFORM_ALIGNMENT (__alignof__ (align_test))
#else
#define EACCELERATOR_PLATFORM_ALIGNMENT (sizeof(align_union))
#endif

#define EACCELERATOR_ALIGN(n) (n) = (void*)((((size_t)(n)-1) & ~(EACCELERATOR_PLATFORM_ALIGNMENT-1)) + EACCELERATOR_PLATFORM_ALIGNMENT)

#define MAX_DUP_STR_LEN 256

#define offsetof(str,fld) ((size_t)&(((str*)NULL)->fld))

#define EACCELERATOR_EXTENSION_NAME "eAccelerator"
#define EACCELERATOR_LOADER_EXTENSION_NAME "eLoader"

#define MMC_ENCODER_VERSION   0x00000003
#define MMC_ENCODER_END       0x00
#define MMC_ENCODER_NAMESPACE 0x01
#define MMC_ENCODER_CLASS     0x02
#define MMC_ENCODER_FUNCTION  0x03

#define EACCELERATOR_VERSION_GUID   "PHPE8EDA1B6-806A-4851-B1C8-A6B4712F44FB"
#define EACCELERATOR_LOGO_GUID      "PHPE6F78DE9-13E4-4dee-8518-5FA2DACEA803"
#define EACCELERATOR_VERSION_STRING ("eAccelerator " EACCELERATOR_VERSION " (PHP " PHP_VERSION ")")

#ifdef ZTS
#  define MMCG(v) TSRMG(eaccelerator_globals_id, zend_eaccelerator_globals*, v)
#else
#  define MMCG(v) (eaccelerator_globals.v)
#endif

/******************************************************************************/

typedef struct _eaccelerator_op_array {
	zend_uchar type;
#ifdef ZEND_ENGINE_2
	zend_bool uses_this;
#else
	zend_bool uses_globals;
#endif
	zend_bool return_reference;
#ifdef ZEND_ENGINE_2
	zend_uint num_args;
	zend_uint required_num_args;
	zend_arg_info *arg_info;
	zend_bool pass_rest_by_reference;
#else
	zend_uchar *arg_types;
#endif
	char *function_name;
	char *function_name_lc;
#ifdef ZEND_ENGINE_2
	char *scope_name;
	int scope_name_len;
	zend_uint fn_flags;
#endif
	zend_op *opcodes;
	zend_uint last;
	zend_uint T;
	zend_brk_cont_element *brk_cont_array;
	zend_uint last_brk_cont;
#ifdef ZEND_ENGINE_2
	zend_try_catch_element *try_catch_array;
	int last_try_catch;
#endif
	HashTable *static_variables;
	char *filename;
#ifdef ZEND_ENGINE_2
	zend_uint line_start;
	zend_uint line_end;
	char *doc_comment;
	zend_uint doc_comment_len;
#endif
} eaccelerator_op_array;

typedef struct _eaccelerator_class_entry {
	char type;
	char *name;
	char *name_lc;
	uint name_length;
	char *parent;
	HashTable function_table;
	HashTable default_properties;
#ifdef ZEND_ENGINE_2
	zend_uint ce_flags;
	HashTable *static_members;
	HashTable properties_info;
	HashTable constants_table;
	zend_uint num_interfaces;
	char **interfaces;
	zend_class_iterator_funcs iterator_funcs;

	 zend_object_value (*create_object) (zend_class_entry *
										 class_type TSRMLS_DC);
	zend_object_iterator *(*get_iterator) (zend_class_entry * ce,
										   zval * object TSRMLS_DC);
	int (*interface_gets_implemented) (zend_class_entry * iface, zend_class_entry * class_type TSRMLS_DC);	/* a class implements this interface */

	char *filename;
	zend_uint line_start;
	zend_uint line_end;
	char *doc_comment;
	zend_uint doc_comment_len;
#endif
} eaccelerator_class_entry;

/*
 * To cache functions and classes.
 */
typedef struct _mm_fc_entry {
	void *fc;
	struct _mm_fc_entry *next;
	int htablen;
	char htabkey[1];			/* must be last element */
} mm_fc_entry;

/*
 * A mm_cache_entry is a bucket for one PHP script file.
 * Nested  functions and classes which defined in the file goes
 * into the list of mm_fc_entry.
 */
typedef struct _mm_cache_entry {
	struct _mm_cache_entry *next;
#ifdef EACCELERATOR_USE_INODE
	dev_t st_dev;				/* file's device                     */
	ino_t st_ino;				/* file's inode                      */
#else
	unsigned int hv;			/* hash value                        */
#endif
	off_t filesize;				/* file size */
	time_t mtime;				/* file last modification time       */
	time_t ttl;					/* expiration time                   */
	int size;					/* entry size (bytes)                */
	int nhits;					/* hits count                        */
	int nreloads;				/* count of reloads                  */
	int use_cnt;				/* how many processes uses the entry */
	eaccelerator_op_array *op_array;	/* script's global scope code        */
	mm_fc_entry *f_head;		/* list of nested functions          */
	mm_fc_entry *c_head;		/* list of nested classes            */
	zend_bool removed;			/* the entry is scheduled to remove  */
	char realfilename[1];		/* real file name (must be last el.) */
} mm_cache_entry;

/*
 * bucket for user's cache
 */
typedef struct _mm_user_cache_entry {
	struct _mm_user_cache_entry *next;
	unsigned int hv;			/* hash value                  */
	long ttl;					/* expiration time             */
	int size;
	zval value;					/* value                       */
	char key[1];				/* key value (must be last el) */
} mm_user_cache_entry;

/*
 * Linked list of mm_cache_entry which are used by process/thread
 */
typedef struct _mm_used_entry {
	struct _mm_used_entry *next;
	mm_cache_entry *entry;
} mm_used_entry;

/*
 * Linked list of locks
 */
typedef struct _mm_lock_entry {
	struct _mm_lock_entry *next;
	pid_t pid;
#ifdef ZTS
	THREAD_T thread;
#endif
	char key[1];
} mm_lock_entry;

typedef struct _mm_file_header {
	char magic[8];				/* "EACCELERATOR" */
	int eaccelerator_version;
	int zend_version;
	int php_version;
	int size;
	time_t mtime;
	unsigned int crc32;
} mm_file_header;

typedef struct {
	MM *mm;
	pid_t owner;
	size_t total;
	unsigned int hash_cnt;
	unsigned int user_hash_cnt;
	zend_bool enabled;
	zend_bool optimizer_enabled;
	unsigned int rem_cnt;
	time_t last_prune;
	mm_cache_entry *removed;
	mm_lock_entry *locks;

	mm_cache_entry *hash[MM_HASH_SIZE];
	mm_user_cache_entry *user_hash[MM_USER_HASH_SIZE];
} eaccelerator_mm;

/*
 * Where to cache
 */
typedef enum _eaccelerator_cache_place {
	eaccelerator_shm_and_disk,	/* in shm and in disk */
	eaccelerator_shm,			/* in shm, but if it is not possible then on disk */
	eaccelerator_shm_only,		/* in shm only  */
	eaccelerator_disk_only,		/* on disk only */
	eaccelerator_none			/* don't cache  */
} eaccelerator_cache_place;

/*
 * conditional filter
 */
typedef struct _mm_cond_entry {
	char *str;
	int len;
	zend_bool not;
	struct _mm_cond_entry *next;
} mm_cond_entry;

/******************************************************************************/

void format_size (char *s, unsigned int size, int legend);
void eaccelerator_prune (time_t t);

int eaccelerator_lock (const char *key, int key_len TSRMLS_DC);
int eaccelerator_unlock (const char *key, int key_len TSRMLS_DC);

void *eaccelerator_malloc2 (size_t size TSRMLS_DC);

unsigned int eaccelerator_crc32 (const char *p, size_t n);
int eaccelerator_md5 (char *s, const char *prefix, const char *key TSRMLS_DC);

void restore_zval (zval * TSRMLS_DC);
void calc_zval (zval * z TSRMLS_DC);
void store_zval (zval * z TSRMLS_DC);
void fixup_zval (zval * z TSRMLS_DC);

inline unsigned int hash_mm (const char *data, int len);

#  ifdef WITH_EACCELERATOR_EXECUTOR
ZEND_DLEXPORT void eaccelerator_execute (zend_op_array * op_array TSRMLS_DC);
#  endif

#  ifdef WITH_EACCELERATOR_OPTIMIZER
void eaccelerator_optimize (zend_op_array * op_array);
#  endif

#ifdef WITH_EACCELERATOR_ENCODER
PHP_FUNCTION (eaccelerator_encode);
#endif

#ifdef WITH_EACCELERATOR_LOADER
zend_op_array *eaccelerator_load (char *src, int src_len TSRMLS_DC);
PHP_FUNCTION (eaccelerator_load);
PHP_FUNCTION (_eaccelerator_loader_file);
PHP_FUNCTION (_eaccelerator_loader_line);
#endif
#endif

/*
 * Globals (different for each process/thread)
 */
ZEND_BEGIN_MODULE_GLOBALS (eaccelerator)
void *used_entries;				/* list of files which are used     */
								/* by process/thread                */
zend_bool enabled;
zend_bool optimizer_enabled;
zend_bool compression_enabled;
zend_bool compiler;
zend_bool encoder;
zend_bool compress;
zend_bool compress_content;
zend_bool in_request;
zend_llist *content_headers;
long compress_level;
char *cache_dir;
char *name_space;
char *mem;
HashTable strings;
zend_class_entry *class_entry;
mm_cond_entry *cond_list;
zend_uint refcount_helper;
char hostname[32];
#ifdef WITH_EACCELERATOR_CRASH_DETECTION
#ifdef SIGSEGV
void (*original_sigsegv_handler) (int);
#endif
#ifdef SIGFPE
void (*original_sigfpe_handler) (int);
#endif
#ifdef SIGBUS
void (*original_sigbus_handler) (int);
#endif
#ifdef SIGILL
void (*original_sigill_handler) (int);
#endif
#ifdef SIGABRT
void (*original_sigabrt_handler) (int);
#endif
#endif
#if defined(DEBUG) || defined(TEST_PERFORMANCE)  || defined(PROFILE_OPCODES)
int xpad;
#endif
#ifdef WITH_EACCELERATOR_SESSIONS
char *session;
#endif
#ifdef PROFILE_OPCODES
int profile_level;
long self_time[256];
#endif
ZEND_END_MODULE_GLOBALS (eaccelerator)

ZEND_EXTERN_MODULE_GLOBALS (eaccelerator)
#endif							/*#ifndef INCLUDED_EACCELERATOR_H */
