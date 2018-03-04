/* Copyright (C) 2015 Cotton Seed

   This file is part of arachne-pnr.  Arachne-pnr is free software;
   you can redistribute it and/or modify it under the terms of the GNU
   General Public License version 2 as published by the Free Software
   Foundation.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>. */

#if !defined(_WIN32) && !defined (_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "util.hh"

#include <iostream>
#include <set>
#include <cstring>

#ifdef _WIN32
#  include <windows.h>
#  include <io.h>
#elif defined(__APPLE__)
#  include <mach-o/dyld.h>
#  include <unistd.h>
#else
#  include <unistd.h>
#endif

#include <limits.h>

std::ostream *logs;

void fatal(const std::string &msg)
{
  std::cerr << "fatal error: " << msg << "\n";
  exit(EXIT_FAILURE);
}

void warning(const std::string &msg)
{
  std::cerr << "warning: " << msg << "\n";
}

void note(const std::string &msg)
{
  std::cerr << "note: " << msg << "\n";
}

std::string
unescape(const std::string &s)
{
  std::string r;
  for (auto i = s.begin(); i != s.end(); ++i)
    {
      if (*i == '\\')
        {
          ++i;
          assert(i != s.end());

          switch(*i)
            {
            case '\'':
              r.push_back('\'');
              break;
            case '\"':
              r.push_back('\"');
              break;
            case '\?':
              r.push_back('\?');
              break;
            case '\\':
              r.push_back('\\');
              break;
            case '\a':
              r.push_back('\a');
              break;
            case '\b':
              r.push_back('\b');
              break;
            case '\f':
              r.push_back('\f');
              break;
            case 'n':
              r.push_back('\n');
              break;
            case 'r':
              r.push_back('\r');
              break;
            case 't':
              r.push_back('\t');
              break;
            case 'v':
              r.push_back('\v');
              break;
            case '0':  case '1':  case '2':  case '3':
            case '4':  case '5':  case '6':  case '7':
            case '8':  case '9':
              {
                int x = *i++ - '0';
                if (i != s.end() && *i >= '0' && *i <= '9')
                  x = x*8 + *i++ - '0';
                if (i != s.end() && *i >= '0' && *i <= '9')
                  x = x*8 + *i++ - '0';
                i--;
                r.push_back(x);
              }
              break;
            }
        }
      else
        r.push_back(*i);
    }
  return r;
}

/* taken from Yosys, yosys/kernel/yosys.cc */
#if defined(__linux__) || defined(__CYGWIN__)
std::string proc_self_dirname()
{
        char path[PATH_MAX];
        ssize_t buflen = readlink("/proc/self/exe", path, sizeof(path));
        if (buflen < 0) {
                fatal(fmt("readlink(\"/proc/self/exe\") failed: " << strerror(errno)));
        }
        while (buflen > 0 && path[buflen-1] != '/')
                buflen--;
        return std::string(path, buflen);
}
#elif defined(__FreeBSD__)
std::string proc_self_dirname()
{
        char path[PATH_MAX];
        ssize_t buflen = readlink("/proc/curproc/file", path, sizeof(path));
        if (buflen < 0) {
                fatal(fmt("readlink(\"/proc/curproc/file\") failed: " << strerror(errno)));
        }
        while (buflen > 0 && path[buflen-1] != '/')
                buflen--;
        return std::string(path, buflen);
}
#elif defined(__APPLE__)
std::string proc_self_dirname()
{
        char *path = NULL;
        uint32_t buflen = 0;
        while (_NSGetExecutablePath(path, &buflen) != 0)
                path = (char *) realloc((void *) path, buflen);
        while (buflen > 0 && path[buflen-1] != '/')
                buflen--;
        return std::string(path, buflen);
}
#elif defined(_WIN32)
std::string proc_self_dirname()
{
        int i = 0;
#  ifdef __MINGW32__
        char longpath[MAX_PATH + 1];
        char shortpath[MAX_PATH + 1];
#  else
        WCHAR longpath[MAX_PATH + 1];
        TCHAR shortpath[MAX_PATH + 1];
#  endif
        if (!GetModuleFileName(0, longpath, MAX_PATH+1))
                fatal("GetModuleFileName() failed.");
        if (!GetShortPathName(longpath, shortpath, MAX_PATH+1))
                fatal("GetShortPathName() failed.");
        while (shortpath[i] != 0)
                i++;
        while (i > 0 && shortpath[i-1] != '/' && shortpath[i-1] != '\\')
                shortpath[--i] = 0;
        std::string path;
        for (i = 0; shortpath[i]; i++)
                path += char(shortpath[i]);
        return path;
}
#elif defined(__EMSCRIPTEN__)
std::string proc_self_dirname()
{
        // This is a fake path, but ../ will always be appended and this will still work.
        return "/bin/";
}
#else
        #error Dont know how to determine process executable base path!
#endif

std::string
expand_filename(const std::string &file)
{
  if (file[0] == '+')
#if defined(_WIN32) && defined(MXE_DIR_STRUCTURE)
    return (proc_self_dirname()
            + std::string(file.begin() + 2,
                          file.end()));
#else
    return (proc_self_dirname()
            + ".."
            + std::string(file.begin() + 1,
                          file.end()));
#endif
  else
    return file;
}
