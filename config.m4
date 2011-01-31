AC_DEFUN([EA_REMOVE_IPC_TEST], [
  # for cygwin ipc error
  if test -f conftest* ; then
    echo $ECHO_N "Wait for conftest* to exit$ECHO_C"
    while ! rm -f conftest* 2>/dev/null ; do
      echo $ECHO_N ".$ECHO_C"
      sleep 1
    done
    echo
  fi
])

PHP_ARG_ENABLE(eaccelerator, whether to enable eaccelerator support,
[  --enable-eaccelerator                    Enable eAccelerator support])

PHP_ARG_ENABLE(eaccelerator-crash-detection, whether to enable eAccelerator crash detection,
[  --disable-eaccelerator-crash-detection   Do not include eaccelerator crash detection], yes, no)

PHP_ARG_ENABLE(eaccelerator-optimizer, whether to enable eAccelerator optimizer,
[  --disable-eaccelerator-optimizer         Do not include eaccelerator optimizer], yes, no)

PHP_ARG_ENABLE(eaccelerator-disassembler, whether to include eAccelerator disassembler,
[  --enable-eaccelerator-disassembler       Include eaccelerator disassembler], no, no)

PHP_ARG_ENABLE(eaccelerator-debug, whether to enable eAccelerator debug code,
[  --enable-eaccelerator-debug              Enable the debug code so eaccelerator logs verbose.], no, no)

PHP_ARG_WITH(eaccelerator-userid, for eAccelerator sysvipc user id,
[  --with-eaccelerator-userid               eAccelerator runs under this userid, only needed when using sysvipc semaphores.], 0, no)

PHP_ARG_ENABLE(eaccelerator-doc-comment-inclusion, whether eAccelerator should retain doc comments,
[  --enable-eaccelerator-doc-comment-inclusion  If you want eAccelerator to retain doc-comments in  internal php structures.], no, no)

default_shm_type="default (sysvipc)"
PHP_ARG_WITH(eaccelerator-shm-type, for eAccelerator shared memory type,
[  --with-eaccelerator-shm-type             eAccelerator shared memory type: sysvipc, mmap_anon, mmap_zero, mmap_file], [$default_shm_type], no)

default_sem_type="default (rwlock)"
PHP_ARG_WITH(eaccelerator-sem-type, for eAccelerator semaphores type,
[  --with-eaccelerator-sem-type             eAccelerator semaphores type: rwlock, spinlock, sysvipc, fcntl, flock, pthread, posix], [$default_sem_type], no)

dnl PHP_BUILD_SHARED
if test "$PHP_EACCELERATOR" != "no"; then
  AC_CHECK_FUNC(mprotect,[
      AC_DEFINE(HAVE_MPROTECT, 1, [Define if you have mprotect function])
  ])

  eac_sources="eaccelerator.c ea_info.c ea_restore.c ea_store.c mm.c fnmatch.c opcodes.c"
  
  if test "$PHP_EACCELERATOR_DOC_COMMENT_INCLUSION" = "yes"; then
    AC_DEFINE(INCLUDE_DOC_COMMENTS, 1, [If you want eAccelerator to retain doc-comments in internal php structures (meta-programming)])
  fi
  if test "$PHP_EACCELERATOR_CRASH_DETECTION" = "yes"; then
    AC_DEFINE(WITH_EACCELERATOR_CRASH_DETECTION, 1, [Define if you like to release eAccelerator resources on PHP crash])
  fi
  if test "$PHP_EACCELERATOR_OPTIMIZER" = "yes"; then
    eac_sources="$eac_sources optimize.c"
    AC_DEFINE(WITH_EACCELERATOR_OPTIMIZER, 1, [Define if you like to use peephole opcode optimization])
  fi
  if test "$PHP_EACCELERATOR_DISASSEMBLER" = "yes"; then
    eac_sources="$eac_sources ea_dasm.c"
    AC_DEFINE(WITH_EACCELERATOR_DISASSEMBLER, 1, [Define if you like to explore Zend bytecode])
  fi
  if test "$PHP_EACCELERATOR_DEBUG" = "yes"; then
    AC_DEFINE(DEBUG, 1, [Define if you want to enable eaccelerator debug code])
  fi

  PHP_NEW_EXTENSION(eaccelerator, $eac_sources, $ext_shared)
  AC_DEFINE(HAVE_EACCELERATOR, 1, [Define if you like to use eAccelerator])

