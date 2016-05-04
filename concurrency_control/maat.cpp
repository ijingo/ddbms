/*
   Copyright 2015 Rachael Harding

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "global.h"
#include "helper.h"
#include "txn.h"
#include "maat.h"
#include "manager.h"
#include "mem_alloc.h"
#include "row_maat.h"

void Maat::init() {
  sem_init(&_semaphore, 0, 1);
}

RC Maat::validate(TxnManager * txn) {
  uint64_t start_time = get_sys_clock();
  sem_wait(&_semaphore);

  INC_STATS(txn->get_thd_id(),maat_cs_wait_time,get_sys_clock() - start_time);
  start_time = get_sys_clock();
  RC rc = RCOK;
  uint64_t lower = time_table.get_lower(txn->get_txn_id());
  uint64_t upper = time_table.get_upper(txn->get_txn_id());
  DEBUG("MAAT Validate Start %ld: [%lu,%lu]\n",txn->get_txn_id(),lower,upper);
  std::set<uint64_t> after;
  std::set<uint64_t> before;
  // lower bound of txn greater than write timestamp
  if(lower <= txn->greatest_write_timestamp) {
    lower = txn->greatest_write_timestamp + 1;
    DEBUG("MAAT %ld: case1 %ld\n",txn->get_txn_id(),lower);
    INC_STATS(txn->get_thd_id(),maat_case1_cnt,1);
  }
  // lower bound of uncommitted writes greater than upper bound of txn
  for(auto it = txn->uncommitted_writes->begin(); it != txn->uncommitted_writes->end();it++) {
    uint64_t it_lower = time_table.get_lower(*it);
    if(upper >= it_lower) {
      MAATState state = time_table.get_state(*it);
      if(state == MAAT_VALIDATED || state == MAAT_COMMITTED) {
        DEBUG("MAAT case2 %ld: %lu < %ld: %lu, %d \n",txn->get_txn_id(),upper,*it,it_lower,state);
        INC_STATS(txn->get_thd_id(),maat_case2_cnt,1);
        if(it_lower > 0) {
          upper = it_lower - 1;
        } else {
          upper = it_lower;
        }
      }
      /*
      if(state == MAAT_ABORTED) {
        //FIXME: potential for race condition if time table entry removed and reinstalled
        time_table.set_lower(*it,upper-1);
      }
      */
      if(state == MAAT_RUNNING) {
        after.insert(*it);
      }
    }
  }
  // lower bound of txn greater than read timestamp
  if(lower <= txn->greatest_read_timestamp) {
    lower = txn->greatest_read_timestamp + 1;
    DEBUG("MAAT %ld: case3 %ld\n",txn->get_txn_id(),lower);
    INC_STATS(txn->get_thd_id(),maat_case3_cnt,1);
  }
  // upper bound of uncommitted reads less than lower bound of txn
  for(auto it = txn->uncommitted_reads->begin(); it != txn->uncommitted_reads->end();it++) {
    uint64_t it_upper = time_table.get_upper(*it);
    if(lower <= it_upper) {
      MAATState state = time_table.get_state(*it);
      if(state == MAAT_VALIDATED || state == MAAT_COMMITTED) {
        DEBUG("MAAT case4 %ld: %lu > %ld: %lu, %d \n",txn->get_txn_id(),lower,*it,it_upper,state);
        INC_STATS(txn->get_thd_id(),maat_case4_cnt,1);
        if(it_upper < UINT64_MAX) {
          lower = it_upper + 1;
        } else {
          lower = it_upper;
        }
      }
      /*
      if(state == MAAT_ABORTED) {
        time_table.set_upper(*it,lower+1);
      }
      */
      if(state == MAAT_RUNNING) {
        before.insert(*it);
      }
    }
  }
  // upper bound of uncommitted write writes less than lower bound of txn
  for(auto it = txn->uncommitted_writes_y->begin(); it != txn->uncommitted_writes_y->end();it++) {
      MAATState state = time_table.get_state(*it);
    uint64_t it_upper = time_table.get_upper(*it);
      if(state == MAAT_ABORTED) {
        continue;
      }
      if(state == MAAT_VALIDATED || state == MAAT_COMMITTED) {
        if(lower <= it_upper) {
          DEBUG("MAAT case5 %ld: %lu > %ld: %lu, %d \n",txn->get_txn_id(),lower,*it,it_upper,state);
          INC_STATS(txn->get_thd_id(),maat_case5_cnt,1);
          if(it_upper < UINT64_MAX) {
            lower = it_upper + 1;
          } else {
            lower = it_upper;
          }
        }
      }
      if(state == MAAT_RUNNING) {
        after.insert(*it);
      }
  }
  if(lower >= upper) {
    // Abort
    time_table.set_state(txn->get_txn_id(),MAAT_ABORTED);
    rc = Abort;
  } else {
    // Validated
    time_table.set_state(txn->get_txn_id(),MAAT_VALIDATED);
    rc = RCOK;
    // TODO: Optimize lower and upper to minimize aborts in before and after
    // Hopefully this would keep upper from being max int...
    for(auto it = after.begin(); it != after.end();it++) {
      uint64_t it_lower = time_table.get_lower(*it);
      if(it_lower < upper && it_lower > lower+1) {
        upper = it_lower - 1;
      }
    }
    for(auto it = before.begin(); it != before.end();it++) {
      uint64_t it_upper = time_table.get_upper(*it);
      if(it_upper > lower && it_upper < upper-1) {
        lower = it_upper + 1;
      }
    }
    assert(lower < upper);
    INC_STATS(txn->get_thd_id(),maat_range,upper-lower);
    INC_STATS(txn->get_thd_id(),maat_commit_cnt,1);
  }
  time_table.set_lower(txn->get_txn_id(),lower);
  time_table.set_upper(txn->get_txn_id(),upper);
  INC_STATS(txn->get_thd_id(),maat_validate_cnt,1);
  INC_STATS(txn->get_thd_id(),maat_validate_time,get_sys_clock() - start_time);
  DEBUG("MAAT Validate End %ld: %d [%lu,%lu]\n",txn->get_txn_id(),rc==RCOK,lower,upper);
  sem_post(&_semaphore);
  return rc;

}

