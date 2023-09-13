/****************************************************************************
 *                                                                          *
 * File    : spawnle.c                                                      *
 *                                                                          *
 * Purpose : _spawnle function -- win32 version.                            *
 *                                                                          *
 * History : Date      Reason                                               *
 *           01-04-05  Created                                              *
 *                                                                          *
 ****************************************************************************/

#include <process.h>

/* spawn a child process with env */
int __cdecl (_spawnle)(int mode, const char *cmd, const char *arg0, ...)
{
    const char **argp;

    /*
     * Walk the arglist until the terminating NULL pointer is found.
     * The next location holds the environment table pointer.
     */
    for (argp = &arg0; *argp++; )
        ;

    return _spawnve(mode, cmd, (char * const *)&arg0, (char * const *)*argp);
}
