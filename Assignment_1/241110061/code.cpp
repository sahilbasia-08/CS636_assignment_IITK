#include <iostream>
#include <string>
#include <fstream>
#include <regex>
#include <chrono>

#include "fasttrack.h"
#include "djit.h"
using namespace std;
using namespace std::chrono;

// ifstream object to open and read the trace file
ifstream parse_log(string path){
    ifstream file(path);
    if(!file.is_open()){
        cout<<"Trace file failed to open"<<endl;
        exit(0);
    }
    return file;
}



int main(int argc, char* argv[]) {
    // setting the command line argument as asked in the assignment
    ifstream trace_file;

    string path = "";
    string algo = "";

    if (argc != 3) {
        cout << "The desired format of command line argument is:\n";
        cout << "./a.out -algo=algo_name -trace=path_to_trace_file" << endl;
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];

        if (arg[0] == '-' && arg.find('=') != string::npos) {
            size_t eq_pos = arg.find('=');
            string key = arg.substr(1, eq_pos - 1);
            string value = arg.substr(eq_pos + 1);

            if (key == "algo") {
                algo = value;
            } else if (key == "trace") {
                path = value;
            } else {
                cout << "Unknown argument: " << arg << endl;
                return 1;
            }
        } else {
            cout << "Invalid argument format: " << arg << endl;
            return 1;
        }
    }
    // command line argument is set now

    for(int i=0; i<algo.size(); ++i){
        if(isalpha(algo[i]))
            algo[i]=tolower(algo[i]);
    }

    trace_file=parse_log(path);
    //double duration_1, duration_2;

    clock_t start, end;
    vector<string> ans1, ans2;
    if(algo=="djit"){

        trace_file.clear();
        trace_file.seekg(0, ios::beg);
        start=clock();
        ans1=parse_trace_1(trace_file);
        for(ll i=0; i<ans1.size(); ++i){
            cout<<ans1[i]<<endl;
        }
        end=clock();
        cout<<endl;
        double duration_1=double(end-start)/double(CLOCKS_PER_SEC);
        cout<<"DJIT algo execuiton time = "<<duration_1<<endl;

    }
    else if(algo=="fasttrack"){

        trace_file.clear();
        trace_file.seekg(0, ios::beg);
        start=clock();
        ans2=parse_trace_2(trace_file);
        for(ll i=0; i<ans2.size(); ++i){
            cout<<ans2[i]<<endl;
        }
        end=clock();
        cout<<endl;
        double duration_1=double(end-start)/double(CLOCKS_PER_SEC);
        cout<<"FASTTRACK algo execuiton time = "<<duration_1<<endl;

        //cout<<"DJIT algo execuiton time = "<<duration_1.count()<<endl;
    }
    else if(algo=="all"){
        // here time the difference of both the protocols


        start=clock();
        ans1=parse_trace_1(trace_file);
        end=clock();
        double duration_1=double(end-start)/double(CLOCKS_PER_SEC);
        cout<<"DJIT algo execuiton time = "<<duration_1<<endl;

        // to reset the trace file pointer so that we have to load the file again
        trace_file.clear();
        trace_file.seekg(0, ios::beg);

        start=clock();
        ans2=parse_trace_2(trace_file);
        end=clock();
        double duration_2=double(end-start)/double(CLOCKS_PER_SEC);
        cout<<"FASTTRACK algo execuiton time = "<<duration_2<<endl;

        cout<<endl<<"Speedup of FASTTRACK over DJIT is = "<<(duration_1/duration_2)<<endl;
    }
    else{
        cout<<"Error! Make sure you are running a.out like this"<<endl;
        cout<<"./a.out -algo=algo_name -trace=path_to_trace_file [algo names=djit/fasttrack]"<<endl;
        cout<<" use (-algo=all ) to print the performance gain of FASTTRACK over DJIT protocol"<<endl<<endl;
    }

    return 0;
}
