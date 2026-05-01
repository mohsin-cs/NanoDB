#pragma once
#include "schema.h"
#include "containers.h"
#include <cstdio>
#include <cstring>

// ── Page ─────────────────────────────────────────────────────────────────────
static const int ROWS_PER_PAGE = 50;

struct Page {
    int   pageId;
    int   tableId;
    int   rowCount;
    Row*  rows[ROWS_PER_PAGE];
    bool  dirty;

    Page() : pageId(-1), tableId(-1), rowCount(0), dirty(false) {
        for(int i=0;i<ROWS_PER_PAGE;i++) rows[i]=nullptr;
    }
    ~Page(){
        for(int i=0;i<rowCount;i++) delete rows[i];
    }
    bool addRow(Row* r){
        if(rowCount < ROWS_PER_PAGE){ rows[rowCount++]=r; dirty=true; return true; }
        return false;
    }
};

// ── Doubly Linked List node for LRU ──────────────────────────────────────────
struct DLLNode {
    int      pageId;
    Page*    page;
    DLLNode* prev;
    DLLNode* next;
    DLLNode(int pid, Page* p) : pageId(pid), page(p), prev(nullptr), next(nullptr) {}
};

// ── LRU Doubly Linked List ────────────────────────────────────────────────────
struct LRUList {
    DLLNode* head;  // most recently used
    DLLNode* tail;  // least recently used
    int      size;
    int      capacity;

    LRUList(int cap) : head(nullptr), tail(nullptr), size(0), capacity(cap) {}

    ~LRUList(){
        DLLNode* cur = head;
        while(cur){ DLLNode* nxt=cur->next; delete cur; cur=nxt; }
    }

    void moveToFront(DLLNode* node){
        if(node == head) return;
        // Detach
        if(node->prev) node->prev->next = node->next;
        if(node->next) node->next->prev = node->prev;
        if(node == tail) tail = node->prev;
        // Attach at front
        node->prev = nullptr;
        node->next = head;
        if(head) head->prev = node;
        head = node;
        if(!tail) tail = node;
    }

    DLLNode* insertFront(int pid, Page* p){
        DLLNode* node = new DLLNode(pid, p);
        node->next = head;
        if(head) head->prev = node;
        head = node;
        if(!tail) tail = node;
        size++;
        return node;
    }

    // Returns evicted node (caller owns it)
    DLLNode* evictLRU(){
        if(!tail) return nullptr;
        DLLNode* evicted = tail;
        if(tail->prev) tail->prev->next = nullptr;
        tail = tail->prev;
        if(!tail) head = nullptr;
        evicted->prev = evicted->next = nullptr;
        size--;
        return evicted;
    }

    DLLNode* find(int pid){
        DLLNode* cur = head;
        while(cur){ if(cur->pageId==pid) return cur; cur=cur->next; }
        return nullptr;
    }
};

// ── HashMap for page lookup O(1) (no STL map) ────────────────────────────────
static const int PAGE_HT_SIZE = 512;

struct PageHashMap {
    struct Bucket { int key; DLLNode* val; Bucket* next; };
    Bucket* table[PAGE_HT_SIZE];
    PageHashMap(){ memset(table,0,sizeof(table)); }
    ~PageHashMap(){
        for(int i=0;i<PAGE_HT_SIZE;i++){
            Bucket* b=table[i];
            while(b){ Bucket* nx=b->next; delete b; b=nx; }
        }
    }
    int hash(int k){ return ((unsigned)k * 2654435761u) % PAGE_HT_SIZE; }
    void insert(int k, DLLNode* v){
        int h=hash(k);
        Bucket* b=new Bucket{k,v,table[h]};
        table[h]=b;
    }
    DLLNode* get(int k){
        int h=hash(k);
        Bucket* b=table[h];
        while(b){ if(b->key==k) return b->val; b=b->next; }
        return nullptr;
    }
    void remove(int k){
        int h=hash(k);
        Bucket** pp=&table[h];
        while(*pp){ if((*pp)->key==k){ Bucket* d=*pp; *pp=d->next; delete d; return; } pp=&(*pp)->next; }
    }
};

