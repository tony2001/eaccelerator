/*
   +----------------------------------------------------------------------+
   | eAccelerator project                                                 |
   +----------------------------------------------------------------------+
   | Copyright (c) 2004 - 2006 eAccelerator                               |
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
   | A copy is availble at http://www.gnu.org/copyleft/gpl.txt            |
   +----------------------------------------------------------------------+
   $Id: cache.c 178 2006-03-06 09:08:40Z bart $
*/

#include "eaccelerator.h"
#include "eaccelerator_version.h"

#ifdef HAVE_EACCELERATOR

#include "zend.h"
#include "zend_API.h"
#include "zend_extensions.h"
#include "debug.h"
#include "ea_store.h"
#include "ea_restore.h"

/* variables needed from eaccelerator.c */
extern long eaccelerator_shm_max;
extern int binary_eaccelerator_version;
extern int binary_php_version;
extern int binary_zend_version;
extern eaccelerator_mm *ea_mm_instance;

static char *build_key(const char *key, int key_len, int *xlen TSRMLS_DC) /* {{{ */
{
    int len;

    /*
     * namespace
     */
    len = strlen(EAG(name_space));
    if (len > 0) {
        char *xkey;
        *xlen = len + key_len + 1;
        xkey = emalloc((*xlen) + 1);
        memcpy(xkey, EAG(name_space), len);
        xkey[len] = ':';
        memcpy(xkey + len + 1, key, key_len + 1);
        return xkey;
    }

	*xlen = key_len;
	return (char *) key;
}
/* }}} */

/* put a key in the cache (shm or disk) */
int eaccelerator_put(const char *key, int key_len, zval * val, time_t ttl TSRMLS_DC) /* {{{ */
{
    ea_user_cache_entry *p, *q;
    unsigned int slot, hv;
    long size;
    int ret = 0;
    int xlen;
    char *xkey;

 	if (ea_mm_instance == NULL) {
		return 0;
	}

    xkey = build_key(key, key_len, &xlen TSRMLS_CC);
    EAG(mem) = NULL;
    zend_hash_init(&EAG(strings), 0, NULL, NULL, 0);
    EACCELERATOR_ALIGN(EAG(mem));
    EAG(mem) += offsetof(ea_user_cache_entry, key) + xlen + 1;
    EAG(mem) += calc_zval(val TSRMLS_CC);
    zend_hash_destroy(&EAG(strings));

    size = (long) EAG(mem);

	EAG(mem) = NULL;
	EACCELERATOR_UNPROTECT();
	EAG(mem) = eaccelerator_malloc(size);
	if (EAG(mem) == NULL) {
		EAG(mem) = eaccelerator_malloc2(size TSRMLS_CC);
	}
    if (EAG(mem)) {
		memset(EAG(mem), 0, size);
        zend_hash_init(&EAG(strings), 0, NULL, NULL, 0);
        EACCELERATOR_ALIGN(EAG(mem));
        q = (ea_user_cache_entry *) EAG(mem);
        q->size = size;
        EAG(mem) += offsetof(ea_user_cache_entry, key) + xlen + 1;
        q->hv = zend_get_hash_value(xkey, xlen + 1);
        memcpy(q->key, xkey, xlen + 1);
        memcpy(&q->value, val, sizeof(zval));
        q->ttl = ttl ? time(0) + ttl : 0;
        q->create = time(0);
        /* set the refcount to 1 */
		Z_SET_REFCOUNT_P(&q->value, 1);
        store_zval(&EAG(mem), &q->value TSRMLS_CC);
        zend_hash_destroy(&EAG(strings));

		/*
		 * storing to shared memory
		 */
		slot = q->hv & EA_USER_HASH_MAX;
		hv = q->hv;
		EACCELERATOR_LOCK_RW();
		ea_mm_instance->user_hash_cnt++;
		q->next = ea_mm_instance->user_hash[slot];
		ea_mm_instance->user_hash[slot] = q;
		p = q->next;
		while (p != NULL) {
			if ((p->hv == hv) && (strcmp(p->key, xkey) == 0)) {
				ea_mm_instance->user_hash_cnt--;
				q->next = p->next;
				eaccelerator_free_nolock(p);
				break;
			}
			q = p;
			p = p->next;
		}
		EACCELERATOR_UNLOCK_RW();
		ret = 1;
	}
	EACCELERATOR_PROTECT();

	if (xlen != key_len)
        efree(xkey);

    return ret;
}
/* }}} */

