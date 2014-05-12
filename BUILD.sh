#!/bin/bash

# for local tests
# BEGIN CUT
PREFIX=$HOME/opt/dovecot-2.2-build
EPREFIX=$PREFIX
# END CUT

PREFIX=${PREFIX:-/opt/dovecot-2.2-build}
EPREFIX=${EPREFIX:-/opt/dovecot-2.2-build}

CFLAGS="-O0 -I$PREFIX/include -g"
CXXFLAGS="-O0 -I$PREFIX/include -g"
CPPFLAGS="-O0 -I$PREFIX/include -g"
LDFLAGS="-Wl,-rpath -Wl,$EPREFIX/lib -L$EPREFIX/lib"
export CFLAGS CXXFLAGS CPPFLAGS LDFLAGS

# make sure we find anything we preinstall
export PATH=$EPREFIX/bin:$PATH

#autoconf-1.14.1
#automake-1.14.1
./configure \
	--prefix=$PREFIX \
	--exec-prefix=$EPREFIX \
	--with-sqlite \
	--with-ldap
