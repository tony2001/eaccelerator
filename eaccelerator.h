/*
   +----------------------------------------------------------------------+
   | eAccelerator project                                                 |
   +----------------------------------------------------------------------+
   | Copyright (c) 2004 - 2010 eAccelerator                               |
   | http://eaccelerator.net                                  		      |
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

#ifndef INCLUDED_EACCELERATOR_H
#define INCLUDED_EACCELERATOR_H

#include "php.h"
#include "zend.h"
#include "zend_API.h"
#include "zend_extensions.h"
#include <signal.h>

#if !defined(ZEND_WIN32) && defined(HAVE_CONFIG_H)
#  if ZEND_MODULE_API_NO >= 20001222
#    include "config.h"
#  else
#    include "php_config.h"
#  endif
#endif

#if PHP_MAJOR_VERSION == 4 || (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 1)
    #error "eAccelerator only supports PHP 5.1 and higher"
#endif

#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 2
#   define ZEND_ENGINE_2_2
#endif

#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 3
#   define ZEND_ENGINE_2_3
#endif

/* fixes compile errors on php5.1 */
#ifdef STR_EMPTY_ALLOC
#	define empty_string STR_EMPTY_ALLOC()
#endif

#ifdef WITH_EACCELERATOR_CRASH_DETECTION
#  include <signal.h>
#endif

#define EACCELERATOR_MM_FILE "/tmp/eaccelerator"

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
#	   include <sys/file.h>
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

#if !defined(_INTPTR_T_DEFINED) && ZEND_WIN32
	typedef intptr_t;
	#define _INTPTR_T_DEFINED
#else
#  ifdef HAVE_STDINT_H
#    include <stdint.h>
#  elif HAVE_INTTYPES_H 
#    include <inttypes.h> 
#  endif 
#endif

#if !defined(ssize_t) && ZEND_WIN32
	/* define ssize_t for Win32. */
	#define ssize_t int
#endif 

#ifdef HAVE_EACCELERATOR

#include "mm.h"

#ifdef ZEND_WIN32
#  include <process.h>
#  ifndef S_ISREG
#    define S_ISREG(mode) (((mode)&S_IFMT) & S_IFREG)
#  endif
#  ifndef S_IRUSR
#    define S_IRUSR S_IREAD
#  endif
#  ifndef S_IWUSR
#    define S_IWUSR S_IWRITE
#  endif
#else
#  include <dirent.h>
#endif

#ifdef ZTS
#  define ZTS_LOCK()    tsrm_mutex_lock(ea_mutex)
#  define ZTS_UNLOCK()  tsrm_mutex_unlock(ea_mutex)
#else
#  define ZTS_LOCK()
#  define ZTS_UNLOCK()
#endif

#if defined(EACCELERATOR_PROTECT_SHM)
#  define EACCELERATOR_PROTECT(instance)    do {mm_protect((instance)->mm, MM_PROT_READ);} while(0)
#  define EACCELERATOR_UNPROTECT(instance)  do {mm_protect((instance)->mm, MM_PROT_READ|MM_PROT_WRITE);} while(0)
#else
#  define EACCELERATOR_PROTECT(instance)
#  define EACCELERATOR_UNPROTECT(instance)
#endif

#define EACCELERATOR_LOCK_RW(instance)   \
		EA_HANDLE_BLOCK_INTERRUPTIONS(); \
		ZTS_LOCK();\
		fprintf(stderr, "wrlock %s:%d\n", __FILE__, __LINE__);\
		mm_lock((instance)->mm, MM_LOCK_RW); \

#define EACCELERATOR_LOCK_RD(instance)   \
		EA_HANDLE_BLOCK_INTERRUPTIONS(); \
		ZTS_LOCK();\
		fprintf(stderr, "rdlock %s:%d\n", __FILE__, __LINE__);\
		mm_lock((instance)->mm, MM_LOCK_RD)

#define EACCELERATOR_UNLOCK(instance)     \
		fprintf(stderr, "unlock %s:%d\n", __FILE__, __LINE__);\
		mm_unlock((instance)->mm); \
		ZTS_UNLOCK(); \
		EA_HANDLE_UNBLOCK_INTERRUPTIONS(); \

#define EACCELERATOR_UNLOCK_RW(instance)  EACCELERATOR_UNLOCK((instance))
#define EACCELERATOR_UNLOCK_RD(instance)  EACCELERATOR_UNLOCK((instance))

#define EA_HANDLE_BLOCK_INTERRUPTIONS() \
		sigset_t oldmask, blockmask; \
		sigfillset(&blockmask); \
		sigprocmask(SIG_BLOCK, &blockmask, &oldmask)

#define EA_HANDLE_UNBLOCK_INTERRUPTIONS() \
		sigprocmask(SIG_SETMASK, &oldmask, NULL)

#define EACCELERATOR_HASH_LEVEL 2
#define EA_HASH_SIZE      512
#define EA_USER_HASH_SIZE      512

