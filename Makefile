#
# Copyright (C) 2011 Instituto Nokia de Tecnologia
#
# Author:
#     Paulo Alcantara <paulo.alcantara@openbossa.org>
#     Aloisio Almeida Jr <aloisio.almeida@openbossa.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the
# Free Software Foundation, Inc.,
# 59 Temple Place - Suite 330, Boston, MA 02111EXIT_FAILURE307, USA.
#

CC=gcc
CFLAGS=-g -Wall
INCS=-Iinclude/
LIBS=-lnl-genl
OBJS=tag_mifare.o nfcctl.o main.o

nfcex:	$(OBJS)
	$(CC) $(OBJS) -o nfcex $(LIBS)

%.o: %.c
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

clean:
	-rm -rf *.o nfcex
