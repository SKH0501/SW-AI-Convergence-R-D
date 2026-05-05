#include "ThreadPool.h"

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <unistd.h>

using namespace std;

enum class WorkloadType {
    CPU,
    IO,
    MIXED
};

struct WorkloadArg {
    WorkloadType type;
};

void workload_function(void* arg) {
    WorkloadArg* warg = static_cast<WorkloadArg*>(arg);

    if (warg->type == WorkloadType::CPU) {
        volatile long long sum = 0;

        for (long long i = 0; i < 1000000; i++) {
            sum += i % 7;
        }
    }
    else if (warg->type == WorkloadType::IO) {
        usleep(10000);
    }
    else if (warg->type == WorkloadType::MIXED) {
        volatile long long sum = 0;

        for (long long i = 0; i < 300000; i++) {
            sum += i % 7;
        }

        usleep(5000);
    }
}

long long now_ns() {
    return chrono::duration_cast<chrono::nanoseconds>(
        chrono::steady_clock::now().time_since_epoch()
    ).count();
}

WorkloadType parse_workload(const string& name) {
    if (name == "cpu") {
        return WorkloadType::CPU;
    }

    if (name == "io") {
        return WorkloadType::IO;
    }

    if (name == "mixed") {
        return WorkloadType::MIXED;
    }

    cerr << "Unknown workload: " << name << "\n";
    exit(1);
}

double average_ms(const vector<long long>& values_ns) {
    if (values_ns.empty()) {
        return 0.0;
    }

    long double sum = 0;

    for (long long value : values_ns) {
        sum += value / 1000000.0;
    }

    return static_cast<double>(sum / values_ns.size());
}

double percentile_ms(vector<long long> values_ns, double percentile) {
    if (values_ns.empty()) {
        return 0.0;
    }

    sort(values_ns.begin(), values_ns.end());

    int index = static_cast<int>((percentile / 100.0) * values_ns.size());

    if (index >= static_cast<int>(values_ns.size())) {
        index = static_cast<int>(values_ns.size()) - 1;
    }

    return values_ns[index] / 1000000.0;
}

void write_csv_header_if_needed(const string& filename) {
    ifstream fin(filename);

    if (fin.good()) {
        return;
    }

    ofstream fout(filename);

    fout << "pool_type,"
         << "thread_count,"
         << "workload,"
         << "task_count,"
         << "warmup_count,"
         << "total_time_ms,"
         << "throughput,"
         << "avg_latency_ms,"
         << "p95_latency_ms,"
         << "avg_queue_wait_ms,"
         << "p95_queue_wait_ms,"
         << "avg_service_time_ms"
         << "\n";
}

void run_experiment(
    int thread_count,
    const string& workload_name,
    int task_count,
    int warmup_count,
    const string& csv_file
) {
    WorkloadType workload = parse_workload(workload_name);

    ThreadPool pool(thread_count, task_count);

    WorkloadArg arg;
    arg.type = workload;

    for (int i = 0; i < warmup_count; i++) {
        pool.submit(workload_function, &arg, POOL_WAIT, nullptr);
    }

    pool.wait_all();

    pool.reset_counter();

    vector<TaskMetric> metrics(task_count);

    long long experiment_start_ns = now_ns();

    for (int i = 0; i < task_count; i++) {
        pool.submit(workload_function, &arg, POOL_WAIT, &metrics[i]);
    }

    pool.wait_all();

    long long experiment_end_ns = now_ns();

    vector<long long> latencies;
    vector<long long> queue_waits;
    vector<long long> service_times;

    latencies.reserve(task_count);
    queue_waits.reserve(task_count);
    service_times.reserve(task_count);

    for (int i = 0; i < task_count; i++) {
        long long submit = metrics[i].submit_time_ns.load();
        long long start = metrics[i].start_time_ns.load();
        long long end = metrics[i].end_time_ns.load();

        long long queue_wait = start - submit;
        long long service_time = end - start;
        long long latency = end - submit;

        queue_waits.push_back(queue_wait);
        service_times.push_back(service_time);
        latencies.push_back(latency);
    }

    double total_time_ms = (experiment_end_ns - experiment_start_ns) / 1000000.0;
    double total_time_sec = total_time_ms / 1000.0;
    double throughput = task_count / total_time_sec;

    double avg_latency_ms = average_ms(latencies);
    double p95_latency_ms = percentile_ms(latencies, 95.0);

    double avg_queue_wait_ms = average_ms(queue_waits);
    double p95_queue_wait_ms = percentile_ms(queue_waits, 95.0);

    double avg_service_time_ms = average_ms(service_times);

    write_csv_header_if_needed(csv_file);

    ofstream fout(csv_file, ios::app);

    fout << "fixed,"
         << thread_count << ","
         << workload_name << ","
         << task_count << ","
         << warmup_count << ","
         << total_time_ms << ","
         << throughput << ","
         << avg_latency_ms << ","
         << p95_latency_ms << ","
         << avg_queue_wait_ms << ","
         << p95_queue_wait_ms << ","
         << avg_service_time_ms
         << "\n";

    fout.close();

    cout << "=============================\n";
    cout << "Pool Type       : Fixed\n";
    cout << "Thread Count    : " << thread_count << "\n";
    cout << "Workload        : " << workload_name << "\n";
    cout << "Task Count      : " << task_count << "\n";
    cout << "Warm-up Count   : " << warmup_count << "\n";
    cout << "Total Time(ms)  : " << total_time_ms << "\n";
    cout << "Throughput      : " << throughput << " tasks/sec\n";
    cout << "Avg Latency(ms) : " << avg_latency_ms << "\n";
    cout << "P95 Latency(ms) : " << p95_latency_ms << "\n";
    cout << "Avg Queue(ms)   : " << avg_queue_wait_ms << "\n";
    cout << "P95 Queue(ms)   : " << p95_queue_wait_ms << "\n";
    cout << "Avg Service(ms) : " << avg_service_time_ms << "\n";
    cout << "=============================\n";
}

int main(int argc, char* argv[]) {
    if (argc < 6) {
        cerr << "Usage:\n";
        cerr << "./pool_runner <thread_count> <workload> <task_count> <warmup_count> <csv_file>\n\n";
        cerr << "Example:\n";
        cerr << "./pool_runner 4 cpu 10000 1000 result.csv\n";
        return 1;
    }

    int thread_count = stoi(argv[1]);
    string workload_name = argv[2];
    int task_count = stoi(argv[3]);
    int warmup_count = stoi(argv[4]);
    string csv_file = argv[5];

    run_experiment(
        thread_count,
        workload_name,
        task_count,
        warmup_count,
        csv_file
    );

    return 0;
}