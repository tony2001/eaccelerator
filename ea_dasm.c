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
   $Id: $
*/

#include "ea_dasm.h"
#include "eaccelerator.h"
#include "opcodes.h"
#include "zend.h"

#ifdef WITH_EACCELERATOR_DISASSEMBLER

extern eaccelerator_mm *eaccelerator_mm_instance;

/* {{{ static const char *extopnames_declare[] */
static const char *extopnames_declare[] = {
	"",							/* 0 */
	"DECLARE_CLASS",			/* 1 */
	"DECLARE_FUNCTION",			/* 2 */
	"DECLARE_INHERITED_CLASS"	/* 3 */
};
/* }}} */

/* {{{ static const char *extopnames_cast[] */
static const char *extopnames_cast[] = {
	"IS_NULL",					/* 0 */
	"IS_LONG",					/* 1 */
	"IS_DOUBLE",				/* 2 */
	"IS_STRING",				/* 3 */
	"IS_ARRAY",					/* 4 */
	"IS_OBJECT",				/* 5 */
	"IS_BOOL",					/* 6 */
	"IS_RESOURCE",				/* 7 */
	"IS_CONSTANT",				/* 8 */
	"IS_CONSTANT_ARRAY"			/* 9 */
};
/* }}} */

/* {{{ static const char *extopnames_fetch[] */
static const char *extopnames_fetch[] = {
	"FETCH_STANDARD",			/* 0 */
	"FETCH_ADD_LOCK"			/* 1 */
};
/* }}} */

/* {{{ static const char *extopnames_fetch_class[] */
static const char *extopnames_fetch_class[] = {
	"FETCH_CLASS_DEFAULT",		/* 0 */
	"FETCH_CLASS_SELF",			/* 1 */
	"FETCH_CLASS_PARENT",		/* 2 */
	"FETCH_CLASS_MAIN",			/* 3 */
	"FETCH_CLASS_GLOBAL",		/* 4 */
	"FETCH_CLASS_AUTO"			/* 5 */
};
/* }}} */

/* {{{ static const char *extopnames_init_fcall[] */
static const char *extopnames_init_fcall[] = {
	"",     					/* 0 */
	"MEMBER_FUNC_CALL",			/* 1 */
	"CTOR_CALL",				/* 2 */
	"CTOR_CALL"					/* 3 */
};
/* }}} */

/* {{{ static const char *extopnames_sendnoref[] */
static const char *extopnames_sendnoref[] = {
	"&nbsp;",					/* 0 */
	"ARG_SEND_BY_REF",			/* 1 */
	"ARG_COMPILE_TIME_BOUND",	/* 2 */
	"ARG_SEND_BY_REF | ZEND_ARG_COMPILE_TIME_BOUND"	/* 3 */
};
/* }}} */

/* {{{ static const char *fetchtypename[] */
static const char *fetchtypename[] = {
	"FETCH_GLOBAL",				/* 0 */
	"FETCH_LOCAL",				/* 1 */
	"FETCH_STATIC"				/* 2 */
#ifdef ZEND_ENGINE_2
	,
	"FETCH_STATIC_MEMBER"		/* 3 */
#ifdef ZEND_ENGINE_2_1
    ,
    "UNKNOWN 1"                 /* 4 */
#endif
#endif
};
/* }}} */

/* {{{ static const char *extopnames_fe[] */
static const char *extopnames_fe[] = {
	"",							/* 0 */
	"FE_FETCH_BYREF",			/* 1 */
	"FE_FETCH_WITH_KEY"			/* 2 */
};
/* }}} */

