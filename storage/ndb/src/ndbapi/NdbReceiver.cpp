/* Copyright (C) 2003 MySQL AB

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

#include <ndb_global.h>
#include "NdbImpl.hpp"
#include <NdbReceiver.hpp>
#include "NdbDictionaryImpl.hpp"
#include <NdbRecAttr.hpp>
#include <AttributeHeader.hpp>
#include <NdbTransaction.hpp>
#include <TransporterFacade.hpp>
#include <signaldata/TcKeyConf.hpp>

NdbReceiver::NdbReceiver(Ndb *aNdb) :
  theMagicNumber(0),
  m_ndb(aNdb),
  m_id(NdbObjectIdMap::InvalidId),
  m_type(NDB_UNINITIALIZED),
  m_owner(0),
  m_using_ndb_record(false)
{
  theCurrentRecAttr = theFirstRecAttr = 0;
  m_defined_rows = 0;
  m_rows = new NdbRecAttr*[0];
}
 
NdbReceiver::~NdbReceiver()
{
  DBUG_ENTER("NdbReceiver::~NdbReceiver");
  if (m_id != NdbObjectIdMap::InvalidId) {
    m_ndb->theImpl->theNdbObjectIdMap.unmap(m_id, this);
  }
  delete[] m_rows;
  DBUG_VOID_RETURN;
}

void
NdbReceiver::init(ReceiverType type, bool useRec, void* owner)
{
  theMagicNumber = 0x11223344;
  m_type = type;
  m_using_ndb_record= useRec;
  m_owner = owner;
  if (m_id == NdbObjectIdMap::InvalidId) {
    if (m_ndb)
      m_id = m_ndb->theImpl->theNdbObjectIdMap.map(this);
  }

  if (useRec)
  {
    m_ndb_record= NULL;
    m_row= NULL;
    m_row_buffer= NULL;
    m_row_offset= 0;
    m_read_range_no= false;
  }
  else
  {
    theFirstRecAttr = NULL;
    theCurrentRecAttr = NULL;
  }
}

void
NdbReceiver::release(){
  if (!m_using_ndb_record)
  {
    NdbRecAttr* tRecAttr = theFirstRecAttr;
    while (tRecAttr != NULL)
    {
      NdbRecAttr* tSaveRecAttr = tRecAttr;
      tRecAttr = tRecAttr->next();
      m_ndb->releaseRecAttr(tSaveRecAttr);
    }
    theFirstRecAttr = NULL;
    theCurrentRecAttr = NULL;
  }
  else
  {
    delete[] m_row_buffer;
    m_row_buffer= NULL;
  }      
}
  
NdbRecAttr *
NdbReceiver::getValue(const NdbColumnImpl* tAttrInfo, char * user_dst_ptr){
  assert(!m_using_ndb_record);

  NdbRecAttr* tRecAttr = m_ndb->getRecAttr();
  if(tRecAttr && !tRecAttr->setup(tAttrInfo, user_dst_ptr)){
    if (theFirstRecAttr == NULL)
      theFirstRecAttr = tRecAttr;
    else
      theCurrentRecAttr->next(tRecAttr);
    theCurrentRecAttr = tRecAttr;
    tRecAttr->next(NULL);
    return tRecAttr;
  }
  if(tRecAttr){
    m_ndb->releaseRecAttr(tRecAttr);
  }    
  return 0;
}

void
NdbReceiver::getValues(const NdbRecord* rec, char *row_ptr)
{
  assert(m_using_ndb_record);

  m_ndb_record= rec;
  m_row= row_ptr;
}

#define KEY_ATTR_ID (~(Uint32)0)

/*
  Compute the batch size (rows between each NEXT_TABREQ / SCAN_TABCONF) to
  use, taking into account limits in the transporter, user preference, etc.

  Hm, there are some magic overhead numbers (4 bytes/attr, 32 bytes/row) here,
  would be nice with some explanation on how these numbers were derived.
*/
void
NdbReceiver::calculate_batch_size(Uint32 key_size,
                                  Uint32 parallelism,
                                  Uint32& batch_size,
                                  Uint32& batch_byte_size,
                                  Uint32& first_batch_size)
{
  TransporterFacade *tp= m_ndb->theImpl->m_transporter_facade;
  Uint32 max_scan_batch_size= tp->get_scan_batch_size();
  Uint32 max_batch_byte_size= tp->get_batch_byte_size();
  Uint32 max_batch_size= tp->get_batch_size();
  Uint32 tot_size= (key_size ? (key_size + 32) : 0); //key + signal overhead
  /* ToDo: Use the NdbRecord to calculate size here instead for NdbRecord operation. */
  NdbRecAttr *rec_attr= theFirstRecAttr;
  while (rec_attr != NULL) {
    Uint32 attr_size= rec_attr->getColumn()->getSizeInBytes();
    attr_size= ((attr_size + 7) >> 2) << 2; //Even to word + overhead
    tot_size+= attr_size;
    rec_attr= rec_attr->next();
  }
  tot_size+= 32; //include signal overhead

  /**
   * Now we calculate the batch size by trying to get upto SCAN_BATCH_SIZE
   * bytes sent for each batch from each node. We do however ensure that
   * no more than MAX_SCAN_BATCH_SIZE is sent from all nodes in total per
   * batch.
   */
  if (batch_size == 0)
  {
    batch_byte_size= max_batch_byte_size;
  }
  else
  {
    batch_byte_size= batch_size * tot_size;
  }
  
  if (batch_byte_size * parallelism > max_scan_batch_size) {
    batch_byte_size= max_scan_batch_size / parallelism;
  }
  batch_size= batch_byte_size / tot_size;
  if (batch_size == 0) {
    batch_size= 1;
  } else {
    if (batch_size > max_batch_size) {
      batch_size= max_batch_size;
    } else if (batch_size > MAX_PARALLEL_OP_PER_SCAN) {
      batch_size= MAX_PARALLEL_OP_PER_SCAN;
    }
  }
  first_batch_size= batch_size;
  return;
}

