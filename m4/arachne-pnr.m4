##
## This file is part of the arachne-pnr project.
##
## Copyright (C) 2016 Joel Holdsworth <joel@airwebreathe.org.uk>
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.
##


## APNR_APPEND(var-name, [list-sep,] element)
##
## Append the shell word <element> to the shell variable named <var-name>,
## prefixed by <list-sep> unless the list was empty before appending. If
## only two arguments are supplied, <list-sep> defaults to a single space
## character.
##
AC_DEFUN([APNR_APPEND],
[dnl
m4_assert([$# >= 2])[]dnl
$1=[$]{$1[}]m4_if([$#], [2], [[$]{$1:+' '}$2], [[$]{$1:+$2}$3])[]dnl
])


AC_DEFUN([APNR_REQUIRE_PROG], [
m4_assert([$# == 2])[]dnl
AC_ARG_VAR(m4_toupper($1), $2)
AC_PATH_PROG(m4_toupper($1), $1)
AS_IF([test "x$m4_toupper($1)" = x],
	[AC_MSG_ERROR([Cannot find $2.])])
])

## APNR_FIND_DIR(dir-name, search-paths)
##
## Searches the <search-paths> directories for a sub-directory named <dir-name>.
## If that directory is found, a variable will be created with an upper-case
## version of <dir-name> with _DIR appended containing the path. For example
## if /path/to/foo is found, FOO_DIR will be set to /path/to/foo.
##
AC_DEFUN([APNR_FIND_DIR], [
m4_assert([$# == 2])[]dnl
AC_MSG_CHECKING([$1])
m4_toupper($1_DIR)=`for dir in $2; do
	test -d $dir/$1 && echo $dir/$1 && break; done`
echo ${m4_toupper($1_DIR)}
AC_SUBST(m4_toupper($1_DIR))
])


## APNR_FIND_ICEBOX
##
## Finds the path to the icebox directory in the standard prefix locations,
## unless ICEBOX_DIR is already set.
##
AC_DEFUN([APNR_FIND_ICEBOX], [
m4_assert([$# == 0])[]dnl
AS_IF([test "x$ICEBOX_DIR" = x], [APNR_FIND_DIR([icebox], 
	[$HOME/share /usr/share/ /usr/local/share])])
AS_IF([test "x$ICEBOX_DIR" = x], [AC_MSG_ERROR([icebox not found installed])])
])


## _APNR_PKG_VERSION_SET(var-prefix, pkg-name, tag-prefix, base-version, major, minor, [micro])
##
m4_define([_APNR_PKG_VERSION_SET],
[dnl
m4_assert([$# >= 6])[]dnl
$1=$4
sr_git_deps=
# Check if we can get revision information from git.
sr_head=`git -C "$srcdir" rev-parse --verify --short HEAD 2>&AS_MESSAGE_LOG_FD`

AS_IF([test "$?" = 0 && test "x$sr_head" != x], [dnl
	test ! -f "$srcdir/.git/HEAD" \
		|| sr_git_deps="$sr_git_deps \$(top_srcdir)/.git/HEAD"

	sr_head_name=`git -C "$srcdir" rev-parse --symbolic-full-name HEAD 2>&AS_MESSAGE_LOG_FD`
	AS_IF([test "$?" = 0 && test -f "$srcdir/.git/$sr_head_name"],
		[sr_git_deps="$sr_git_deps \$(top_srcdir)/.git/$sr_head_name"])

	# Append the revision hash unless we are exactly on a tagged release.
	git -C "$srcdir" describe --match "$3$4" \
		--exact-match >&AS_MESSAGE_LOG_FD 2>&AS_MESSAGE_LOG_FD \
		|| $1="[$]$1-git-$sr_head"
])
# Use $(wildcard) so that things do not break if for whatever
# reason these files do not exist anymore at make time.
AS_IF([test -n "$sr_git_deps"],
	[APNR_APPEND([CONFIG_STATUS_DEPENDENCIES], ["\$(wildcard$sr_git_deps)"])])
AC_SUBST([CONFIG_STATUS_DEPENDENCIES])[]dnl
AC_SUBST([$1])[]dnl
dnl
AC_DEFINE([$1_MAJOR], [$5], [Major version number of $2.])[]dnl
AC_DEFINE([$1_MINOR], [$6], [Minor version number of $2.])[]dnl
m4_ifval([$7], [AC_DEFINE([$1_MICRO], [$7], [Micro version number of $2.])])[]dnl
AC_DEFINE_UNQUOTED([$1_STRING], ["[$]$1"], [Version of $2.])[]dnl
])


## APNR_PKG_VERSION_SET(var-prefix, version-triple)
##
## Set up substitution variables and macro definitions for the package
## version components. Derive the version suffix from the repository
## revision if possible.
##
## Substitutions: <var-prefix>
## Macro defines: <var-prefix>_{MAJOR,MINOR,MICRO,STRING}
##
AC_DEFUN([APNR_PKG_VERSION_SET],
[dnl
m4_assert([$# >= 2])[]dnl
_APNR_PKG_VERSION_SET([$1],
	m4_defn([AC_PACKAGE_NAME]),
	m4_defn([AC_PACKAGE_TARNAME])[-],
	m4_expand([$2]),
	m4_unquote(m4_split(m4_expand([$2]), [\.])))
])