/* {{{ get_zval: create a string from the given zval */
static char *get_zval(zval *v)
{
    char *str = NULL;
    char buf[512];
    size_t size;

    switch (Z_TYPE_P(v) & ~IS_CONSTANT_INDEX) {
        case IS_NULL:
            str = emalloc(sizeof("null"));
            strncpy(str, "null", sizeof("null"));
            break;
        case IS_LONG:
            snprintf(buf, sizeof(buf), "long(%ld)", Z_LVAL_P(v));
            str = emalloc(strlen(buf) + 1);
            strcpy(str, buf);
            break;
        case IS_DOUBLE:
            snprintf(buf, sizeof(buf), "double(%e)", Z_DVAL_P(v));
            str = emalloc(strlen(buf) + 1);
            strcpy(str, buf);
            break;
        case IS_STRING:
            size = Z_STRLEN_P(v) + 1 + sizeof("string('')");
            str = emalloc(size);
            snprintf(str, size, "string('%s')", Z_STRVAL_P(v)); 
            break;
        case IS_BOOL:
            if (Z_LVAL_P(v)) {
                str = emalloc(sizeof("bool(true)"));
                strcpy(str, "bool(true)");
            } else {
                str = emalloc(sizeof("bool(false)"));
                strcpy(str, "bool(false)");
            }
            break;
        case IS_ARRAY:
            str = emalloc(sizeof("array(?)"));
            strcpy(str, "array(?)");
            break;
        case IS_OBJECT:
            str = emalloc(sizeof("object(?)"));
            strcpy(str, "object(?)");
            break;
        case IS_RESOURCE:
            str = emalloc(sizeof("resource(?)"));
            strcpy(str, "resource(?)");
            break;
        case IS_CONSTANT:
            size = Z_STRLEN_P(v) + 1 + sizeof("constant('')");
            str = emalloc(size);
            snprintf(str, size, "constant('%s')", Z_STRVAL_P(v)); 
            break;
        case IS_CONSTANT_ARRAY:
            str = emalloc(sizeof("constant_array(?)"));
            strcpy(str, "constant_array(?)");
            break;
        default:
            snprintf(buf, sizeof(buf), "unknown(type=%d)", Z_TYPE_P(v));
            str = emalloc(strlen(buf) + 1);
            strcpy(str, buf);
    }
    return str;
}
/* }}} */

/* {{{ get_op_array: return a php array with the given op_array structure 
 * array() { [0] .. [n] =>
 *      array () {
 *			[lineno]		=> // the line number in the source code
 *          [opcode]        => // the opcode
 *          [extended_value]=> // the extended value field
 *          [op1]           => // the first opcode
 *          [op2]           => // the second opcode
 *          [result]        => // the result 
 *      }
 * }
 */
