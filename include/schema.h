#pragma once
#include <cstring>
#include <cstdio>

// ── Field types ──────────────────────────────────────────────────────────────
enum FieldType { FIELD_INT, FIELD_FLOAT, FIELD_STRING };

// Abstract base – NO STL
struct Field {
    FieldType type;
    virtual ~Field() {}
    virtual Field*  clone()                    const = 0;
    virtual void    print()                    const = 0;
    virtual void    serialize(FILE* f)         const = 0;
    virtual void    deserialize(FILE* f)             = 0;
    // Comparisons
    virtual bool operator==(const Field& o)    const = 0;
    virtual bool operator< (const Field& o)    const = 0;
    virtual bool operator> (const Field& o)    const = 0;
    virtual bool operator<=(const Field& o)    const = 0;
    virtual bool operator>=(const Field& o)    const = 0;
    virtual bool operator!=(const Field& o)    const = 0;
};

// ── IntField ─────────────────────────────────────────────────────────────────
struct IntField : Field {
    int value;
    IntField(int v = 0)        { type = FIELD_INT; value = v; }
    Field* clone()             const override { return new IntField(value); }
    void   print()             const override { printf("%d", value); }
    void   serialize(FILE* f)  const override { fwrite(&value, sizeof(int), 1, f); }
    void   deserialize(FILE* f)      override { fread (&value, sizeof(int), 1, f); }
    bool operator==(const Field& o) const override { return value == static_cast<const IntField&>(o).value; }
    bool operator< (const Field& o) const override { return value <  static_cast<const IntField&>(o).value; }
    bool operator> (const Field& o) const override { return value >  static_cast<const IntField&>(o).value; }
    bool operator<=(const Field& o) const override { return value <= static_cast<const IntField&>(o).value; }
    bool operator>=(const Field& o) const override { return value >= static_cast<const IntField&>(o).value; }
    bool operator!=(const Field& o) const override { return value != static_cast<const IntField&>(o).value; }
};

// ── FloatField ────────────────────────────────────────────────────────────────
struct FloatField : Field {
    float value;
    FloatField(float v = 0.f)  { type = FIELD_FLOAT; value = v; }
    Field* clone()             const override { return new FloatField(value); }
    void   print()             const override { printf("%.2f", value); }
    void   serialize(FILE* f)  const override { fwrite(&value, sizeof(float), 1, f); }
    void   deserialize(FILE* f)      override { fread (&value, sizeof(float), 1, f); }
    bool operator==(const Field& o) const override { return value == static_cast<const FloatField&>(o).value; }
    bool operator< (const Field& o) const override { return value <  static_cast<const FloatField&>(o).value; }
    bool operator> (const Field& o) const override { return value >  static_cast<const FloatField&>(o).value; }
    bool operator<=(const Field& o) const override { return value <= static_cast<const FloatField&>(o).value; }
    bool operator>=(const Field& o) const override { return value >= static_cast<const FloatField&>(o).value; }
    bool operator!=(const Field& o) const override { return value != static_cast<const FloatField&>(o).value; }
};

// ── StringField ───────────────────────────────────────────────────────────────
struct StringField : Field {
    static const int MAX_LEN = 64;
    char value[MAX_LEN];
    StringField()              { type = FIELD_STRING; value[0] = '\0'; }
    StringField(const char* s) { type = FIELD_STRING; strncpy(value, s, MAX_LEN-1); value[MAX_LEN-1]='\0'; }
    Field* clone()             const override { return new StringField(value); }
    void   print()             const override { printf("%s", value); }
    void   serialize(FILE* f)  const override { fwrite(value, sizeof(char), MAX_LEN, f); }
    void   deserialize(FILE* f)      override { fread (value, sizeof(char), MAX_LEN, f); }
    bool operator==(const Field& o) const override { return strcmp(value, static_cast<const StringField&>(o).value)==0; }
    bool operator< (const Field& o) const override { return strcmp(value, static_cast<const StringField&>(o).value)<0;  }
    bool operator> (const Field& o) const override { return strcmp(value, static_cast<const StringField&>(o).value)>0;  }
    bool operator<=(const Field& o) const override { return strcmp(value, static_cast<const StringField&>(o).value)<=0; }
    bool operator>=(const Field& o) const override { return strcmp(value, static_cast<const StringField&>(o).value)>=0; }
    bool operator!=(const Field& o) const override { return strcmp(value, static_cast<const StringField&>(o).value)!=0; }
};

// ── Row ───────────────────────────────────────────────────────────────────────
struct Row {
    static const int MAX_FIELDS = 16;
    Field*  fields[MAX_FIELDS];
    int     fieldCount;
    bool    deleted;

    Row() : fieldCount(0), deleted(false) {
        for(int i=0;i<MAX_FIELDS;i++) fields[i]=nullptr;
    }
    ~Row() { for(int i=0;i<fieldCount;i++) delete fields[i]; }

    void addField(Field* f) {
        if(fieldCount < MAX_FIELDS) fields[fieldCount++] = f;
    }

    void print() const {
        for(int i=0;i<fieldCount;i++){
            if(i) printf("|");
            fields[i]->print();
        }
        printf("\n");
    }

    // Deep copy
    Row* clone() const {
        Row* r = new Row();
        r->deleted = deleted;
        for(int i=0;i<fieldCount;i++) r->addField(fields[i]->clone());
        return r;
    }
};

// ── Schema (column definitions) ───────────────────────────────────────────────
struct ColumnDef {
    char      name[64];
    FieldType type;
};

struct TableSchema {
    static const int MAX_COLS = 16;
    ColumnDef cols[MAX_COLS];
    int       colCount;
    TableSchema() : colCount(0) {}
    void addCol(const char* name, FieldType t){
        if(colCount<MAX_COLS){
            strncpy(cols[colCount].name,name,63);
            cols[colCount].type = t;
            colCount++;
        }
    }
    int getColIndex(const char* name) const {
        for(int i=0;i<colCount;i++)
            if(strcmp(cols[i].name,name)==0) return i;
        return -1;
    }
};