/* add a key in the cache (fail if it already exists) */
int eaccelerator_add(const char *key, int key_len, zval * val, time_t ttl TSRMLS_DC) /* {{{ */
{
	ea_user_cache_entry *p, *q, *x = NULL;
	unsigned int slot, hv;
	long size;
	int ret = 0;
	int xlen;
	char *xkey;

	if (ea_mm_instance == NULL) {
		return 0;
	}

	xkey = build_key(key, key_len, &xlen TSRMLS_CC);
	hv = zend_get_hash_value(xkey, xlen + 1);
	slot = hv & EA_USER_HASH_MAX;

	EACCELERATOR_UNPROTECT();
	EACCELERATOR_LOCK_RW();

	q = NULL;
	p = ea_mm_instance->user_hash[slot];
	while (p != NULL) {
		if ((p->hv == hv) && (strcmp(p->key, xkey) == 0)) {
			x = p;
			break;
		}
		q = p;
		p = p->next;
	}

	if (x) {
		EACCELERATOR_UNLOCK_RW();
		EACCELERATOR_PROTECT();
		/* an item with such key already exists, bail out */
		if (xlen != key_len) {
			efree(xkey);
		}
		return 0;
	}

	EAG(mem) = NULL;
	zend_hash_init(&EAG(strings), 0, NULL, NULL, 0);
	EACCELERATOR_ALIGN(EAG(mem));
	EAG(mem) += offsetof(ea_user_cache_entry, key) + xlen + 1;
	EAG(mem) += calc_zval(val TSRMLS_CC);
	zend_hash_destroy(&EAG(strings));

	size = (long) EAG(mem);

	EAG(mem) = NULL;
	EAG(mem) = eaccelerator_malloc_nolock(size);
	if (EAG(mem)) {
		memset(EAG(mem), 0, size);
		zend_hash_init(&EAG(strings), 0, NULL, NULL, 0);
		EACCELERATOR_ALIGN(EAG(mem));
		q = (ea_user_cache_entry *) EAG(mem);
		q->size = size;
		EAG(mem) += offsetof(ea_user_cache_entry, key) + xlen + 1;
		q->hv = zend_get_hash_value(xkey, xlen + 1);
		memcpy(q->key, xkey, xlen + 1);
		memcpy(&q->value, val, sizeof(zval));
		q->ttl = ttl ? time(0) + ttl : 0;
		q->create = time(0);
		/* set the refcount to 1 */
		Z_SET_REFCOUNT_P(&q->value, 1);
		store_zval(&EAG(mem), &q->value TSRMLS_CC);
		zend_hash_destroy(&EAG(strings));

		/*
		 * storing to shared memory
		 */
		slot = q->hv & EA_USER_HASH_MAX;
		hv = q->hv;

		ea_mm_instance->user_hash_cnt++;
		q->next = ea_mm_instance->user_hash[slot];
		ea_mm_instance->user_hash[slot] = q;
		p = q->next;
		while (p != NULL) {
			if ((p->hv == hv) && (strcmp(p->key, xkey) == 0)) {
				ea_mm_instance->user_hash_cnt--;
				q->next = p->next;
				eaccelerator_free_nolock(p);
				break;
			}
			q = p;
			p = p->next;
		}
		ret = 1;
	}
	EACCELERATOR_UNLOCK_RW();
	EACCELERATOR_PROTECT();

	if (xlen != key_len) {
		efree(xkey);
	}
	return ret;
}
/* }}} */

/* get a key from the cache */
int eaccelerator_get(const char *key, int key_len, zval * return_value TSRMLS_DC) /* {{{ */
{
    unsigned int hv, slot;
    int xlen;
    char *xkey;
	ea_user_cache_entry *p;
	ea_user_cache_entry *x = NULL;

 	if (ea_mm_instance == NULL) {
		return 0;
	}

    xkey = build_key(key, key_len, &xlen TSRMLS_CC);
    hv = zend_get_hash_value(xkey, xlen + 1);
    slot = hv & EA_USER_HASH_MAX;

	EACCELERATOR_UNPROTECT();
	EACCELERATOR_LOCK_RD();
	p = ea_mm_instance->user_hash[slot];
	while (p != NULL) {
		if ((p->hv == hv) && (strcmp(p->key, xkey) == 0)) {
			x = p;
			if (p->ttl != 0 && p->ttl < time(0)) {
				x = NULL;
			}
			break;
		}
		p = p->next;
	}
	EACCELERATOR_UNLOCK_RD();
	EACCELERATOR_PROTECT();
	if (x) {
		memcpy(return_value, &x->value, sizeof(zval));
		restore_zval(return_value TSRMLS_CC);
		if (xlen != key_len) {
			efree(xkey);
		}
		return 1;
	}
    return 0;
}
/* }}} */

