/* debuginfod utilities for GDB.
   Copyright (C) 2020 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "defs.h"
#include <errno.h>
#include "cli/cli-style.h"
#include "gdbsupport/scoped_fd.h"
#include "debuginfod-support.h"

#ifndef HAVE_LIBDEBUGINFOD
scoped_fd
debuginfod_source_query (const unsigned char *build_id,
			 int build_id_len,
			 const char *srcpath,
			 gdb::unique_xmalloc_ptr<char> *destname)
{
  return scoped_fd (-ENOSYS);
}

scoped_fd
debuginfod_debuginfo_query (const unsigned char *build_id,
			    int build_id_len,
			    const char *filename,
			    gdb::unique_xmalloc_ptr<char> *destname)
{
  return scoped_fd (-ENOSYS);
}
#else
#include <elfutils/debuginfod.h>

struct user_data
{
  user_data (const char *desc, const char *fname)
    : desc (desc), fname (fname), last_percent_printed (-1)
  { }

  const char * const desc;
  const char * const fname;
  long last_percent_printed;
};

/* Deleter for a debuginfod_client.  */

struct debuginfod_client_deleter
{
  void operator() (debuginfod_client *c)
  {
    debuginfod_end (c);
  }
};

using debuginfod_client_up
  = std::unique_ptr<debuginfod_client, debuginfod_client_deleter>;

static int
progressfn (debuginfod_client *c, long cur, long total)
{
  user_data *data = static_cast<user_data *> (debuginfod_get_user_data (c));

  if (check_quit_flag ())
    {
      printf_filtered ("Cancelling download of %s %ps...\n",
		       data->desc,
		       styled_string (file_name_style.style (), data->fname));
      return 1;
    }

  if (total != 0)
    {
      /* Print this message only once.  */
      long percent = 100 * cur / total;
      if (percent != data->last_percent_printed)
	{
	  data->last_percent_printed = percent;
	  printf_filtered ("\rDownloading %.2f MB %s %ps.. %3ld%%%s",
			   1.0f * total / (1024 * 1024),
			   data->desc,
			   styled_string (file_name_style.style (), data->fname),
			   percent, cur == total ? "\n" : "");
	  gdb_flush (gdb_stdout);
	}
    }

  return 0;
}

static debuginfod_client_up
debuginfod_init ()
{
  debuginfod_client_up c (debuginfod_begin ());

  if (c != nullptr)
    debuginfod_set_progressfn (c.get (), progressfn);

  return c;
}

/* See debuginfod-support.h  */

scoped_fd
debuginfod_source_query (const unsigned char *build_id,
			 int build_id_len,
			 const char *srcpath,
			 gdb::unique_xmalloc_ptr<char> *destname)
{
  const char *urls_env_var = getenv (DEBUGINFOD_URLS_ENV_VAR);
  if (urls_env_var == NULL || urls_env_var[0] == '\0')
    return scoped_fd (-ENOSYS);

  debuginfod_client_up c = debuginfod_init ();

  if (c == nullptr)
    return scoped_fd (-ENOMEM);

  user_data data ("source file", srcpath);

  debuginfod_set_user_data (c.get (), &data);
  scoped_fd fd (debuginfod_find_source (c.get (),
					build_id,
					build_id_len,
					srcpath,
					nullptr));

  /* TODO: Add 'set debug debuginfod' command to control when error messages are shown.  */
  if (fd.get () < 0 && fd.get () != -ENOENT)
    printf_filtered (_("Download failed: %s.  Continuing without source file %ps.\n"),
		     safe_strerror (-fd.get ()),
		     styled_string (file_name_style.style (),  srcpath));

  if (fd.get () >= 0)
    *destname = make_unique_xstrdup (srcpath);

  return fd;
}

/* See debuginfod-support.h  */

scoped_fd
debuginfod_debuginfo_query (const unsigned char *build_id,
			    int build_id_len,
			    const char *filename,
			    gdb::unique_xmalloc_ptr<char> *destname)
{
  const char *urls_env_var = getenv (DEBUGINFOD_URLS_ENV_VAR);
  if (urls_env_var == NULL || urls_env_var[0] == '\0')
    return scoped_fd (-ENOSYS);

  debuginfod_client_up c = debuginfod_init ();

  if (c == nullptr)
    return scoped_fd (-ENOMEM);

  char *dname = nullptr;
  user_data data ("separate debug info for", filename);

  debuginfod_set_user_data (c.get (), &data);
  scoped_fd fd (debuginfod_find_debuginfo (c.get (), build_id, build_id_len,
					   &dname));

  if (fd.get () < 0 && fd.get () != -ENOENT)
    printf_filtered (_("Download failed: %s.  Continuing without debug info for %ps.\n"),
		     safe_strerror (-fd.get ()),
		     styled_string (file_name_style.style (),  filename));

  if (fd.get () >= 0)
    destname->reset (dname);

  return fd;
}
#endif