/*
  Call getValue() on all required attributes in all rows in the batch to be
  received.
*/
void
NdbReceiver::do_get_value(NdbReceiver * org, 
			  Uint32 batch_size, 
			  Uint32 key_size,
			  Uint32 range_no){
  assert(!m_using_ndb_record);
  if(batch_size > m_defined_rows){
    delete[] m_rows;
    m_defined_rows = batch_size;
    m_rows = new NdbRecAttr*[batch_size + 1]; 
  }
  m_rows[batch_size] = 0;
  
  NdbColumnImpl key;
  if(key_size){
    key.m_attrId = KEY_ATTR_ID;
    /*
      We need to add one extra word to key size due to extra info word at end.
    */
    key.m_arraySize = key_size+1;
    key.m_attrSize = 4;
    key.m_nullable = true; // So that receive works w.r.t KEYINFO20
  }
  m_hidden_count = (key_size ? 1 : 0) + range_no ;
  
  for(Uint32 i = 0; i<batch_size; i++){
    NdbRecAttr * prev = theCurrentRecAttr;
    assert(prev == 0 || i > 0);
    
    // Put key-recAttr fir on each row
    if(key_size && !getValue(&key, (char*)0)){
      abort();
      return ; // -1
    }
    
    if(range_no && 
       !getValue(&NdbColumnImpl::getImpl(* NdbDictionary::Column::RANGE_NO),0))
    {
      abort();
    }

    NdbRecAttr* tRecAttr = org->theFirstRecAttr;
    while(tRecAttr != 0){
      if(getValue(&NdbColumnImpl::getImpl(*tRecAttr->m_column), (char*)0) != 0)
	tRecAttr = tRecAttr->next();
      else
	break;
    }
    
    if(tRecAttr){
      abort();
      return ;// -1;
    }

    // Store first recAttr for each row in m_rows[i]
    if(prev){
      m_rows[i] = prev->next();
    } else {
      m_rows[i] = theFirstRecAttr;
    }
  } 

  prepareSend();
  return;
}

int
NdbReceiver::do_setup_ndbrecord(const NdbRecord *ndb_record, Uint32 batch_size,
                                Uint32 key_size, Uint32 read_range_no)
{
  Uint32 rowsize= ndb_record->m_row_size;
  /* Room for range_no. */
  if (read_range_no)
    rowsize+= 4;
  /*
    If keyinfo, need room for max. key + 4 bytes of actual key length + 4
    bytes of scan info (all from KEYINFO20 signal).
  */
  if (key_size)
    rowsize+= 8 + key_size*4;
  /* Ensure 4-byte alignment. */
  rowsize= (rowsize+3) & 0xfffffffc;

  char *row_buffer= new char[batch_size*rowsize];
  if (!row_buffer)
    return -1;

  m_using_ndb_record= true;
  m_ndb_record= ndb_record;
  m_row= row_buffer;
  m_row_buffer= row_buffer;
  m_row_offset= rowsize;
  m_read_range_no= read_range_no;

  return 0;
}