#define EA_HASH_MAX       (EA_HASH_SIZE-1)
#define EA_USER_HASH_MAX       (EA_USER_HASH_SIZE-1)

#define eaccelerator_malloc(instance, size)        mm_malloc_lock((instance)->mm, size)
#define eaccelerator_free(instance, x)             mm_free_lock((instance)->mm, x)
#define eaccelerator_malloc_nolock(instance, size) mm_malloc_nolock((instance)->mm, size)
#define eaccelerator_free_nolock(instance, x)      mm_free_nolock((instance)->mm, x)

#if (defined (__GNUC__) && __GNUC__ >= 2)
#define EACCELERATOR_PLATFORM_ALIGNMENT (__alignof__ (align_test))
#else
#define EACCELERATOR_PLATFORM_ALIGNMENT (sizeof(align_union))
#endif

#define EACCELERATOR_ALIGN(n) (n) = (void*)((((size_t)(n)-1) & ~(EACCELERATOR_PLATFORM_ALIGNMENT-1)) + EACCELERATOR_PLATFORM_ALIGNMENT)
#define EA_SIZE_ALIGN(n) (n) = ((((size_t)(n)-1) & ~(EACCELERATOR_PLATFORM_ALIGNMENT-1)) + EACCELERATOR_PLATFORM_ALIGNMENT)

#ifdef ZEND_ENGINE_2_3
    #define RESET_PZVAL_REFCOUNT(z) Z_SET_REFCOUNT_P(z, 1)
#else
    #define RESET_PZVAL_REFCOUNT(z) (z)->refcount = 1;
#endif

#define MAX_DUP_STR_LEN 256

/******************************************************************************/

/* get the type of the zvalue */
#ifdef ZEND_ENGINE_2_3
#   define EA_ZV_TYPE_P(zv) (Z_TYPE_P(zv) & IS_CONSTANT_TYPE_MASK)
#else
#   define EA_ZV_TYPE_P(zv) (Z_TYPE_P(zv) & ~IS_CONSTANT_INDEX)
#endif

#define GET_NEW_CAS(instance) ++(instance)->cas

#ifndef offsetof
#  define offsetof(str,fld) ((size_t)&(((str*)NULL)->fld))
#endif

typedef struct _eaccelerator_op_array {
	zend_uchar type;
#ifdef ZEND_ENGINE_2_3
    zend_uint early_binding;
    zend_uint this_var;
#else
	zend_bool uses_this;
#endif
	zend_bool return_reference;
	zend_uint num_args;
	zend_uint required_num_args;
	zend_arg_info *arg_info;
	zend_bool pass_rest_by_reference;
	char *function_name;
	char *function_name_lc;
	int function_name_len;
	char *scope_name;
	char *scope_name_lc;
	int scope_name_len;
	zend_uint fn_flags;
	zend_op *opcodes;
	zend_uint last;
	zend_compiled_variable *vars;
    int last_var;
	zend_uint T;
	zend_brk_cont_element *brk_cont_array;
	zend_uint last_brk_cont;
	zend_try_catch_element *try_catch_array;
	int last_try_catch;
	HashTable *static_variables;
	char *filename;
	zend_uint line_start;
	zend_uint line_end;
#ifdef INCLUDE_DOC_COMMENTS
    char *doc_comment;
    zend_uint doc_comment_len;
#endif
} ea_op_array;

typedef struct _eaccelerator_class_entry {
	char type;
	char *name;
	zend_uint name_length;
	char *parent;
	HashTable function_table;
	HashTable default_properties;
	HashTable properties_info;
	HashTable default_static_members;
	HashTable *static_members;
	HashTable constants_table;
	zend_uint ce_flags;
	zend_uint num_interfaces;

	char *filename;
	zend_uint line_start;
	zend_uint line_end;
#  ifdef INCLUDE_DOC_COMMENTS
    char *doc_comment;
    zend_uint doc_comment_len;
#  endif
} ea_class_entry;

/*
 * To cache functions and classes.
 */
typedef struct _ea_fc_entry {
	void *fc;
	struct _ea_fc_entry *next;
	int htablen;
	char htabkey[1];			/* must be last element */
} ea_fc_entry;


/*
 * A ea_cache_entry is a bucket for one PHP script file.
 * User functions and classes defined in the file go into
 * the list of ea_fc_entry.
 */
typedef struct _ea_cache_entry {
	struct _ea_cache_entry *next;
	unsigned int hv;			/* hash value                            */
	off_t filesize;				/* file size */
	time_t mtime;				/* file last modification time           */
	time_t ttl;				/* expiration time (updated on each hit) */
	time_t ts;				/* timestamp of cache entry              */
	unsigned int size;			/* entry size (bytes)                    */
	unsigned int nhits;			/* hits count                            */
	unsigned int nreloads;			/* count of reloads                      */
	int use_cnt;			/* how many processes uses the entry     */
	ea_op_array *op_array;	/* script's global scope code        */
	ea_fc_entry *f_head;		/* list of nested functions          */
	ea_fc_entry *c_head;		/* list of nested classes            */
	zend_bool removed;			/* the entry is scheduled to remove  */
	int realfilename_len;
	char realfilename[1];		/* real file name (must be last el.) */
} ea_cache_entry;