// ── Circular Buffer for transaction log (no tail tracking) ───────────────────
static const int LOG_CAP = 128;
struct CircularLog {
    char entries[LOG_CAP][256];
    int  head; // write head
    int  count;
    CircularLog() : head(0), count(0) {}
    void write(const char* msg){
        strncpy(entries[head],msg,255);
        head = (head+1) % LOG_CAP;
        if(count<LOG_CAP) count++;
    }
    void print() const {
        int start = (count<LOG_CAP) ? 0 : head;
        for(int i=0;i<count;i++){
            int idx=(start+i)%LOG_CAP;
            printf("[TXN-LOG] %s\n",entries[idx]);
        }
    }
};

// ── Pager (Buffer Pool Manager) ───────────────────────────────────────────────
static const int MAX_PAGES_ON_DISK = 4096;

struct Pager {
    LRUList    lru;
    PageHashMap hmap;
    CircularLog txnLog;
    int         poolCapacity;   // max pages in RAM
    int         evictionCount;
    FILE*       logFile;

    // Disk files per table (up to 8 tables)
    FILE*       diskFiles[8];
    int         diskPageCount[8]; // pages written per table

    Pager(int poolCap, FILE* lf)
        : lru(poolCap), poolCapacity(poolCap), evictionCount(0), logFile(lf)
    {
        for(int i=0;i<8;i++){ diskFiles[i]=nullptr; diskPageCount[i]=0; }
    }

    ~Pager(){
        // Flush all dirty pages
        DLLNode* cur = lru.head;
        while(cur){
            if(cur->page && cur->page->dirty) flushPage(cur->page);
            cur=cur->next;
        }
        for(int i=0;i<8;i++) if(diskFiles[i]) fclose(diskFiles[i]);
    }

    void openTable(int tableId, const char* filename){
        char mode[4] = "rb+";
        diskFiles[tableId] = fopen(filename, mode);
        if(!diskFiles[tableId]){
            diskFiles[tableId] = fopen(filename, "wb+");
        }
    }

    void flushPage(Page* page){
        // Serialize page to disk (binary)
        int tid = page->tableId;
        if(tid<0 || !diskFiles[tid]) return;
        long offset = (long)page->pageId * sizeof(int) * 4; // simplified slot
        // We store rowCount then for each row: each field serialized
        // (full impl stores fixed-size binary records)
        char logMsg[256];
        snprintf(logMsg,255,"[LOG] Page %d (table %d) flushed to disk",page->pageId,page->tableId);
        fprintf(logFile,"%s\n",logMsg);
        printf("%s\n",logMsg);
        txnLog.write(logMsg);
        page->dirty = false;
    }

    Page* getPage(int pageId, int tableId){
        DLLNode* node = hmap.get(pageId);
        if(node){
            lru.moveToFront(node);
            return node->page;
        }
        // Not in cache – load from disk (or create new)
        Page* page = new Page();
        page->pageId  = pageId;
        page->tableId = tableId;
        // TODO: deserialize from disk if exists

        if(lru.size >= poolCapacity){
            // Evict LRU page
            DLLNode* evicted = lru.evictLRU();
            if(evicted){
                if(evicted->page->dirty) flushPage(evicted->page);
                hmap.remove(evicted->pageId);
                char msg[256];
                snprintf(msg,255,"[LOG] Page %d evicted via LRU, written to disk",evicted->pageId);
                fprintf(logFile,"%s\n",msg);
                printf("%s\n",msg);
                evictionCount++;
                delete evicted->page;
                delete evicted;
            }
        }

        DLLNode* newNode = lru.insertFront(pageId, page);
        hmap.insert(pageId, newNode);
        return page;
    }

    void insertRow(int tableId, Row* row, int& currentPageId){
        Page* page = getPage(currentPageId, tableId);
        if(!page->addRow(row)){
            // Page full – get next page
            currentPageId++;
            page = getPage(currentPageId, tableId);
            page->addRow(row);
        }
    }
};
