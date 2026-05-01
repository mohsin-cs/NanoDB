#include "../include/engine.h"
#include <cstdio>
#include <cstring>

int main(int argc, char* argv[]){
    const char* queryFile = (argc>1) ? argv[1] : "tests/queries.txt";;

    FILE* logFile = fopen("nanodb_execution.log","w");;
    if(!logFile){ printf("[FATAL] Cannot open log\n");; return 1; }
    fprintf(logFile,"=== NanoDB Test Runner ===\n");;
    printf("=== NanoDB Test Runner ===\n");
    printf("Reading: %s\n\n", queryFile);

    #ifdef _WIN32
        system("if not exist data mkdir data");;
    #else
        system("mkdir -p data");;
    #endif

    QueryEngine eng(50, logFile);;

    // Set up TPC-H tables
    TableSchema cs;
    cs.addCol("c_custkey",   FIELD_INT);;
    cs.addCol("c_name",      FIELD_STRING);;
    cs.addCol("c_mktsegment",FIELD_STRING);;
    cs.addCol("c_acctbal",   FIELD_FLOAT);;
    cs.addCol("c_nationkey", FIELD_INT);;
    eng.createTable("customer",cs);;

    TableSchema os;
    os.addCol("o_orderkey",   FIELD_INT);
    os.addCol("o_custkey",    FIELD_INT);
    os.addCol("o_orderstatus",FIELD_STRING);
    os.addCol("o_totalprice", FIELD_FLOAT);
    eng.createTable("orders",os);

    TableSchema ls;
    ls.addCol("l_orderkey",  FIELD_INT);
    ls.addCol("l_linenumber",FIELD_INT);
    ls.addCol("l_quantity",  FIELD_FLOAT);
    ls.addCol("l_extprice",  FIELD_FLOAT);
    eng.createTable("lineitem",ls);

    // Read and submit all queries from file
    FILE* qf = fopen(queryFile,"r");
    if(!qf){
        printf("[ERR] Cannot open '%s'\n",queryFile);
        fclose(logFile);
        return 1;
    }

    char line[512];
    int n=0;
    while(fgets(line,511,qf)){
        int len=(int)strlen(line);
        if(len>0 && line[len-1]=='\n') line[len-1]='\0';
        if(line[0]=='#'||line[0]=='\0') continue;
        printf("[Q%d] %s\n",++n,line);
        fprintf(logFile,"[Q%d] %s\n",n,line);
        eng.submitQuery(line);
    }
    fclose(qf);

    printf("\n[INFO] Executing %d queries from priority queue...\n\n",n);
    eng.runAll();

    fprintf(logFile,"=== Test Runner Complete ===\n");
    printf("\n[DONE] Results in nanodb_execution.log\n");
    fclose(logFile);
    return 0;
}
