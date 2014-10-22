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

#ifdef HAVE_EACCELERATOR

#include "debug.h"
#include "ea_restore.h"
#include "opcodes.h"
#include "zend.h"
#include "zend_API.h"
#include "zend_extensions.h"
#include "zend_vm.h"

#ifndef INCOMPLETE_CLASS
#  define INCOMPLETE_CLASS "__PHP_Incomplete_Class"
#endif
#ifndef MAGIC_MEMBER
#  define MAGIC_MEMBER "__PHP_Incomplete_Class_Name"
#endif

typedef void (*fixup_bucket_t) (char *, void *TSRMLS_DC);

#define fixup_zval_hash(base, from) \
    fixup_hash(base, from, (fixup_bucket_t)fixup_zval TSRMLS_CC)

static inline void fixup_hash(char *base, HashTable * source, fixup_bucket_t fixup_bucket TSRMLS_DC) /* {{{ */
{
	unsigned int i;
	Bucket *p;

	if (source->nNumOfElements > 0) {
		if (source->arBuckets != NULL) {
			FIXUP(base, source->arBuckets);
			for (i = 0; i < source->nTableSize; i++) {
				FIXUP(base, source->arBuckets[i]);
			}
		}
		FIXUP(base, source->pListHead);
		FIXUP(base, source->pListTail);

		p = source->pListHead;
		while (p) {
			FIXUP(base, p->pNext);
			FIXUP(base, p->pLast);
			FIXUP(base, p->pData);
			FIXUP(base, p->pDataPtr);
			FIXUP(base, p->pListLast);
			FIXUP(base, p->pListNext);
			if (p->pDataPtr) {
				fixup_bucket(base, p->pDataPtr TSRMLS_CC);
				p->pData = &p->pDataPtr;
			} else {
				fixup_bucket(base, p->pData TSRMLS_CC);
			}
			p = p->pListNext;
		}
		source->pInternalPointer = source->pListHead;
	}
}
/* }}} */

static inline void fixup_zval(char *base, zval * zv TSRMLS_DC) /* {{{ */
{
	switch (EA_ZV_TYPE_P(zv)) {
		case IS_CONSTANT:           /* fallthrough */
		case IS_OBJECT:             /* fallthrough: object are serialized */
		case IS_STRING:
			FIXUP(base, Z_STRVAL_P(zv));
			break;

		case IS_ARRAY:              /* fallthrough */
		case IS_CONSTANT_ARRAY:
			FIXUP(base, Z_ARRVAL_P(zv));
			fixup_zval_hash(base, Z_ARRVAL_P(zv));
			break;

		default:
			break;
	}
}
/* }}} */


/******************************************************************************/
/* Functions to restore a php script from shared memory                       */
/******************************************************************************/

typedef void *(*restore_bucket_t) (void *TSRMLS_DC);

static inline zval *restore_zval_ptr(zval * from TSRMLS_DC);
static inline HashTable *restore_hash(HashTable * target, HashTable * source, restore_bucket_t copy_bucket TSRMLS_DC);
static inline HashTable *restore_hash_zval_ptr(HashTable * target, HashTable * source TSRMLS_DC);

void restore_zval(zval * zv TSRMLS_DC) /* {{{ */
{
	switch (EA_ZV_TYPE_P(zv)) {
		case IS_CONSTANT:
		case IS_OBJECT:
		case IS_STRING:
			if (Z_STRVAL_P(zv) == NULL || Z_STRVAL_P(zv)[0] == '\0' || Z_STRLEN_P(zv) == 0) {
				Z_STRLEN_P(zv) = 0;
				Z_STRVAL_P(zv) = empty_string;
				return;
			} else {
				char *p = emalloc(Z_STRLEN_P(zv) + 1);
				memcpy(p, Z_STRVAL_P(zv), Z_STRLEN_P(zv) + 1);
				Z_STRVAL_P(zv) = p;
			}
			return;

		case IS_ARRAY:
		case IS_CONSTANT_ARRAY:
			if (Z_ARRVAL_P(zv) != NULL && Z_ARRVAL_P(zv) != &EG(symbol_table)) {
				Z_ARRVAL_P(zv) = restore_hash_zval_ptr(NULL, Z_ARRVAL_P(zv) TSRMLS_CC);
				Z_ARRVAL_P(zv)->pDestructor = ZVAL_PTR_DTOR;
			}
			return;
	}
}
/* }}} */

static inline zval *restore_zval_ptr(zval * from TSRMLS_DC) /* {{{ */
{
	zval *p;
	ALLOC_ZVAL(p);
	memcpy(p, from, sizeof(zval));
	restore_zval(p TSRMLS_CC);
	/* hrak: reset refcount to make sure there is one reference to this val, and prevent memleaks */
#ifdef ZEND_ENGINE_2_3
	Z_SET_REFCOUNT_P(p, 1);
#else
	p->refcount = 1;
#endif
	return p;
}
/* }}} */

