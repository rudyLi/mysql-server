/* Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef _CMD_GEN_H_
#define _CMD_GEN_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "string.h"

static const int hunkLength = 1024;

struct CmdHunk {
	CmdHunk*	next;
	char		data[hunkLength];
	};

class CmdGen
{
public:
	CmdGen(void);
	~CmdGen(void);
	void gen(const char* command, ...);
	void put(const char* command);
	
	char	*ptr;
	char	*temp;
	char	buffer[hunkLength];
	CmdHunk	*hunks;
	CmdHunk	*currentHunk;
	size_t	remaining;
	size_t	totalLength;
	const char* getString(void);
	void reset(void);
	void init(void);
};

#endif