static zval *get_op_array(ea_op_array *op_array TSRMLS_DC) 
{
    zval *return_value;
    MAKE_STD_ZVAL(return_value);
    array_init(return_value);

    if (op_array->opcodes) {
        zend_op *opline;
        zend_op *end;
        
        opline = op_array->opcodes;
        end = opline + op_array->last;

        for (; opline < end; ++opline) {
            zval *el;
            char buf[512];
            const opcode_dsc *op = get_opcode_dsc(opline->opcode);
            int zval_used = 0;

            MAKE_STD_ZVAL(el);
            array_init(el);

			/* lineno */
			add_assoc_long(el, "lineno", opline->lineno);
           
            /* opname */
            if (op != NULL) {
                add_assoc_string(el, "opcode", (char *)op->opname, 1);
            } else {
                snprintf(buf, sizeof(buf), "UNKNOWN OPCODE %d", opline->opcode);
                add_assoc_string(el, "opcode", buf, 1);
            }

            /* extended value */
            if ((op->ops & EXT_MASK) == EXT_OPLINE) {
                snprintf(buf, sizeof(buf), "opline(%lu)", opline->extended_value); 
            } else if ((op->ops & EXT_MASK) == EXT_FCALL) {
                snprintf(buf, sizeof(buf), "args(%lu)", opline->extended_value);
            } else if ((op->ops & EXT_MASK) == EXT_ARG) {
                snprintf(buf, sizeof(buf), "arg(%lu)", opline->extended_value);
            } else if ((op->ops & EXT_MASK) == EXT_SEND) {
                strncpy(buf, get_opcode_dsc(opline->extended_value)->opname, sizeof(buf));
            } else if ((op->ops & EXT_MASK) == EXT_CAST) {
                strncpy(buf, extopnames_cast[opline->extended_value], sizeof(buf));
            } else if ((op->ops & EXT_MASK) == EXT_INIT_FCALL) {
                strncpy(buf, extopnames_init_fcall[opline->extended_value], sizeof(buf));
            } else if ((op->ops & EXT_MASK) == EXT_FETCH) {
                strncpy(buf, extopnames_fetch[opline->extended_value], sizeof(buf));
            } else if ((op->ops & EXT_MASK) == EXT_FE) {
                strncpy(buf, extopnames_fe[opline->extended_value], sizeof(buf));
            } else if ((op->ops & EXT_MASK) == EXT_DECLARE) {
                strncpy(buf, extopnames_declare[opline->extended_value], sizeof(buf));
            } else if ((op->ops & EXT_MASK) == EXT_SEND_NOREF) {
                strncpy(buf, extopnames_sendnoref[opline->extended_value], sizeof(buf));
            } else if ((op->ops & EXT_MASK) == EXT_FCLASS) {
                // XXX: hack to prevent segfault 
                snprintf(buf, sizeof(buf), "%s", extopnames_fetch_class[opline->extended_value]);
            } else if ((op->ops & EXT_MASK) == EXT_IFACE) {
                snprintf(buf, sizeof(buf), "interface(%lu)", opline->extended_value);
            } else if ((op->ops & EXT_MASK) == EXT_CLASS) {
                snprintf(buf, sizeof(buf), "$class%u", VAR_NUM(opline->extended_value));
            } else if ((op->ops & EXT_MASK) == EXT_BIT) {
                if (opline->extended_value) {
                    snprintf(buf, sizeof(buf), "true");
                } else {
                    snprintf(buf, sizeof(buf), "false");
                }
            } else if ((op->ops & EXT_MASK) == EXT_ISSET) {
                if (opline->extended_value == ZEND_ISSET) {
                    snprintf(buf, sizeof(buf), "ZEND_ISSET");
                } else if (opline->extended_value == ZEND_ISEMPTY) {
                    snprintf(buf, sizeof(buf), "ZEND_ISEMPTY");
                } else {
					buf[0] = '\0';
                }
#ifdef ZEND_ENGINE_2
            } else if ((op->ops & EXT_MASK) == EXT_ASSIGN) {
                if (opline->extended_value == ZEND_ASSIGN_OBJ) {
                    snprintf(buf, sizeof(buf), "ZEND_ASSIGN_OBJ");
                } else if (opline->extended_value == ZEND_ASSIGN_DIM) {
                    snprintf(buf, sizeof(buf), "ZEND_ASSIGN_DIM");
                } else {
					buf[0] = '\0';
                }
#ifndef ZEND_ENGINE_2_1
            } else if (opline->opcode == ZEND_UNSET_DIM_OBJ) {
                if (opline->extended_value == ZEND_UNSET_DIM) {
                    snprintf(buf, sizeof(buf), "ZEND_UNSET_DIM");
                } else if (opline->extended_value == ZEND_UNSET_OBJ) {
                    snprintf(buf, sizeof(buf), "ZEND_UNSET_OBJ");
                } else {
					buf[0] = '\0';
                }
#endif
#endif
            } else if (opline->extended_value != 0) {
                snprintf(buf, sizeof(buf), "%ld", opline->extended_value);
            } else {
				buf[0] = '\0';
            }
            add_assoc_string(el, "extended_value", buf, 1);

            /* op1 */
            zval_used = 0;
#ifdef ZEND_ENGINE_2_1
            if (opline->op1.op_type == IS_CV) {
                snprintf(buf, sizeof(buf), "$cv%u(%s)", opline->op1.u.var, op_array->vars[opline->op1.u.var].name);
            } else
#endif
            if ((op->ops & OP1_MASK) == OP1_OPLINE) {
                snprintf(buf, sizeof(buf), "opline(%d)", opline->op1.u.opline_num);
#ifdef ZEND_ENGINE_2
            } else if ((op->ops & OP1_MASK) == OP1_JMPADDR) {
                snprintf(buf, sizeof(buf), "opline(%u)", (unsigned int)(opline->op1.u.jmp_addr - op_array->opcodes));
            } else if ((op->ops & OP1_MASK) == OP1_CLASS) {
                snprintf(buf, sizeof(buf), "$class%u", VAR_NUM(opline->op1.u.var));
            } else if ((op->ops & OP1_MASK) == OP1_UCLASS) {
                if (opline->op1.op_type == IS_UNUSED) {
					buf[0] = '\0';
                } else {
                    snprintf(buf, sizeof(buf), "$class%u", VAR_NUM(opline->op1.u.var));
                }
#endif
            } else if ((op->ops & OP1_MASK) == OP1_BRK) {
                if (opline->op1.u.opline_num != -1 && opline->op2.op_type == IS_CONST && opline->op2.u.constant.type == IS_LONG) {
                    int level = opline->op2.u.constant.value.lval;
                    zend_uint offset = opline->op1.u.opline_num;
                    zend_brk_cont_element *jmp_to;
                    do {
                        if (offset >= op_array->last_brk_cont) {
                            goto brk_failed;
                        }
						jmp_to = &op_array->brk_cont_array[offset];
                        offset = jmp_to->parent;
                    } while (--level > 0);
                    snprintf(buf, sizeof(buf), "opline(%d)", jmp_to->brk);
                } else {
brk_failed:
                    snprintf(buf, sizeof(buf), "brk_cont(%u)", opline->op1.u.opline_num);
                }
            } else if ((op->ops & OP1_MASK) == OP1_CONT) {
                if (opline->op1.u.opline_num != -1 && opline->op2.op_type == IS_CONST && opline->op2.u.constant.type == IS_LONG) {
                    int level = opline->op2.u.constant.value.lval;
                    zend_uint offset = opline->op1.u.opline_num;
                    zend_brk_cont_element *jmp_to;
                    do {
                        if (offset >= op_array->last_brk_cont) {
                            goto cont_failed;
                        }
                        jmp_to = &op_array->brk_cont_array[offset];
                        offset = jmp_to->parent;
                    } while (--level > 0);
                    snprintf(buf, sizeof(buf), "opline(%d)", jmp_to->cont);
                } else {
cont_failed:
                    snprintf(buf, sizeof(buf), "brk_cont(%u)", opline->op1.u.opline_num);
                }
            } else if ((op->ops & OP1_MASK) == OP1_ARG) {
                snprintf(buf, sizeof(buf), "arg(%ld)", opline->op1.u.constant.value.lval);
            } else if ((op->ops & OP1_MASK) == OP1_VAR) {
                snprintf(buf, sizeof(buf), "$var%u", VAR_NUM(opline->op1.u.var));
            } else if ((op->ops & OP1_MASK) == OP1_TMP) {
                snprintf(buf, sizeof(buf), "$tmp%u", VAR_NUM(opline->op1.u.var));
            } else {
                if (opline->op1.op_type == IS_CONST) {
                    zval_used = 1;
                    add_assoc_string(el, "op1", get_zval(&opline->op1.u.constant), 0);
                } else if (opline->op1.op_type == IS_TMP_VAR) {
                    snprintf(buf, sizeof(buf), "$tmp%u", VAR_NUM(opline->op1.u.var));
                } else if (opline->op1.op_type == IS_VAR) {
                    snprintf(buf, sizeof(buf), "$var%u", VAR_NUM(opline->op1.u.var));
                } else if (opline->op1.op_type == IS_UNUSED) {
					buf[0] = '\0';
                } else {
                    snprintf(buf, sizeof(buf), "UNKNOWN NODE %d", opline->op1.op_type);
                }
            }
            if (zval_used == 0) {
                add_assoc_string(el, "op1", buf, 1);
            }

            /* op2 */
            zval_used = 0;
#ifdef ZEND_ENGINE_2_1
            if (opline->op2.op_type == IS_CV) {
                snprintf(buf, sizeof(buf), "$cv%u(%s)", opline->op2.u.var, op_array->vars[opline->op2.u.var].name);
            } else 
#endif
			if ((op->ops & OP2_MASK) == OP2_OPLINE) {
				snprintf(buf, sizeof(buf), "opline(%d)", opline->op2.u.opline_num);
#ifdef ZEND_ENGINE_2
			} else if ((op->ops & OP2_MASK) == OP2_JMPADDR) {
				snprintf(buf, sizeof(buf), "opline(%u)", (unsigned int) (opline->op2.u.jmp_addr - op_array->opcodes));
			} else if ((op->ops & OP2_MASK) == OP2_CLASS) {
				snprintf(buf, sizeof(buf), "$class%u", VAR_NUM(opline->op2.u.var));
#endif
			} else if ((op->ops & OP2_MASK) == OP2_VAR) {
				snprintf(buf, sizeof(buf), "$var%u", VAR_NUM(opline->op2.u.var));
			} else if ((op->ops & OP2_MASK) == OP2_FETCH) {
#ifdef ZEND_ENGINE_2
				if (opline->op2.u.EA.type == ZEND_FETCH_STATIC_MEMBER) {
					snprintf(buf, sizeof(buf), "%s $class%u", fetchtypename[opline->op2.u.EA.type], VAR_NUM(opline->op2.u.var));
				} else {
					snprintf(buf, sizeof(buf), "%s", fetchtypename[opline->op2.u.EA.type]);
				}
#else
				strncpy(buf, fetchtypename[opline->op2.u.fetch_type], sizeof(buf));
#endif
			} else if ((op->ops & OP2_MASK) == OP2_INCLUDE) {
				if (opline->op2.u.constant.value.lval == ZEND_EVAL) {
					snprintf(buf, sizeof(buf), "ZEND_EVAL");
				} else if (opline->op2.u.constant.value.lval == ZEND_INCLUDE) {
                    snprintf(buf, sizeof(buf), "ZEND_INCLUDE");
				} else if (opline->op2.u.constant.value.lval == ZEND_INCLUDE_ONCE) {
					snprintf(buf, sizeof(buf), "ZEND_INCLUDE_ONCE");
				} else if (opline->op2.u.constant.value.lval == ZEND_REQUIRE) {
				    snprintf(buf, sizeof(buf), "ZEND_REQUIRE");
				} else if (opline->op2.u.constant.value.lval == ZEND_REQUIRE_ONCE) {
					snprintf(buf, sizeof(buf), "ZEND_REQUIRE_ONCE");
				} else {
					buf[0] = '\0';
				}
			} else if ((op->ops & OP2_MASK) == OP2_ARG) {
			    snprintf(buf, sizeof(buf), "arg(%u)", opline->op2.u.opline_num);
			} else if ((op->ops & OP2_MASK) == OP2_ISSET) {
				if (opline->op2.u.constant.value.lval == ZEND_ISSET) {
					snprintf(buf, sizeof(buf), "ZEND_ISSET");
				} else if (opline->op2.u.constant.value.lval == ZEND_ISEMPTY) {
					snprintf(buf, sizeof(buf), "ZEND_ISEMPTY");
				} else {
					buf[0] = '\0';
				}
			} else {
                if (opline->op2.op_type == IS_CONST) {
                    zval_used = 1;
                    add_assoc_string(el, "op2", get_zval(&opline->op2.u.constant), 0);
                } else if (opline->op2.op_type == IS_TMP_VAR) {
					snprintf(buf, sizeof(buf), "$tmp%u", VAR_NUM(opline->op2.u.var));
				} else if (opline->op2.op_type == IS_VAR) {
					snprintf(buf, sizeof(buf), "$var%u", VAR_NUM(opline->op2.u.var));
				} else if (opline->op2.op_type == IS_UNUSED) {
					buf[0] = '\0';
				} else {
					snprintf(buf, sizeof(buf), "UNKNOWN NODE %d", opline->op2.op_type);
				}
			}
            if (zval_used == 0) {
                add_assoc_string(el, "op2", buf, 1);
            }

            /* result */
            zval_used = 0;
#ifdef ZEND_ENGINE_2_1
            if (opline->result.op_type == IS_CV) {
                snprintf(buf, sizeof(buf), "$cv%u(%s)", opline->result.u.var, op_array->vars[opline->result.u.var].name);
            } else 
#endif 
			switch (op->ops & RES_MASK) {
			    case RES_STD:
    				if (opline->result.op_type == IS_CONST) {
                        zval_used = 1;
                        add_assoc_string(el, "result", get_zval(&opline->result.u.constant), 0);
    				} else if (opline->result.op_type == IS_TMP_VAR) {
    					snprintf(buf, sizeof(buf), "$tmp%u", VAR_NUM(opline->result.u.var)); 
                    } else if (opline->result.op_type == IS_VAR) {
    					if ((opline->result.u.EA.type & EXT_TYPE_UNUSED) != 0) {
    						snprintf(buf, sizeof(buf), "$var%u (unused)", VAR_NUM(opline->result.u.var));
                        } else {
    						snprintf(buf, sizeof(buf), "$var%u", VAR_NUM(opline->result.u.var));
                        }
    				} else if (opline->result.op_type == IS_UNUSED) {
						buf[0] = '\0';
    				} else {
    					snprintf(buf, sizeof(buf), "UNKNOWN NODE %d", opline->result.op_type);
    				}
    				break;
	    		case RES_CLASS:
    				snprintf(buf, sizeof(buf), "$class%u", VAR_NUM(opline->result.u.var));
    				break;
    			case RES_TMP:
    				snprintf(buf, sizeof(buf), "$tmp%u", VAR_NUM(opline->result.u.var));
    				break;
    			case RES_VAR:
    				if ((opline->result.u.EA.type & EXT_TYPE_UNUSED) != 0) {
    					snprintf(buf, sizeof(buf), "$var%u (unused)", VAR_NUM(opline->result.u.var));
    				} else {
    					snprintf(buf, sizeof(buf), "$var%u", VAR_NUM(opline->result.u.var));
    				}
    				break;
    			case RES_UNUSED:
					buf[0] = '\0';
    				break;
    			default:
    				snprintf(buf, sizeof(buf), "UNKNOWN NODE %d", opline->result.op_type);
    				break;
			}
            if (zval_used == 0) {
                add_assoc_string(el, "result", buf, 1);
            }

            add_next_index_zval(return_value, el);
        }
    }
    return return_value;
}
/* }}} */

