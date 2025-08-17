#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>

using namespace std;
using ll = long long;


// threads vector clock
struct vector_clock_1 {
    vector<ll> clock;

    // filling the vector clock of each thread with 1's
    void thread_clock_init_1(ll t_num) {
        clock.resize(t_num, 1);
    }
    //to resize the older threads vector clocks
    void resize_1(ll newsize) {
        if (clock.size() < newsize) {
            clock.resize(newsize, 1);
        }
    }
    //after lock release we have to update the threads vector clock
    void inc_1(ll tid) {
        clock[tid]++;
    }
    //after lock aquire updaing the threads vector clock with max of locks or threads clock
    void update_1_lock_1(const vector<ll> &lock_vc) {
        for (ll i = 0; i < (ll)clock.size(); ++i) {
            clock[i] = max(clock[i], lock_vc[i]);
        }
    }
};

// Memory clock
struct memory_clock_1 {
    // read and write vector clocks
    vector<ll> r_v;
    vector<ll> w_v;
    //constructor call to initialize the read and write vector clocks of the accessed momoery address with 0
    //memory_clock_1(ll num_t=0) : r_v(num_t, 0), w_v(num_t, 0) {}
    //to initialize the vector clock of read and write of that accessed address by the thread
    void m_init_1(unsigned long num_t) {
        r_v.resize(num_t, 0);
        w_v.resize(num_t, 0);
    }
    //to resize the oldre read and write vector clocks when in future new threads are created
    void resize_1(ll t_count_1) {
        if (r_v.size() < t_count_1 || w_v.size() < t_count_1) {
            r_v.resize(t_count_1, 0);
            w_v.resize(t_count_1, 0);
        }
    }
};

// Lock clock
struct lock_clock_1 {
    vector<ll> lock;

    // lock_clock_1(ll num=0) : lock(num, 0) {}
    //to initialize the vector clock for locks with all zeroes
    void lock_init_1(ll num_t=0) {
        lock.resize(num_t, 0);
    }
    // to resize the older lock cloks when new threads are created in fturure
    void resize_1(ll t_num) {
        if (lock.size() < t_num) {
            lock.resize(t_num, 0);
        }
    }
    //after lock releas to update the lock clock with max between locks and thtread vectror clcok
    void update_1(const vector_clock_1 &t_releaser) {
        // merge thread's vector clock
        for(ll i = 0; i < (ll)lock.size(); ++i){
            lock[i] = max(lock[i], t_releaser.clock[i]);
        }
    }
};

//global maps and varibnles for strcutries defined above
unordered_map<ll, vector_clock_1> t_vc_1;                // thread_id mapped to its vector clock object
unordered_map<unsigned long, memory_clock_1> m_vc_1;      // memoruy_addres_varaible mapped to its vector clock object
unordered_map<unsigned long, lock_clock_1> l_vc_1;        // lcok_addres mapped to its vector clock object
unordered_map<string, ll> data_races_1;
ll t_count_1 = 0;                                       // to keep treack of total thrads created