dnl
dnl Do some tests for OS support
dnl

  AC_HAVE_HEADERS(unistd.h limits.h sys/param.h sched.h)

  AC_MSG_CHECKING(mandatory system headers)
  AC_TRY_CPP([#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>],msg=yes,msg=no)
  AC_MSG_RESULT([$msg])

dnl Test for union semun
  AC_MSG_CHECKING(whether union semun is defined in sys/sem.h)
  AC_TRY_COMPILE([
  #include <sys/types.h>
  #include <sys/ipc.h>
  #include <sys/sem.h>
  ],[
  union semun arg;
  semctl(0, 0, 0, arg);
  ],
  AC_DEFINE(HAVE_UNION_SEMUN, 1, [Define if you have semun union in sys/sem.h])
  msg=yes,msg=no)
  AC_MSG_RESULT([$msg])

  mm_shm_sysvipc=no
  mm_shm_mmap_anon=no
  mm_shm_mmap_zero=no
  mm_shm_mmap_file=no
  mm_shm_mmap_posix=no

dnl sysvipc shared memory
  AC_MSG_CHECKING(for sysvipc shared memory support)
  AC_TRY_RUN([#define MM_SEM_NONE
#define MM_SHM_IPC
#define MM_TEST_SHM
#include "$ext_srcdir/mm.c"
],dnl
    mm_shm_sysvipc=yes
    msg=yes,msg=no,msg=no)
  AC_MSG_RESULT([$msg])
  EA_REMOVE_IPC_TEST()

dnl mmap shared memory
  AC_MSG_CHECKING(for mmap shared memory support)
  AC_TRY_RUN([#define MM_SEM_NONE
#define MM_SHM_MMAP_FILE
#define MM_TEST_SHM
#include "$ext_srcdir/mm.c"
],dnl
    mm_shm_mmap_file=yes
    msg=yes,msg=no,msg=no)
  AC_MSG_RESULT([$msg])

dnl mmap zero shared memory
  AC_MSG_CHECKING(for mmap on /dev/zero shared memory support)
  AC_TRY_RUN([#define MM_SEM_NONE
#define MM_SHM_MMAP_ZERO
#define MM_TEST_SHM
#include "$ext_srcdir/mm.c"
],dnl
    mm_shm_mmap_zero=yes
    msg=yes,msg=no,msg=no)
  AC_MSG_RESULT([$msg])

dnl mmap anonymous shared memory
  AC_MSG_CHECKING(for anonymous mmap shared memory support)
  AC_TRY_RUN([#define MM_SEM_NONE
#define MM_SHM_MMAP_ANON
#define MM_TEST_SHM
#include "$ext_srcdir/mm.c"
],dnl
    mm_shm_mmap_anon=yes
    msg=yes,msg=no,msg=no)
  AC_MSG_RESULT([$msg])

dnl posix mmap shared memory support
  AC_MSG_CHECKING(for posix mmap shared memory support)
  AC_TRY_RUN([#define MM_SEM_NONE
#define MM_SHM_MMAP_POSIX
#define MM_TEST_SHM
#include "$ext_srcdir/mm.c"
],dnl
    mm_shm_mmap_posix=yes
    msg=yes,msg=no,msg=no)
  AC_MSG_RESULT([$msg])


  shm_name_sysvipc="sysvipc"
  shm_name_mmap_anon="anonymous mmap"
  shm_name_mmap_zero="mmap on /dev/zero"
  shm_name_mmap_posix="posix mmap"
  shm_name_mmap_file="mmap on temporary file"

dnl determine the best type
  if test "$PHP_EACCELERATOR_SHM_TYPE" = "$default_shm_type"; then
    AC_MSG_CHECKING(for best shared memory type)
    dnl shm types in the order or preference 
    if test "$mm_shm_sysvipc" = "yes"; then
      shm_type="sysvipc"
    elif test "$mm_shm_mmap_anon" = "yes"; then
      shm_type="mmap_anon"
    elif test "$mm_shm_mmap_zero" = "yes"; then
      shm_type="mmap_zero"
    elif test "$mm_shm_mmap_posix" = "yes"; then
      shm_type="mmap_posix"
    elif test "$mm_shm_mmap_file" = "yes"; then
      shm_type="mmap_file"
    else
      shm_type="unknown"
    fi
   
    if test "$shm_type" = "unknown" ; then
      AC_MSG_ERROR([could not detect shared memory type])
    fi

    namevar="shm_name_$shm_type"
    shm_name=`eval echo \\$$namevar`
    AC_MSG_RESULT([$shm_name])
  else
    varname="mm_shm_$PHP_EACCELERATOR_SHM_TYPE"
    varvalue=`eval echo \\$$varname`
    
    AC_MSG_CHECKING(for shared memory type)
    if test "$varvalue" = "yes"; then
      namevar="shm_name_$PHP_EACCELERATOR_SHM_TYPE"
      shm_name=`eval echo \\$$namevar`
      shm_type=$PHP_EACCELERATOR_SHM_TYPE
      AC_MSG_RESULT([$shm_name])
    else
      AC_MSG_ERROR([Shared memory type '$PHP_EACCELERATOR_SHM_TYPE' is not available])
    fi
  fi

  case "$shm_type" in
    sysvipc)
      AC_DEFINE(MM_SHM_IPC, 1, [Define if you like to use sysvipc based shared memory])
      ;;
    mmap_anon)
      AC_DEFINE(MM_SHM_MMAP_ANON, 1, [Define if you like to use anonymous mmap based shared memory])
      ;;
    mmap_zero)
      AC_DEFINE(MM_SHM_MMAP_ZERO, 1, [Define if you like to use mmap on /dev/zero based shared memory])
      ;;
    mmap_posix)
      AC_DEFINE(MM_SHM_MMAP_POSIX, 1, [Define if you like to use posix mmap based shared memory])
      ;;
    mmap_file)
      AC_DEFINE(MM_SHM_MMAP_FILE, 1, [Define if you like to use mmap on temporary file shared memory])
      ;;
  esac

  AC_DEFINE_UNQUOTED(EAC_SHM_TYPE, $shm_type, [eAccelerator shared memory type.]) 

