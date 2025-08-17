#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>

using namespace std;
using ll = long long;


// This struct represents the thread vector clock. 
// Har thread ke liye ek vector clock maintain karta hai jisme initial value 1 hoti hai.

struct vector_clock {
    vector<ll> clock;

    // filling the vector clock of each thread with 1's
    void thread_clock_init(ll t_num) {
        clock.resize(t_num, 1);  // Initialize with 1 for each thread slot
    }

    // Resize the older threads vector clocks when new threads are created.
    void resize(ll newsize) {
        if (clock.size() < newsize) {
            clock.resize(newsize, 1);  // Resize and fill new elements with 1
        }
    }

    // Increment the clock value for a specific thread (by its index tid)
    void inc(ll tid) {
        clock[tid]++;
    }

    // Update the thread's vector clock with the maximum of its own values and the given lock vector clock.
    void update_lock(const vector<ll> &lock_vc) {
        for (ll i = 0; i < (ll)clock.size(); ++i) {
            clock[i] = max(clock[i], lock_vc[i]);
        }
    }
};

// 
// This struct represents the memory clock.
// It stores the write information (which thread last wrote and what its clock value was)
// and also the read vector clock when the memory is read by multiple threads.
// 
struct memory_clock {
    ll writeTid;       // Thread ID that last performed a write
    ll writeClockVal;  // The clock value of the writing thread at the time of write

    bool read_shared;  // Flag to indicate if multiple threads have read the value
    ll readTid;        // Thread ID if only one thread has read it so far
    ll readClockVal;   // The clock value of that reader thread

    vector<ll> readVC; // If more than one thread reads, this vector stores each reader's clock value

    // Constructor initializing values
    memory_clock() {
        writeTid      = -1;
        writeClockVal = 0;
        read_shared   = false;
        readTid       = -1;
        readClockVal  = 0;
    }

    // Initialize the read vector clock with size num_t, filled with 0.
    void m_init(ll num_t){
        readVC.resize(num_t, 0);
    }

    // Resize the readVC vector when more threads are added.
    void resize(ll t_count){
        if (readVC.size() < (size_t)t_count) {
            readVC.resize(t_count, 0);
        }
    }
};

// 
// This struct represents the lock clock.
// It keeps the latest vector clock of the thread that released the lock.
// 
struct lock_clock {
    vector<ll> lock;

    // Constructor that initializes the lock vector with a given size (default 0)
    lock_clock(ll num=0) : lock(num,0) {}

    // Initialize the lock vector with size num_t (all values 0)
    void lock_init(ll num_t=0) {
        lock.resize(num_t,0);
    }

    // Resize the lock vector if more threads are added.
    void resize(ll t_num) {
        if (lock.size() < t_num) {
            lock.resize(t_num, 0);
        }
    }

    // Update the lock clock with the maximum of the current lock clock and the releasing thread's vector clock.
    void update(const vector_clock &t_releaser) {
        for (ll i = 0; i < (ll)lock.size(); ++i) {
            lock[i] = max(lock[i], t_releaser.clock[i]);
        }
    }
};

// 
// Global maps to maintain vector clocks for threads, memory addresses, and locks.
// Also, a map to store detected data races, and a global thread count (t_count).
// 
unordered_map<ll, vector_clock> t_vc;          // TID -> vector_clock
unordered_map<unsigned long, memory_clock> m_vc; // address -> memory_clock
unordered_map<unsigned long, lock_clock> l_vc;   // lockAddr -> lock_clock
unordered_map<string, ll> data_races;            // Map for storing race descriptions and counts
ll t_count = 0;                                  // Total number of threads created

// 
// Returns the current "epoch" (clock value) of the given thread (indexed by tid).
// 
ll currentEpochOf(ll tid) {
    return t_vc[tid].clock[tid];
}

