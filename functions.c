/*
 * Copyright (c) 2005-2010 Denise H. G. All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <pwd.h>
#include <grp.h>

#include "search.h"

static int opt_empty;
static int opt_delete;

static int get_type(const char *);
static int cook_entry(const char *, const char *);
static gid_t get_gid(const char *);
static uid_t get_uid(const char *);
static int tell_grname(const char *, const gid_t);
static int tell_pwname(const char *, const uid_t);

void lookup_option(int, char **);
void comp_regex(reg_t *);
int exec_regex(const char *, reg_t *);
int exec_name(const char *, reg_t *);
void free_regex(reg_t *);
void walk_through(const char *, const char *);
void display_usage(void);
void display_version(void);

void
lookup_option(int argc, char *argv[])
{
  int ch;
  
  static struct option longopts[] = {
	{ "gid",     required_argument, NULL,        2  },
	{ "group",   required_argument, NULL,        3  },
	{ "path",    required_argument, NULL,       'f' },
	{ "name",    required_argument, NULL,       'n' },
	{ "regex",   required_argument, NULL,       'r' },
	{ "type",    required_argument, NULL,       't' },
	{ "user",    required_argument, NULL,        4  },
	{ "uid",     required_argument, NULL,        5  },
	{ "empty",   no_argument,       &opt_empty,  1  },
	{ "delete",  no_argument,       &opt_delete, 1  },
	{ "sort",    no_argument,       NULL,       's' },
	{ "version", no_argument,       NULL,       'v' },
	{ NULL,      0,                 NULL,        0  }
  };

  while ((ch = getopt_long(argc, argv, "EILPsvf:n:r:t:", longopts, NULL)) != -1)
	switch (ch) {
	case 2:
	  opts->find_gid = 1;
	  bzero(opts->gid, LINE_MAX);
	  strncpy(opts->gid, optarg, LINE_MAX);
	  break;
	case 3:
	  opts->find_group = 1;
	  bzero(opts->group, LINE_MAX);
	  strncpy(opts->group, optarg, LINE_MAX);
	  break;
	case 4:
	  opts->find_user = 1;
	  bzero(opts->user, LINE_MAX);
	  strncpy(opts->user, optarg, LINE_MAX);
	  break;
	case 5:
	  opts->find_uid = 1;
	  bzero(opts->uid, LINE_MAX);
	  strncpy(opts->uid, optarg, LINE_MAX);
	  break;
	case 'f':
	  opts->find_path = 1;
	  bzero(opts->path, MAXPATHLEN);
	  strncpy(opts->path, optarg, MAXPATHLEN);
	  break;
	case 'n':
	  bzero(rep->re_str, LINE_MAX);
	  strncpy(rep->re_str, optarg, LINE_MAX);
	  opts->exec_func = exec_name;
	  break;
	case 'r':
	  bzero(rep->re_str, LINE_MAX);
	  strncpy(rep->re_str, optarg, LINE_MAX);
	  opts->exec_func = exec_regex;
	  break;
	case 0:
	  if (opt_empty == 1)
		opts->find_empty = 1;
	  if (opt_delete == 1)
		opts->delete = 1;
	  break;
	case 's':
	  opts->sort = 1;
	  break;
	case 'v':
	  display_version();
	  break;
	case 't':
	  switch (optarg[0]) {
	  case 'f':
		opts->n_type = NT_ISFIFO;
		break;
	  case 'c':
		opts->n_type = NT_ISCHR;
		break;
	  case 'd':
		opts->n_type = NT_ISDIR;
		break;
	  case 'b':
		opts->n_type = NT_ISBLK;
		break;
	  case 'l':
		opts->n_type = NT_ISLNK;
		opts->stat_func = lstat; 
		break;
	  case 's':
		opts->n_type = NT_ISSOCK;
		break;
#ifndef _OpenBSD_
	  case 'w':
		opts->n_type = NT_ISWHT;
		break;
#endif
	  case 'r':
	  case '\0':
		opts->n_type = NT_ISREG;
		break;
	  default:
		display_usage();
		break;
	  }
	  break;
	case 'E':
	  rep->re_cflag |= REG_EXTENDED;
	  break;
	case 'I':
	  rep->re_cflag |= REG_ICASE;
	  break;
	case 'L':
	  opts->stat_func = stat;
	  break;
	case 'P':
	  break;
	default:
	  display_usage();
	  break;
	}
}

int
exec_name(const char *d_name, reg_t *rep)
{
  int flag, len, matched;
  char *pattern;
  
  pattern = rep->re_str;
  len = strlen(pattern);
  flag = rep->re_cflag;
  matched = FNM_NOMATCH;
  
  if (len == 0)
	pattern = "*";
  
  if ( (flag & REG_ICASE) == 0) {
	flag = 0;
  } else {
	flag = FNM_CASEFOLD | FNM_PERIOD | FNM_PATHNAME | FNM_NOESCAPE;
  }
  
#ifdef _DEBUG_
  (void)fprintf(stderr,
				"pattern=%s, name=%s\n",
				pattern,
				d_name);
#endif
  matched = fnmatch(pattern, d_name, flag);
    
  return ((matched == 0) ? 1 : 0);

}

void
comp_regex(reg_t *rep)
{
  int retval, len, cflag;
  regex_t *fmt;
  char msg[LINE_MAX];
  char *str;
  
  len = strlen(rep->re_str);
  cflag = rep->re_cflag;
  
  bzero(msg, LINE_MAX);
  str = rep->re_str;
  fmt = &(rep->re_fmt);

  if (len == 0)
	str = ".*";		/* defaults to search all types. */

  retval = regcomp(fmt, str, cflag);

  if (retval != 0) {
	if (regerror(retval, fmt, msg, LINE_MAX) > 0) {
	  (void)fprintf(stderr,
					"%s: %s: %s\n",
					opts->prog_name,
					str,
					msg);
	} else {
	  (void)fprintf(stderr,
					"%s: %s: %s\n",
					opts->prog_name,
					str,
					strerror(errno));
	}
	regfree(fmt);
	exit(0);
  }
}


