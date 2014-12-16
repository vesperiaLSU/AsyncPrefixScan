#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_CONTINUATION
#define BOOST_RESULT_OF_USE_DECLTYPE
#define BOOST_ALL_DYN_LINK
#define BOOST_THREAD_VERSION 4
#define USED_THREAD_API USE_STD

#include "PrefixScan.h"
#include <vector>
#include <algorithm>
#include <future>
#include <thread>
#include <iostream>
#include <string>
#include "boost\thread\future.hpp"
#include "boost\thread.hpp"

using namespace std;

unsigned int create_seed(){
    unsigned int seed = (unsigned int)time(0);
    srand(seed);
    cout << "Seed generated for this random input: " << seed << endl;
    return seed;
}

void print_results(const vector<vector<double>>& vec_input){
    for (size_t threadNum = 0; threadNum < vec_input.size(); ++threadNum)
    {
        for_each(begin(vec_input[threadNum]), end(vec_input[threadNum]), [](double d){
            std::cout << d << " ";
        });
        std::cout << endl;
    }
    std::cout << endl;
}

int main(){
    vector<boost::future<int>> vec_f, vec_f_continue;                       //stores vector of future from first state
    vector<vector<double>> vec_ret;                            //stores vector of final results
    vector<double> input, intermediate_input, intermediate_output;
    size_t data_size = 0;

    //Input Generation....//
    unsigned int x = create_seed();
    std::cout << "Enter the data size to process" << endl;
    std::cin >> data_size;
    input.reserve(data_size);
    input_generation(data_size, back_inserter(input));

    //Parallel Execution..// hardware_thread > 1;
    clock_t start = clock();
    int hardware_threads = thread::hardware_concurrency();
    vector<double>::size_type partition_size = input.size() / hardware_threads;
    //pre-generate output vectors//
    intermediate_input.resize(hardware_threads);
    vec_ret.resize(hardware_threads);

    for (auto threadNum = 0; threadNum < hardware_threads - 1; ++threadNum)
    {
        boost::future<int> f = boost::async(boost::launch::async, [&vec_ret, &input, &intermediate_input, threadNum, partition_size]()->int{
            vector<double> sequential_ret;
            sequential_ret.reserve(partition_size);
            inclusive_scan(begin(input) + threadNum*partition_size, begin(input) + (threadNum + 1)*partition_size, back_inserter(sequential_ret));
            intermediate_input[threadNum] = sequential_ret.back();
            vec_ret[threadNum] = boost::move(sequential_ret);
            return 0;
        });
        vec_f.push_back(boost::move(f));
    }

    ////Main Thread Execution..//
    vector<double> sequential_main;
    sequential_main.reserve(partition_size);
    inclusive_scan(begin(input) + (hardware_threads - 1)*partition_size, end(input), back_inserter(sequential_main));
    intermediate_input[hardware_threads - 1] = sequential_main.back();
    vec_ret[hardware_threads - 1] = boost::move(sequential_main);

    for (auto threadNum = 0; threadNum < hardware_threads - 1; ++threadNum){
        boost::future<int> f2 = vec_f[threadNum].then([&intermediate_input, &intermediate_output, &vec_ret, threadNum](boost::future<int>&& f){
            if (threadNum != 0)
            {
                /*this following intermediate step is moved to here from main thread because 
                it has to wait for all previous thread to finish before getting invoked.*/
                inclusive_scan(begin(intermediate_input), end(intermediate_input), back_inserter(intermediate_output));  
                for_each(begin(vec_ret[threadNum]), end(vec_ret[threadNum]), [&](double& d){
                    d += intermediate_output[threadNum - 1];
                });
            }
            return 0;
        });
        vec_f_continue.push_back(boost::move(f2));
    }

    ////Main Thread Execution..////
    inclusive_scan(begin(intermediate_input), end(intermediate_input), back_inserter(intermediate_output));
    for_each(begin(vec_ret[hardware_threads - 1]), end(vec_ret[hardware_threads - 1]), [&](double& d)->void{
        d += intermediate_output[hardware_threads - 2];
    });

    //wait for all additional threads to finish executing
    for (size_t threadNum = 0; threadNum < vec_f_continue.size(); ++threadNum)
    {
        vec_f_continue[threadNum].wait();
    }

    double duration = ((clock() - start) / (double)CLOCKS_PER_SEC) * 1000;
    std::cout << intermediate_output.back() << endl;
    std::cout << "Parallel Execution completed! It took " << duration << " milliseconds!" << endl;
    //print_results(vec_ret);

    return 0;
}