// 
// This function reports a data race by forming a string (with details about the memory
// address, type of race, and involved thread IDs) and then increments its count in the map.
// 
void reportRace(unsigned long baseAddr, unsigned long offset, const string &type, ll t1, ll t2)
{
    stringstream ss;
    ss << "0x" << std::hex << baseAddr << " +" << std::dec << offset << " " << type 
       << " TID:" << t1 << " TID:" << t2 << " ";
    data_races[ss.str()]++;
}

// 
// fasttrack_read: Implements the FastTrack algorithm's read rule for detecting data races.
// Yeh function check karta hai ki agar kisi memory address pe pehle koi write hua tha jiski clock value 
// current thread ke clock se zyada hai, to race report kare.
// 
void fasttrack_read(unsigned long baseAddr, unsigned long offset, memory_clock &m, ll tid)
{
    ll curEpoch = currentEpochOf(tid);

    // Check for W-R (Write-Read) race: Agar memory pe kisi aur thread ne write kiya tha
    if (m.writeTid != -1 && m.writeTid != tid) {
        ll wTid = m.writeTid;
        ll wClk = m.writeClockVal;
        ll seen = t_vc[tid].clock[wTid];
        if (wClk >= seen) {
            reportRace(baseAddr, offset, "W-R", tid, wTid);
        }
    }

    // Update the read clock values
    if (!m.read_shared) {
        if (m.readTid == -1) {
            m.readTid = tid;
            m.readClockVal = curEpoch;
        }
        else if (m.readTid == tid) {
            if (curEpoch > m.readClockVal) {
                m.readClockVal = curEpoch;
            }
        }
        else {
            m.read_shared = true;
            // Agar multiple threads read kar rahe hain, ensure readVC ka size sahi hai.
            if ((ll)m.readVC.size() <= max(m.readTid, tid)) {
                m.readVC.resize(t_count, 0);
            }
            m.readVC[m.readTid] = m.readClockVal;
            m.readVC[tid] = curEpoch;
        }
    }
    else {
        // Agar already multiple readers hai, ensure vector size before update.
        if ((ll)m.readVC.size() <= tid) {
            m.readVC.resize(t_count, 0);
        }
        m.readVC[tid] = max(m.readVC[tid], curEpoch);
    }
}


// fasttrack_write: Implements the FastTrack algorithm's write rule for detecting data races.
// Yeh function check karta hai ki agar kisi memory address pe pehle koi write ya read hua tha
// jiska clock value current thread ke clock ke hisaab se purana hai, to race report kare.

void fasttrack_write(unsigned long baseAddr, unsigned long offset, memory_clock &m, ll tid)
{
    ll curEpoch = currentEpochOf(tid);

    // Check for W-W (Write-Write) race: Agar memory pe kisi aur thread ka write present hai.
    if (m.writeTid != -1 && m.writeTid != tid) { // <-- fix here
        ll wTid = m.writeTid;
        ll wClk = m.writeClockVal;
        ll seen = t_vc[tid].clock[wTid];
        if (wClk >= seen) {
            reportRace(baseAddr, offset, "W-W", tid, wTid);
        }
    }

    // Check for R-W (Read-Write) race: Agar memory pe kisi aur thread ka read hua hai.
    if (!m.read_shared) {
        if (m.readTid != -1 && m.readTid != tid) { // <-- fix
            ll rTid = m.readTid;
            ll rClk = m.readClockVal;
            ll seen = t_vc[tid].clock[rTid];
            if (rClk >= seen) {
                reportRace(baseAddr, offset, "R-W", tid, rTid);
            }
        }
    }
    else {
        for (ll rTid = 0; rTid < (ll)m.readVC.size(); rTid++) {
            if (rTid == tid) continue;
            ll rVal = m.readVC[rTid];
            if (rVal > 0) {
                ll seen = t_vc[tid].clock[rTid];
                if (rVal >= seen) {
                    reportRace(baseAddr, offset, "R-W", tid, rTid);
                }
            }
        }
    }

    // Update memory clock after write
    m.writeTid = tid;
    m.writeClockVal = curEpoch;

    // Reset read flags after write
    m.read_shared = false;
    m.readTid = -1;
    m.readClockVal = 0;
}

