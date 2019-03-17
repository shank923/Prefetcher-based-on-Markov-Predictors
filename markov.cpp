#include "interface.hh"
#include <vector>
#include <algorithm>

#define SIZE 6000
#define SIZE_P 8
#define LOOKAHEAD 32

using namespace std;

struct predictor{
  Addr addr;
  int weight;
  Tick time;
};

struct markov{
  Addr addr;
  vector<predictor> predictors;
  Tick time;
};

struct find_addr : std::unary_function<markov, bool> {
    Addr addr;
    find_addr(Addr addr):addr(addr) { }
    bool operator()(markov const& m) const {
        return m.addr == addr;
    }
};

struct find_addr_predictor : std::unary_function<predictor, bool> {
    Addr addr;
    find_addr_predictor(Addr addr):addr(addr) { }
    bool operator()(predictor const& m) const {
        return m.addr == addr;
    }
};

void push_addr(AccessStat stat);
void push_predictors(AccessStat stat);
Addr get_addr(markov curr_node);
Addr get_node(Addr addr);

vector<markov> node_addr;

void push_addr(AccessStat stat){
    vector<markov>::iterator iter = find_if(node_addr.begin(), node_addr.end(), find_addr(stat.mem_addr));;

    if(!node_addr.empty())
      push_predictors(stat);

    if(iter == node_addr.end()){
      struct markov new_node;
      new_node.addr = stat.mem_addr;
      new_node.time = stat.time;

      //find and remove the oldest node
      if(node_addr.size() == SIZE){
          Tick old_timer = stat.time;
          for(vector<markov>::iterator it = node_addr.begin(); it != node_addr.end(); ++it){
            if(old_timer > it->time){
              old_timer = it->time;
              iter = it;
            }
          }
        node_addr.erase(iter);
      }
      node_addr.push_back(new_node);
    }

    else{
      iter->time = stat.time;
    }
}

void push_predictors(AccessStat stat){
  vector<predictor>::iterator iter = find_if(node_addr.back().predictors.begin(), node_addr.back().predictors.end(), find_addr_predictor(stat.mem_addr));
  if(iter == node_addr.back().predictors.end()){
    struct predictor new_node;
    new_node.addr = stat.mem_addr;
    new_node.time = stat.time;
    new_node.weight = 0;

    //find and remove the oldest predictor node_addr
    if(node_addr.back().predictors.size() == SIZE_P){
      Tick old_timer = stat.time;
      for(vector<predictor>::iterator it = node_addr.back().predictors.begin(); it != node_addr.back().predictors.end(); ++it){
        if(old_timer > it->time){
          old_timer = it->time;
          iter = it;
        }
      }
      node_addr.back().predictors.erase(iter);
    }
    node_addr.back().predictors.push_back(new_node);
  }
  else{
    iter->weight = iter->weight + 1;
    iter->time = stat.time;
  }
}

Addr get_addr(markov curr_node){
    Addr next_addr = 0;
    int max_weight = -1;

    if(!curr_node.predictors.empty()){
      for(vector<predictor>::iterator it = curr_node.predictors.begin(); it != curr_node.predictors.end(); ++it){
        if(it->weight > max_weight){
          max_weight = it->weight;
          next_addr = it->addr;
        }
      }
    }

    return next_addr;
}

Addr get_node(Addr addr){
  vector<markov>::iterator iter = find_if(node_addr.begin(), node_addr.end(), find_addr(addr));

  if(iter == node_addr.end())
    return 0;

  else{
    return get_addr(*iter);
  }
}

void prefetch_init(void)
{
    /* Called before any calls to prefetch_access. */
    /* This is the place to initialize data structures. */

    //DPRINTF(HWPrefetch, "Initialized fucking markov prefetcher\n");
	cout << "\nInitialized fucking markov prefetcher\n";
}

void prefetch_access(AccessStat stat)
{
    /* pf_addr is now an address within the _next_ cache block */
    Addr pf_addr = 0;
    /*
     * Issue a prefetch request if a demand miss occured,
     * and the block is not already in cache.
     */
     if(stat.miss){
      push_addr(stat);
      pf_addr = get_node(stat.mem_addr);
      for(int i = 0; i < LOOKAHEAD; i++){
        if(pf_addr == 0){
          pf_addr = stat.mem_addr + BLOCK_SIZE;
          if(!in_mshr_queue(pf_addr) && !in_cache(pf_addr))
            issue_prefetch(pf_addr);

          pf_addr = stat.mem_addr + BLOCK_SIZE * 2;
          if(!in_mshr_queue(pf_addr) && !in_cache(pf_addr))
            issue_prefetch(pf_addr);

            break;
        } else{
          pf_addr = get_node(pf_addr);
        }
        if(!in_mshr_queue(pf_addr) && !in_cache(pf_addr)){
          issue_prefetch(pf_addr);
      }
    }
  }
}

void prefetch_complete(Addr addr) {
    /*
     * Called when a block requested by the prefetcher has been loaded.
     */
}
