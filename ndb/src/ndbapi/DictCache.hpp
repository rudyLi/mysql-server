/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef DictCache_H
#define DictCache_H

#include <ndb_types.h>
#include <kernel_types.h>
#include <NdbError.hpp>
#include <BaseString.hpp>
#include <Vector.hpp>
#include <UtilBuffer.hpp>
#include <NdbDictionary.hpp>
#include <Ndb.hpp>
#include "NdbLinHash.hpp"

/**
 * A non thread safe dict cache
 */
class LocalDictCache {
public:
  LocalDictCache();
  ~LocalDictCache();
  
  NdbTableImpl * get(const char * name);
  
  void put(const char * name, NdbTableImpl *);
  void drop(const char * name);
  
  NdbLinHash<NdbTableImpl> m_tableHash; // On name
};

/**
 * A thread safe dict cache
 */
class GlobalDictCache : public NdbLockable {
public:
  GlobalDictCache();
  ~GlobalDictCache();
  
  NdbTableImpl * get(const char * name);
  
  NdbTableImpl* put(const char * name, NdbTableImpl *);
  void drop(NdbTableImpl *);
  void release(NdbTableImpl *);
public:
  enum Status {
    OK = 0,
    DROPPED = 1,
    RETREIVING = 2
  };
  
private:
  struct TableVersion {
    Uint32 m_version;
    Uint32 m_refCount;
    NdbTableImpl * m_impl;
    Status m_status;
  };
  
  NdbLinHash<Vector<TableVersion> > m_tableHash;
  NdbCondition * m_waitForTableCondition;
};

#endif

