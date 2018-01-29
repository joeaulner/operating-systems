/*---------------------------------------------------------------*/
/* A (very) simple shell                                         */
/* Stanley Wileman                                               */
/* Last change: 8/29/2016                                        */
/*                                                               */
/* Process command lines (100 char max) with at most 16 "words"  */
/* and one command. No wildcard or shell variable processing.    */
/* A word on the command line contains at most MAXWORDLEN chars. */
/* Words are expected to contain only printable characters.      */
/* Words are separated by spaces and/or tab characters.          */
/*                                                               */
/* No pipes, conditional or sequential command handling is done. */
/* No redirection of file descriptors is provided.               */
/* No "wildcard" characters (e.g. * and ?) are processed.        */
/* No shell variables are recognized.                            */
/*                                                               */
/* This file is provided to students in CSCI 4500 for their use  */
/* in developing solutions to the first programming assignment.  */
/*---------------------------------------------------------------*/
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>

#define MAXLINELEN 100        /* max chars in an input line */
#define NWORDS 16         /* max words on command line */
#define MAXWORDLEN 64         /* maximum word length */

extern char **environ;        /* environment */

char line[MAXLINELEN+1];      /* input line */
char *words[NWORDS];              /* ptrs to words from the command line */
int nwds;             /* # of words in the command line */
char path[MAXWORDLEN];        /* path to the command */
char *argv[NWORDS+1];         /* argv structure for execve */

/*------------------------------------------------------------------*/
/* Get a line from the standard input. Return 1 on success, or 0 at */
/* end of file. If the line contains only whitespace, ignore it and */
/* get another line. If the line is too long, display a message and */
/* get another line. If read fails, diagnose the error and abort.   */
/*------------------------------------------------------------------*/
/* This function will display a prompt ("# ") if the input comes    */
/* from a terminal. Otherwise no prompt is displayed, but the       */
/* input is echoed to the standard output. If the input is from a   */
/* terminal, the input is automatically echoed (assuming we're in   */
/* "cooked" mode).                                                  */
/*------------------------------------------------------------------*/
/* Students: Make certain you understand the reason the input is    */
/* read one character at a time. It could be read in larger pieces  */
/* but then additional code would be required to guarantee only one */
/* line of input is processed when the standard input (that is,     */
/* file descriptor 1) is redirected to a file.                      */
/*------------------------------------------------------------------*/
int Getline(void)
{
    int n;      /* result of read system call */
    int len;        /* length of input line */
    int gotnb;      /* non-zero when non-whitespace was seen */
    char c;     /* current input character */
    char *msg;      /* error message */
    int isterm;     /* non-zero if input is from a terminal */

    isterm = isatty(0);     /* see if file descriptor 0 is a terminal */
    for(;;) {
        if (isterm)
            write(1,"# ",2);
        gotnb = len = 0;
        for(;;) {

            n = read(0,&c,1);       /* read one character */

            if (n == 0)         /* end of file? */
                return 0;       /* yes, so return 0 */

            if (n == -1) {      /* error reading? */
                perror("Error reading command line");
                exit(1);
            }

            if (!isterm)        /* if input not from a terminal */
                write (1,&c,1);     /* echo the character */

            if (c == '\n')      /* end of line? */
                break;

            if (len >= MAXLINELEN) {    /* too many chars? */
                len++;          /* yes, so just update the length */
                continue;
            }

            if (c != ' ' && c != '\t')  /* was input not whitespace? */
                gotnb = 1;

            line[len++] = c;        /* save the input character */
        }

        if (len >= MAXLINELEN) {    /* if the input line was too long... */
            char *msg;
            msg = "Input line is too long.\n";
            write(2,msg,strlen(msg));
            continue;
        }
        if (gotnb == 0)         /* line contains only whitespace */
            continue;

        line[len] = '\0';       /* terminate the line */
        return 1;
    }
}

/*------------------------------------------------*/
/* Identify "words" and sequencing operators on   */
/* a command line (in the global array 'line').   */
/* Put a pointer to each null-terminated string   */
/* with a word or sequencing operator (that is,   */
/* ";" or "||" or "&&") in the 'words' array, and */
/* store the number of such items in 'nwds'.      */
/*                                                */
/* On success, return 1.                          */
/* If there are too many words, or a word is too  */
/* long, diagnose the error (by writing a message */
/* to the standard error device, file descriptor  */
/* 2) and return 0.                               */
/*                                                */
/* Note that except for space and tab characters, */
/* No characters are explicitly tested in this    */
/* function. We expect that the 'line' variable   */
/* does NOT include an end of line character, but */
/* that it is terminated by a null byte ('\0').   */
/*------------------------------------------------*/
int lex(void)
{
    char *p;            /* pointer to current word */
    char *msg;          /* error message */

    nwds = 0;
    p = strtok(line," \t"); /* get pointer to a word, if any exists */
    while (p != NULL) {
        if (nwds == NWORDS) {
            msg = "*** ERROR: Too many words.\n";
            write(2,msg,strlen(msg));
            return 0;       /* return error indication */
        }
        if (strlen(p) >= MAXWORDLEN) {
            msg = "*** ERROR: Word too long.\n";
            write(2,msg,strlen(msg));
            return 0;       /* return error indication */
        }
        words[nwds] = p;    /* save pointer to the word */
        nwds++;         /* increase the word count */
        p = strtok(NULL," \t"); /* get pointer to next word, if any */
    }
    return 1;           /* success! */
}