NdbRecAttr*
NdbReceiver::copyout(NdbReceiver & dstRec){
  assert(!m_using_ndb_record);
  NdbRecAttr *src = m_rows[m_current_row++];
  NdbRecAttr *dst = dstRec.theFirstRecAttr;
  NdbRecAttr *start = src;
  Uint32 tmp = m_hidden_count;
  while(tmp--)
    src = src->next();
  
  while(dst){
    Uint32 len = src->get_size_in_bytes();
    dst->receive_data((Uint32*)src->aRef(), len);
    src = src->next();
    dst = dst->next();
  }

  return start;
}

int
NdbReceiver::get_range_no() const
{
  int range_no;
  assert(m_using_ndb_record);
  Uint32 idx= m_current_row;
  if (idx == 0 || !m_read_range_no)
    return -1;
  memcpy(&range_no,
         m_row_buffer + (idx-1)*m_row_offset + m_ndb_record->m_row_size,
         4);
  return range_no;
}

int
NdbReceiver::get_keyinfo20(Uint32 & scaninfo, Uint32 & length,
                           const char * & data_ptr) const
{
  assert(m_using_ndb_record);
  Uint32 idx= m_current_row;
  if (idx == 0)
    return -1;                                  // No rows fetched yet
  const char *p= m_row_buffer + (idx-1)*m_row_offset + m_ndb_record->m_row_size;
  if (m_read_range_no)
    p+= 4;
  scaninfo= uint4korr(p);
  p+= 4;
  length= uint4korr(p);
  p+= 4;
  data_ptr= p;
  return 0;
}

/* Set NdbRecord field to non-NULL value. */
static void assignToRec(const NdbRecord::Attr *col,
                        char *row,
                        const Uint32 *src,
                        Uint32 byteSize)
{
  /* Set NULLable attribute to "not NULL". */
  if (col->flags & NdbRecord::IsNullable)
    row[col->nullbit_byte_offset]&= ~(1 << col->nullbit_bit_in_byte);

  memcpy(&row[col->offset], src, byteSize);
}

/* Set NdbRecord field to NULL. */
static void setRecToNULL(const NdbRecord::Attr *col,
                         char *row)
{
  assert(col->flags & NdbRecord::IsNullable);
  row[col->nullbit_byte_offset]|= 1 << col->nullbit_bit_in_byte;
}

