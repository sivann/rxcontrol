#include <unistd.h>
#include <mntent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "fstab.h"
#include "sundries.h"		/* for xmalloc() etc */


#define streq(s, t)	(strcmp ((s), (t)) == 0)

#define PROC_MOUNTS		"/proc/mounts"


/* Information about mtab. ------------------------------------*/
static int have_mtab_info = 0;
static int var_mtab_does_not_exist = 0;
static int var_mtab_is_a_symlink = 0;

static void
get_mtab_info(void) {
     struct stat mtab_stat;

     if (!have_mtab_info) {
	  if (lstat(MOUNTED, &mtab_stat))
	       var_mtab_does_not_exist = 1;
	  else if (S_ISLNK(mtab_stat.st_mode))
	       var_mtab_is_a_symlink = 1;
	  have_mtab_info = 1;
     }
}

int
mtab_does_not_exist(void) {
     get_mtab_info();
     return var_mtab_does_not_exist;
}

int
mtab_is_a_symlink(void) {
     get_mtab_info();
     return var_mtab_is_a_symlink;
}

int
mtab_is_writable() {
     static int ret = -1;

     /* Should we write to /etc/mtab upon an update?
	Probably not if it is a symlink to /proc/mounts, since that
	would create a file /proc/mounts in case the proc filesystem
	is not mounted. */
     if (mtab_is_a_symlink())
	  return 0;

     if (ret == -1) {
	  int fd = open(MOUNTED, O_RDWR | O_CREAT, 0644);
	  if (fd >= 0) {
	       close(fd);
	       ret = 1;
	  } else
	       ret = 0;
     }
     return ret;
}

/* Contents of mtab and fstab ---------------------------------*/

struct mntentchn mounttable, fstab;
static int got_mtab = 0;
static int got_fstab = 0;

static void read_mounttable(void), read_fstab(void);

struct mntentchn *
mtab_head() {
     if (!got_mtab)
	  read_mounttable();
     return &mounttable;
}

struct mntentchn *
fstab_head() {
     if (!got_fstab)
	  read_fstab();
     return &fstab;
}

static void
read_mntentchn(FILE *fp, const char *fnam, struct mntentchn *mc0) {
     struct mntentchn *mc = mc0;
     struct mntent *mnt;

     while (!feof(fp) && !ferror(fp)) {
	  if ((mnt = getmntent (fp)) != NULL 	       /* ignore blank lines */
	       && *mnt->mnt_fsname != '#' 	       /* and comment lines */
	       && !streq (mnt->mnt_type, MNTTYPE_IGNORE)) {
	       mc->nxt = (struct mntentchn *) xmalloc(sizeof(*mc));
	       mc->nxt->prev = mc;
	       mc = mc->nxt;
	       mc->mnt_fsname = xstrdup(mnt->mnt_fsname);
	       mc->mnt_dir = xstrdup(mnt->mnt_dir);
	       mc->mnt_type = xstrdup(mnt->mnt_type);
	       mc->mnt_opts = xstrdup(mnt->mnt_opts);
	       mc->nxt = NULL;
	  }
     }
     mc0->prev = mc;
     if (ferror (fp)) {
	  error("warning: error reading %s: %s", fnam, strerror (errno));
	  mc0->nxt = mc0->prev = NULL;
     }
     endmntent(fp);
}

/*
 * Read /etc/mtab.  If that fails, try /proc/mounts.
 * This produces a linked list. The list head mounttable is a dummy.
 * Return 0 on success.
 */
static void
read_mounttable() {
     FILE *fp = NULL;
     const char *fnam;
     struct mntentchn *mc = &mounttable;

     got_mtab = 1;
     mc->nxt = mc->prev = NULL;

     fnam = MOUNTED;
     if ((fp = setmntent (fnam, "r")) == NULL) {
	  int errsv = errno;
	  fnam = PROC_MOUNTS;
	  if ((fp = setmntent (fnam, "r")) == NULL) {
	       error("warning: can't open %s: %s", MOUNTED, strerror (errsv));
	       return;
	  }
	  if (verbose)
	       printf ("mount: could not open %s - using %s instead\n",
		       MOUNTED, PROC_MOUNTS);
     }
     read_mntentchn(fp, fnam, mc);
}