/*--------------------------------------------------------------------------*/
/* Put in 'path' the relative or absolute path to the command identified by */
/* words[0]. If the file identified by 'path' is not executable, return -1. */
/* Otherwise return 0.                                                      */
/*--------------------------------------------------------------------------*/
/* Note that 'path' is a global variable. When this function is used, it    */
/* will overwrite any existing contents in that global variable. If there   */
/* is "precious" data in 'path', then it may need to be saved elsewhere.    */
/*--------------------------------------------------------------------------*/
int execok(int w)
{
    char *p;
    char *pathenv;

    /*-------------------------------------------------------*/
    /* If words[0] is already a relative or absolute path... */
    /*-------------------------------------------------------*/
    if (strchr(words[w],'/') != NULL) {     /* if it has no '/' */
        strcpy(path,words[w]);          /* copy it to path */
        return access(path,X_OK);       /* return executable status */
    }

    /*-------------------------------------------------------------------*/
    /* Otherwise search for a valid executable in the PATH directories.  */
    /* We do this by getting a COPY of the value of the PATH environment */
    /* variable, and checking each directory identified there to see it  */
    /* contains an executable file named word[0]. If a directory does    */
    /* have such a file, return 0. Otherwise, return -1. In either case, */
    /* always free the storage allocated for the value of PATH.          */
    /*-------------------------------------------------------------------*/
    /* A copy of the PATH environment variable is created in dynamically */
    /* allocated storage (named 'pathenv'). That is because each process */
    /* is given a single copy of the PATH as a character string. If the  */
    /* process should change that string (as would happen if strtok was  */
    /* used directly on it), then later uses of PATH would get the wrong */
    /* value! Note that the dynamically allocated storage ('pathenv')    */
    /* must be deallocated/freed in all cases. Otherwise the shell will  */
    /* have a "memory leak."                                             */
    /*-------------------------------------------------------------------*/
    pathenv = strdup(getenv("PATH"));       /* get copy of PATH value */
    p = strtok(pathenv,":");            /* find first directory */
    while (p != NULL) {
        strcpy(path,p);             /* copy directory to path */
        strcat(path,"/");           /* append a slash */
        strcat(path,words[w]);          /* append executable's name */
        if (access(path,X_OK) == 0) {       /* if it's executable */
            free(pathenv);              /* free PATH copy */
            return 0;                   /* and return 0 */
        }
        p = strtok(NULL,":");           /* get next directory */
    }
    free(pathenv);              /* free PATH copy */
    return -1;                  /* say we didn't find it */
}

/*--------------------------------------------------*/
/* Execute the command, if possible.                */
/* If it is not executable, return -1.              */
/* Otherwise return 0 if it completed successfully, */
/* or 1 if it did not.                              */
/*--------------------------------------------------*/
/* This is the "meat" of the shell. It creates the  */
/* child process to execute a command, and then it  */
/* replaces the "cloned" shell in the child process */
/*  with the executable program of the new command, */
/* passing it the command line arguments and a copy */
/* of the shell's environment.                      */
/*--------------------------------------------------*/
int execute(void)
{
    int i, j;
    int which, status;
    pid_t pid;
    char msg[100];
    char *cmd[NWORDS];
    char *w;

    i = 0;
    while (i < nwds) {
        if (execok(i) == 0) {            /* is it executable? */
            j = 0;
            for (;;) {                   /* yes; obtain arguments for command */
                if (i >= nwds) {
                    cmd[j] = NULL;
                    break;
                }

                w = words[i];
                if (strcmp(w, ";") == 0) {
                    cmd[j] = NULL;
                    break;
                } else if (strcmp(w, "&&") == 0) {
                    cmd[j] = NULL;
                    break;
                } else if (strcmp(w, "||") == 0) {
                    cmd[j] = NULL;
                    break;
                }
                cmd[j] = w;
                i++;
                j++;
            }

            pid = fork();            /* create a new process */

            if (pid == -1) {         /* verify fork succeeded */
                perror("fork");
                exit(1);
            } else if (pid == 0) {
                status = execve(path,cmd,environ); /* try to execute it */

                perror("execve");           /* we only get here if */
                exit(0);                /* execve failed... */
            } else {
                which = wait(&status);              /* wait for process to end */

                if (which == -1) {
                    write(2,"Wait failed.\n",12);
                    exit(1);
                }
            }
        } else {
            /*----------------------------------------------------------*/
            /* Command cannot be executed. Display appropriate message. */
            /*----------------------------------------------------------*/
            sprintf(msg,"*** ERROR: '%s' cannot be executed.\n", words[i]);
            write(2,msg,strlen(msg));
            break;
        }

        if (status == 0) {
            if (strcmp(w, ";") == 0 || strcmp(w, "&&") == 0) {
                i++;
            } else {
                break;
            }
        } else {
            if (strcmp(w, ";") == 0 || strcmp(w, "||") == 0) {
                i++;
            } else {
                break;
            }
        }
    }
}

/*---------------------------------------------------------------*/
/* Execution effectively always begins with the 'main' function. */
/* Somewhat obviously (and hopefully simply) is repeatedly gets  */
/* a command line, does lexical analysis on it to obtain the     */
/* words and sequencing operators, and then executes it.         */
/*                                                               */
/* This repeats until there is no more input to the shell.       */
/*---------------------------------------------------------------*/
int main(int argc, char *argv[])
{
    while (Getline()) {         /* get command line */
        if (!lex())         /* do lexical analysis to get words */
            continue;               /* some problem, so ignore it */
        execute();          /* execute the command */
    }
    write(1,"\n",1);            /* display an end of line. */
    return 0;               /* successful shell termination */
}