#pragma once
#include "containers.h"
#include <cstdio>
#include <cstring>

// ── Graph edge (join cost between two tables) ─────────────────────────────────
struct Edge {
    int  u, v;          // table indices
    int  cost;          // estimated join cost (row counts product)
    char label[128];    // e.g. "customer JOIN orders ON c_custkey==o_custkey"
    Edge() : u(0),v(0),cost(0){ label[0]='\0'; }
    Edge(int u,int v,int c,const char* l) : u(u),v(v),cost(c){
        strncpy(label,l,127); label[127]='\0';
    }
};

// ── Union-Find (for Kruskal's cycle detection, NO STL) ───────────────────────
struct UnionFind {
    int* parent;
    int* rank;
    int  n;
    UnionFind(int n) : n(n){
        parent = new int[n];
        rank   = new int[n];
        for(int i=0;i<n;i++){ parent[i]=i; rank[i]=0; }
    }
    ~UnionFind(){ delete[] parent; delete[] rank; }

    int find(int x){
        while(parent[x]!=x) x=parent[x]=parent[parent[x]]; // path compression
        return x;
    }
    bool unite(int a, int b){
        a=find(a); b=find(b);
        if(a==b) return false;
        if(rank[a]<rank[b]) { int t=a;a=b;b=t; }
        parent[b]=a;
        if(rank[a]==rank[b]) rank[a]++;
        return true;
    }
};

// ── Graph (adjacency list via raw arrays) ────────────────────────────────────
static const int MAX_TABLES_GRAPH = 16;

struct Graph {
    char  tableNames[MAX_TABLES_GRAPH][64];
    int   tableCount;
    Array<Edge> edges;
    FILE* logFile;

    Graph(FILE* lf) : tableCount(0), logFile(lf) {
        for(int i=0;i<MAX_TABLES_GRAPH;i++) tableNames[i][0]='\0';
    }

    int addTable(const char* name){
        for(int i=0;i<tableCount;i++)
            if(strcmp(tableNames[i],name)==0) return i;
        strncpy(tableNames[tableCount],name,63);
        return tableCount++;
    }

    void addEdge(const char* t1, const char* t2, int cost, const char* label){
        int u = addTable(t1);
        int v = addTable(t2);
        edges.push(Edge(u,v,cost,label));
        char msg[256];
        snprintf(msg,255,"[LOG] Graph: edge added %s — %s  cost=%d",t1,t2,cost);
        fprintf(logFile,"%s\n",msg);
        printf("%s\n",msg);
    }

    // ── Sort edges by cost (insertion sort, NO std::sort) ─────────────────
    void sortEdges(){
        for(int i=1;i<edges.size();i++){
            Edge key=edges[i];
            int j=i-1;
            while(j>=0 && edges[j].cost>key.cost){ edges[j+1]=edges[j]; j--; }
            edges[j+1]=key;
        }
    }

    // ── Kruskal's MST ─────────────────────────────────────────────────────
    // Returns MST path as string written into outPath
    void kruskalMST(char* outPath, int outLen){
        sortEdges();
        UnionFind uf(tableCount);
        Array<Edge> mst;

        for(int i=0;i<edges.size();i++){
            Edge& e=edges[i];
            if(uf.unite(e.u,e.v)){
                mst.push(e);
                if(mst.size()==tableCount-1) break;
            }
        }

        // Build path string
        outPath[0]='\0';
        for(int i=0;i<mst.size();i++){
            if(i) strncat(outPath," -> ",outLen-strlen(outPath)-1);
            strncat(outPath,tableNames[mst[i].u],outLen-strlen(outPath)-1);
            if(i==mst.size()-1){
                strncat(outPath," -> ",outLen-strlen(outPath)-1);
                strncat(outPath,tableNames[mst[i].v],outLen-strlen(outPath)-1);
            }
        }

        char msg[512];
        snprintf(msg,511,"[LOG] Multi-table join routed via MST: %s",outPath);
        fprintf(logFile,"%s\n",msg);
        printf("%s\n",msg);

        // Print MST details
        printf("=== MST Join Plan ===\n");
        int totalCost=0;
        for(int i=0;i<mst.size();i++){
            printf("  %s JOIN %s  cost=%d  [%s]\n",
                tableNames[mst[i].u],tableNames[mst[i].v],mst[i].cost,mst[i].label);
            totalCost+=mst[i].cost;
        }
        printf("  Total estimated cost: %d\n",totalCost);
    }

    void printGraph(){
        printf("=== Join Graph ===\n");
        for(int i=0;i<edges.size();i++){
            printf("  %s -- %s  cost=%d\n",
                tableNames[edges[i].u],tableNames[edges[i].v],edges[i].cost);
        }
    }
};
