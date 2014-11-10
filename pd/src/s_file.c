/* Copyright (c) 1997-2004 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/*
 * this file implements a mechanism for storing and retrieving preferences.
 * Should later be renamed "preferences.c" or something.
 *
 * In unix this is handled by the "~/.pd-l2ork/user.settings" file, in windows by
 * the registry, and in MacOS by the Preferences system.
 */

#include "config.h"

#include "m_pd.h"
#include "s_stuff.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_UNISTD_H
/* XXX Hack!  This should be done with a cleaner check. */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#ifdef MSW
#include <windows.h>
#include <tchar.h>
#endif
#ifdef _MSC_VER  /* This is only for Microsoft's compiler, not cygwin, e.g. */
#define snprintf sprintf_s
#endif

int sys_defeatrt;
t_symbol *sys_flags = &s_;
void sys_doflags( void);

#ifdef UNIX

static char *sys_prefbuf;
static int sys_prefbufsize;

static void sys_initloadpreferences( void)
{
    char filenamebuf[FILENAME_MAX], *homedir = getenv("HOME");
    int fd, length;
    char user_prefs_file[FILENAME_MAX]; /* user prefs file */
        /* default prefs embedded in the package */
    char default_prefs_file[FILENAME_MAX];
    struct stat statbuf;

    snprintf(default_prefs_file, FILENAME_MAX, "%s/default.settings", 
             sys_libdir->s_name);

    if (homedir)
        snprintf(user_prefs_file, FILENAME_MAX, "%s/.pd-l2ork/user.settings", homedir);
    if (stat(user_prefs_file, &statbuf) == 0) 
        strncpy(filenamebuf, user_prefs_file, FILENAME_MAX);
    else if (stat(default_prefs_file, &statbuf) == 0)
        strncpy(filenamebuf, default_prefs_file, FILENAME_MAX);
    else
        return;
    filenamebuf[FILENAME_MAX-1] = 0;
    if ((fd = open(filenamebuf, 0)) < 0)
    {
        if (sys_verbose)
            perror(filenamebuf);
        return;
    }
    length = lseek(fd, 0, 2);
    if (length < 0)
    {
        if (sys_verbose)
            perror(filenamebuf);
        close(fd);
        return;
    }
    lseek(fd, 0, 0);
    if (!(sys_prefbuf = malloc(length + 2)))
    {
        error("couldn't allocate memory for preferences buffer");
        close(fd);
        return;
    }
    sys_prefbuf[0] = '\n';
    if (read(fd, sys_prefbuf+1, length) < length)
    {
        perror(filenamebuf);
        sys_prefbuf[0] = 0;
        close(fd);
        return;
    }
    sys_prefbuf[length+1] = 0;
    close(fd);
    if (sys_verbose)
        post("success reading preferences from: %s", filenamebuf);
}

static int sys_getpreference(const char *key, char *value, int size)
{
    char searchfor[80], *where, *whereend;
    if (!sys_prefbuf)
        return (0);
    sprintf(searchfor, "\n%s:", key);
    where = strstr(sys_prefbuf, searchfor);
    if (!where)
        return (0);
    where += strlen(searchfor);
    while (*where == ' ' || *where == '\t')
        where++;
    for (whereend = where; *whereend && *whereend != '\n'; whereend++)
        ;
    if (*whereend == '\n')
        whereend--;
    if (whereend > where + size - 1)
        whereend = where + size - 1;
    strncpy(value, where, whereend+1-where);
    value[whereend+1-where] = 0;
    return (1);
}

static void sys_doneloadpreferences( void)
{
    if (sys_prefbuf)
        free(sys_prefbuf);
}

static FILE *sys_prefsavefp;