/* remove a key from the cache */
int eaccelerator_rm(const char *key, int key_len TSRMLS_DC) /* {{{ */
{
    unsigned int hv, slot;
    ea_user_cache_entry *p, *q;
    int xlen;
    char *xkey;

 	if (ea_mm_instance == NULL) {
		return 0;
	}

    xkey = build_key(key, key_len, &xlen TSRMLS_CC);

	hv = zend_get_hash_value(xkey, xlen + 1);
	slot = hv & EA_USER_HASH_MAX;

	EACCELERATOR_UNPROTECT();
	EACCELERATOR_LOCK_RW();
	q = NULL;
	p = ea_mm_instance->user_hash[slot];
	while (p != NULL) {
		if ((p->hv == hv) && (strcmp(p->key, xkey) == 0)) {
			if (q == NULL) {
				ea_mm_instance->user_hash[slot] = p->next;
			} else {
				q->next = p->next;
			}
			ea_mm_instance->user_hash_cnt--;
			eaccelerator_free_nolock(p);
			break;
		}
		q = p;
		p = p->next;
	}
	EACCELERATOR_UNLOCK_RW();
	EACCELERATOR_PROTECT();
    if (xlen != key_len) {
        efree(xkey);
    }
    return 1;
}
/* }}} */

/* do garbage collection on the keys */
size_t eaccelerator_gc(TSRMLS_D) /* {{{ */
{
    size_t size = 0;
    unsigned int i;
    time_t t = time(0);

    if (ea_mm_instance == NULL) {
        return 0;
    }

    EACCELERATOR_UNPROTECT();
    EACCELERATOR_LOCK_RW();
    for (i = 0; i < EA_USER_HASH_SIZE; i++) {
        ea_user_cache_entry **p = &ea_mm_instance->user_hash[i];
        while (*p != NULL) {
            if ((*p)->ttl != 0 && (*p)->ttl < t) {
                ea_user_cache_entry *r = *p;
                *p = (*p)->next;
                ea_mm_instance->user_hash_cnt--;
                size += r->size;
                eaccelerator_free_nolock(r);
            } else {
                p = &(*p)->next;
            }
        }
    }
    EACCELERATOR_UNLOCK_RW();
    EACCELERATOR_PROTECT();
    return size;
}
/* }}} */

/* get list of all keys stored in memory that matches namespace */
int eaccelerator_list_keys(zval *return_value TSRMLS_DC) /* {{{ */
{
    unsigned int i, xlen;
    zval *list;
    char *xkey = "";
    ea_user_cache_entry *p;
    time_t t = time(0);

 	if (ea_mm_instance == NULL) {
		return 0;
	}

    // create key prefix for current host / namespace
    xlen = strlen(EAG(name_space));
    if (xlen > 0) {
        xkey = emalloc(xlen + 1);
        memcpy(xkey, EAG(name_space), xlen);
    }

    // initialize return value as an array
    array_init(return_value);

    for (i = 0; i < EA_USER_HASH_SIZE; ++i) {
        p = ea_mm_instance->user_hash[i];
        while(p != NULL) {
            if (!xlen || strncmp(p->key, xkey, xlen) == 0) {
                list = NULL;
                ALLOC_INIT_ZVAL(list);
                array_init(list);

                if (strlen(p->key) > xlen) {
                    add_assoc_string(list, "name", (p->key) + xlen, 1);
                } else {
                    add_assoc_string(list, "name", p->key, 1);
                }

                if (p->ttl) {
                    if (p->ttl < t) {
                        add_assoc_long(list, "ttl", p->ttl); // ttl
                    } else {
                        add_assoc_long(list, "ttl", -1); // expired
                    }
                } else {
                    add_assoc_long(list, "ttl", 0); // no ttl
                }

                add_assoc_long(list, "created", p->create);
                add_assoc_long(list, "size", p->size);
                add_next_index_zval(return_value, list);
            }
            p = p->next;
        }
    }

    if (xlen > 0)
        efree(xkey);
    return 1;
}
/* }}} */

#endif /* HAVE_EACCELERATOR */

