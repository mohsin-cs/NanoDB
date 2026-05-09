#include "engine.h"
#include <cstdio>
#include <cstring>
#include <ctime>

// ── Helpers ───────────────────────────────────────────────────────────────────
void printBanner(const char* msg){
	//banners
    printf("\n======================================\n");
    printf(" %s\n", msg);
    printf("======================================\n");
}

// ── Build TPC-H schemas ───────────────────────────────────────────────────────
TableSchema makeCustomerSchema(){
    TableSchema s;
    s.addCol("c_custkey",   FIELD_INT);
    s.addCol("c_name",      FIELD_STRING);
    s.addCol("c_mktsegment",FIELD_STRING);
    s.addCol("c_acctbal",   FIELD_FLOAT);
    s.addCol("c_nationkey", FIELD_INT);
    return s;
}

TableSchema makeOrdersSchema(){
    TableSchema s;
    s.addCol("o_orderkey",   FIELD_INT);
    s.addCol("o_custkey",    FIELD_INT);
    s.addCol("o_orderstatus",FIELD_STRING);
    s.addCol("o_totalprice", FIELD_FLOAT);
    return s;
}

TableSchema makeLineitemSchema(){
    TableSchema s;
    s.addCol("l_orderkey",  FIELD_INT);
    s.addCol("l_linenumber",FIELD_INT);
    s.addCol("l_quantity",  FIELD_FLOAT);
    s.addCol("l_extprice",  FIELD_FLOAT);
    return s;
}

// ── Seed test data ────────────────────────────────────────────────────────────
void seedData(QueryEngine& eng){
    printBanner("Seeding TPC-H test data...");

    // Customers
    Table* cust = eng.getTable("customer");
    const char* segs[]={"BUILDING","FURNITURE","AUTOMOBILE","MACHINERY","HOUSEHOLD"};
    for(int i=1;i<=200;i++){
        Row* r=new Row();
        r->addField(new IntField(i));
        char nm[64]; snprintf(nm,63,"Customer#%06d",i);
        r->addField(new StringField(nm));
        r->addField(new StringField(segs[i%5]));
        r->addField(new FloatField((float)(1000 + (i*137)%9000)));
        r->addField(new IntField(i%25));
        cust->addRow(r);
    }

    // Orders
    Table* ord = eng.getTable("orders");
    const char* statuses[]={"O","F","P"};
    for(int i=1;i<=300;i++){
        Row* r=new Row();
        r->addField(new IntField(i));
        r->addField(new IntField((i%200)+1));   // references customer
        r->addField(new StringField(statuses[i%3]));
        r->addField(new FloatField((float)(5000 + (i*251)%95000)));
        ord->addRow(r);;
    }

    // Lineitemss
    Table* li = eng.getTable("lineitem");
    for(int i=1;i<=500;i++){
        Row* r=new Row();
        r->addField(new IntField((i%300)+1));  // references order
        r->addField(new IntField(i%7+1));
        r->addField(new FloatField((float)((i*13)%50+1)));
        r->addField(new FloatField((float)((i*97)%5000+100)));
        li->addRow(r);
    }

    printf("[INFO] Seeded: %d customers, %d orders, %d lineitems\n",
        eng.getTable("customer")->rowCount,
        eng.getTable("orders")->rowCount,
        eng.getTable("lineitem")->rowCount);
}