/* {{{ get_cache_entry: get the cache_entry for the given file */
static ea_cache_entry *get_cache_entry(const char *file) {
    unsigned int slot;
    ea_cache_entry *p;
    ea_cache_entry *result = NULL;
    
   	if (file != NULL) {
		EACCELERATOR_UNPROTECT();
		EACCELERATOR_LOCK_RD();
		EACCELERATOR_PROTECT();
		for (slot = 0; slot < EA_HASH_SIZE; slot++) {
			p = eaccelerator_mm_instance->hash[slot];
			while (p != NULL) {
				if (strcmp(p->realfilename, file) == 0) {
					result = p;
				}
				p = p->next;
			}
		}
	    EACCELERATOR_UNPROTECT();
		EACCELERATOR_UNLOCK_RD();
		EACCELERATOR_PROTECT();
    }
    return result;
}
/* }}} */

/* {{{ PHP_FUNCTION(eaccelerator_dasm_file): get the op_arrays from a file that's cached
 * This function will return an array with this elements:
 * array() {
 *      [op_array] => array() // the op_array
 *      [classes]  => array() { // an array with classes
 *          [class1] => array() { // an array with class methods
 *                  [method1] => array() // the op_array of that method
 *              }
 *          }
 *      [functions]=> array() {
 *          [function1] => array() // the op_array of that function
 *      }
 */
