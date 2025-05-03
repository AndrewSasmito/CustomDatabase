#pragma once

struct fraction {
    int top;
    int bottom;

    // Constructor
    fraction(int t = 0, int b = 1) : top(t), bottom(b) {}

    // Operator overloads
    bool operator<(const fraction& other) const {
        return static_cast<long long>(top) * other.bottom < static_cast<long long>(other.top) * bottom;
    }

    bool operator>(const fraction& other) const {
        return static_cast<long long>(top) * other.bottom > static_cast<long long>(other.top) * bottom;
    }

    bool operator==(const fraction& other) const {
        return top * other.bottom == other.top * bottom;
    }

    bool operator!=(const fraction& other) const {
        return !(*this == other);
    }
};
