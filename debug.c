/*
   +----------------------------------------------------------------------+
   | eAccelerator project                                                 |
   +----------------------------------------------------------------------+
   | Copyright (c) 2004 - 2005 eAccelerator                               |
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
   | Author(s): Dmitry Stogov <dstogov@users.sourceforge.net>             |
   |            Bart Vanbrabant <zoeloelip@users.sourceforge.net>         |
   +----------------------------------------------------------------------+
   $Id$
*/

#include "eaccelerator.h"

#ifdef HAVE_EACCELERATOR

#include "debug.h"
#include <ctype.h>
#include <stdio.h>

FILE *F_fp = NULL;
long eaccelerator_debug = 0;

/**
 * Init the debug system. This must be called before any debug
 * functions are used.
 */
void ea_debug_init (TSRMLS_D)
{
    /* register ini entries */
    REGISTER_MAIN_LONG_CONSTANT ("EA_LOG", EA_LOG, CONST_PERSISTENT | CONST_CS);
    REGISTER_MAIN_LONG_CONSTANT ("EA_DEBUG", EA_DEBUG, CONST_PERSISTENT | CONST_CS);
    REGISTER_MAIN_LONG_CONSTANT ("EA_PROFILE_OPCODES", EA_PROFILE_OPCODES,
                            CONST_PERSISTENT | CONST_CS);
    REGISTER_MAIN_LONG_CONSTANT ("EA_TEST_PERFORMANCE", EA_TEST_PERFORMANCE,
                            CONST_PERSISTENT | CONST_CS);
    REGISTER_MAIN_LONG_CONSTANT ("EA_LOG_HASHKEYS", EA_LOG_HASHKEYS,
                            CONST_PERSISTENT | CONST_CS);

    F_fp = fopen (MMCG (eaccelerator_log_file), "a");
    if (!F_fp)
        F_fp = stderr;
}

/**
 * Close the debug system.
 */
void ea_debug_shutdown ()
{
    fflush (F_fp);
    fclose (F_fp);
    F_fp = NULL;
}

/**
 * Print a log message that will be print when the debug level is
 * equal to EA_LOG. This function is always called even if ea isn't
 * compiled with DEBUG and the log level is not equal to EA_LOG.
 */
void ea_debug_log (char *format, ...)
{
    if (eaccelerator_debug & EA_LOG) {
        char output_buf[512];
        va_list args;

        va_start (args, format);
        vsnprintf (output_buf, sizeof (output_buf), format, args);
        va_end (args);

#ifdef ZEND_WIN32
        OutputDebugString (*output_buf);
#else
        fputs (output_buf, F_fp);
        fflush (F_fp);
#endif
    }
}

/**
 * Output an error message to stderr. This message are always printed
 * no matter what log level is used.
 */
void ea_debug_error (char *format, ...)
{
    char output_buf[512];
    va_list args;

    va_start (args, format);
    vsnprintf (output_buf, sizeof (output_buf), format, args);
    va_end (args);

#ifdef ZEND_WIN32
    OutputDebugString (*buffer);
#else
    fputs (output_buf, stderr);
    fflush (stderr);
#endif
}

/* 
 * All these functions aren't compiled when eA isn't compiled with DEBUG. They
 * are replaced with function with no body, so it's optimized away by the compiler.
 * Even if the debug level is ok.
 */

/**
 * Print a debug message
 */
#ifdef DEBUG
void ea_debug_printf (long debug_level, char *format, ...)
{
    if (eaccelerator_debug & debug_level) {
        char output_buf[512];
        va_list args;

        va_start (args, format);
        vsnprintf (output_buf, sizeof (output_buf), format, args);
        va_end (args);

        fputs (output_buf, F_fp);
        fflush (F_fp);
    }
}
#else
void ea_debug_printf (long debug_level, char *format, ...)
{
}
#endif

/**
 * Put a debug message
 */
#ifdef DEBUG
void ea_debug_put (long debug_level, char *message)
{
    if (debug_level & eaccelerator_debug) {
        fputs (message, F_fp);
        fflush (F_fp);
    }
}
#else
void ea_debug_put (long debug_level, char *message)
{
}
#endif

/**
 * Print a binary message
 */
#ifdef DEBUG
void ea_debug_binary_print (long debug_level, char *p, int len)
{
    if (eaccelerator_debug & debug_level) {
        while (len--) {
            fputc (*p++, F_fp);
        }
        fputc ('\n', F_fp);
        fflush (F_fp);
    }
}
#else
void ea_debug_binary_print (long debug_level, char *p, int len)
{
}
#endif

/**
 * Log a hashkey
 */
#ifdef DEBUG
void ea_debug_log_hashkeys (char *p, HashTable * ht)
{
    if (eaccelerator_debug & EA_LOG_HASHKEYS) {
        Bucket *b;
        int i = 0;

        b = ht->pListHead;

        fputs (p, F_fp);
        while (b) {
            fprintf (F_fp, "[%d] ", i);
            ea_debug_binary_print (EA_LOG_HASHKEYS, b->arKey, b->nKeyLength);

            b = b->pListNext;
            i++;
        }
        fflush (F_fp);
    }
}
#else
void ea_debug_log_hashkeys (char *p, HashTable * ht)
{
}
#endif

/**
 * Pad the message with the current pad level.
 */
#ifdef DEBUG
void ea_debug_pad (long debug_level TSRMLS_DC)
{
    if (eaccelerator_debug & debug_level) {
        int i = MMCG (xpad);
        while (i-- > 0) {
            fputc ('\t', F_fp);
        }
    }
}
#else
void ea_debug_pad (long debug_level TSRMLS_DC)
{
}
#endif

void ea_debug_start_time (struct timeval *tvstart)
{
    gettimeofday (tvstart, NULL);
}

long ea_debug_elapsed_time (struct timeval *tvstart)
{
    struct timeval tvend;
    int sec, usec;
    gettimeofday (&tvend, NULL);
    sec = tvend.tv_sec - tvstart->tv_sec;
    usec = tvend.tv_usec - tvstart->tv_usec;
    return sec * 1000000 + usec;
}

#endif /* #ifdef HAVE_EACCELERATOR */