static inline HashTable *restore_hash(HashTable * target, HashTable * source, restore_bucket_t copy_bucket TSRMLS_DC) /* {{{ */
{
	Bucket *p, *np, *prev_p;
	int nIndex;

	if (target == NULL) {
		ALLOC_HASHTABLE(target);
	}
	memcpy(target, source, sizeof(HashTable));
	target->arBuckets =
		(Bucket **) emalloc(target->nTableSize * sizeof(Bucket *));
	memset(target->arBuckets, 0, target->nTableSize * sizeof(Bucket *));
	target->pDestructor = NULL;
	target->persistent = 0;
	target->pListHead = NULL;
	target->pListTail = NULL;
#if HARDENING_PATCH_HASH_PROTECT
	target->canary = zend_hash_canary;
#endif

	p = source->pListHead;
	prev_p = NULL;
	np = NULL;
	while (p) {
		np = (Bucket *) emalloc(sizeof(Bucket) + p->nKeyLength);
		nIndex = p->h % source->nTableSize;
		if (target->arBuckets[nIndex]) {
			np->pNext = target->arBuckets[nIndex];
			np->pLast = NULL;
			np->pNext->pLast = np;
		} else {
			np->pNext = NULL;
			np->pLast = NULL;
		}
		target->arBuckets[nIndex] = np;
		np->h = p->h;
		np->nKeyLength = p->nKeyLength;

		if (p->pDataPtr == NULL) {
			np->pData = copy_bucket(p->pData TSRMLS_CC);
			np->pDataPtr = NULL;
		} else {
			np->pDataPtr = copy_bucket(p->pDataPtr TSRMLS_CC);
			np->pData = &np->pDataPtr;
		}
		np->pListLast = prev_p;
		np->pListNext = NULL;
		np->arKey = ((char *)np + sizeof(Bucket));

		memcpy((char *)np->arKey, p->arKey, p->nKeyLength);

		if (prev_p) {
			prev_p->pListNext = np;
		} else {
			target->pListHead = np;
		}
		prev_p = np;
		p = p->pListNext;
	}
	target->pListTail = np;
	target->pInternalPointer = target->pListHead;
	return target;
}
/* }}} */

static inline HashTable *restore_hash_zval_ptr(HashTable * target, HashTable * source TSRMLS_DC) /* {{{ */
{
	Bucket *p, *np, *prev_p;
	int nIndex;

	if (target == NULL) {
		ALLOC_HASHTABLE(target);
	}
	memcpy(target, source, sizeof(HashTable));
	target->arBuckets =
		(Bucket **) emalloc(target->nTableSize * sizeof(Bucket *));
	memset(target->arBuckets, 0, target->nTableSize * sizeof(Bucket *));
	target->pDestructor = NULL;
	target->persistent = 0;
	target->pListHead = NULL;
	target->pListTail = NULL;
#if HARDENING_PATCH_HASH_PROTECT
	target->canary = zend_hash_canary;
#endif

	p = source->pListHead;
	prev_p = NULL;
	np = NULL;
	while (p) {
		np = (Bucket *) emalloc(sizeof(Bucket) + p->nKeyLength);
		nIndex = p->h % source->nTableSize;
		if (target->arBuckets[nIndex]) {
			np->pNext = target->arBuckets[nIndex];
			np->pLast = NULL;
			np->pNext->pLast = np;
		} else {
			np->pNext = NULL;
			np->pLast = NULL;
		}
		target->arBuckets[nIndex] = np;
		np->h = p->h;
		np->nKeyLength = p->nKeyLength;

		if (p->pDataPtr == NULL) {
			zval *d, *from;

			from = (zval *)p->pData;
			ALLOC_ZVAL(d);
			memcpy(d, from, sizeof(zval));
			restore_zval(d TSRMLS_CC);
			/* hrak: reset refcount to make sure there is one reference to this val, and prevent memleaks */
#ifdef ZEND_ENGINE_2_3
			Z_SET_REFCOUNT_P(d, 1);
#else
			d->refcount = 1;
#endif
			np->pData = d;
			np->pDataPtr = NULL;
		} else {
			zval *d, *from;

			from = (zval *)p->pDataPtr;
			ALLOC_ZVAL(d);
			memcpy(d, from, sizeof(zval));
			restore_zval(d TSRMLS_CC);
			/* hrak: reset refcount to make sure there is one reference to this val, and prevent memleaks */
#ifdef ZEND_ENGINE_2_3
			Z_SET_REFCOUNT_P(d, 1);
#else
			d->refcount = 1;
#endif
			np->pDataPtr = d;
			np->pData = &np->pDataPtr;
		}
		np->pListLast = prev_p;
		np->pListNext = NULL;
		np->arKey = ((char *)np + sizeof(Bucket));

		memcpy((char *)np->arKey, p->arKey, p->nKeyLength);

		if (prev_p) {
			prev_p->pListNext = np;
		} else {
			target->pListHead = np;
		}
		prev_p = np;
		p = p->pListNext;
	}
	target->pListTail = np;
	target->pInternalPointer = target->pListHead;
	return target;
}
/* }}} */

#endif /* HAVE_EACCELERATOR */