static void sys_initsavepreferences( void)
{
    char filenamebuf[FILENAME_MAX], *homedir = getenv("HOME");
    FILE *fp;

    if (!homedir)
        return;
    snprintf(filenamebuf, FILENAME_MAX, "%s/.pd-l2ork/user.settings", homedir);
    filenamebuf[FILENAME_MAX-1] = 0;
    if ((sys_prefsavefp = fopen(filenamebuf, "w")) == NULL)
    {
        //snprintf(errbuf, FILENAME_MAX, "%s: %s",filenamebuf, strerror(errno));
        pd_error(0, "%s: %s",filenamebuf, strerror(errno));
    }
}

static void sys_putpreference(const char *key, const char *value)
{
    if (sys_prefsavefp)
        fprintf(sys_prefsavefp, "%s: %s\n",
            key, value);
}

static void sys_donesavepreferences( void)
{
    if (sys_prefsavefp)
    {
        fclose(sys_prefsavefp);
        sys_prefsavefp = 0;
    }
}

#endif /* UNIX */

#ifdef MSW

static void sys_initloadpreferences( void)
{
}

static int sys_getpreference(const char *key, char *value, int size)
{
    HKEY hkey;
    DWORD bigsize = size;
    LONG err = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
        "Software\\Pd-extended", 0,  KEY_QUERY_VALUE, &hkey);
    if (err != ERROR_SUCCESS)
    {
        return (0);
    }
    err = RegQueryValueEx(hkey, key, 0, 0, value, &bigsize);
    if (err != ERROR_SUCCESS)
    {
        RegCloseKey(hkey);
        return (0);
    }
    RegCloseKey(hkey);
    return (1);
}

static void sys_doneloadpreferences( void)
{
}

static void sys_initsavepreferences( void)
{
}

static void sys_putpreference(const char *key, const char *value)
{
    HKEY hkey;
    LONG err = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
        "Software\\Pd-extended", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE,
        NULL, &hkey, NULL);
    if (err != ERROR_SUCCESS)
    {
        post("unable to create registry entry: %s\n", key);
        return;
    }
    err = RegSetValueEx(hkey, key, 0, REG_EXPAND_SZ, value, strlen(value)+1);
    if (err != ERROR_SUCCESS)
        post("unable to set registry entry: %s\n", key);
    RegCloseKey(hkey);
}

static void sys_donesavepreferences( void)
{
}

#endif /* MSW */

#ifdef __APPLE__

// prefs file that is currently the one to save to
static char current_prefs[FILENAME_MAX] = "org.puredata.pd-l2ork"; 

static void sys_initloadpreferences( void)
{
}

static int sys_getpreference(const char *key, char *value, int size)
{
    char cmdbuf[256];
    int nread = 0, nleft = size;
    char default_prefs[FILENAME_MAX]; // default prefs embedded in the package
    char embedded_prefs[FILENAME_MAX]; // overrides others for standalone app
    char embedded_prefs_file[FILENAME_MAX];
    char user_prefs_file[FILENAME_MAX];
    char *homedir = getenv("HOME");
    struct stat statbuf;
    /* the 'defaults' command expects the filename without .plist at the end */
    snprintf(default_prefs, FILENAME_MAX, "%s/../org.puredata.pd-l2ork.default", 
             sys_libdir->s_name);
    snprintf(embedded_prefs, FILENAME_MAX, "%s/../org.puredata.pd-l2ork", 
             sys_libdir->s_name);
    snprintf(embedded_prefs_file, FILENAME_MAX, "%s.plist", embedded_prefs);
    snprintf(user_prefs_file, FILENAME_MAX, 
             "%s/Library/Preferences/org.puredata.pd-l2ork.plist", homedir);
    if (stat(embedded_prefs_file, &statbuf) == 0) 
    {
        snprintf(cmdbuf, FILENAME_MAX + 20, 
                 "defaults read '%s' %s 2> /dev/null\n", embedded_prefs, key);
        strncpy(current_prefs, embedded_prefs, FILENAME_MAX);
    }
    else if (stat(user_prefs_file, &statbuf) == 0) 
    {
        snprintf(cmdbuf, FILENAME_MAX + 20, 
                 "defaults read org.puredata.pd-l2ork %s 2> /dev/null\n", key);
        strcpy(current_prefs, "org.puredata.pd-l2ork");
    }
    else 
    {
        snprintf(cmdbuf, FILENAME_MAX + 20, 
                 "defaults read '%s' %s 2> /dev/null\n", default_prefs, key);
        strcpy(current_prefs, "org.puredata.pd-l2ork");
    }
    FILE *fp = popen(cmdbuf, "r");
    while (nread < size)
    {
        int newread = fread(value+nread, 1, size-nread, fp);
        if (newread <= 0)
            break;
        nread += newread;
    }
    pclose(fp);
    if (nread < 1)
        return (0);
    if (nread >= size)
        nread = size-1;
    value[nread] = 0;
    if (value[nread-1] == '\n')     /* remove newline character at end */
        value[nread-1] = 0;
    return(1);
}

