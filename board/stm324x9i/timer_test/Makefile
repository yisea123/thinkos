# File:		Makefile
# Module: 
# Target:
# Comment:  
# Copyright(C) 2012 Bob Mittmann. All Rights Reserved.
# 
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
# 


PROG = timer_test

CFILES = timer_test.c

SDK = ../../../sdk

LIBDIRS =  $(SDK)/libcm3 $(SDK)/libstm32f $(SDK)/libconsole \
		   $(SDK)/libutil $(SDK)/libc

LIBS = cm3 stm32f console util c 

INCPATH = $(SDK)/include ..

CFLAGS = -g -O1 -Wall

MACH = stm32f429
THINKOS = 1
include ../../../mk/stm32f4xx.mk