PHP_FUNCTION(eaccelerator_dasm_file)
{
    const char *file;
    int file_len;
	ea_cache_entry *p;
    ea_fc_entry *fc;
    zval *functions;
    zval *classes;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &file, &file_len) == FAILURE)
		return;

	if (php_check_open_basedir(file TSRMLS_CC)) {
		RETURN_NULL();
	}
    
    p = get_cache_entry(file);
    if (p == NULL) {
        RETURN_NULL();
    }
  
    array_init(return_value);
   
    /* file op_array */
    add_assoc_zval(return_value, "op_array", get_op_array(p->op_array TSRMLS_CC));

    /* file functions */
	fc = p->f_head;
    MAKE_STD_ZVAL(functions);
    array_init(functions);
	while (fc != NULL) {
        add_assoc_zval(functions, fc->htabkey, get_op_array((ea_op_array *)fc->fc TSRMLS_CC));
		fc = fc->next;
	}
    add_assoc_zval(return_value, "functions", functions);

    /* file classes */
	fc = p->c_head;
    MAKE_STD_ZVAL(classes);
    array_init(classes);
	if (fc != NULL) {
		while (fc != NULL) {
			ea_class_entry *c = (ea_class_entry *) fc->fc;
            if (c->type == ZEND_USER_CLASS) { /* get methods */
                zval *methods;
                Bucket *q;

                MAKE_STD_ZVAL(methods);
                array_init(methods);
                q = c->function_table.pListHead;
                while (q) {
                    ea_op_array *func = (ea_op_array *) q->pData;
                    if (func->type == ZEND_USER_FUNCTION) {
                        add_assoc_zval(methods, func->function_name, get_op_array(func TSRMLS_CC));
                    }
                    q = q->pListNext;
                }
                add_assoc_zval(classes, c->name, methods);
            }
            fc = fc->next;
		}
	}
    add_assoc_zval(return_value, "classes", classes); 
}
/* }}} */

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