static void
read_fstab() {
     FILE *fp = NULL;
     const char *fnam;
     struct mntentchn *mc = &fstab;

     got_fstab = 1;
     mc->nxt = mc->prev = NULL;

     fnam = _PATH_FSTAB;
     if ((fp = setmntent (fnam, "r")) == NULL) {
	  error("warning: can't open %s: %s", _PATH_FSTAB, strerror (errno));
	  return;
     }
     read_mntentchn(fp, fnam, mc);
}
     

/* Given the name NAME, try to find it in mtab.  */ 
struct mntentchn *
getmntfile (const char *name) {
    struct mntentchn *mc;

    for (mc = mtab_head()->nxt; mc; mc = mc->nxt)
        if (streq (mc->mnt_dir, name) || (streq (mc->mnt_fsname, name)))
	    break;

    return mc;
}

/* Given the name FILE, try to find the option "loop=FILE" in mtab.  */ 
struct mntentchn *
getmntoptfile (const char *file)
{
     struct mntentchn *mc;
     char *opts, *s;
     int l;

     if (!file)
	  return NULL;

     l = strlen(file);

     for (mc = mtab_head()->nxt; mc; mc = mc->nxt)
	  if ((opts = mc->mnt_opts) != NULL
	      && (s = strstr(opts, "loop="))
	      && !strncmp(s+5, file, l)
	      && (s == opts || s[-1] == ',')
	      && (s[l+5] == 0 || s[l+5] == ','))
	       return mc;

     return NULL;
}

/* Find the dir FILE in fstab.  */
struct mntentchn *
getfsfile (const char *file) {
    struct mntentchn *mc;

    for (mc = fstab_head()->nxt; mc; mc = mc->nxt)
        if (streq (mc->mnt_dir, file))
	    break;

    return mc;
}

/* Find the device SPEC in fstab.  */
struct mntentchn *
getfsspec (const char *spec)
{
    struct mntentchn *mc;

    for (mc = fstab_head()->nxt; mc; mc = mc->nxt)
        if (streq (mc->mnt_fsname, spec))
	    break;

    return mc;
}

/* Updating mtab ----------------------------------------------*/

/* File descriptor for lock.  Value tested in unlock_mtab() to remove race.  */
static int lock = -1;

/* Flag for already existing lock file. */
static int old_lockfile = 1;

/* Ensure that the lock is released if we are interrupted.  */
static void
handler (int sig) {
     die (EX_USER, "%s", sys_siglist[sig]);
}

static void
setlkw_timeout (int sig) {
     /* nothing, fcntl will fail anyway */
}

/* Create the lock file.  The lock file will be removed if we catch a signal
   or when we exit.  The value of lock is tested to remove the race.  */
void
lock_mtab (void) {
     int sig = 0;
     struct sigaction sa;
     struct flock flock;

     /* If this is the first time, ensure that the lock will be removed.  */
     if (lock < 0) {
	  struct stat st;
	  sa.sa_handler = handler;
	  sa.sa_flags = 0;
	  sigfillset (&sa.sa_mask);
  
	  while (sigismember (&sa.sa_mask, ++sig) != -1 && sig != SIGCHLD) {
	       if (sig == SIGALRM)
		    sa.sa_handler = setlkw_timeout;
	       else
		    sa.sa_handler = handler;
	       sigaction (sig, &sa, (struct sigaction *) 0);
	  }

	  /* This stat is performed so we know when not to be overly eager
	     when cleaning up after signals. The window between stat and
	     open is not significant. */
	  if (lstat (MOUNTED_LOCK, &st) < 0 && errno == ENOENT)
	       old_lockfile = 0;

	  lock = open (MOUNTED_LOCK, O_WRONLY|O_CREAT, 0);
	  if (lock < 0) {
	       die (EX_FILEIO, "can't create lock file %s: %s "
		       "(use -n flag to override)",
		    MOUNTED_LOCK, strerror (errno));
	  }

	  flock.l_type = F_WRLCK;
	  flock.l_whence = SEEK_SET;
	  flock.l_start = 0;
	  flock.l_len = 0;

	  alarm(LOCK_BUSY);
	  if (fcntl (lock, F_SETLKW, &flock) < 0) {
	       close (lock);
	       /* The file should not be removed */
	       lock = -1;
	       die (EX_FILEIO, "can't lock lock file %s: %s",
		    MOUNTED_LOCK,
		    errno == EINTR ? "timed out" : strerror (errno));
	  }
	  /* We have now access to the lock, and it can always be removed */
	  old_lockfile = 0;
     }
}