/*
 * Linked list of ea_cache_entry which are used by process/thread
 */
typedef struct _ea_used_entry {
	struct _ea_used_entry *next;
	ea_cache_entry *entry;
} ea_used_entry;

typedef struct _ea_file_header {
	char magic[8];				/* "EACCELERATOR" */
	int eaccelerator_version[2];
	int zend_version[2];
	int php_version[2];
	int size;
	time_t mtime;
	time_t ts;
	unsigned int crc32;
} ea_file_header;

int check_header(ea_file_header *hdr);
void init_header(ea_file_header *hdr);

/*
 * bucket for user's cache
 */
typedef struct _ea_user_cache_entry {
	struct _ea_user_cache_entry *next;
	unsigned int hv;			/* hash value                  */
	long ttl;					/* expiration time             */
	unsigned long cas;			/* cas value for the entry */
	long create;
	int size;
	zval value;					/* value                       */
	char key[1];				/* key value (must be last el) */
} ea_user_cache_entry;

typedef struct {
	MM *mm;
	pid_t owner;
	size_t total;
	unsigned int hash_cnt;
	unsigned int user_hash_cnt;
	zend_bool enabled;
	zend_bool check_mtime_enabled;
	unsigned int rem_cnt;
	time_t start_time;
	time_t last_prune;
	ea_cache_entry *removed;
	ea_cache_entry *hash[EA_HASH_SIZE];
	ea_user_cache_entry *user_hash[EA_USER_HASH_SIZE];
	unsigned long cas;
} eaccelerator_mm;

typedef union align_union {
  double d;
  void *v;
  int (*func)(int);
  long l;
} align_union;

#ifdef ZEND_ENGINE_2_2
typedef union _align_test {
  void *ptr;
  double dbl;
  long lng;
} align_test;
#endif

/******************************************************************************/

#ifdef ZTS
#  ifdef __APPLE__
/* Workaround to prevent 'multiple definitions of symbol' during build on OSX */
static MUTEX_T ea_mutex;
#  else
MUTEX_T ea_mutex;
#  endif
#endif

/* needed to compile eA as a static php module */
extern zend_module_entry eaccelerator_module_entry;
#define phpext_eaccelerator_ptr &eaccelerator_module_entry


void format_size (char *s, unsigned int size, int legend);
void eaccelerator_prune (eaccelerator_mm *mm_instance, time_t t);

void *eaccelerator_malloc2 (eaccelerator_mm* instance, size_t size TSRMLS_DC);

unsigned int eaccelerator_crc32 (const char *p, size_t n);

#ifdef ZTS
#  define EAG(v) TSRMG(eaccelerator_globals_id, zend_eaccelerator_globals*, v)
#else
#  define EAG(v) (eaccelerator_globals.v)
#endif

struct ea_pattern_t {
  struct ea_pattern_t *next;
  char *pattern;
};

/*
 * Globals (different for each process/thread)
 */
ZEND_BEGIN_MODULE_GLOBALS (eaccelerator)
void *used_entries;			/* list of files which are used     */
					/* by process/thread                */
zend_bool enabled;
zend_bool check_mtime_enabled;
zend_bool compiler;
zend_bool in_request;
char *ea_log_file;
char *mem;
char *allowed_admin_path;
char *name_space;
time_t req_start;			/* time of request start (set in RINIT) */
HashTable strings;
HashTable restored;
zend_class_entry *class_entry;
zend_uint refcount_helper;
struct ea_pattern_t *pattern_list;
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
#ifdef WITH_EACCELERATOR_DEBUG
int xpad;
int profile_level;
long self_time[256];
#endif
ZEND_END_MODULE_GLOBALS (eaccelerator)

ZEND_EXTERN_MODULE_GLOBALS (eaccelerator)

#define EACCELERATOR_EXTENSION_NAME "eAccelerator"

#define EA_MAGIC "EACCELERATOR"

#define EACCELERATOR_VERSION_GUID   "PHPE8EDA1B6-806A-4851-B1C8-A6B4712F44FB"
#define EACCELERATOR_LOGO_GUID      "PHPE6F78DE9-13E4-4dee-8518-5FA2DACEA803"
#define EACCELERATOR_VERSION_STRING (EACCELERATOR_EXTENSION_NAME " " EACCELERATOR_VERSION " (PHP " PHP_VERSION ")")

#endif 		/* HAVE_EACCELERATOR */
#endif		/* #ifndef INCLUDED_EACCELERATOR_H */