int
NdbReceiver::execTRANSID_AI(const Uint32* aDataPtr, Uint32 aLength)
{
  if (m_using_ndb_record)
  {
    Uint32 exp= m_expected_result_length;
    Uint32 tmp= m_received_result_length + aLength;
    const NdbRecord *rec= m_ndb_record;
    Uint32 rec_pos= 0;

    while (aLength > 0)
    {
      AttributeHeader ah(* aDataPtr++);
      const Uint32 attrId= ah.getAttributeId();
      Uint32 attrSize= ah.getByteSize();
      aLength--;

      /* Special case for RANGE_NO, which is stored just after the row. */
      if (attrId==AttributeHeader::RANGE_NO)
      {
        assert(m_read_range_no);
        assert(attrSize==4);
        memcpy(m_row+m_ndb_record->m_row_size, aDataPtr++, 4);
        aLength--;
        continue;
      }

      /*
        Skip all not returned columns.
        The rows should be returned in the same order as we requested them
        (which is in any case in attribute ID order).
      */
      while (rec_pos < rec->noOfColumns &&
             rec->columns[rec_pos].attrId < attrId)
        rec_pos++;

      /* We should never get back an attribute not originally requested. */
      assert(rec_pos < rec->noOfColumns &&
             rec->columns[rec_pos].attrId == attrId);

      if (attrSize == 0)
      {
        setRecToNULL(&rec->columns[rec_pos], m_row);
      }
      else
      {
        assert(attrSize <= rec->columns[rec_pos].maxSize);
        Uint32 sizeInWords= (attrSize+3)>>2;
        /* Not sure how to deal with this, shouldn't happen. */
        if (unlikely(sizeInWords > aLength))
        {
          sizeInWords= aLength;
          attrSize= 4*aLength;
        }

        assignToRec(&rec->columns[rec_pos], m_row, aDataPtr, attrSize);
        aDataPtr+= sizeInWords;
        aLength-= sizeInWords;
      }
      rec_pos++;
    }

    m_received_result_length = tmp;
    m_row+= m_row_offset;

    return (tmp == exp || (exp > TcKeyConf::SimpleReadBit) ? 1 : 0);
  }

  /* The old way, using getValue() and NdbRecAttr. */
  NdbRecAttr* currRecAttr = theCurrentRecAttr;
  
  for (Uint32 used = 0; used < aLength ; used++){
    AttributeHeader ah(* aDataPtr++);
    const Uint32 tAttrId = ah.getAttributeId();
    const Uint32 tAttrSize = ah.getByteSize();

    /**
     * Set all results to NULL if  not found...
     */
    while(currRecAttr && currRecAttr->attrId() != tAttrId){
      currRecAttr = currRecAttr->next();
    }
    
    if(currRecAttr && currRecAttr->receive_data(aDataPtr, tAttrSize)){
      Uint32 add= (tAttrSize + 3) >> 2;
      used += add;
      aDataPtr += add;
      currRecAttr = currRecAttr->next();
    } else {
      /*
        This should not happen: we got back an attribute for which we have no
        stored NdbRecAttr recording that we requested said attribute (or we got
        back attributes in the wrong order).
        So dump some info for debugging, and abort.
      */
      ndbout_c("this=%p: tAttrId: %d currRecAttr: %p theCurrentRecAttr: %p "
               "tAttrSize: %d %d", this,
	       tAttrId, currRecAttr, theCurrentRecAttr, tAttrSize,
               currRecAttr ? currRecAttr->get_size_in_bytes() : 0);
      currRecAttr = theCurrentRecAttr;
      while(currRecAttr != 0){
	ndbout_c("%d ", currRecAttr->attrId());
	currRecAttr = currRecAttr->next();
      }
      abort();
      return -1;
    }
  }

  theCurrentRecAttr = currRecAttr;
  
  /**
   * Update m_received_result_length
   */
  Uint32 exp = m_expected_result_length; 
  Uint32 tmp = m_received_result_length + aLength;
  m_received_result_length = tmp;

  return (tmp == exp || (exp > TcKeyConf::SimpleReadBit) ? 1 : 0);
}

int
NdbReceiver::execKEYINFO20(Uint32 info, const Uint32* aDataPtr, Uint32 aLength)
{
  if (m_using_ndb_record)
  {
    /* Copy in the keyinfo after the user row and any range_no value. */

    char *keyinfo_ptr= m_row_buffer + 
                       m_current_row++ * m_row_offset +
                       m_ndb_record->m_row_size;
    if (m_read_range_no)
      keyinfo_ptr+= 4;

    int4store(keyinfo_ptr, info);
    keyinfo_ptr+= 4;
    int4store(keyinfo_ptr, aLength);
    keyinfo_ptr+= 4;
    memcpy(keyinfo_ptr, aDataPtr, 4*aLength);

    Uint32 tmp= m_received_result_length + aLength;
    m_received_result_length = tmp;
  
    return (tmp == m_expected_result_length ? 1 : 0);
  }

  /* The old method, using NdbRecAttr. */
  NdbRecAttr* currRecAttr = m_rows[m_current_row++];
  assert(currRecAttr->attrId() == KEY_ATTR_ID);
  /*
    This is actually reading data one word off the end of the received
    signal (or off the end of the long signal data section 0, for a
    long signal), due to the aLength+1. This is to ensure the correct length
    being set for the NdbRecAttr (one extra word for the scanInfo word placed
    at the end), overwritten immediately below.
    But it's a bit ugly that we rely on being able to read one word over the
    end of the signal without crashing...
  */
  currRecAttr->receive_data(aDataPtr, 4*(aLength + 1));
  
  /**
   * Save scanInfo in the end of keyinfo
   */
  ((Uint32*)currRecAttr->aRef())[aLength] = info;
  
  Uint32 tmp = m_received_result_length + aLength;
  m_received_result_length = tmp;
  
  return (tmp == m_expected_result_length ? 1 : 0);
}

void
NdbReceiver::setErrorCode(int code)
{
  theMagicNumber = 0;
  NdbOperation* op = (NdbOperation*)getOwner();
  op->setErrorCode(code);
}
