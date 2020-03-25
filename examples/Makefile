################################################################################
## libasyncd
##
## Copyright (c) 2014 Seungyoung Kim.
## All rights reserved.
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are met:
##
## 1. Redistributions of source code must retain the above copyright notice,
##    this list of conditions and the following disclaimer.
## 2. Redistributions in binary form must reproduce the above copyright notice,
##    this list of conditions and the following disclaimer in the documentation
##    and/or other materials provided with the distribution.
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
## AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
## ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
## LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
## CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
## SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
## INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
## CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
## ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
## POSSIBILITY OF SUCH DAMAGE.
################################################################################

prefix		= /usr/local
exec_prefix	= ${prefix}

## Utilities
AR		= /usr/bin/ar
ARFLAGS		= -rc
CHMOD		= /bin/chmod
INSTALL		= /usr/bin/install -c
INSTALL_DATA	= ${INSTALL} -m 644
LN_S		= ln -s
MKDIR_P		= .././install-sh -c -d
RMDIR		= rmdir
RANLIB		= ranlib
RM		= /bin/rm
LDFLAGS =  -L/usr/lib -L/usr/local/lib -L/usr/local/opt/openssl/lib
## libasyncd related.
CPPFLAGS        += -I../include -I../src
LIBS            += ../src/libasyncd.a

## qlibc related.
CPPFLAGS        += -I../lib/qlibc/include
ifeq ($(shell uname), Darwin)
LIBS            += ../lib/qlibc/lib/libqlibc-static.a
endif
ifeq ($(shell uname), Linux)
LIBS            += ../lib/qlibc/lib/libqlibc.a
endif

## Which compiler & options for release
CC		= gcc -std=gnu99
CFLAGS		= -Wall -Wstrict-prototypes -fPIC -g -O2
CPPFLAGS	+=  -I/usr/include -I/usr/local/include -I/usr/local/opt/openssl/include \
		   -D_GNU_SOURCE -DBUILD_DEBUG 
LIBS		+=  -lpthread -levent -levent_pthreads -levent_openssl -lssl -lcrypto


TARGETS		= helloworld_tcp_server helloworld_http_server \
		  echo_tcp_server echo_http_server

## Make Library
all: $(TARGETS)

helloworld_tcp_server: helloworld_tcp_server.o
	$(CC) $(LDFLAGS) -o $@ helloworld_tcp_server.o $(LIBS) 

helloworld_http_server: helloworld_http_server.o
	$(CC) $(LDFLAGS) -o $@ helloworld_http_server.o $(LIBS) 

helloworld_ssl_server: helloworld_ssl_server.o
	$(CC) $(LDFLAGS) -o $@ helloworld_ssl_server.o $(LIBS) 

echo_tcp_server: echo_tcp_server.o
	$(CC) $(LDFLAGS) -o $@ echo_tcp_server.o $(LIBS) 

echo_http_server: echo_http_server.o
	$(CC) $(LDFLAGS) -o $@ echo_http_server.o $(LIBS) 

clean:
	$(RM) -f *.o $(TARGETS)

## Compile
.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
