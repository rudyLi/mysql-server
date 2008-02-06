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

// Values.cpp: implementation of the Values class.
//
//////////////////////////////////////////////////////////////////////

// copyright (c) 1999 - 2000 by James A. Starkey


#include "Engine.h"
#include "Values.h"
#include "Value.h"


#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Values::Values()
{
	count = 0;
	values = NULL;
}

Values::~Values()
{
	if (values)
		delete [] values;
}

void Values::alloc(int number)
{
	if (number == count)
		{
		for (int n = 0; n < count; ++n)
			values [n].clear();
		return;
		}

	if (values)
		delete [] values;

	count = number;
	values = new Value [count];
}

void Values::clear()
{
	for (int n = 0; n < count; ++n)
		values [n].clear();
}

Value& Values::operator [](int n)
{
	return values [n];
}