int
exec_regex(const char *d_name, reg_t *rep)
{
  int retval, len, matched;
  regex_t *fmt;
  regmatch_t pmatch;
  char msg[LINE_MAX];
  char *str;

  fmt = &(rep->re_fmt);
  str = rep->re_str;
  len = strlen(d_name);

  bzero(msg, LINE_MAX);
  pmatch.rm_so = 0;
  pmatch.rm_eo = len;
  matched = 0;

  retval = regexec(fmt, d_name, 1, &pmatch, REG_STARTEND);

  if (retval != 0 && retval != REG_NOMATCH) {
	
	if (regerror(retval, fmt, msg, LINE_MAX) > 0) {
	  (void)fprintf(stderr,
					"%s: %s: %s\n",
					opts->prog_name,
					str,
					msg);
	} else {
	  (void)fprintf(stderr,
					"%s: %s: %s\n",
					opts->prog_name,
					str,
					strerror(errno));
	}
	regfree(fmt);
	exit(0);
  }

  matched = retval == 0 && pmatch.rm_so == 0 && pmatch.rm_eo == len;
  return ((matched) ? 1 : 0);
}

void
free_regex(reg_t *rep)
{
  regfree(&(rep->re_fmt));
  free(rep);
  rep = NULL;
  return;
}

