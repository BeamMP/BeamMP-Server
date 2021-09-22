//
// Created by Anonymous275 on 9/5/2021.
//

#pragma once
#include <cstring>
#include <iostream>

namespace fst {
    template<size_t Cap>
    class stack_string {
    public:
        stack_string() noexcept {
            memset(Data, 0, Cap);
        }
        explicit stack_string(const char* Ptr) {
            size_t len = strlen(Ptr);
            Copy(Ptr, len);
            memset(Data + len, 0, Cap - len);
        }
        stack_string(const char* Ptr, size_t PSize) {
            Copy(Ptr, PSize);
            memset(Data + PSize, 0, Cap - PSize);
        }
        inline size_t capacity() noexcept {
            return Cap;
        }
        inline size_t size() noexcept {
            return Size;
        }
        inline size_t length() noexcept {
            return Size;
        }
        inline char* get() {
            return Data;
        }
        [[nodiscard]] inline const char* c_str() const noexcept {
            return Data;
        }
        char& operator[](size_t idx) {
            if (idx >= Size) {
                throw std::exception("stack_string out of boundaries operator[]");
            }
            return Data[idx];
        }
        inline void resize(size_t newSize) noexcept {
            Size = newSize;
        }
        inline void push_back(const char* Ptr) {
            Copy(Ptr, strlen(Ptr));
        }
        inline void push_back(const char* Ptr, size_t Count) {
            Copy(Ptr, Count);
        }
        inline void push_back(char Ptr) {
            Copy(&Ptr, 1);
        }
        friend std::ostream& operator<<(std::ostream& os, const stack_string& obj) {
            os << obj.Data;
            return os;
        }
        inline stack_string& operator+=(const char* Ptr) {
            push_back(Ptr);
            return *this;
        }
        inline stack_string& operator+=(char Ptr) {
            push_back(Ptr);
            return *this;
        }
    private:
        inline void Copy(const char* Ptr, size_t PSize) {
            if((PSize + Size) <= Cap) {
                memcpy(&Data[Size], Ptr, PSize);
                Size += PSize;
            } else throw std::exception("stack_string out of boundaries copy");
        }
        char Data[Cap]{};
        size_t Size{0};
    };
}
