import itertools
import math
# Experiments to run and analyze
# Go to end of file to fill in experiments 

# Format: [#Nodes,#Txns,Workload,CC_ALG,MPR]
fmt_tpcc = [["NODE_CNT","MAX_TXN_PER_PART","WORKLOAD","CC_ALG","MPR","THREAD_CNT","NUM_WH","MAX_TXN_IN_FLIGHT"]]
fmt_nd = [["NODE_CNT","MAX_TXN_PER_PART","WORKLOAD","CC_ALG","MPR","THREAD_CNT","NUM_WH","MAX_TXN_IN_FLIGHT","NETWORK_DELAY"]]
fmt_ycsb = [["CLIENT_NODE_CNT","NODE_CNT","MAX_TXN_PER_PART","WORKLOAD","CC_ALG","MPR","CLIENT_THREAD_CNT","THREAD_CNT","MAX_TXN_IN_FLIGHT","ZIPF_THETA","READ_PERC","WRITE_PERC","PART_CNT"]]
fmt_nt = [["NODE_CNT","CLIENT_NODE_CNT","NETWORK_TEST"]]


def test():
    fmt = fmt_ycsb
    exp = [
    [1,2,50000,'YCSB','WAIT_DIE',50,1,1,10,0.6,0.5,0.5],
    ]
    return fmt[0],exp

# Performance: throughput vs. node count
# Vary: Node count, % writes
def experiment_1():
    fmt = fmt_ycsb
    nnodes = [1,2,4,8]
    nmpr=[0,0.01,0.1,1]
    nalgos=['WAIT_DIE']
    #nalgos=['WAIT_DIE','HSTORE','HSTORE_SPEC']
    #nalgos=['WAIT_DIE','NO_WAIT','OCC','MVCC','HSTORE','HSTORE_SPEC','VLL','TIMESTAMP']
    nthreads=[3,1]
    ncthreads=[4]
    ntifs=[1000]
    nzipf=[0.6]
    nwr_perc=[0.5,0.0]
    ntxn=2000000
    exp = [[int(math.ceil(n/2)) if n > 1 else 1,n,ntxn,'YCSB',cc,m,ct,t,tif,z,1.0-wp,wp] for ct,t,tif,z,wp,m,cc,n in itertools.product(ncthreads,nthreads,ntifs,nzipf,nwr_perc,nmpr,nalgos,nnodes)]
    return fmt[0],exp

def experiment_1_plot(summary,summary_client):
    from plot_helper import tput,abort_rate,lat
    fmt = fmt_ycsb
    nnodes = [1,2,4,8]
    nmpr=[0,0.01,0.1,1]
    nalgos=['WAIT_DIE']
    #nalgos=['WAIT_DIE','HSTORE','HSTORE_SPEC']
    #nalgos=['WAIT_DIE','NO_WAIT','OCC','MVCC','HSTORE','HSTORE_SPEC','VLL','TIMESTAMP']
    nthreads=[1,3]
    ncthreads=[4]
    ntifs=[1000]
    nzipf=[0.6]
    nwr_perc=[0.0,0.5]
    ntxn=2000000
    for wr,tif,cc,t in itertools.product(nwr_perc,ntifs,nalgos,nthreads):
        _cfg_fmt = ["CC_ALG","MAX_TXN_PER_PART","WORKLOAD","THREAD_CNT","MAX_TXN_IN_FLIGHT","ZIPF_THETA","READ_PERC","WRITE_PERC"]
        _cfg=[cc,ntxn,'YCSB',t,tif,nzipf[0],1.0-wr,wr]
        _title="{} {} {}% Writes {} TIF {} threads".format(cc,'YCSB',wr*100,tif,t)
        tput(nnodes,nmpr,summary,cfg_fmt=_cfg_fmt,cfg=_cfg,xname="NODE_CNT",vname="MPR",title=_title)

    # x-axis: nodes, y-axis: latency
    for wr,tif,cc,t in itertools.product(nwr_perc,ntifs,nalgos,nthreads):
        _cfg_fmt = ["CC_ALG","MAX_TXN_PER_PART","WORKLOAD","THREAD_CNT","MAX_TXN_IN_FLIGHT","ZIPF_THETA","READ_PERC","WRITE_PERC"]
        _cfg=[cc,ntxn,'YCSB',t,tif,nzipf[0],1.0-wr,wr]
        _title="{} {} {}% Writes {} TIF {} threads".format(cc,'YCSB',wr*100,tif,t)
        lat(nnodes,nmpr,summary_client,cfg_fmt=_cfg_fmt,cfg=_cfg,xname="NODE_CNT",vname="MPR",title=_title)
    return


    # x-axis: nodes; one plot for each wr %
    for wr,mpr,tif in itertools.product(nwr_perc,nmpr,ntifs):
        _cfg_fmt = ["MPR","MAX_TXN_PER_PART","WORKLOAD","THREAD_CNT","MAX_TXN_IN_FLIGHT","ZIPF_THETA","READ_PERC","WRITE_PERC"]
        _cfg=[mpr,ntxn,'YCSB',nthreads[0],tif,nzipf[0],1.0-wr,wr]
        _title="{} {}% Writes {} MPR {} TIF".format('YCSB',wr*100,mpr,tif)
        tput(nnodes,nalgos,summary,cfg_fmt=_cfg_fmt,cfg=_cfg,xname="NODE_CNT",vname="CC_ALG",title=_title)
        abort_rate(nnodes,nalgos,summary,cfg_fmt=_cfg_fmt,cfg=_cfg,xname="NODE_CNT",vname="CC_ALG",title=_title)

