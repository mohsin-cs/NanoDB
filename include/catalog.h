#pragma once
#include "schema.h"
#include "containers.h"
#include <cstring>
#include <cstdio>

// ── System Catalog: maps table name → metadata ────────────────────────────────
// Custom chaining HashMap (NO STL map)

static const int CATALOG_HT = 64;

struct TableMeta {
    char        name[64];
    int         tableId;
    char        filePath[128];
    TableSchema schema;
    int         currentPageId;  // last used page
    int         rowCount;
    TableMeta() : tableId(-1), currentPageId(0), rowCount(0) {
        name[0]='\0'; filePath[0]='\0';
    }
};

struct CatalogNode {
    TableMeta  meta;
    CatalogNode* next;
    CatalogNode(const TableMeta& m) : meta(m), next(nullptr) {}
};

struct SystemCatalog {
    CatalogNode* table[CATALOG_HT];
    int          tableCount;
    FILE*        logFile;

    SystemCatalog(FILE* lf) : tableCount(0), logFile(lf) {
        memset(table, 0, sizeof(table));
    }

    ~SystemCatalog(){
        for(int i=0;i<CATALOG_HT;i++){
            CatalogNode* n=table[i];
            while(n){ CatalogNode* nx=n->next; delete n; n=nx; }
        }
    }

    // djb2 hash
    int hash(const char* key) const {
        unsigned long h = 5381;
        int c;
        const char* p = key;
        while((c=*p++)) h = ((h<<5)+h)^c;
        return (int)(h % CATALOG_HT);
    }

    // Register a new table
    void registerTable(const char* name, int id, const char* path, const TableSchema& schema){
        int h = hash(name);
        TableMeta m;
        strncpy(m.name,    name, 63);
        strncpy(m.filePath,path,127);
        m.tableId = id;
        m.schema  = schema;
        CatalogNode* node = new CatalogNode(m);
        node->next = table[h];
        table[h]   = node;
        tableCount++;
        char msg[256];
        snprintf(msg,255,"[LOG] Catalog: table '%s' registered at slot %d (id=%d)",name,h,id);
        fprintf(logFile,"%s\n",msg);
        printf("%s\n",msg);
    }

    // O(1) average lookup
    TableMeta* getTable(const char* name){
        int h = hash(name);
        CatalogNode* n = table[h];
        while(n){
            if(strcmp(n->meta.name,name)==0) return &n->meta;
            n=n->next;
        }
        return nullptr;
    }

    void updateRowCount(const char* name, int delta){
        TableMeta* m = getTable(name);
        if(m) m->rowCount += delta;
    }

    void printAll() const {
        printf("=== System Catalog ===\n");
        for(int i=0;i<CATALOG_HT;i++){
            CatalogNode* n=table[i];
            while(n){
                printf("  [%d] %s  id=%d  rows=%d  path=%s\n",
                    i,n->meta.name,n->meta.tableId,n->meta.rowCount,n->meta.filePath);
                n=n->next;
            }
        }
    }
};
