#include "SharedData.h"
#include "Workload.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

constexpr std::size_t TOTAL_OPERATIONS = 120'000;
constexpr int REPETITIONS = 5;

using Clock = std::chrono::steady_clock;

class StartGate {
private:
    std::mutex mutex_;
    std::condition_variable condition_;

    int ready_ = 0;
    bool open_ = false;

public:
    void wait() {
        std::unique_lock<std::mutex> lock(mutex_);

        ++ready_;
        condition_.notify_all();

        condition_.wait(lock, [&] {
            return open_;
        });
    }

    Clock::time_point start(int workers) {
        std::unique_lock<std::mutex> lock(mutex_);

        condition_.wait(lock, [&] {
            return ready_ == workers;
        });

        const auto started = Clock::now();

        open_ = true;

        lock.unlock();
        condition_.notify_all();

        return started;
    }

    void cancel() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            open_ = true;
        }

        condition_.notify_all();
    }
};

struct WorkerResult {
    std::uint64_t checksum = 0;
    std::exception_ptr error;
};

struct Times {
    double one = 0;
    double two = 0;
    double three = 0;
};

double runOnce(
    const std::string& first,
    const std::string& second,
    const std::string& third,
    int threads,
    std::uint64_t& checksum
) {
    SharedData data;
    StartGate gate;

    WorkerResult result1;
    WorkerResult result2;
    WorkerResult result3;

    auto worker = [&](
        const std::string& script,
        WorkerResult& result
    ) {
        try {
            gate.wait();

            std::string_view remaining(script);
            Command command;

            std::uint64_t sum = 0;

            while (nextCommand(remaining, command)) {
                if (command.type == 'r') {
                    sum = sum * 131
                        + static_cast<std::uint64_t>(
                            data.get(command.field)
                        );
                } else if (command.type == 'w') {
                    data.set(command.field, command.value);

                    sum = sum * 131
                        + static_cast<std::uint64_t>(
                            command.value
                        );
                } else {
                    const auto snapshot =
                        static_cast<std::string>(data);

                    for (unsigned char character : snapshot) {
                        sum = sum * 131 + character;
                    }
                }
            }

            result.checksum = sum;
        } catch (...) {
            result.error = std::current_exception();
        }
    };

    std::thread thread1;
    std::thread thread2;
    std::thread thread3;

    auto joinAll = [&] {
        if (thread1.joinable()) {
            thread1.join();
        }

        if (thread2.joinable()) {
            thread2.join();
        }

        if (thread3.joinable()) {
            thread3.join();
        }
    };

    try {
        thread1 = std::thread(
            worker,
            std::cref(first),
            std::ref(result1)
        );

        if (threads >= 2) {
            thread2 = std::thread(
                worker,
                std::cref(second),
                std::ref(result2)
            );
        }

        if (threads == 3) {
            thread3 = std::thread(
                worker,
                std::cref(third),
                std::ref(result3)
            );
        }
    } catch (...) {
        gate.cancel();
        joinAll();
        throw;
    }

    const auto started = gate.start(threads);

    joinAll();

    const auto finished = Clock::now();

    if (result1.error) {
        std::rethrow_exception(result1.error);
    }

    if (result2.error) {
        std::rethrow_exception(result2.error);
    }

    if (result3.error) {
        std::rethrow_exception(result3.error);
    }

    checksum =
        result1.checksum
        + result2.checksum
        + result3.checksum;

    return std::chrono::duration<double, std::milli>(
        finished - started
    ).count();
}

double averageTime(
    const std::string& text,
    int threads,
    std::uint64_t& totalChecksum
) {
    std::string first;
    std::string second;
    std::string third;

    splitCommands(
        text,
        TOTAL_OPERATIONS,
        threads,
        first,
        second,
        third
    );

    std::uint64_t checksum = 0;

    runOnce(first, second, third, threads, checksum);

    double sum = 0;

    for (
        int repetition = 0;
        repetition < REPETITIONS;
        ++repetition
    ) {
        sum += runOnce(
            first,
            second,
            third,
            threads,
            checksum
        );

        totalChecksum += checksum;
    }

    return sum / REPETITIONS;
}

Times benchmarkFile(
    const char* filename,
    std::uint64_t& checksum
) {
    const std::string text = loadFile(filename);

    if (countCommands(text) != TOTAL_OPERATIONS) {
        throw std::runtime_error(
            "Unexpected number of operations"
        );
    }

    Times times;

    times.one = averageTime(text, 1, checksum);
    times.two = averageTime(text, 2, checksum);
    times.three = averageTime(text, 3, checksum);

    return times;
}

void printRow(const char* filename, const Times& times) {
    std::cout
        << std::left << std::setw(20) << filename
        << std::right << std::setw(14) << times.one
        << std::setw(14) << times.two
        << std::setw(14) << times.three
        << std::endl;
}

int main() {
    try {
        if (
            TOTAL_OPERATIONS == 0
            || TOTAL_OPERATIONS % 60 != 0
            || REPETITIONS <= 0
        ) {
            throw std::invalid_argument(
                "TOTAL_OPERATIONS must be a positive multiple of 60"
            );
        }

        std::cout
            << "Variant 17; operations per run: "
            << TOTAL_OPERATIONS
            << "; measured repetitions: "
            << REPETITIONS
            << '\n'
            << "Generating three workload files..."
            << std::endl;

        generateFile(
            "variant17.txt",
            TOTAL_OPERATIONS,
            Mix{1, 1, 6, 1, 11},
            1701
        );

        generateFile(
            "equal.txt",
            TOTAL_OPERATIONS,
            Mix{4, 4, 4, 4, 4},
            1702
        );

        generateFile(
            "opposite.txt",
            TOTAL_OPERATIONS,
            Mix{1, 9, 1, 8, 1},
            1703
        );

        std::cout
            << "Mean elapsed time, ms; "
            << "one warm-up per configuration\n"
            << std::left << std::setw(20) << "File"
            << std::right << std::setw(14) << "1 thread"
            << std::setw(14) << "2 threads"
            << std::setw(14) << "3 threads"
            << std::endl;

        std::cout
            << std::fixed
            << std::setprecision(3);

        std::uint64_t checksum = 0;

        printRow(
            "variant17.txt",
            benchmarkFile("variant17.txt", checksum)
        );

        printRow(
            "equal.txt",
            benchmarkFile("equal.txt", checksum)
        );

        printRow(
            "opposite.txt",
            benchmarkFile("opposite.txt", checksum)
        );

        std::cout
            << "Checksum: "
            << checksum
            << std::endl;

        return 0;
    } catch (const std::exception& error) {
        std::cerr
            << "Error: "
            << error.what()
            << std::endl;

        return 1;
    }
}