// function to read the trace file and will read address in byte granularity
// its retruning a vector of strings so that i can print the output explicitly
vector<string> parse_trace_1(ifstream &file) {
    string line;
    unsigned long parent_tid, no_of_child = 0;
    bool is_parent = false;

    while (getline(file, line)) {
        unsigned long ip, addr;
        unsigned long tid, is_read;
        int size = 0;
        ll parent = 0;

        //reading the address in regex format to get the required info from the line readed
        if (sscanf(line.c_str(), "TID: %lx, IP: %lx, ADDR: %lx, Size (B): %d, isRead: %lx",  &tid, &ip, &addr, &size, &is_read) == 5)
        {

            if(t_vc_1.find(tid) == t_vc_1.end()){
                t_count_1++;
                t_vc_1[tid].thread_clock_init_1(t_count_1);
                for(auto &p: t_vc_1)
                    p.second.resize_1(t_count_1);
                for(auto &p: l_vc_1)
                    p.second.resize_1(t_count_1);
                for(auto &p: m_vc_1)
                    p.second.resize_1(t_count_1);
            }
            // ****************************************************************************************************

            for (unsigned long k = 0; k < (unsigned long)size; ++k) {

                // if addr not already present inside the map, intialize new entry and using the object init function to manage initialization
                if(m_vc_1.find(addr+k) == m_vc_1.end()){
                    m_vc_1[addr+k].m_init_1(t_count_1);
                }

                if(is_read == 0){
                    // according to djit paper, updating the particular entry of memory addr vector clock with accessing thread
                    // entry only . i.e. only one entry is copied of tid's vector clock; not whole vector clock is copied

                    /* ADDED SAFETY CHECK:
                       Before using t_vc_1[tid].clock[tid] and m_vc_1[addr+k].w_v[tid],
                       we check that the vector sizes are large enough to avoid out-of-range indexing. */
                    if(t_vc_1[tid].clock.size() <= tid) {
                        t_vc_1[tid].resize_1(tid+1);
                    }
                    if(m_vc_1[addr+k].w_v.size() <= tid) {
                        m_vc_1[addr+k].resize_1(tid+1);
                    }
                    m_vc_1[addr+k].w_v[tid] = t_vc_1[tid].clock[tid];

                    // checking W-W data races
                    for(unsigned long i = 0; i < t_count_1; ++i){
                        if(i == tid){
                            // skipping if same i as thread id
                            continue;
                        }
                        if(m_vc_1[addr+k].w_v[i] >= t_vc_1[tid].clock[i]) {
                            //using string stream to strei output in required format asked in assignment
                            stringstream ss;
                            ss << "0x" << std::hex << addr << " +" << to_string(k)
                               << " W-W TID:" << tid << " TID:" << i << " ";
                            data_races_1[ss.str()]++;
                        }
                    }

                    // checking W-R races
                    for(unsigned long i = 0; i < t_count_1; ++i){
                        if(i == tid)
                            continue;
                        if(m_vc_1[addr+k].r_v[i] >= t_vc_1[tid].clock[i]) {
                            // same as R-W comments
                            stringstream ss;
                            ss << "0x" << std::hex << addr << " +" << to_string(k)
                               << " R-W TID:" << tid << " TID:" << i << " ";
                            data_races_1[ss.str()]++;
                        }
                    }
                }
                // if memory access is read access
                else {
                    /* ADDED SAFETY CHECK:
                       Before using t_vc_1[tid].clock[tid] and m_vc_1[addr+k].r_v[tid],
                       we check that the vector sizes are large enough. */
                    if(t_vc_1[tid].clock.size() <= tid) {
                        t_vc_1[tid].resize_1(tid+1);
                    }
                    if(m_vc_1[addr+k].r_v.size() <= tid) {
                        m_vc_1[addr+k].resize_1(tid+1);
                    }
                    m_vc_1[addr+k].r_v[tid] = t_vc_1[tid].clock[tid];

                    // checking R-W races [ R is currect access and W is older ]
                    for(unsigned long i = 0; i < t_count_1; ++i){
                        if(i == tid)
                            continue;
                        if(m_vc_1[addr+k].w_v[i] >= t_vc_1[tid].clock[i]) {
                            // same as W-W comment
                            stringstream ss;
                            ss << "0x" << std::hex << addr << " +" << to_string(k)
                               << " W-R TID:" << tid << " TID:" << i << " ";
                            data_races_1[ss.str()]++;
                        }
                    }
                }
            }
        }
        else {
            // THis means new thread has been created therefore will
            // 1. init new thread vector clocks
            // 2. resize older tjhread vector clocks
            // 3. resize all read, write and locks vector clocks witn new index as that of this thread's id
            if (sscanf(line.c_str(), "Thread begin: %lx", &tid) == 1) {
                t_count_1++;

                if(t_vc_1.find(tid) == t_vc_1.end()){
                    t_vc_1[tid].thread_clock_init_1(t_count_1);
                }

                for(auto &p: t_vc_1){
                    p.second.resize_1(t_count_1);
                }
                for(auto &p: l_vc_1){
                    p.second.resize_1(t_count_1);
                }
                for(auto &p: m_vc_1){
                    p.second.resize_1(t_count_1);
                }
                // if current thread is child of any paratn threqad then copying the vector clock of parent into child thread
                if(no_of_child > 0){
                    t_vc_1[tid] = t_vc_1[parent_tid];
                }
            }
            else if (sscanf(line.c_str(), "Before pthread_create(): Parent: %lx", &parent_tid) == 1) {
                // here i am tracking the praetns in case of fork join and othter parent and child thread relation
                is_parent = true;
                no_of_child++;
            }
            else if (sscanf(line.c_str(), "After lock acquire: TID: %lx, Lock address: %lx", &tid, &addr) == 2) {
                // if no entry of lock address in map , init the new entry
                if (l_vc_1.find(addr) == l_vc_1.end()) {
                    l_vc_1[addr].lock_init_1(t_count_1);
                }
                /// updating the current tid thread 's vector clocsk with max of locks and current thread vector clock
                t_vc_1[tid].update_1_lock_1(l_vc_1[addr].lock);
            }
            else if (sscanf(line.c_str(), "After lock release: TID: %lx, Lock address: %lx", &tid, &addr) == 2) {
                // increementing the therad vector clocks value
                //
                t_vc_1[tid].inc_1(tid);
                // after increamenting updating the locks vector clcjwith nax of therad and locks vector clock
                l_vc_1[addr].update_1(t_vc_1[tid]);
            }
            else if (sscanf(line.c_str(), "Thread ended: %lx", &tid) == 1) {
                // tracking the whihc thread ended if child ended then decremnign the no of child crreonoposndinly
                if(is_parent){
                    no_of_child--;
                    if(no_of_child == 0){
                        is_parent = false;
                    }
                }
            }
        }
    }
    // this is to store all the races string in vector and then finally returning the vector to print explictly when needed
    vector<string> records;
    // Print final data races
    for(auto &p : data_races_1){
        // cout << p.first << p.second << endl;
        records.push_back(p.first + to_string(p.second));
    }
    return records;
}