void
walk_through(const char *n_name, const char *d_name)
{
  int found;
  char tmp_buf[MAXPATHLEN];
  struct dirent *dir;
  DIR *dirp;
  DLIST *slist;
  DLIST *frem, *drem;
  
  if (get_type(n_name) == NT_ERROR) {
	(void)fprintf(stderr,
				  "%s: %s: %s\n",
				  opts->prog_name,
				  n_name,
				  strerror(errno));
	return;
  }

  slist = dl_init();
  if (opts->delete == 1) {
	frem = dl_init();
	drem = dl_init();
  }

  found = cook_entry(n_name, d_name);
  
  if (found == 1) {
	if (opts->delete == 1) {
	  if (node_stat->type != NT_ISDIR) {
#ifdef _DEBUG
		fprintf(stderr, "delete: %s\n", n_name);
#endif
		dl_append(n_name, frem);
	  } else {
		if ((strncmp(n_name, ".", 2) != 0) &&
			(strncmp(n_name, "..", 3) != 0)) {
#ifdef _DEBUG_
		  fprintf(stderr, "delete: %s\n", n_name);
#endif
		  dl_append(n_name, drem);
		}
	  }
	}
  }

  if (node_stat->type == NT_ISDIR) {
	
	if ( (dirp = opendir(n_name)) == NULL) {
	  (void)fprintf(stderr,
					"%s: %s: %s\n",
					opts->prog_name,
					n_name,
					strerror(errno));
	  return;
	}
	
	while ( (dir = readdir(dirp)) != NULL) {
	  if ((strncmp(dir->d_name, ".", 2) != 0) &&
		  (strncmp(dir->d_name, "..", 3) != 0)) {
		dl_append(dir->d_name, slist);
	  }
	}

	if (opts->sort == 1)
	  dl_sort(slist);

	slist->cur = slist->head;
	while (slist->cur != NULL) {
	  bzero(tmp_buf, MAXPATHLEN);
	  strncpy(tmp_buf, n_name, MAXPATHLEN);
	  if (tmp_buf[strlen(tmp_buf) - 1] != '/')
		strncat(tmp_buf, "/", MAXPATHLEN);
	  strncat(tmp_buf, slist->cur->node, MAXPATHLEN);
	  walk_through(tmp_buf, slist->cur->node);
	  slist->cur = slist->cur->next;
	}
	closedir(dirp);
  }

  if (opts->delete == 1) {
	if (frem != NULL) {
	  frem->cur = frem->head;
	  while (frem->cur != NULL) {
		if (unlink(frem->cur->node) < 0)
		  (void)fprintf(stderr,
						"%s: --delete: unlink(%s): %s\n",
						opts->prog_name,
						frem->cur->node,
						strerror(errno));
		frem->cur = frem->cur->next;
	  }
	}
	  
	if (drem != NULL) {
	  drem->cur = drem->head;
	  while (drem->cur != NULL) {
		if (rmdir(drem->cur->node) < 0)
		  (void)fprintf(stderr,
						"%s: --delete: rmdir(%s): %s\n",
						opts->prog_name,
						drem->cur->node,
						strerror(errno));
		drem->cur = drem->cur->next;
	  }
	}
  }

  if (opts->delete == 1) {
	if (frem != NULL) {
	  dl_free(frem);
	  frem = NULL;
	}
	if (drem != NULL) {
	  dl_free(drem);
	  drem = NULL;
	}
  }
  
  if (slist != NULL) {
	dl_free(slist);
	slist = NULL;
  }
  
  return;
}

void
display_usage(void)
{
  static const char *usage = "usage:\t%s [-EILPsv]\
 ...\
 [-f|--path ...]\
 [-n|--name ...]\
 [-r|--regex ...]\
 [-t|--type ...]\
 [--empty]\
 [--sort]\n\
 \t%s [-EILPsv]\
 -f | --path ...\
 [...]\
 [-n|--name ...]\
 [-r|--regex ...]\
 [-t|--type ...]\
 [--empty]\
 [--sort]\n";

  (void)fprintf(stderr,
				usage,
				opts->prog_name,
				opts->prog_name);
  exit (0);
}

void
display_version(void)
{  
  (void)fprintf(stderr,
				"%s version %s\n",
				opts->prog_name,
				opts->prog_version);
  exit (0);
}