// ─────────────────────────────────────────────────────────────────────────────
int main(){
    // Open log file
    FILE* logFile = fopen("nanodb_execution.log","w");
    if(!logFile){ printf("[FATAL] Cannot open log file\n"); return 1; }
    fprintf(logFile,"=== NanoDB Execution Log ===\n");

    // Create data directory
    #ifdef _WIN32
        system("if not exist data mkdir data");
    #else
        system("mkdir -p data");
    #endif

    // ── Boot engine with 50-page buffer pool (Test D constraint) ──────────
    QueryEngine eng(50, logFile);

    // Register tables
    eng.createTable("customer", makeCustomerSchema());
    eng.createTable("orders",   makeOrdersSchema());
    eng.createTable("lineitem", makeLineitemSchema());

    seedData(eng);

    // ═════════════════════════════════════════════════════════════════════
    // TEST CASE A — Parser & Evaluator
    // ═════════════════════════════════════════════════════════════════════
    printBanner("TEST CASE A: Parser & Evaluator");
    {
        const char* complexWhere =
            "(c_acctbal > 5000 AND c_mktsegment == \"BUILDING\") OR c_nationkey == 15";
        fprintf(logFile,"\n--- TEST A ---\n");
        Query q; q.type=Q_SELECT;
        strncpy(q.table,"customer",63);
        strncpy(q.whereClause,complexWhere,511);
        eng.executeQuery(q);
    }

    // ═════════════════════════════════════════════════════════════════════
    // TEST CASE B — Index Optimizer
    // ═════════════════════════════════════════════════════════════════════
    printBanner("TEST CASE B: Sequential Scan vs AVL Index");
    {
        fprintf(logFile,"\n--- TEST B ---\n");
        eng.doSelectIndexed("customer", 42);
    }

    // ═════════════════════════════════════════════════════════════════════
    // TEST CASE C — Join Optimizer (MST)
    // ═════════════════════════════════════════════════════════════════════
    printBanner("TEST CASE C: 3-Table Join via MST");
    {
        fprintf(logFile,"\n--- TEST C ---\n");
        Query q;; q.type=Q_JOIN;
        strncpy(q.table, "customer",63);
        strncpy(q.table2,"orders",  63);
        strncpy(q.table3,"lineitem",63);
        strncpy(q.joinOn,"c_custkey == o_custkey AND o_orderkey == l_orderkey",255);
        eng.executeQuery(q);
    }

    // ═════════════════════════════════════════════════════════════════════
    // TEST CASE D — Memory Stress Test (LRU eviction count)
    // ═════════════════════════════════════════════════════════════════════
    printBanner("TEST CASE D: Memory Stress (50-page pool, 500 lineitem rows)");
    {
        fprintf(logFile,"\n--- TEST D ---\n");
        printf("[INFO] Buffer pool capacity: 50 pages\n");
        printf("[INFO] Scanning all lineitem rows...\n");
        Query q; q.type=Q_SELECT;
        strncpy(q.table,"lineitem",63);
        q.whereClause[0]='\0';
        eng.executeQuery(q);
        printf("[RESULT] Total LRU evictions: %d\n", eng.pager->evictionCount);
        fprintf(logFile,"[RESULT] Total LRU evictions during stress test: %d\n",
            eng.pager->evictionCount);
    }

    // ═════════════════════════════════════════════════════════════════════
    // TEST CASE E — Priority Queue Concurrency
    // ═════════════════════════════════════════════════════════════════════
    printBanner("TEST CASE E: Priority Queue — Admin query intercepts 50 SELECTs");
    {
        fprintf(logFile,"\n--- TEST E ---\n");
        // Submit 50 normal SELECT queries
        for(int i=0;i<50;i++){
            char cmd[128];
            snprintf(cmd,127,"SELECT customer WHERE c_custkey > %d",i*4);
            eng.submitQuery(cmd);
        }
        // Submit 1 ADMIN UPDATE — should execute FIRST
        eng.submitQuery("UPDATE customer SET c_acctbal = 9999.99 WHERE c_custkey == 1 PRIORITY ADMIN");

        printf("[INFO] PQ size before drain: %d\n", eng.pq->sz);
        printf("[INFO] Draining priority queue...\n");
        // Execute just the first few to show ordering
        int execCount=0;
        while(!eng.pq->empty() && execCount<5){
            Query q=eng.pq->dequeue();
            printf("[EXEC] type=%d table='%s' priority=%d\n",(int)q.type,q.table,q.priority);
            execCount++;
        }
        // Clear remaining
        while(!eng.pq->empty()) eng.pq->dequeue();
    }

    // ═════════════════════════════════════════════════════════════════════
    // TEST CASE F — Deep Expression Tree
    // ═════════════════════════════════════════════════════════════════════
    printBanner("TEST CASE F: Deep Nested Expression");
    {
        fprintf(logFile,"\n--- TEST F ---\n");
        const char* deepExpr =
            "( (o_totalprice * 1.5) > 100000 AND (o_custkey % 2 == 0) ) OR (o_orderstatus != \"O\")";
        Query q; q.type=Q_SELECT;
        strncpy(q.table,"orders",63);
        strncpy(q.whereClause,deepExpr,511);
        eng.executeQuery(q);
    }

    // ═════════════════════════════════════════════════════════════════════
    // TEST CASE G — Durability & Persistence
    // ═════════════════════════════════════════════════════════════════════
    printBanner("TEST CASE G: Durability — Insert 5 rows, shutdown, reload");
    {
        fprintf(logFile,"\n--- TEST G ---\n");
        printf("[STEP 1] Inserting 5 new customer records...\n");
        for(int i=901;i<=905;i++){
            char cmd[256];
            snprintf(cmd,255,"INSERT customer %d|PersistTest#%d|BUILDING|7777.77|5",i,i);
            eng.submitQuery(cmd);
        }
        eng.runAll();

        printf("[STEP 2] Shutting down NanoDB (flushing to disk)...\n");
        eng.shutdown();
        printf("[STEP 3] Simulating reboot — loading customer table from disk...\n");

        QueryEngine eng2(50, logFile);
        bool loaded = eng2.loadFromDisk("customer");
        if(loaded){
            printf("[STEP 4] Querying for inserted rows (c_custkey >= 901)...\n");
            Query q; q.type=Q_SELECT;
            strncpy(q.table,"customer",63);
            strncpy(q.whereClause,"c_custkey >= 901",511);
            eng2.executeQuery(q);
        } else {
            printf("[WARN] Disk file not found — run again after first flush\n");
        }
    }

    // ═════════════════════════════════════════════════════════════════════
    // Run queries.txt workload file
    // ═════════════════════════════════════════════════════════════════════
    printBanner("Running queries.txt workload file");
    {
        FILE* qf = fopen("tests/queries.txt","r");
        if(qf){
            char line[512];
            int lineNum=0;
            while(fgets(line,511,qf)){
                int len=(int)strlen(line);
                if(len>0 && line[len-1]=='\n') line[len-1]='\0';
                if(line[0]=='#' || line[0]=='\0') continue;  // skip comments
                printf("[WORKLOAD %d] %s\n",++lineNum,line);
                fprintf(logFile,"[WORKLOAD %d] %s\n",lineNum,line);
                eng.submitQuery(line);
            }
            fclose(qf);
            eng.runAll();
        } else {
            printf("[WARN] tests/queries.txt not found\n");
        }
    }

    fprintf(logFile,"\n=== Execution complete ===\n");
    printf("\n[DONE] Log written to nanodb_execution.log\n");
    fclose(logFile);
    return 0;
}