RC Maat::find_bound(TxnManager * txn) {
  RC rc = RCOK;
  uint64_t lower = time_table.get_lower(txn->get_txn_id());
  uint64_t upper = time_table.get_upper(txn->get_txn_id());
  if(lower >= upper) {
    time_table.set_state(txn->get_txn_id(),MAAT_VALIDATED);
    rc = Abort;
  } else {
    time_table.set_state(txn->get_txn_id(),MAAT_COMMITTED);
    // TODO: pick commit_time in a smarter way
    //txn->commit_timestamp = (upper - lower) / 2;
    txn->commit_timestamp = lower; 
  }
  DEBUG("MAAT Bound %ld: %d [%lu,%lu] %lu\n",txn->get_txn_id(),rc,lower,upper,txn->commit_timestamp);
  return rc;
}

void TimeTable::init() {
  table_size = g_inflight_max * g_node_cnt * 2 + 1;
  DEBUG_M("TimeTable::init table alloc\n");
  table = (TimeTableNode*) mem_allocator.alloc(sizeof(TimeTableNode) * table_size);
  for(uint64_t i = 0; i < table_size;i++) {
    table[i].init();
  }
}

uint64_t TimeTable::hash(uint64_t key) {
  return key % table_size;
}

TimeTableEntry* TimeTable::find(uint64_t key) {
  TimeTableEntry * entry = table[hash(key)].head;
  while(entry) {
    if(entry->key == key) 
      break;
    entry = entry->next;
  }
  return entry;

}

void TimeTable::init(uint64_t key) {
  uint64_t idx = hash(key);
  pthread_mutex_lock(&table[idx].mtx);
  TimeTableEntry* entry = find(key);
  if(!entry) {
    DEBUG_M("TimeTable::init entry alloc\n");
    entry = (TimeTableEntry*) mem_allocator.alloc(sizeof(TimeTableEntry));
    entry->init(key);
    LIST_PUT_TAIL(table[idx].head,table[idx].tail,entry);
  }
  pthread_mutex_unlock(&table[idx].mtx);
}

void TimeTable::release(uint64_t key) {
  uint64_t idx = hash(key);
  pthread_mutex_lock(&table[idx].mtx);
  TimeTableEntry* entry = find(key);
  if(entry) {
    LIST_REMOVE_HT(entry,table[idx].head,table[idx].tail);
    DEBUG_M("TimeTable::release entry free\n");
    mem_allocator.free(entry,sizeof(TimeTableEntry));
  }
  pthread_mutex_unlock(&table[idx].mtx);
}

uint64_t TimeTable::get_lower(uint64_t key) {
  uint64_t idx = hash(key);
  uint64_t value = 0;
  pthread_mutex_lock(&table[idx].mtx);
  TimeTableEntry* entry = find(key);
  if(entry) {
    value = entry->lower;
  }
  pthread_mutex_unlock(&table[idx].mtx);
  return value;
}

uint64_t TimeTable::get_upper(uint64_t key) {
  uint64_t idx = hash(key);
  uint64_t value = UINT64_MAX;
  pthread_mutex_lock(&table[idx].mtx);
  TimeTableEntry* entry = find(key);
  if(entry) {
    value = entry->upper;
  }
  pthread_mutex_unlock(&table[idx].mtx);
  return value;
}


void TimeTable::set_lower(uint64_t key, uint64_t value) {
  uint64_t idx = hash(key);
  pthread_mutex_lock(&table[idx].mtx);
  TimeTableEntry* entry = find(key);
  if(entry) {
    entry->lower = value;
  }
  pthread_mutex_unlock(&table[idx].mtx);
}

void TimeTable::set_upper(uint64_t key, uint64_t value) {
  uint64_t idx = hash(key);
  pthread_mutex_lock(&table[idx].mtx);
  TimeTableEntry* entry = find(key);
  if(entry) {
    entry->upper = value;
  }
  pthread_mutex_unlock(&table[idx].mtx);
}

MAATState TimeTable::get_state(uint64_t key) {
  uint64_t idx = hash(key);
  MAATState state = MAAT_ABORTED;
  pthread_mutex_lock(&table[idx].mtx);
  TimeTableEntry* entry = find(key);
  if(entry) {
    state = entry->state;
  }
  pthread_mutex_unlock(&table[idx].mtx);
  return state;
}

void TimeTable::set_state(uint64_t key, MAATState value) {
  uint64_t idx = hash(key);
  pthread_mutex_lock(&table[idx].mtx);
  TimeTableEntry* entry = find(key);
  if(entry) {
    entry->state = value;
  }
  pthread_mutex_unlock(&table[idx].mtx);
}