// 
// parse_trace_2: Reads the trace file line by line, parses the events,
// updates thread/memory/lock vector clocks, and detects data races.
// 
vector<string> parse_trace_2(ifstream &file) {
    string line;
    unsigned long parent_tid, no_of_child = 0;
    bool is_parent = false;

    while (getline(file, line)) {
        unsigned long ip, addr;
        unsigned long tid, is_read;
        int size = 0;

        // If the line matches a memory access event
        if (sscanf(line.c_str(),"TID: %lx, IP: %lx, ADDR: %lx, Size (B): %d, isRead: %lx",&tid, &ip, &addr, &size, &is_read) == 5)
        {

            if(t_vc.find(tid) == t_vc.end()){
                t_count++;
                t_vc[tid].thread_clock_init(t_count);
                for(auto &p: t_vc)
                    p.second.resize(t_count);
                for(auto &p: l_vc)
                    p.second.resize(t_count);
                for(auto &p: m_vc)
                    p.second.resize(t_count);
            }
            // ***************************************************************************

            // Process each byte (subaddress) in the memory access
            for (int k = 0; k < size; ++k) {
                unsigned long subaddr = addr + k;
                // Initialize the memory clock for this address if not already done
                if (m_vc.find(subaddr) == m_vc.end()) {
                    m_vc[subaddr] = memory_clock();
                    m_vc[subaddr].m_init(t_count);
                }
                // For write access, call fasttrack_write; otherwise, fasttrack_read
                if (is_read == 0) {
                    fasttrack_write(addr, k, m_vc[subaddr], tid);
                } else {
                    fasttrack_read(addr, k, m_vc[subaddr], tid);
                }
            }
        }
        else {
            // Else, process thread and lock events
            if (sscanf(line.c_str(), "Thread begin: %lx", &tid) == 1) {
                t_count++;
                if (t_vc.find(tid) == t_vc.end()) {
                    t_vc[tid].thread_clock_init(t_count);
                }
                // Resize all vector clocks for newly added thread slot
                for (auto &p: t_vc) {
                    p.second.resize(t_count);
                }
                for (auto &p: l_vc) {
                    p.second.resize(t_count);
                }
                for (auto &p: m_vc) {
                    p.second.resize(t_count);
                }
            }
            else if (sscanf(line.c_str(), "Before pthread_create(): Parent: %lx", &parent_tid) == 1) {
                is_parent = true;
                no_of_child++;
            }
            else if (sscanf(line.c_str(), "After lock acquire: TID: %lx, Lock address: %lx", &tid, &addr) == 2) {
                // Initialize lock clock if not already done
                if (l_vc.find(addr) == l_vc.end()) {
                    l_vc[addr].lock_init(t_count);
                }
                // Update the thread's vector clock with the lock's vector clock
                t_vc[tid].update_lock(l_vc[addr].lock);
            }
            else if (sscanf(line.c_str(), "After lock release: TID: %lx, Lock address: %lx", &tid, &addr) == 2) {
                // Increment the thread's clock after releasing the lock
                t_vc[tid].inc(tid);
                // Update the lock clock with the thread's vector clock after release
                l_vc[addr].update(t_vc[tid]);
            }
            else if (sscanf(line.c_str(), "Thread ended: %lx", &tid) == 1) {
                // If thread ended and is a child thread, update the child count accordingly
                if (is_parent) {
                    no_of_child--;
                    if (no_of_child == 0) {
                        is_parent = false;
                    }
                }
            }
        }
    }

    // Collect all race reports into a vector of strings for final output.
    vector<string> records;
    for (auto &p: data_races) {
        records.push_back(p.first + to_string(p.second));
    }
    return records;
}