dnl spinlock test
  AC_MSG_CHECKING(for spinlock semaphores support)
  AC_TRY_RUN([#define MM_SEM_SPINLOCK
#define MM_TEST_SEM
#include "$ext_srcdir/mm.c"
],dnl
    mm_sem_spinlock=yes
    msg=yes,msg=no,msg=no)
  AC_MSG_RESULT([$msg])

oldLIBS="$LIBS"
LIBS="-lpthread"
dnl pthread support
  AC_MSG_CHECKING(for pthread semaphores support)
  AC_TRY_RUN([#define MM_SEM_PTHREAD
#define MM_TEST_SEM
#include "$ext_srcdir/mm.c"
],dnl
    mm_sem_pthread=yes
    msg=yes,msg=no,msg=no)
  AC_MSG_RESULT([$msg])

dnl pthread rwlock support
  AC_MSG_CHECKING(for pthread rwlock semaphores support)
  AC_TRY_RUN([#define MM_SEM_RWLOCK
#define MM_TEST_SEM
#include "$ext_srcdir/mm.c"
],dnl
    mm_sem_rwlock=yes
    msg=yes,msg=no,msg=no)
  AC_MSG_RESULT([$msg])

dnl posix semaphore support
  AC_MSG_CHECKING(for posix semaphores support)
  AC_TRY_RUN([#define MM_SEM_POSIX
#define MM_TEST_SEM
#include "$ext_srcdir/mm.c"
],dnl
    mm_sem_posix=yes
    msg=yes,msg=no,msg=no)
  AC_MSG_RESULT([$msg])

LIBS="$oldLIBS"
dnl sysvipc semaphore support
  AC_MSG_CHECKING(for sysvipc semaphores support)
  AC_TRY_RUN([#define MM_SEM_IPC
#define MM_TEST_SEM
#include "$ext_srcdir/mm.c"
],dnl
    mm_sem_sysvipc=yes
    msg=yes,msg=no,msg=no)
  AC_MSG_RESULT([$msg])
  EA_REMOVE_IPC_TEST()

dnl fnctl semaphore support
  AC_MSG_CHECKING(for fcntl semaphores support)
  AC_TRY_RUN([#define MM_SEM_FCNTL
#define MM_TEST_SEM
#include "$ext_srcdir/mm.c"
],dnl
    mm_sem_fcntl=yes
    msg=yes,msg=no,msg=no)
  AC_MSG_RESULT([$msg])

