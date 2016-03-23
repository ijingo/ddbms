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
#include "manager.h"
#include "thread.h"
#include "io_thread.h"
#include "query.h"
#include "ycsb_query.h"
#include "tpcc_query.h"
#include "mem_alloc.h"
#include "transport.h"
#include "math.h"
#include "msg_thread.h"
#include "msg_queue.h"
#include "message.h"
#include "client_txn.h"
#include "work_queue.h"

void InputThread::setup() {

  std::vector<Message*> msgs;
  while(!simulation->is_setup_done()) {
    msgs = tport_man.recv_msg();
    while(!msgs.empty()) {
      Message * msg = msgs.front();
      if(msg->rtype == INIT_DONE) {
        printf("Received INIT_DONE from node %ld\n",msg->return_node_id);
        fflush(stdout);
        simulation->process_setup_msg();
      } else {
        assert(ISSERVER);
        work_queue.enqueue(g_thread_cnt,msg,false);
      }
      msgs.erase(msgs.begin());
    }
  }
}

RC InputThread::run() {
  tsetup();

  if(ISCLIENT) {
    client_recv_loop();
  } else {
    server_recv_loop();
  }

  return FINISH;

}

RC InputThread::client_recv_loop() {
	int rsp_cnts[g_servers_per_client];
	memset(rsp_cnts, 0, g_servers_per_client * sizeof(int));

	run_starttime = get_sys_clock();
  uint64_t return_node_offset;
  uint64_t inf;

  std::vector<Message*> msgs;

	while (!simulation->is_done()) {
		msgs = tport_man.recv_msg();
    //while((m_query = work_queue.get_next_query(get_thd_id())) != NULL) {
    //Message * msg = work_queue.dequeue();
    while(!msgs.empty()) {
      Message * msg = msgs.front();
			assert(msg->rtype == CL_RSP);
      return_node_offset = msg->return_node_id - g_server_start_node;
      assert(return_node_offset < g_servers_per_client);
      rsp_cnts[return_node_offset]++;
      inf = client_man.dec_inflight(return_node_offset);
      assert(inf >=0);
      // TODO: delete message
      msgs.erase(msgs.begin());
    }

	}

  printf("FINISH %ld:%ld\n",_node_id,_thd_id);
  fflush(stdout);
  return FINISH;
}

RC InputThread::server_recv_loop() {

	myrand rdm;
	rdm.init(get_thd_id());
	RC rc = RCOK;
	assert (rc == RCOK);

  uint64_t thd_prof_start;
  std::vector<Message*> msgs;
	while (!simulation->is_done()) {
    thd_prof_start = get_sys_clock();
		msgs = tport_man.recv_msg();
    while(!msgs.empty()) {
      Message * msg = msgs.front();
      work_queue.enqueue(g_thread_cnt,msg,false);
      msgs.erase(msgs.begin());
    }
    INC_STATS(_thd_id,rthd_prof_1,get_sys_clock() - thd_prof_start);

	}
  printf("FINISH %ld:%ld\n",_node_id,_thd_id);
  fflush(stdout);
  return FINISH;
}

void OutputThread::setup() {
  messager = (MessageThread *) mem_allocator.alloc(sizeof(MessageThread));
  messager->init(_thd_id);
	while (!simulation->is_setup_done()) {
    messager->run();
  }
}

RC OutputThread::run() {

  tsetup();

	while (!simulation->is_done()) {
    messager->run();
  }

  printf("FINISH %ld:%ld\n",_node_id,_thd_id);
  fflush(stdout);
  return FINISH;
}