def partition_sweep():
    fmt = fmt_ycsb
    nnodes = [1,2,4,8,16]
    nmpr=[1]
    nalgos=['WAIT_DIE']
    #nalgos=['WAIT_DIE','NO_WAIT','OCC','MVCC','HSTORE','HSTORE_SPEC','VLL','TIMESTAMP']
    nthreads=[1]
    ncthreads=[4]
    ntifs=[1000]
    nzipf=[0.6]
    nwr_perc=[0.5]
    ntxn=5000000
    exp = []
    for node in nnodes:
        nparts = range(2,node,2)
        tmp = [[int(math.ceil(node/2)) if node > 1 else 1,node,ntxn,'YCSB',cc,m,ct,t,tif,z,1.0-wp,wp,part] for ct,t,tif,z,wp,m,cc,part in itertools.product(ncthreads,nthreads,ntifs,nzipf,nwr_perc,nmpr,nalgos,nparts)]
        exp = exp + tmp
    return fmt[0],exp

def mpr_sweep():
    fmt = fmt_ycsb
    nnodes = [1,2,4,8,16]
    nmpr=[0,1,5,10,20,30,40,50,60,70,80,90,100]
    nalgos=['WAIT_DIE']
    #nalgos=['WAIT_DIE','NO_WAIT','OCC','MVCC','HSTORE','HSTORE_SPEC','VLL','TIMESTAMP']
    nthreads=[1]
    ncthreads=[4]
    ntifs=[1000]
    nzipf=[0.6]
    nwr_perc=[0.5]
    ntxn=5000000
    nparts = [2]
    exp = [[int(math.ceil(n/2)) if n > 1 else 1,n,ntxn,'YCSB',cc,m,ct,t,tif,z,1.0-wp,wp,p] for ct,t,tif,z,wp,m,cc,n,p in itertools.product(ncthreads,nthreads,ntifs,nzipf,nwr_perc,nmpr,nalgos,nnodes,nparts)]
    return fmt[0],exp

def tif_sweep():
    fmt = fmt_ycsb
    nnodes = [1,2,4,8,16]
    nmpr=[1]
    nalgos=['WAIT_DIE']
    #nalgos=['WAIT_DIE','NO_WAIT','OCC','MVCC','HSTORE','HSTORE_SPEC','VLL','TIMESTAMP']
    nthreads=[1]
    ncthreads=[4]
    ntifs=[1,2,10,35,100,1000]
    nzipf=[0.6]
    nwr_perc=[0.5]
    ntxn=5000000
    nparts = [2]
    exp = [[int(math.ceil(n/2)) if n > 1 else 1,n,ntxn,'YCSB',cc,m,ct,t,tif,z,1.0-wp,wp,p] for ct,t,tif,z,wp,m,cc,n,p in itertools.product(ncthreads,nthreads,ntifs,nzipf,nwr_perc,nmpr,nalgos,nnodes,nparts)]
    return fmt[0],exp

def write_ratio_sweep():
    fmt = fmt_ycsb
    nnodes = [1,2,4,8,16]
    nmpr=[1]
    nalgos=['WAIT_DIE']
    #nalgos=['WAIT_DIE','NO_WAIT','OCC','MVCC','HSTORE','HSTORE_SPEC','VLL','TIMESTAMP']
    nthreads=[1]
    ncthreads=[4]
    ntifs=[1000]
    nzipf=[0.6]
    nwr_perc=[0.0,0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,1.0]
    ntxn=5000000
    nparts = [2]
    exp = [[int(math.ceil(n/2)) if n > 1 else 1,n,ntxn,'YCSB',cc,m,ct,t,tif,z,round(1.0-wp,1),wp,p] for ct,t,tif,z,wp,m,cc,n,p in itertools.product(ncthreads,nthreads,ntifs,nzipf,nwr_perc,nmpr,nalgos,nnodes,nparts)]
    return fmt[0],exp

def skew_sweep():
    fmt = fmt_ycsb
    nnodes = [1,2,4,8,16]
    nmpr=[1]
    nalgos=['WAIT_DIE']
    #nalgos=['WAIT_DIE','NO_WAIT','OCC','MVCC','HSTORE','HSTORE_SPEC','VLL','TIMESTAMP']
    nthreads=[1]
    ncthreads=[4]
    ntifs=[1000]
    nzipf=[0.0,0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,1.0]
    nwr_perc=[0.5]
    ntxn=5000000
    nparts = [2]
    exp = [[int(math.ceil(n/2)) if n > 1 else 1,n,ntxn,'YCSB',cc,m,ct,t,tif,z,1.0-wp,wp,p] for ct,t,tif,z,wp,m,cc,n,p in itertools.product(ncthreads,nthreads,ntifs,nzipf,nwr_perc,nmpr,nalgos,nnodes,nparts)]
    return fmt[0],exp

