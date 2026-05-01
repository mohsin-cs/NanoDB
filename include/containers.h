#pragma once
#include <cstdlib>
#include <cstring>
#include <cstdio>

// ── Generic raw-array Stack (NO STL) ─────────────────────────────────────────
template<typename T>
struct Stack {
    T*  data;
    int top;
    int capacity;

    Stack(int cap = 64) : top(-1), capacity(cap) {
        data = new T[cap];
    }
    ~Stack() { delete[] data; }

    void push(const T& val) {
        if(top+1 < capacity) data[++top] = val;
    }
    T pop() {
        return data[top--];
    }
    T peek() const { return data[top]; }
    bool isEmpty() const { return top < 0; }
    int  size()    const { return top+1; }
};

// ── Generic raw-array dynamic array (replaces std::vector) ───────────────────
template<typename T>
struct Array {
    T*  data;
    int sz;
    int cap;

    Array(int initCap = 16) : sz(0), cap(initCap) {
        data = new T[cap];
    }
    // Deep copy constructor
    Array(const Array& o) : sz(o.sz), cap(o.cap) {
        data = new T[cap];
        for(int i=0;i<sz;i++) data[i]=o.data[i];
    }
    // Copy assignment
    Array& operator=(const Array& o){
        if(this==&o) return *this;
        delete[] data;
        sz=o.sz; cap=o.cap;
        data=new T[cap];
        for(int i=0;i<sz;i++) data[i]=o.data[i];
        return *this;
    }
    ~Array() { delete[] data; }

    void push(const T& val) {
        if(sz == cap){
            cap *= 2;
            T* tmp = new T[cap];
            for(int i=0;i<sz;i++) tmp[i]=data[i];
            delete[] data;
            data = tmp;
        }
        data[sz++] = val;
    }
    void removeAt(int i){
        for(int j=i;j<sz-1;j++) data[j]=data[j+1];
        sz--;
    }
    T& operator[](int i)       { return data[i]; }
    const T& operator[](int i) const { return data[i]; }
    int size() const { return sz; }
    void clear() { sz=0; }
};

// ── Fixed char buffer (safe string, no std::string) ──────────────────────────
struct FixedStr {
    static const int CAP = 256;
    char buf[CAP];
    int  len;
    FixedStr()            { buf[0]='\0'; len=0; }
    FixedStr(const char*s){ set(s); }
    void set(const char* s){
        strncpy(buf,s,CAP-1); buf[CAP-1]='\0';
        len=(int)strlen(buf);
    }
    void append(char c){
        if(len<CAP-1){ buf[len++]=c; buf[len]='\0'; }
    }
    bool equals(const char* s) const { return strcmp(buf,s)==0; }
    const char* c_str() const { return buf; }
    void clear(){ buf[0]='\0'; len=0; }
};