static node_t
get_type(const char *d_name)
{
  struct stat stbuf;
  
  if (opts->stat_func(d_name, &stbuf)  < 0 )
	return (NT_ERROR);

  node_stat->empty = 0;
  node_stat->gid = stbuf.st_gid;
  node_stat->uid = stbuf.st_uid;
  
  if (S_ISBLK(stbuf.st_mode))
	node_stat->type = NT_ISBLK;
  if (S_ISCHR(stbuf.st_mode))
	node_stat->type = NT_ISCHR;
  if (S_ISDIR(stbuf.st_mode))
	node_stat->type = NT_ISDIR;
  if (S_ISFIFO(stbuf.st_mode))
	node_stat->type = NT_ISFIFO;
  if (S_ISLNK(stbuf.st_mode))
	node_stat->type = NT_ISLNK;
  if (S_ISREG(stbuf.st_mode))
	node_stat->type = NT_ISREG;
  if (S_ISSOCK(stbuf.st_mode))
	node_stat->type = NT_ISSOCK;
#ifndef _OpenBSD_
  if (S_ISWHT(stbuf.st_mode))
	node_stat->type = NT_ISWHT;
#endif
  
  if (node_stat->type == NT_ISDIR) {
	if ((stbuf.st_nlink <= 2) &&
		(stbuf.st_size <= 2)) {
	  node_stat->empty = 1;
#ifdef _DEBUG_
	  (void)fprintf(stderr,"%s: empty directory.\n", d_name);
#endif
	}
  } else { /* node is not a directory. */
	if (stbuf.st_size == 0) {
	  node_stat->empty = 1;
#ifdef _DEBUG_
	  (void)fprintf(stderr, "%s: empty file.\n", d_name);
#endif
	}
  }
  
  return (NT_UNKNOWN);
}

static int
cook_entry(const char *n_name, const char *d_name)
{
  unsigned int found;

  found = 0;
  
  if ((opts->exec_func(d_name, rep) == 1) &&
	  ((opts->n_type == NT_UNKNOWN) ||
	   (opts->n_type == node_stat->type))) {

	found = 1;
	
	if (opts->find_empty == 1) {
	  if (node_stat->empty != 1)
		found = 0;
	}

	if (opts->find_gid == 1) {
	  if (node_stat->gid != get_gid(opts->gid))
		found = 0;
	}
	
	if (opts->find_uid == 1) {
	  if (node_stat->uid != get_uid(opts->uid))
		found = 0;
	}

	if (opts->find_group == 1) {
	  if (1 != tell_grname(opts->group, node_stat->gid))
		found = 0;
	}

	if (opts->find_user == 1) {
	  if (1 != tell_pwname(opts->user, node_stat->uid))
		found = 0;
	}

	if (opts->delete != 1 && found == 1) {
	  (void)fprintf(stdout, "%s\n", n_name);
	}
  }

  return (found);
}

static gid_t
get_gid(const char *sgid)
{
  gid_t id;
  char *p;
  
  if (sgid == NULL)
	return (-1);

  id = strtol(sgid, &p, 0);
  if (p[0] == '\0') {
	return (id);
  } else {
	(void)fprintf(stderr, "%s: --gid: %s: no such group\n",
				  opts->prog_name, p);
	exit (0);
  }
  return (-1);
}

static uid_t
get_uid(const char *suid)
{
  uid_t id;
  char *p;
  
  id = strtol(suid, &p, 0);
  if (p[0] == '\0') {
	return (id);
  } else {
	(void)fprintf(stderr, "%s: --uid: %s: no such user\n",
				  opts->prog_name, p);
	exit (0);
  }
  return (-1);
}

static int
tell_grname(const char *name, const gid_t id)
{
  char *p;
  struct group *grp;

  if (id < 0)
	return (0);
  
  if (name == NULL)
	return (0);
  
  if (opts->find_group == 1) {
	if ((grp = getgrnam(name)) != NULL) {
	  if (id == grp->gr_gid)
		return (1);
	} else {
	  (void)fprintf(stderr, "%s: --group: %s: no such group\n",
					opts->prog_name, name);
	  exit (0);
	}
  }

  return (0);
}

static int
tell_pwname(const char *name, const uid_t id)
{
  char *p;
  struct passwd *pw;
  
  if (opts->find_user == 1) {
	if ((pw = getpwnam(name)) != NULL) {
	  if (id == pw->pw_uid)
		return (1);
	} else {
	  (void)fprintf(stderr, "%s: --user: %s: no such user\n",
					opts->prog_name, name);
	  exit (0);
	}
  }
  
  return (0);
}
	
	