static void sys_doneloadpreferences( void)
{
}

static void sys_initsavepreferences( void)
{
}

static void sys_putpreference(const char *key, const char *value)
{
    char cmdbuf[MAXPDSTRING];
    snprintf(cmdbuf, MAXPDSTRING, 
             "defaults write '%s' %s \"%s\" 2> /dev/null\n",
             current_prefs, key, value);
    system(cmdbuf);
}

static void sys_donesavepreferences( void)
{
}

#endif /* __APPLE__ */


void sys_loadpreferences( void)
{
    int naudioindev, audioindev[MAXAUDIOINDEV], chindev[MAXAUDIOINDEV];
    int naudiooutdev, audiooutdev[MAXAUDIOOUTDEV], choutdev[MAXAUDIOOUTDEV];
    int nmidiindev, midiindev[MAXMIDIINDEV];
    int nmidioutdev, midioutdev[MAXMIDIOUTDEV];
    int i, rate = 0, advance = -1, callback = 0, blocksize = 0,
        api, nolib, maxi;
    char prefbuf[MAXPDSTRING], keybuf[80];

    sys_initloadpreferences();
        /* load audio preferences */
    if (sys_getpreference("audioapi", prefbuf, MAXPDSTRING)
        && sscanf(prefbuf, "%d", &api) > 0)
            sys_set_audio_api(api);
            /* JMZ/MB: brackets for initializing */
    if (sys_getpreference("noaudioin", prefbuf, MAXPDSTRING) &&
        (!strcmp(prefbuf, ".") || !strcmp(prefbuf, "True")))
            naudioindev = 0;
    else
    {
        for (i = 0, naudioindev = 0; i < MAXAUDIOINDEV; i++)
        {
            sprintf(keybuf, "audioindev%d", i+1);
            if (!sys_getpreference(keybuf, prefbuf, MAXPDSTRING))
                break;
            if (sscanf(prefbuf, "%d %d", &audioindev[i], &chindev[i]) < 2)
                break;
            naudioindev++;
        }
            /* if no preferences at all, set -1 for default behavior */
        if (naudioindev == 0)
            naudioindev = -1;
    }
        /* JMZ/MB: brackets for initializing */
    if (sys_getpreference("noaudioout", prefbuf, MAXPDSTRING) &&
        (!strcmp(prefbuf, ".") || !strcmp(prefbuf, "True")))
            naudiooutdev = 0;
    else
    {
        for (i = 0, naudiooutdev = 0; i < MAXAUDIOOUTDEV; i++)
        {
            sprintf(keybuf, "audiooutdev%d", i+1);
            if (!sys_getpreference(keybuf, prefbuf, MAXPDSTRING))
                break;
            if (sscanf(prefbuf, "%d %d", &audiooutdev[i], &choutdev[i]) < 2)
                break;
            naudiooutdev++;
        }
        if (naudiooutdev == 0)
            naudiooutdev = -1;
    }
    if (sys_getpreference("rate", prefbuf, MAXPDSTRING))
        sscanf(prefbuf, "%d", &rate);
    if (sys_getpreference("audiobuf", prefbuf, MAXPDSTRING))
        sscanf(prefbuf, "%d", &advance);
    if (sys_getpreference("callback", prefbuf, MAXPDSTRING))
        sscanf(prefbuf, "%d", &callback);
    if (sys_getpreference("blocksize", prefbuf, MAXPDSTRING))
        sscanf(prefbuf, "%d", &blocksize);
    sys_set_audio_settings(naudioindev, audioindev, naudioindev, chindev,
        naudiooutdev, audiooutdev, naudiooutdev, choutdev, rate, advance,
        callback, blocksize);
        
        /* load MIDI preferences */
        /* JMZ/MB: brackets for initializing */
    if (sys_getpreference("nomidiin", prefbuf, MAXPDSTRING) &&
        (!strcmp(prefbuf, ".") || !strcmp(prefbuf, "True")))
            nmidiindev = 0;
    else for (i = 0, nmidiindev = 0; i < MAXMIDIINDEV; i++)
    {
        sprintf(keybuf, "midiindev%d", i+1);
        if (!sys_getpreference(keybuf, prefbuf, MAXPDSTRING))
            break;
        if (sscanf(prefbuf, "%d", &midiindev[i]) < 1)
            break;
        nmidiindev++;
    }
        /* JMZ/MB: brackets for initializing */
    if (sys_getpreference("nomidiout", prefbuf, MAXPDSTRING) &&
        (!strcmp(prefbuf, ".") || !strcmp(prefbuf, "True")))
            nmidioutdev = 0;
    else for (i = 0, nmidioutdev = 0; i < MAXMIDIOUTDEV; i++)
    {
        sprintf(keybuf, "midioutdev%d", i+1);
        if (!sys_getpreference(keybuf, prefbuf, MAXPDSTRING))
            break;
        if (sscanf(prefbuf, "%d", &midioutdev[i]) < 1)
            break;
        nmidioutdev++;
    }
    sys_open_midi(nmidiindev, midiindev, nmidioutdev, midioutdev, 0);

        /* search path */
    if (sys_getpreference("npath", prefbuf, MAXPDSTRING))
        sscanf(prefbuf, "%d", &maxi);
    else maxi = 0x7fffffff;
    for (i = 0; i<maxi; i++)
    {
        sprintf(keybuf, "path%d", i+1);
        if (!sys_getpreference(keybuf, prefbuf, MAXPDSTRING))
            break;
        sys_searchpath = namelist_append_files(sys_searchpath, prefbuf);
    }
    if (sys_getpreference("standardpath", prefbuf, MAXPDSTRING))
        sscanf(prefbuf, "%d", &sys_usestdpath);
    if (sys_getpreference("verbose", prefbuf, MAXPDSTRING))
        sscanf(prefbuf, "%d", &sys_verbose);

        /* startup settings */
    if (sys_getpreference("nloadlib", prefbuf, MAXPDSTRING))
        sscanf(prefbuf, "%d", &maxi);
    else maxi = 0x7fffffff;
    for (i = 0; i<maxi; i++)
    {
        sprintf(keybuf, "loadlib%d", i+1);
        if (!sys_getpreference(keybuf, prefbuf, MAXPDSTRING))
            break;
        sys_externlist = namelist_append_files(sys_externlist, prefbuf);
    }
    if (sys_getpreference("defeatrt", prefbuf, MAXPDSTRING))
        sscanf(prefbuf, "%d", &sys_defeatrt);
    if (sys_getpreference("flags", prefbuf, MAXPDSTRING))
    {
        if (strcmp(prefbuf, "."))
            sys_flags = gensym(prefbuf);
    }
    sys_doflags();

    if (sys_defeatrt)
        sys_hipriority = 0;
    else
#ifdef UNIX
        sys_hipriority = 1; //!geteuid();
#else
#ifdef MSW
        sys_hipriority = 0;
#else
        sys_hipriority = 1;
#endif
#endif
}

