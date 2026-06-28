#pragma once
#include <string>
#include <functional>

namespace util {

template <typename Value, typename Tag>
class Tagged {
public:
    using ValueType = Value;
    using TagType = Tag;

    explicit Tagged(Value value) : value_(std::move(value)) {}
    
    const Value& operator*() const { return value_; }
    Value& operator*() { return value_; }
    
    bool operator==(const Tagged& other) const { return value_ == other.value_; }
    bool operator!=(const Tagged& other) const { return value_ != other.value_; }

private:
    Value value_;
};

template <typename TaggedValue>
struct TaggedHasher {
    size_t operator()(const TaggedValue& value) const {
        return std::hash<typename TaggedValue::ValueType>{}(*value);
    }
};

}  // namespace util