/* Remove lock file.  */
void
unlock_mtab (void) {
     if (lock != -1) {
	  close (lock);
	  if (!old_lockfile)
	       unlink (MOUNTED_LOCK);
     }
}

/*
 * Update the mtab.
 *  Used by umount with null INSTEAD: remove any DIR entries.
 *  Used by mount upon a remount: update option part,
 *   and complain if a wrong device or type was given.
 *   [Note that often a remount will be a rw remount of /
 *    where there was no entry before, and we'll have to believe
 *    the values given in INSTEAD.]
 */

void
update_mtab (const char *dir, struct mntent *instead) {
     struct mntent *mnt;
     struct mntent *next;
     struct mntent remnt;
     int added = 0;
     FILE *fp, *ftmp;

     if (mtab_does_not_exist() || mtab_is_a_symlink())
	  return;

     lock_mtab();

     if ((fp = setmntent(MOUNTED, "r")) == NULL) {
	  error ("cannot open %s (%s) - mtab not updated",
		 MOUNTED, strerror (errno));
	  goto leave;
     }

     if ((ftmp = setmntent (MOUNTED_TEMP, "w")) == NULL) {
	  error ("can't open %s (%s) - mtab not updated",
		 MOUNTED_TEMP, strerror (errno));
	  goto leave;
     }
  
     while ((mnt = getmntent (fp))) {
	  if (streq (mnt->mnt_dir, dir)) {
	       added++;
	       if (instead) {	/* a remount */
		    remnt = *instead;
		    next = &remnt;
		    remnt.mnt_fsname = mnt->mnt_fsname;
		    remnt.mnt_type = mnt->mnt_type;
		    if (instead->mnt_fsname
			&& !streq(mnt->mnt_fsname, instead->mnt_fsname))
			 printf("mount: warning: cannot change "
				"mounted device with a remount\n");
		    else if (instead->mnt_type
			     && !streq(instead->mnt_type, "unknown")
			     && !streq(mnt->mnt_type, instead->mnt_type))
			 printf("mount: warning: cannot change "
				"filesystem type with a remount\n");
	       } else
		    next = NULL;
	  } else
	       next = mnt;
	  if (next && addmntent(ftmp, next) == 1)
	       die (EX_FILEIO, "error writing %s: %s",
		    MOUNTED_TEMP, strerror (errno));
     }
     if (instead && !added && addmntent(ftmp, instead) == 1)
	  die (EX_FILEIO, "error writing %s: %s",
	       MOUNTED_TEMP, strerror (errno));

     endmntent (fp);
     if (fchmod (fileno (ftmp), S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) < 0)
	  fprintf(stderr, "error changing mode of %s: %s\n", MOUNTED_TEMP,
		  strerror (errno));
     endmntent (ftmp);

     if (rename (MOUNTED_TEMP, MOUNTED) < 0)
	  fprintf(stderr, "can't rename %s to %s: %s\n", MOUNTED_TEMP, MOUNTED,
		  strerror(errno));

leave:
     unlock_mtab();
}



/*
 * Support functions.  Exported functions are prototyped in sundries.h.
 * sundries.c,v 1.1.1.1 1993/11/18 08:40:51 jrs Exp
 *
 * added fcntl locking by Kjetil T. (kjetilho@math.uio.no) - aeb, 950927
 */
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <mntent.h>		/* for MNTTYPE_SWAP */


/* String list constructor.  (car() and cdr() are defined in "sundries.h").  */
string_list
cons (char *a, const string_list b) {
     string_list p;

     p = xmalloc (sizeof *p);
     car (p) = a;
     cdr (p) = b;
     return p;
}

void *
xmalloc (size_t size) {
     void *t;

     if (size == 0)
	  return NULL;

     t = malloc (size);
     if (t == NULL)
	  die (EX_SYSERR, "not enough memory");
  
     return t;
}

char *
xstrdup (const char *s) {
     char *t;

     if (s == NULL)
	  return NULL;
 
     t = strdup (s);

     if (t == NULL)
	  die (EX_SYSERR, "not enough memory");

     return t;
}

char *
xstrndup (const char *s, int n) {
     char *t;

     if (s == NULL)
	  die (EX_SOFTWARE, "bug in xstrndup call");

     t = xmalloc(n+1);
     strncpy(t,s,n);
     t[n] = 0;

     return t;
}