void glob_savepreferences(t_pd *dummy)
{
    int naudioindev, audioindev[MAXAUDIOINDEV], chindev[MAXAUDIOINDEV];
    int naudiooutdev, audiooutdev[MAXAUDIOOUTDEV], choutdev[MAXAUDIOOUTDEV];
    int i, rate, advance, callback, blocksize;
    char buf1[MAXPDSTRING], buf2[MAXPDSTRING];
    int nmidiindev, midiindev[MAXMIDIINDEV];
    int nmidioutdev, midioutdev[MAXMIDIOUTDEV];

    sys_initsavepreferences();


        /* audio settings */
    sprintf(buf1, "%d", sys_audioapi);
    sys_putpreference("audioapi", buf1);

    sys_get_audio_params(&naudioindev, audioindev, chindev,
        &naudiooutdev, audiooutdev, choutdev, &rate, &advance, &callback,
            &blocksize);

    sys_putpreference("noaudioin", (naudioindev <= 0 ? "True" : "False"));
    for (i = 0; i < naudioindev; i++)
    {
        sprintf(buf1, "audioindev%d", i+1);
        sprintf(buf2, "%d %d", audioindev[i], chindev[i]);
        sys_putpreference(buf1, buf2);
    }
    sys_putpreference("noaudioout", (naudiooutdev <= 0 ? "True" : "False"));
    for (i = 0; i < naudiooutdev; i++)
    {
        sprintf(buf1, "audiooutdev%d", i+1);
        sprintf(buf2, "%d %d", audiooutdev[i], choutdev[i]);
        sys_putpreference(buf1, buf2);
   }

    sprintf(buf1, "%d", advance);
    sys_putpreference("audiobuf", buf1);

    sprintf(buf1, "%d", rate);
    sys_putpreference("rate", buf1);

    sprintf(buf1, "%d", callback);
    sys_putpreference("callback", buf1);

    sprintf(buf1, "%d", blocksize);
    sys_putpreference("blocksize", buf1);

        /* MIDI settings */
    sys_get_midi_params(&nmidiindev, midiindev, &nmidioutdev, midioutdev);
    sys_putpreference("nomidiin", (nmidiindev <= 0 ? "True" : "False"));
    for (i = 0; i < nmidiindev; i++)
    {
        sprintf(buf1, "midiindev%d", i+1);
        sprintf(buf2, "%d", midiindev[i]);
        sys_putpreference(buf1, buf2);
    }
    sys_putpreference("nomidiout", (nmidioutdev <= 0 ? "True" : "False"));
    for (i = 0; i < nmidioutdev; i++)
    {
        sprintf(buf1, "midioutdev%d", i+1);
        sprintf(buf2, "%d", midioutdev[i]);
        sys_putpreference(buf1, buf2);
    }
        /* file search path */

    for (i = 0; 1; i++)
    {
        char *pathelem = namelist_get(sys_searchpath, i);
        if (!pathelem)
            break;
        sprintf(buf1, "path%d", i+1);
        sys_putpreference(buf1, pathelem);
    }
    sprintf(buf1, "%d", i);
    sys_putpreference("npath", buf1);
    sprintf(buf1, "%d", sys_usestdpath);
    sys_putpreference("standardpath", buf1);
    sprintf(buf1, "%d", sys_verbose);
    sys_putpreference("verbose", buf1);
    
        /* startup */
    for (i = 0; 1; i++)
    {
        char *pathelem = namelist_get(sys_externlist, i);
        if (!pathelem)
            break;
        sprintf(buf1, "loadlib%d", i+1);
        sys_putpreference(buf1, pathelem);
    }
    sprintf(buf1, "%d", i);
    sys_putpreference("nloadlib", buf1);
    sprintf(buf1, "%d", sys_defeatrt);
    sys_putpreference("defeatrt", buf1);
    sys_putpreference("flags", 
        (sys_flags ? sys_flags->s_name : ""));
    sys_donesavepreferences();
    
}
