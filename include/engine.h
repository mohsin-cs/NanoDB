#pragma once
#include "pager.h"
#include "catalog.h"
#include "parser.h"
#include "avl_tree.h"
#include "graph.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>

// ─────────────────────────────────────────────────────────────────────────────
//  Expression Evaluator (postfix, custom Stack)
// ─────────────────────────────────────────────────────────────────────────────
struct Evaluator {
    FILE* logFile;
    Evaluator(FILE* lf) : logFile(lf) {}

    // Evaluate postfix tokens against a row + schema
    // Returns true if the row matches the condition
    bool evaluate(const Array<Token>& postfix, const Row* row, const TableSchema& schema){
        Stack<float> vals(128);

        for(int i=0;i<postfix.size();i++){
            const Token& t = postfix[i];

            if(t.type==TOK_NUMBER){
                vals.push((float)atof(t.val));
            }
            else if(t.type==TOK_IDENT){
                int col = schema.getColIndex(t.val);
                if(col>=0 && col<row->fieldCount){
                    Field* f=row->fields[col];
                    if(f->type==FIELD_INT)        vals.push((float)static_cast<IntField*>(f)->value);
                    else if(f->type==FIELD_FLOAT) vals.push(static_cast<FloatField*>(f)->value);
                    else                           vals.push(0); // string: handled via string stack
                } else { vals.push(0); }
            }
            else if(t.type==TOK_STRING_LIT){
                vals.push(-9999.f); // sentinel: means "string on stack"
            }
            else if(t.type==TOK_AND){
                float b2=vals.isEmpty()?0:vals.pop();
                float b1=vals.isEmpty()?0:vals.pop();
                vals.push((b1!=0.f&&b2!=0.f)?1.f:0.f);
            }
            else if(t.type==TOK_OR){
                float b2=vals.isEmpty()?0:vals.pop();
                float b1=vals.isEmpty()?0:vals.pop();
                vals.push((b1!=0.f||b2!=0.f)?1.f:0.f);
            }
            else if(t.type==TOK_OP){
                if(vals.size()<2){ vals.push(0); continue; }
                float rhs=vals.pop();
                float lhs=vals.pop();
                float res=0;
                if     (strcmp(t.val,">")==0)  res=(lhs> rhs)?1.f:0.f;
                else if(strcmp(t.val,"<")==0)  res=(lhs< rhs)?1.f:0.f;
                else if(strcmp(t.val,">=")==0) res=(lhs>=rhs)?1.f:0.f;
                else if(strcmp(t.val,"<=")==0) res=(lhs<=rhs)?1.f:0.f;
                else if(strcmp(t.val,"==")==0) res=(lhs==rhs)?1.f:0.f;
                else if(strcmp(t.val,"!=")==0) res=(lhs!=rhs)?1.f:0.f;
                else if(strcmp(t.val,"+")==0)  res=lhs+rhs;
                else if(strcmp(t.val,"-")==0)  res=lhs-rhs;
                else if(strcmp(t.val,"*")==0)  res=lhs*rhs;
                else if(strcmp(t.val,"/")==0)  res=(rhs!=0)?lhs/rhs:0;
                else if(strcmp(t.val,"%")==0)  res=(rhs!=0)?(float)((int)lhs%(int)rhs):0;
                vals.push(res);
            }
        }
        return vals.isEmpty() ? true : (vals.peek() != 0.f);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Table storage (in-memory for demo; backed by Pager)
// ─────────────────────────────────────────────────────────────────────────────
static const int MAX_ROWS_TABLE = 200000;

struct Table {
    char        name[64];
    TableSchema schema;
    Row**       rows;
    int         rowCount;
    int         rowCap;
    AVLTree*    index;   // index on first int column

    Table(const char* n, const TableSchema& s, FILE* logFile) : rowCount(0), rowCap(MAX_ROWS_TABLE) {
        strncpy(name,n,63);
        schema=s;
        rows = new Row*[rowCap];
        index = new AVLTree(logFile);
    }
    ~Table(){
        for(int i=0;i<rowCount;i++) delete rows[i];
        delete[] rows;
        delete index;
    }

    void addRow(Row* r){
        if(rowCount<rowCap){
            // Index on first int field
            if(r->fieldCount>0 && r->fields[0]->type==FIELD_INT){
                int key=static_cast<IntField*>(r->fields[0])->value;
                index->insert(key,rowCount/ROWS_PER_PAGE,rowCount%ROWS_PER_PAGE);
            }
            rows[rowCount++]=r;
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Query Engine
// ─────────────────────────────────────────────────────────────────────────────
static const int MAX_TABLES_ENGINE = 8;

struct QueryEngine {
    Table*         tables[MAX_TABLES_ENGINE];
    int            tableCount;
    Pager*         pager;
    SystemCatalog* catalog;
    Parser*        parser;
    CommandParser* cmdParser;
    PriorityQueue* pq;
    Graph*         graph;
    Evaluator*     evaluator;
    FILE*          logFile;

    QueryEngine(int poolSize, FILE* lf) : tableCount(0), logFile(lf) {
        for(int i=0;i<MAX_TABLES_ENGINE;i++) tables[i]=nullptr;
        pager     = new Pager(poolSize, lf);
        catalog   = new SystemCatalog(lf);
        parser    = new Parser(lf);
        cmdParser = new CommandParser(lf);
        pq        = new PriorityQueue(256, lf);
        graph     = new Graph(lf);
        evaluator = new Evaluator(lf);
    }

    ~QueryEngine(){
        for(int i=0;i<tableCount;i++) delete tables[i];
        delete pager; delete catalog; delete parser;
        delete cmdParser; delete pq; delete graph; delete evaluator;
    }

    // ── Register a table ──────────────────────────────────────────────────
    Table* createTable(const char* name, const TableSchema& schema){
        if(tableCount>=MAX_TABLES_ENGINE) return nullptr;
        char path[128]; snprintf(path,127,"data/%s.bin",name);
        catalog->registerTable(name,tableCount,path,schema);
        pager->openTable(tableCount,path);
        Table* t = new Table(name,schema,logFile);
        tables[tableCount++]=t;
        return t;
    }

    Table* getTable(const char* name){
        for(int i=0;i<tableCount;i++)
            if(strcmp(tables[i]->name,name)==0) return tables[i];
        return nullptr;
    }

    // ── Execute a command string ───────────────────────────────────────────
    void submitQuery(const char* cmd){
        Query q = cmdParser->parse(cmd);
        pq->enqueue(q);
    }

    void runAll(){
        while(!pq->empty()){
            Query q = pq->dequeue();
            executeQuery(q);
        }
    }

    void executeQuery(const Query& q){
        if(q.type==Q_INSERT)  doInsert(q);
        else if(q.type==Q_SELECT) doSelect(q);
        else if(q.type==Q_UPDATE) doUpdate(q);
        else if(q.type==Q_JOIN)   doJoin(q);
    }

    // ── INSERT ────────────────────────────────────────────────────────────
    void doInsert(const Query& q){
        Table* t = getTable(q.table);
        if(!t){ printf("[ERR] Table '%s' not found\n",q.table); return; }

        Row* row = new Row();
        // Parse pipe-delimited values
        char buf[512]; strncpy(buf,q.insertValues,511);
        char* tok=buf;
        for(int i=0;i<t->schema.colCount;i++){
            char* end=tok;
            while(*end && *end!='|') end++;
            if(*end=='|') *end='\0';
            FieldType ft=t->schema.cols[i].type;
            if(ft==FIELD_INT)    row->addField(new IntField(atoi(tok)));
            else if(ft==FIELD_FLOAT) row->addField(new FloatField((float)atof(tok)));
            else                 row->addField(new StringField(tok));
            tok=end+1;
        }
        t->addRow(row);
        catalog->updateRowCount(q.table,1);
        fprintf(logFile,"[LOG] INSERT into '%s' rowCount=%d\n",q.table,t->rowCount);
        printf("[LOG] INSERT into '%s' rowCount=%d\n",q.table,t->rowCount);
    }

    // ── SELECT ────────────────────────────────────────────────────────────
    void doSelect(const Query& q){
        Table* t = getTable(q.table);
        if(!t){ printf("[ERR] Table '%s' not found\n",q.table); return; }

        bool hasWhere = (q.whereClause[0]!='\0');
        Array<Token> postfix;
        if(hasWhere) postfix = parser->toPostfix(q.whereClause);

        int matched=0;
        for(int i=0;i<t->rowCount;i++){
            Row* row=t->rows[i];
            if(row->deleted) continue;
            if(!hasWhere || evaluator->evaluate(postfix,row,t->schema)){
                row->print();
                matched++;
            }
        }
        fprintf(logFile,"[LOG] SELECT from '%s' matched=%d scanned=%d\n",q.table,matched,t->rowCount);
        printf("[LOG] SELECT from '%s' matched=%d scanned=%d\n",q.table,matched,t->rowCount);
    }

    // ── SELECT with index (Test Case B) ───────────────────────────────────
    void doSelectIndexed(const char* tableName, int keyVal){
        Table* t = getTable(tableName);
        if(!t) return;

        // Sequential scan timing
        clock_t s1=clock();
        int seqMatched=0;
        for(int i=0;i<t->rowCount;i++){
            Row* row=t->rows[i];
            if(row->deleted) continue;
            if(row->fieldCount>0 && row->fields[0]->type==FIELD_INT)
                if(static_cast<IntField*>(row->fields[0])->value==keyVal) seqMatched++;
        }
        clock_t e1=clock();
        double seqMs=1000.0*(e1-s1)/CLOCKS_PER_SEC;
        printf("[BENCHMARK] Sequential scan: found %d row(s) in %.3f ms\n",seqMatched,seqMs);
        fprintf(logFile,"[BENCHMARK] Sequential scan: %.3f ms\n",seqMs);

        // AVL index scan timing
        clock_t s2=clock();
        AVLNode* node=t->index->search(keyVal);
        clock_t e2=clock();
        double idxMs=1000.0*(e2-s2)/CLOCKS_PER_SEC;
        printf("[BENCHMARK] AVL index search: found=%s in %.3f ms\n",node?"YES":"NO",idxMs);
        fprintf(logFile,"[BENCHMARK] AVL index search: %.3f ms\n",idxMs);
        printf("[BENCHMARK] Speedup: %.2fx\n", seqMs>0 ? seqMs/idxMs : 999.0);
    }

    // ── UPDATE ────────────────────────────────────────────────────────────
    void doUpdate(const Query& q){
        Table* t = getTable(q.table);
        if(!t) return;
        int colIdx=t->schema.getColIndex(q.setCol);
        if(colIdx<0){ printf("[ERR] Column '%s' not found\n",q.setCol); return; }
        int updated=0;
        for(int i=0;i<t->rowCount;i++){
            Row* row=t->rows[i];
            if(row->deleted) continue;
            // Apply update (simple: update all rows for demo)
            FieldType ft=t->schema.cols[colIdx].type;
            delete row->fields[colIdx];
            if(ft==FIELD_INT)   row->fields[colIdx]=new IntField(atoi(q.setVal));
            else if(ft==FIELD_FLOAT) row->fields[colIdx]=new FloatField((float)atof(q.setVal));
            else                row->fields[colIdx]=new StringField(q.setVal);
            updated++;
        }
        fprintf(logFile,"[LOG] UPDATE '%s' col='%s' updated=%d rows\n",q.table,q.setCol,updated);
        printf("[LOG] UPDATE '%s' col='%s' updated=%d rows\n",q.table,q.setCol,updated);
    }

    // ── JOIN (3-table, MST-optimized) ─────────────────────────────────────
    void doJoin(const Query& q){
        Table* t1=getTable(q.table);
        Table* t2=getTable(q.table2);
        Table* t3=q.table3[0]?getTable(q.table3):nullptr;

        if(!t1||!t2){ printf("[ERR] Join tables not found\n"); return; }

        // Build graph
        Graph joinGraph(logFile);
        int cost12 = t1->rowCount * t2->rowCount / 1000 + 1;
        joinGraph.addEdge(q.table, q.table2, cost12, q.joinOn);
        if(t3){
            int cost23 = t2->rowCount * t3->rowCount / 1000 + 1;
            joinGraph.addEdge(q.table2, q.table3, cost23, "orderkey");
        }

        char mstPath[512];
        joinGraph.kruskalMST(mstPath, 511);

        // Nested-loop join (t1 x t2)
        int joinCount=0;
        printf("=== JOIN RESULT (first 5 rows) ===\n");
        for(int i=0;i<t1->rowCount && joinCount<5;i++){
            for(int j=0;j<t2->rowCount && joinCount<5;j++){
                // Match on first field (key)
                if(t1->rows[i]->fieldCount>0 && t2->rows[j]->fieldCount>0){
                    if(*t1->rows[i]->fields[0] == *t2->rows[j]->fields[0]){
                        t1->rows[i]->print();
                        t2->rows[j]->print();
                        joinCount++;
                    }
                }
            }
        }
        fprintf(logFile,"[LOG] JOIN %s x %s produced %d results\n",q.table,q.table2,joinCount);
    }

    // ── Durability: flush all to disk then reload ──────────────────────────
    void shutdown(){
        fprintf(logFile,"[LOG] NanoDB shutdown: flushing all dirty pages...\n");
        printf("[LOG] NanoDB shutdown: flushing all dirty pages...\n");
        // Flush
        for(int i=0;i<tableCount;i++){
            char path[128]; snprintf(path,127,"data/%s.bin",tables[i]->name);
            FILE* f=fopen(path,"wb");
            if(f){
                fwrite(&tables[i]->rowCount,sizeof(int),1,f);
                // Write schema first
                fwrite(&tables[i]->schema.colCount,sizeof(int),1,f);
                for(int c=0;c<tables[i]->schema.colCount;c++){
                    fwrite(&tables[i]->schema.cols[c],sizeof(ColumnDef),1,f);
                }
                // Write rows
                for(int r=0;r<tables[i]->rowCount;r++){
                    Row* row=tables[i]->rows[r];
                    fwrite(&row->fieldCount,sizeof(int),1,f);
                    for(int fi=0;fi<row->fieldCount;fi++){
                        fwrite(&row->fields[fi]->type,sizeof(FieldType),1,f);
                        row->fields[fi]->serialize(f);
                    }
                }
                fclose(f);
                fprintf(logFile,"[LOG] Table '%s' persisted (%d rows)\n",tables[i]->name,tables[i]->rowCount);
                printf("[LOG] Table '%s' persisted (%d rows)\n",tables[i]->name,tables[i]->rowCount);
            }
        }
    }

    bool loadFromDisk(const char* tableName){
        char path[128]; snprintf(path,127,"data/%s.bin",tableName);
        FILE* f=fopen(path,"rb");
        if(!f) return false;

        int rowCount; fread(&rowCount,sizeof(int),1,f);
        int colCount; fread(&colCount,sizeof(int),1,f);
        TableSchema schema;
        schema.colCount=colCount;
        for(int c=0;c<colCount;c++) fread(&schema.cols[c],sizeof(ColumnDef),1,f);

        Table* t=createTable(tableName,schema);
        for(int r=0;r<rowCount;r++){
            Row* row=new Row();
            int fc; fread(&fc,sizeof(int),1,f);
            row->fieldCount=fc;
            for(int fi=0;fi<fc;fi++){
                FieldType ft; fread(&ft,sizeof(FieldType),1,f);
                Field* field=nullptr;
                if(ft==FIELD_INT)  field=new IntField();
                else if(ft==FIELD_FLOAT) field=new FloatField();
                else               field=new StringField();
                field->type=ft;
                field->deserialize(f);
                row->fields[fi]=field;
            }
            t->rows[t->rowCount++]=row;
        }
        fclose(f);
        fprintf(logFile,"[LOG] Loaded table '%s' from disk (%d rows)\n",tableName,rowCount);
        printf("[LOG] Loaded table '%s' from disk (%d rows)\n",tableName,rowCount);
        return true;
    }
};