dnl flock semaphore support
  AC_MSG_CHECKING(for flock semaphores support)
  AC_TRY_RUN([#define MM_SEM_FLOCK
#define MM_TEST_SEM
#include "$ext_srcdir/mm.c"
],dnl
    mm_sem_flock=yes
    msg=yes,msg=no,msg=no)
  AC_MSG_RESULT([$msg])

  sem_name_rwlock="pthread rwlock"
  sem_name_spinlock="spinlock"
  sem_name_sysvipc="sysvipc"
  sem_name_fcntl="fcntl"
  sem_name_flock="flock"
  sem_name_pthread="pthread mutex"
  sem_name_posix="posix"

dnl Determine the best type
  if test "$PHP_EACCELERATOR_SEM_TYPE" = "$default_sem_type"; then 
    AC_MSG_CHECKING(for best semaphores type)
    dnl semaphore types in the order of preference
    if test "$mm_sem_rwlock" = "yes"; then
      sem_type="rwlock"
    elif test "$mm_sem_spinlock" = "yes"; then
      sem_type="spinlock"
    elif test "$mm_sem_sysvipc" = "yes"; then
      if test "$PHP_EACCELERATOR_USERID" = "0"; then
        AC_MSG_ERROR("You need to pass the user id eAccelerator will be running under when using sysvipc semaphores")
      else
        sem_type="sysvipc"
      fi
    elif test "$mm_sem_fcntl" = "yes"; then
      sem_type="fcntl"
    elif test "$mm_sem_flock" = "yes"; then
      sem_type="flock"
    elif test "$mm_sem_pthread" = "yes"; then
      sem_type="pthread"
    elif test "$mm_sem_posix" = "yes"; then
      sem_type="posix"
    else
      sem_type="unknown"
    fi

    if test "$sem_type" = "unknown" ; then
      AC_MSG_ERROR([could not detect semaphores type])
    fi
  
    namevar="sem_name_$sem_type"
    sem_name=`eval echo \\$$namevar`
    AC_MSG_RESULT([$sem_name])
  else
    varname="mm_sem_$PHP_EACCELERATOR_SEM_TYPE"
    varvalue=`eval echo \\$$varname`
    
    AC_MSG_CHECKING(for semaphores type)
    if test "$varvalue" = "yes"; then
      namevar="sem_name_$PHP_EACCELERATOR_SEM_TYPE"
      sem_name=`eval echo \\$$namevar`
      sem_type="$PHP_EACCELERATOR_SEM_TYPE"
      AC_MSG_RESULT([$sem_name])
    else
      AC_MSG_ERROR([Semaphores type '$PHP_EACCELERATOR_SEM_TYPE' is not available])
    fi
  fi

  case "$sem_type" in
    rwlock)
      AC_DEFINE(MM_SEM_RWLOCK, 1, [Define if you like to use pthread rwlock based semaphores])
      PHP_EVAL_LIBLINE([-pthread], EACCELERATOR_SHARED_LIBADD)
      ;;
    spinlock)
      AC_DEFINE(MM_SEM_SPINLOCK, 1, [Define if you like to use spinlock based semaphores])
      ;; 
    sysvipc)
      if test "$PHP_EACCELERATOR_USERID" = "0"; then
        AC_MSG_ERROR("You need to pass the user id eAccelerator will be running under when using sysvipc semaphores")
      fi
      AC_DEFINE(MM_SEM_IPC, 1, [Define if you like to use sysvipc based semaphores])
      ;;
    fcntl)
      AC_DEFINE(MM_SEM_FCNTL, 1, [Define if you like to use fcntl based semaphores])
      ;;
    flock)
      AC_DEFINE(MM_SEM_FLOCK, 1, [Define if you like to use flock based semaphores])
      ;;
    pthread)
      AC_DEFINE(MM_SEM_PTHREAD, 1, [Define if you like to use pthread based semaphores])
      PHP_EVAL_LIBLINE([-pthread], EACCELERATOR_SHARED_LIBADD)
      ;;
    posix)
      AC_DEFINE(MM_SEM_POSIX, 1, [Define if you like to use posix based semaphores])
      ;;
  esac

  AC_DEFINE_UNQUOTED(EAC_SEM_TYPE, $sem_type, [eAccelerator semaphores type.]) 
  AC_DEFINE_UNQUOTED(EA_USERID, $PHP_EACCELERATOR_USERID, [The userid eAccelerator will be running under.]) 
  PHP_SUBST(EACCELERATOR_SHARED_LIBADD)
fi

dnl vim:et:sw=2:ts=2:

