#pragma once
#include "containers.h"
#include "schema.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

// ─────────────────────────────────────────────────────────────────────────────
//  Token
// ─────────────────────────────────────────────────────────────────────────────
enum TokenType {
    TOK_IDENT, TOK_NUMBER, TOK_STRING_LIT,
    TOK_OP,    TOK_LPAREN, TOK_RPAREN,
    TOK_AND,   TOK_OR,     TOK_NOT,
    TOK_EOF
};

struct Token {
    TokenType type;
    char      val[128];
    Token(){ type=TOK_EOF; val[0]='\0'; }
    Token(TokenType t, const char* v){ type=t; strncpy(val,v,127); val[127]='\0'; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Tokenizer / Lexer
// ─────────────────────────────────────────────────────────────────────────────
struct Tokenizer {
    const char* src;
    int         pos;
    int         len;

    Tokenizer(const char* s) : src(s), pos(0) { len=(int)strlen(s); }

    void skipWS(){ while(pos<len && src[pos]==' ') pos++; }

    bool isDigit(char c){ return c>='0'&&c<='9'; }
    bool isAlpha(char c){ return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'; }

    Token next(){
        skipWS();
        if(pos>=len) return Token(TOK_EOF,"");
        char c=src[pos];

        // String literal
        if(c=='"'){
            pos++;
            char buf[128]; int bi=0;
            while(pos<len && src[pos]!='"') buf[bi++]=src[pos++];
            buf[bi]='\0'; pos++;
            return Token(TOK_STRING_LIT,buf);
        }
        // Parentheses
        if(c=='('){ pos++; return Token(TOK_LPAREN,"("); }
        if(c==')'){ pos++; return Token(TOK_RPAREN,")"); }

        // Operators: >=  <=  ==  !=  >  <  +  -  *  /  %
        if(c=='>'||c=='<'||c=='='||c=='!'||c=='+'||c=='-'||c=='*'||c=='/'||c=='%'){
            char buf[4]={c,'\0','\0','\0'};
            if(pos+1<len && (src[pos+1]=='='||src[pos+1]=='>')){
                buf[1]=src[pos+1]; buf[2]='\0'; pos+=2;
            } else { pos++; }
            return Token(TOK_OP,buf);
        }
        // Number
        if(isDigit(c)||c=='.'){
            char buf[64]; int bi=0;
            while(pos<len && (isDigit(src[pos])||src[pos]=='.')) buf[bi++]=src[pos++];
            buf[bi]='\0';
            return Token(TOK_NUMBER,buf);
        }
        // Identifier / keyword
        if(isAlpha(c)){
            char buf[128]; int bi=0;
            while(pos<len && (isAlpha(src[pos])||isDigit(src[pos]))) buf[bi++]=src[pos++];
            buf[bi]='\0';
            if(strcmp(buf,"AND")==0) return Token(TOK_AND,"AND");
            if(strcmp(buf,"OR") ==0) return Token(TOK_OR, "OR");
            if(strcmp(buf,"NOT")==0) return Token(TOK_NOT,"NOT");
            return Token(TOK_IDENT,buf);
        }
        pos++;
        return Token(TOK_EOF,"");
    }

    Array<Token> tokenizeAll(){
        Array<Token> out;
        while(true){
            Token t=next();
            out.push(t);
            if(t.type==TOK_EOF) break;
        }
        return out;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Infix → Postfix (Shunting-Yard, custom Stack)
// ─────────────────────────────────────────────────────────────────────────────
struct Parser {
    FILE* logFile;
    Parser(FILE* lf) : logFile(lf) {}

    int precedence(const char* op){
        if(strcmp(op,"OR") ==0)               return 1;
        if(strcmp(op,"AND")==0)               return 2;
        if(strcmp(op,"NOT")==0)               return 3;
        if(strcmp(op,"==")==0||strcmp(op,"!=")==0) return 4;
        if(strcmp(op,"<")==0||strcmp(op,">")==0||
           strcmp(op,"<=")==0||strcmp(op,">=")==0) return 4;
        if(strcmp(op,"+")==0||strcmp(op,"-")==0)   return 5;
        if(strcmp(op,"*")==0||strcmp(op,"/")==0||
           strcmp(op,"%")==0)                      return 6;
        return 0;
    }

    bool isOperator(const Token& t){
        return t.type==TOK_OP || t.type==TOK_AND || t.type==TOK_OR || t.type==TOK_NOT;
    }

    // Returns postfix token array; also writes infix and postfix to log
    Array<Token> toPostfix(const char* infixExpr){
        Tokenizer tz(infixExpr);
        Array<Token> tokens = tz.tokenizeAll();

        Stack<Token> opStack(128);
        Array<Token> output;

        for(int i=0;i<tokens.size();i++){
            Token& t = tokens[i];
            if(t.type==TOK_EOF) break;

            if(t.type==TOK_NUMBER || t.type==TOK_IDENT || t.type==TOK_STRING_LIT){
                output.push(t);
            } else if(isOperator(t)){
                while(!opStack.isEmpty() &&
                      isOperator(opStack.peek()) &&
                      precedence(opStack.peek().val) >= precedence(t.val)){
                    output.push(opStack.pop());
                }
                opStack.push(t);
            } else if(t.type==TOK_LPAREN){
                opStack.push(t);
            } else if(t.type==TOK_RPAREN){
                while(!opStack.isEmpty() && opStack.peek().type!=TOK_LPAREN){
                    output.push(opStack.pop());
                }
                if(!opStack.isEmpty()) opStack.pop(); // discard '('
            }
        }
        while(!opStack.isEmpty()) output.push(opStack.pop());

        // Build postfix string for log
        char postfixStr[512]="";
        for(int i=0;i<output.size();i++){
            if(i) strcat(postfixStr," ");
            strncat(postfixStr,output[i].val,511-strlen(postfixStr));
        }
        char msg[768];
        snprintf(msg,767,"[LOG] Infix \"%s\" converted to Postfix \"%s\"",infixExpr,postfixStr);
        fprintf(logFile,"%s\n",msg);
        printf("%s\n",msg);

        return output;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Parsed Query
// ─────────────────────────────────────────────────────────────────────────────
enum QueryType { Q_SELECT, Q_INSERT, Q_UPDATE, Q_JOIN, Q_UNKNOWN };

struct Query {
    QueryType   type;
    char        table[64];
    char        table2[64];
    char        table3[64];
    char        whereClause[512];
    char        setCol[64];
    char        setVal[128];
    int         priority;       // 0=normal, 10=admin
    char        joinOn[256];
    // Parsed fields for INSERT
    char        insertValues[512];

    Query(){
        type=Q_UNKNOWN; priority=0;
        table[0]=table2[0]=table3[0]='\0';
        whereClause[0]=setCol[0]=setVal[0]='\0';
        joinOn[0]=insertValues[0]='\0';
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  SQL-like command parser
// ─────────────────────────────────────────────────────────────────────────────
struct CommandParser {
    FILE* logFile;
    CommandParser(FILE* lf) : logFile(lf) {}

    // Trim leading whitespace
    const char* ltrim(const char* s){
        while(*s==' ') s++;
        return s;
    }

    // Copy word until space/null
    int readWord(const char* src, char* dst, int maxLen){
        int i=0;
        while(src[i] && src[i]!=' ' && i<maxLen-1){ dst[i]=src[i]; i++; }
        dst[i]='\0';
        return i;
    }

    Query parse(const char* cmd){
        Query q;
        const char* p = ltrim(cmd);
        char keyword[32];
        int consumed = readWord(p,keyword,32);
        p = ltrim(p+consumed);

        if(strcmp(keyword,"SELECT")==0){
            q.type=Q_SELECT;
            // SELECT <table> WHERE <expr>  or  SELECT <table>
            char tbl[64]; consumed=readWord(p,tbl,64);
            strncpy(q.table,tbl,63);
            p=ltrim(p+consumed);
            if(strncmp(p,"WHERE",5)==0){
                p=ltrim(p+5);
                strncpy(q.whereClause,p,511);
            }
        } else if(strcmp(keyword,"INSERT")==0){
            q.type=Q_INSERT;
            char tbl[64]; consumed=readWord(p,tbl,64);
            strncpy(q.table,tbl,63);
            p=ltrim(p+consumed);
            strncpy(q.insertValues,p,511);
        } else if(strcmp(keyword,"UPDATE")==0){
            q.type=Q_UPDATE;
            char tbl[64]; consumed=readWord(p,tbl,64);
            strncpy(q.table,tbl,63);
            p=ltrim(p+consumed);
            // SET col = val WHERE ...  or  SET ... PRIORITY ADMIN
            if(strncmp(p,"SET",3)==0){
                p=ltrim(p+3);
                char col[64]; consumed=readWord(p,col,64);
                strncpy(q.setCol,col,63);
                p=ltrim(p+consumed);
                if(*p=='=') p=ltrim(p+1);
                char val[128]; consumed=readWord(p,val,128);
                strncpy(q.setVal,val,127);
                p=ltrim(p+consumed);
            }
            if(strstr(p,"PRIORITY ADMIN")) q.priority=10;
        } else if(strcmp(keyword,"JOIN")==0){
            q.type=Q_JOIN;
            char t1[64]; consumed=readWord(p,t1,64); strncpy(q.table,t1,63); p=ltrim(p+consumed);
            char t2[64]; consumed=readWord(p,t2,64); strncpy(q.table2,t2,63); p=ltrim(p+consumed);
            // optional 3rd table
            if(*p && strncmp(p,"ON",2)!=0){
                char t3[64]; consumed=readWord(p,t3,64); strncpy(q.table3,t3,63); p=ltrim(p+consumed);
            }
            if(strncmp(p,"ON",2)==0){ p=ltrim(p+2); strncpy(q.joinOn,p,255); }
        }

        char msg[512];
        snprintf(msg,511,"[LOG] Parsed query type=%d table='%s' where='%s' priority=%d",
            (int)q.type,q.table,q.whereClause,q.priority);
        fprintf(logFile,"%s\n",msg);
        printf("%s\n",msg);
        return q;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Priority Queue (max-heap, raw array, NO STL)
// ─────────────────────────────────────────────────────────────────────────────
struct PQEntry {
    Query q;
    int   priority;
};

struct PriorityQueue {
    PQEntry* heap;
    int      sz;
    int      cap;
    FILE*    logFile;

    PriorityQueue(int c, FILE* lf) : sz(0), cap(c), logFile(lf) {
        heap = new PQEntry[c];
    }
    ~PriorityQueue(){ delete[] heap; }

    void swap(int a, int b){ PQEntry tmp=heap[a]; heap[a]=heap[b]; heap[b]=tmp; }

    void heapifyUp(int i){
        while(i>0){
            int p=(i-1)/2;
            if(heap[p].priority < heap[i].priority){ swap(p,i); i=p; }
            else break;
        }
    }
    void heapifyDown(int i){
        while(true){
            int l=2*i+1, r=2*i+2, largest=i;
            if(l<sz && heap[l].priority>heap[largest].priority) largest=l;
            if(r<sz && heap[r].priority>heap[largest].priority) largest=r;
            if(largest==i) break;
            swap(i,largest); i=largest;
        }
    }

    void enqueue(const Query& q){
        if(sz<cap){
            heap[sz]={q,q.priority};
            heapifyUp(sz++);
            char msg[256];
            snprintf(msg,255,"[LOG] PQ: enqueued query (table='%s' priority=%d) heap_size=%d",
                q.table,q.priority,sz);
            fprintf(logFile,"%s\n",msg);
            printf("%s\n",msg);
        }
    }

    Query dequeue(){
        Query q=heap[0].q;
        char msg[256];
        snprintf(msg,255,"[LOG] PQ: dequeued query (table='%s' priority=%d) — executing next",
            q.table,q.priority);
        fprintf(logFile,"%s\n",msg);
        printf("%s\n",msg);
        heap[0]=heap[--sz];
        heapifyDown(0);
        return q;
    }

    bool empty() const { return sz==0; }
};
