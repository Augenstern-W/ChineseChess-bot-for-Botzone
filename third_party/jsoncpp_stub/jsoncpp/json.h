#ifndef JSONCPP_STUB_H
#define JSONCPP_STUB_H

// 【仅用于本地语法检查】Botzone 提交版的 jsoncpp 桩实现。
// 平台编译时使用的是平台自带的真实 jsoncpp，本文件不参与提交。
// 只覆盖 botzone_main.cpp 用到的最小 API 子集。

#include <cstddef>
#include <string>

namespace Json {

    class Value {
    public:
        Value() {}
        Value& operator[](const std::string&) { return *this; }
        const Value& operator[](const std::string&) const { return *this; }
        Value& operator[](int) { return *this; }
        const Value& operator[](int) const { return *this; }
        Value& operator=(const std::string&) { return *this; }
        bool isMember(const std::string&) const { return false; }
        bool isArray() const { return false; }
        bool isObject() const { return false; }
        bool isInt() const { return false; }
        bool isUInt() const { return false; }
        bool isString() const { return false; }
        int asInt() const { return 0; }
        unsigned int asUInt() const { return 0u; }
        std::string asString() const { return std::string(); }
        size_t size() const { return 0; }
    };

    class Reader {
    public:
        bool parse(const std::string&, Value&) { return true; }
    };

    class FastWriter {
    public:
        std::string write(const Value&) { return std::string(); }
    };

} // namespace Json

#endif // JSONCPP_STUB_H
