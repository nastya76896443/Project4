#pragma once

#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>

class SharedData {
private:
    int field0_ = 0;
    int field1_ = 0;

    mutable std::shared_mutex mutex0_;
    mutable std::shared_mutex mutex1_;

    static void checkField(int field) {
        if (field != 0 && field != 1) {
            throw std::out_of_range("Field must be 0 or 1");
        }
    }

public:
    int get(int field) const {
        checkField(field);

        if (field == 0) {
            std::shared_lock<std::shared_mutex> lock(mutex0_);
            return field0_;
        }

        std::shared_lock<std::shared_mutex> lock(mutex1_);
        return field1_;
    }

    void set(int field, int value) {
        checkField(field);

        if (field == 0) {
            std::unique_lock<std::shared_mutex> lock(mutex0_);
            field0_ = value;
        } else {
            std::unique_lock<std::shared_mutex> lock(mutex1_);
            field1_ = value;
        }
    }

    operator std::string() const {
        int first;
        int second;

        {
            std::shared_lock<std::shared_mutex> lock0(mutex0_);
            std::shared_lock<std::shared_mutex> lock1(mutex1_);
            first = field0_;
            second = field1_;
        }

        return "[" + std::to_string(first) + ", "
                   + std::to_string(second) + "]";
    }
};