#Should do this one on an ISTC machine
def network_sweep():
    fmt = fmt_ycsb + ["NETWORK_DELAY"]
    nnodes = [1,2,4,8,16]
    nmpr=[1]
    nalgos=['WAIT_DIE']
    #nalgos=['WAIT_DIE','NO_WAIT','OCC','MVCC','HSTORE','HSTORE_SPEC','VLL','TIMESTAMP']
    nthreads=[1]
    ncthreads=[4]
    ntifs=[1000]
    nzipf=[0.6]
    nwr_perc=[0.5]
    nparts=[2]
    network_delay = ["0UL","10000UL","100000UL","1000000UL","10000000UL","100000000UL"]
    ntxn=5000000
    exp = [[int(math.ceil(n/2)) if n > 1 else 1,n,ntxn,'YCSB',cc,m,ct,t,tif,z,1.0-wp,wp,p,nd] for ct,t,tif,z,wp,m,cc,n,p,nd in itertools.product(ncthreads,nthreads,ntifs,nzipf,nwr_perc,nmpr,nalgos,nnodes,nparts,network_delay)]
    return fmt[0],exp

def tpcc_sweep():
    fmt = fmt_tpcc
    nnodes = [1,2,4,8,16]
    nmpr=[1]
    nalgos=['WAIT_DIE']
    #nalgos=['WAIT_DIE','NO_WAIT','OCC','MVCC','HSTORE','HSTORE_SPEC','VLL','TIMESTAMP']
    nthreads=[1]
    ncthreads=[4]
    ntifs=[1000]
    nzipf=[0.6]
    nwr_perc=[0.5]
    ntxn=5000000
    exp = [[int(math.ceil(n/2)) if n > 1 else 1,n,ntxn,'YCSB',cc,m,ct,t,tif,z,1.0-wp,wp] for ct,t,tif,z,wp,m,cc,n in itertools.product(ncthreads,nthreads,ntifs,nzipf,nwr_perc,nmpr,nalgos,nnodes)]
    return fmt[0],exp



def network_experiment():
    fmt = fmt_nt
    nnodes = [1]
    cnodes = [1]
    ntest = ["true"]
    exp = [nnodes,cnodes,ntest]
    exp = [[n,c,t] for n,c,t in itertools.product(nnodes,cnodes,ntest)]
    return fmt[0],exp

def network_experiment_plot(all_exps,all_nodes,timestamps):
    from plot_helper import lat_node_tbls,lat_tbl
    fmt = fmt_nt
    nnodes = [1]
    cnodes = [1]
    ntest = ["true"]
    rexp = [nnodes,cnodes,ntest]
    rexp = [[n,c,t] for n,c,t in itertools.product(nnodes,cnodes,ntest)]
    for i,exp in enumerate(all_exps):
        lat_node_tbls(exp[:-1],all_nodes[i],exp[0].keys(),timestamps[i])
        lat_tbl(exp[-1],exp[-1].keys(),timestamps[i])


experiment_map = {
    'test': test,
    'experiment_1': experiment_1,
    'experiment_1_plot': experiment_1_plot,
    'partition_sweep': partition_sweep,
    'mpr_sweep': mpr_sweep,
    'tif_sweep': tif_sweep,
    'write_ratio_sweep': write_ratio_sweep,
    'skew_sweep': skew_sweep,
    'network_sweep': network_sweep,
    'tpcc_sweep': tpcc_sweep,
    'network_experiment' : network_experiment,
    'network_experiment_plot' : network_experiment_plot,
}


# Default values for variable configurations
configs = {
    "NODE_CNT" : 2,
    "CLIENT_NODE_CNT" : 1,
    "CLIENT_THREAD_CNT" : 2,
    "CLIENT_REM_THREAD_CNT" : 1,
    "MAX_TXN_PER_PART" : 100,
    "WORKLOAD" : "YCSB",
    "CC_ALG" : "NO_WAIT",
    "MPR" : 0.0,
    "TPORT_TYPE":"\"ipc\"",
    "TPORT_TYPE_IPC":"true",
    "TPORT_PORT":"\"_.ipc\"",
    "REM_THREAD_CNT": 1,
    "THREAD_CNT": 1,
    "PART_CNT": 2,
    "NUM_WH": 2,
    "MAX_TXN_IN_FLIGHT": 1,
    "NETWORK_DELAY": '0UL',
    "DONE_TIMER": "5 * 60 * BILLION // 5 minutes",
    "NETWORK_TEST" : "false",
#YCSB
    "READ_PERC":0.5,
    "WRITE_PERC":0.5,
    "ZIPF_THETA":0.6,
    "PART_PER_TXN": 2,
}

