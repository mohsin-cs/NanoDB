#pragma once
#include <cstdio>
#include <cstring>

// ── AVL Tree Node ─────────────────────────────────────────────────────────────
struct AVLNode {
    int      key;       // c_custkey or any int key
    int      pageId;    // where the row lives
    int      rowIdx;    // index within page
    int      height;
    AVLNode* left;
    AVLNode* right;

    AVLNode(int k, int pid, int ridx)
        : key(k), pageId(pid), rowIdx(ridx),
          height(1), left(nullptr), right(nullptr) {}
};

// ── AVL Tree ──────────────────────────────────────────────────────────────────
struct AVLTree {
    AVLNode* root;
    FILE*    logFile;
    int      nodeCount;

    AVLTree(FILE* lf) : root(nullptr), logFile(lf), nodeCount(0) {}

    ~AVLTree(){ destroyAll(root); }

    void destroyAll(AVLNode* n){
        if(!n) return;
        destroyAll(n->left);
        destroyAll(n->right);
        delete n;
    }

    // ── Height & Balance ───────────────────────────────────────────────────
    int height(AVLNode* n){ return n ? n->height : 0; }

    int balanceFactor(AVLNode* n){
        return n ? height(n->left)-height(n->right) : 0;
    }

    void updateHeight(AVLNode* n){
        if(!n) return;
        int lh=height(n->left), rh=height(n->right);
        n->height = 1 + (lh>rh ? lh : rh);
    }

    // ── Rotations ─────────────────────────────────────────────────────────
    AVLNode* rotateRight(AVLNode* y){
        AVLNode* x  = y->left;
        AVLNode* T2 = x->right;
        x->right = y;
        y->left  = T2;
        updateHeight(y);
        updateHeight(x);
        char msg[128];
        snprintf(msg,127,"[LOG] AVL: rotate-right at key=%d",y->key);
        fprintf(logFile,"%s\n",msg);
        return x;
    }

    AVLNode* rotateLeft(AVLNode* x){
        AVLNode* y  = x->right;
        AVLNode* T2 = y->left;
        y->left  = x;
        x->right = T2;
        updateHeight(x);
        updateHeight(y);
        char msg[128];
        snprintf(msg,127,"[LOG] AVL: rotate-left at key=%d",x->key);
        fprintf(logFile,"%s\n",msg);
        return y;
    }

    // ── Balance node ───────────────────────────────────────────────────────
    AVLNode* balance(AVLNode* n){
        updateHeight(n);
        int bf = balanceFactor(n);

        if(bf > 1){
            if(balanceFactor(n->left) < 0)
                n->left = rotateLeft(n->left);   // LR
            return rotateRight(n);               // LL
        }
        if(bf < -1){
            if(balanceFactor(n->right) > 0)
                n->right = rotateRight(n->right); // RL
            return rotateLeft(n);                 // RR
        }
        return n;
    }

    // ── Insert ─────────────────────────────────────────────────────────────
    AVLNode* insertNode(AVLNode* n, int key, int pageId, int rowIdx){
        if(!n){ nodeCount++; return new AVLNode(key,pageId,rowIdx); }
        if(key < n->key)      n->left  = insertNode(n->left, key,pageId,rowIdx);
        else if(key > n->key) n->right = insertNode(n->right,key,pageId,rowIdx);
        else { n->pageId=pageId; n->rowIdx=rowIdx; return n; } // update
        return balance(n);
    }

    void insert(int key, int pageId, int rowIdx){
        root = insertNode(root, key, pageId, rowIdx);
        char msg[128];
        snprintf(msg,127,"[LOG] AVL: inserted key=%d  tree_height=%d  nodes=%d",
            key,height(root),nodeCount);
        fprintf(logFile,"%s\n",msg);
    }

    // ── Search ─────────────────────────────────────────────────────────────
    AVLNode* search(int key){
        AVLNode* cur = root;
        int steps = 0;
        while(cur){
            steps++;
            if(key == cur->key){
                char msg[128];
                snprintf(msg,127,"[LOG] AVL: found key=%d in %d steps (O(log N))",key,steps);
                fprintf(logFile,"%s\n",msg);
                printf("%s\n",msg);
                return cur;
            }
            cur = (key < cur->key) ? cur->left : cur->right;
        }
        char msg[128];
        snprintf(msg,127,"[LOG] AVL: key=%d not found after %d steps",key,steps);
        fprintf(logFile,"%s\n",msg);
        printf("%s\n",msg);
        return nullptr;
    }

    // ── Min node (for delete) ──────────────────────────────────────────────
    AVLNode* minNode(AVLNode* n){
        while(n->left) n=n->left;
        return n;
    }

    AVLNode* deleteNode(AVLNode* n, int key){
        if(!n) return nullptr;
        if(key < n->key) n->left  = deleteNode(n->left, key);
        else if(key > n->key) n->right = deleteNode(n->right,key);
        else {
            if(!n->left || !n->right){
                AVLNode* tmp = n->left ? n->left : n->right;
                delete n; nodeCount--;
                return tmp;
            }
            AVLNode* mn = minNode(n->right);
            n->key    = mn->key;
            n->pageId = mn->pageId;
            n->rowIdx = mn->rowIdx;
            n->right  = deleteNode(n->right, mn->key);
        }
        return balance(n);
    }

    void remove(int key){ root = deleteNode(root, key); }

    // ── In-order print ─────────────────────────────────────────────────────
    void inorder(AVLNode* n){
        if(!n) return;
        inorder(n->left);
        printf("  key=%d page=%d row=%d\n",n->key,n->pageId,n->rowIdx);
        inorder(n->right);
    }
    void printAll(){ inorder(root); }
};