char *
xstrconcat2 (const char *s, const char *t) {
     char *res;

     if (!s) s = "";
     if (!t) t = "";
     res = xmalloc(strlen(s) + strlen(t) + 1);
     strcpy(res, s);
     strcat(res, t);
     return res;
}

char *
xstrconcat3 (const char *s, const char *t, const char *u) {
     char *res;

     if (!s) s = "";
     if (!t) t = "";
     if (!u) u = "";
     res = xmalloc(strlen(s) + strlen(t) + strlen(u) + 1);
     strcpy(res, s);
     strcat(res, t);
     strcat(res, u);
     return res;
}

char *
xstrconcat4 (const char *s, const char *t, const char *u, const char *v) {
     char *res;

     if (!s) s = "";
     if (!t) t = "";
     if (!u) u = "";
     if (!v) v = "";
     res = xmalloc(strlen(s) + strlen(t) + strlen(u) + strlen(v) + 1);
     strcpy(res, s);
     strcat(res, t);
     strcat(res, u);
     strcat(res, v);
     return res;
}

/* Call this with SIG_BLOCK to block and SIG_UNBLOCK to unblock.  */
void
block_signals (int how) {
     sigset_t sigs;

     sigfillset (&sigs);
     sigdelset(&sigs, SIGTRAP);
     sigdelset(&sigs, SIGSEGV);
     sigprocmask (how, &sigs, (sigset_t *) 0);
}


/* Non-fatal error.  Print message and return.  */
/* (print the message in a single printf, in an attempt
    to avoid mixing output of several threads) */
void
error (const char *fmt, ...) {
     va_list args;
     char *fmt2;

     if (mount_quiet)
	  return;
     fmt2 = xstrconcat2 (fmt, "\n");
     va_start (args, fmt);
     vfprintf (stderr, fmt2, args);
     va_end (args);
     free (fmt2);
}

/* Fatal error.  Print message and exit.  */
void
die (int err, const char *fmt, ...) {
     va_list args;

     va_start (args, fmt);
     vfprintf (stderr, fmt, args);
     fprintf (stderr, "\n");
     va_end (args);

     unlock_mtab ();
     exit (err);
}

/* Parse a list of strings like str[,str]... into a string list.  */
string_list
parse_list (char *strings) {
     string_list list;
     char *t;

     if (strings == NULL)
	  return NULL;

     list = cons (strtok (strings, ","), NULL);

     while ((t = strtok (NULL, ",")) != NULL)
	  list = cons (t, list);

     return list;
}

/* True if fstypes match.  Null *TYPES means match anything,
   except that swap types always return false.
   This routine has some ugliness to deal with ``no'' types.
   Fixed bug: the `no' part comes at the end - aeb, 970216  */
int
matching_type (const char *type, string_list types) {
     char *notype;
     int foundyes, foundno;
     int no;			/* true if a "no" type match, eg -t nominix */

     if (streq (type, MNTTYPE_SWAP))
	  return 0;
     if (types == NULL)
	  return 1;

     if ((notype = alloca (strlen (type) + 3)) == NULL)
	  die (EX_SYSERR, "mount: out of memory");
     sprintf (notype, "no%s", type);

     foundyes = foundno = no = 0;
     while (types != NULL) {
	  if (cdr (types) == NULL)
	       no = (car (types)[0] == 'n') && (car (types)[1] == 'o');
	  if (streq (type, car (types)))
	       foundyes = 1;
	  else if (streq (notype, car (types)))
	       foundno = 1;
	  types = cdr (types);
     }

     return (foundno ? 0 : (no ^ foundyes));
}

/* Make a canonical pathname from PATH.  Returns a freshly malloced string.
   It is up the *caller* to ensure that the PATH is sensible.  i.e.
   canonicalize ("/dev/fd0/.") returns "/dev/fd0" even though ``/dev/fd0/.''
   is not a legal pathname for ``/dev/fd0''.  Anything we cannot parse
   we return unmodified.   */
char *
canonicalize (const char *path) {
     char *canonical;
  
     if (path == NULL)
	  return NULL;

     if (streq(path, "none") || streq(path, "proc"))
	  return xstrdup(path);

     canonical = xmalloc (PATH_MAX + 1);
  
     if (realpath (path, canonical))
	  return canonical;

     free(canonical);
     return xstrdup(path);
}
