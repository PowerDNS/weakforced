AC_DEFUN([WFORCE_CHECK_READLINE], [
  AC_CHECK_HEADER([readline/readline.h], [ : ], [ AC_MSG_ERROR([readline.h not found]) ])
])

