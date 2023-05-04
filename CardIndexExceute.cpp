#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <chrono>
#include <map>
#include <random>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <vector>
#include <time.h>
#include <math.h>
using namespace std;
using namespace chrono;
#define M (100)
#define MAXL1CHILD (1000)
#define LIMIT_M_2 (M % 2 ? (M + 1) / 2 : M / 2)

int *tolbits = new int;
typedef struct BPlusNode *BPlusTree, *Position;
typedef bool *ZtupleBin;

ofstream ofs("./DCIresult.csv");

typedef struct ZTuple
{
    ZtupleBin bin;
    long long *values;
    long long z32;
} ZTuple;

typedef struct ZTab
{
    int r;
    int c;
    vector<ZTuple *> D;
} ZTab;
typedef ZTuple *KeyType;

struct BPlusNode
{
    int KeyNum;
    // bool tagX=0;//用于连接深度模型
    KeyType* Key;
    BPlusTree* Children;
    BPlusTree Next;
};

typedef struct Query
{
    int queryid;      // 唯一编码Queryid
    int columnNumber; // how many columns
    int *binaryLength;
    long long *leftupBound;
    long long *rightdownBound;
} Query;
typedef struct Querys
{
    Query *Qs;
    int queryNumber;
} Querys;
map<int, long> qid2TrueNumber;
typedef struct MADENet
{
    int zdr;
    int zdc;
    int connectlen;
    int leafnums = 100;
    int diglen;
    float *fc1w;
    float *fc1b;
    float *fc2w;
    float *fc2b;
} MADENet;
typedef struct MADE2BPlus
{
    BPlusTree transferLayer[MAXL1CHILD];
    bool Flag[MAXL1CHILD];
    int curnum = 200;
} MiddleLayer;
typedef struct CardIndex
{
    MADENet *Mnet;
    MiddleLayer *trans;
    BPlusNode *Head;
} CardIndex;
int getBelongNum(CardIndex *C, ZTuple *zti);

/* 初始化 */
extern BPlusTree Initialize();
/* 插入 */
extern BPlusTree Insert(BPlusTree T, KeyType Key);
/* 删除 */
// extern BPlusTree Remove(BPlusTree T,KeyType Key);
/* 销毁 */
extern BPlusTree Destroy(BPlusTree T);
/* 遍历节点 */
extern void Travel(BPlusTree T);
/* 遍历树叶节点的数据 */
extern void TravelData(BPlusTree T);
float cdfCalculate(MADENet *Mnet, ZTuple *ztup);

KeyType Unavailable = NULL;

// bool ZTLeq(ZTuple *v1, ZTuple *v2)
// {
//     if((unsigned)v1->z32 == (unsigned)v2->z32){
//         for(int i = 0 ;i< 50;i++){
//             if(v1->bin[i] == v2->bin[i]){
//                 continue;
//             }else{
//                 return v1->bin[i] < v2->bin[i];
//             }
//         }
//     }
//     return (unsigned)v1->z32 < (unsigned)v2->z32;
// }
default_random_engine e;
uniform_real_distribution<float> u(0.0, 1.0);
float midlle[30];
int infL = 30;
int randG(float oneProb)
{
    if (u(e) <= oneProb)
    {
        return 1;
    }
    return 0;
}
bool ZTcmp(ZTuple *v1, ZTuple *v2)
{
    return (unsigned)v1->z32 < (unsigned)v2->z32;
}
void longlong2digVec(long long valx, int *vx, int diglen)
{
    for (int i = 0; i < diglen; i++)
    {
        vx[diglen - i - 1] = (valx % 2);
        valx /= 2;
    }
    if (valx != 0)
    { // overflow
        // cout << "overflow" << endl;
        for (int i = 0; i < diglen; i++)
        {
            vx[diglen - i - 1] = 1;
        }
    }
}
ZTuple *makeZT(bool *zx, int binaryLen)
{
    ZTuple *ZT0 = new ZTuple;
    ZT0->bin = zx;
    int z32 = 0;
    for (int j = 0; j < min(binaryLen, 31); j++)
    {
        z32 *= 2;
        z32 += zx[j];
    }
    ZT0->z32 = z32;
    return ZT0;
}
ZTab *loadZD(string filePath)
{
    cout << "Start2Load " << endl;
    ZTab *ZT = new ZTab;
    ifstream infile(filePath);
    int r, c;
    int r2, c2;
    infile >> r >> c >> r2 >> c2;
    cout << "Rows:" << r << " Cols:" << c << endl;
    ZT->r = r;
    ZT->c = c;

    for (int i = 0; i < r; i++)
    {
        // cout<<"\r LOADING"<<i;
        ZtupleBin zx = new bool[c];
        // ZTuple *ZT0 = new ZTuple;
        for (int j = 0; j < c; j++)
        {
            bool v;
            infile >> v;
            zx[j] = v;
        }
        ZTuple *ZT0 = makeZT(zx, c);
        ZT0->values = new long long[c];
        for (int j = 0; j < c2; j++)
        {
            infile >> ZT0->values[j];
        }
        // for (int j = 0; j < c2; j++)
        // {
        //     cout<<ZT0->values[j]<<" ";
        // }
        // exit(1);
        // ZT0->bin = zx;
        // int z32 = 0;
        // for (int j = 0; j < min(c, 31); j++)
        // {
        //     z32 *= 2;
        //     z32 += zx[j];
        // }
        // ZT0->z32 = z32;
        ZT->D.push_back(ZT0);
    }
    // cout<<endl;
    infile.close();
    sort(ZT->D.begin(), ZT->D.end(), ZTcmp);
    return ZT;
}

/* 生成节点并初始化 */
static BPlusTree MallocNewNode()
{
    BPlusTree NewNode;
    int i;
    NewNode = (BPlusTree)malloc(sizeof(struct BPlusNode));
    if (NewNode == NULL)
        exit(EXIT_FAILURE);
    NewNode->Key = new KeyType[M+1];
    NewNode->Children = new BPlusTree[M+1];
    i = 0;
    while (i < M + 1)
    {
        NewNode->Key[i] = Unavailable;
        NewNode->Children[i] = NULL;
        i++;
    }
    NewNode->Next = NULL;
    NewNode->KeyNum = 0;

    return NewNode;
}

/* 初始化 */
extern BPlusTree Initialize()
{

    BPlusTree T;
    if (M < (3))
    {
        printf("M最小等于3！");
        exit(EXIT_FAILURE);
    }
    /* 根结点 */
    T = MallocNewNode();

    return T;
}

static Position FindMostLeft(Position P)
{
    Position Tmp;

    Tmp = P;

    while (Tmp != NULL && Tmp->Children[0] != NULL)
    {
        Tmp = Tmp->Children[0];
    }
    return Tmp;
}

static Position FindMostRight(Position P)
{
    Position Tmp;

    Tmp = P;

    while (Tmp != NULL && Tmp->Children[Tmp->KeyNum - 1] != NULL)
    {
        Tmp = Tmp->Children[Tmp->KeyNum - 1];
    }
    return Tmp;
}

/* 寻找一个兄弟节点，其存储的关键字未满，否则返回NULL */
static Position FindSibling(Position Parent, int i)
{
    Position Sibling;
    int Limit;

    Limit = M;

    Sibling = NULL;
    if (i == 0)
    {
        if (Parent->Children[1]->KeyNum < Limit)
            Sibling = Parent->Children[1];
    }
    else if (Parent->Children[i - 1]->KeyNum < Limit)
        Sibling = Parent->Children[i - 1];
    else if (i + 1 < Parent->KeyNum && Parent->Children[i + 1]->KeyNum < Limit)
    {
        Sibling = Parent->Children[i + 1];
    }

    return Sibling;
}

/* 查找兄弟节点，其关键字数大于M/2 ;没有返回NULL*/
static Position FindSiblingKeyNum_M_2(Position Parent, int i, int *j)
{
    int Limit;
    Position Sibling;
    Sibling = NULL;

    Limit = LIMIT_M_2;

    if (i == 0)
    {
        if (Parent->Children[1]->KeyNum > Limit)
        {
            Sibling = Parent->Children[1];
            *j = 1;
        }
    }
    else
    {
        if (Parent->Children[i - 1]->KeyNum > Limit)
        {
            Sibling = Parent->Children[i - 1];
            *j = i - 1;
        }
        else if (i + 1 < Parent->KeyNum && Parent->Children[i + 1]->KeyNum > Limit)
        {
            Sibling = Parent->Children[i + 1];
            *j = i + 1;
        }
    }
    return Sibling;
}

/* 当要对X插入Key的时候，i是X在Parent的位置，j是Key要插入的位置
   当要对Parent插入X节点的时候，i是要插入的位置，Key和j的值没有用
 */
static Position InsertElement(int isKey, Position Parent, Position X, KeyType Key, int i, int j)
{

    int k;
    if (isKey)
    {
        /* 插入key */
        k = X->KeyNum - 1;
        while (k >= j)
        {
            X->Key[k + 1] = X->Key[k];
            k--;
        }
        X->Key[j] = Key;
        if (Parent != NULL)
        {
            // cout << "PKa:" << Parent->KeyNum <<" i: "<<i<< endl;
            Parent->Key[i] = X->Key[0];
            // cout << "PKa:" << Parent->KeyNum << endl;
        }
        X->KeyNum++;
    }
    else
    {
        /* 插入节点 */

        /* 对树叶节点进行连接 */
        if (X->Children[0] == NULL)
        {
            if (i > 0)
                Parent->Children[i - 1]->Next = X;
            X->Next = Parent->Children[i];
        }

        k = Parent->KeyNum - 1;
        while (k >= i)
        {
            Parent->Children[k + 1] = Parent->Children[k];
            Parent->Key[k + 1] = Parent->Key[k];
            k--;
        }
        Parent->Key[i] = X->Key[0];
        Parent->Children[i] = X;

        Parent->KeyNum++;
    }
    return X;
}

static Position RemoveElement(int isKey, Position Parent, Position X, int i, int j)
{

    int k, Limit;

    if (isKey)
    {
        Limit = X->KeyNum;
        /* 删除key */
        k = j + 1;
        while (k < Limit)
        {
            X->Key[k - 1] = X->Key[k];
            k++;
        }

        X->Key[X->KeyNum - 1] = Unavailable;

        Parent->Key[i] = X->Key[0];

        X->KeyNum--;
    }
    else
    {
        /* 删除节点 */

        /* 修改树叶节点的链接 */
        if (X->Children[0] == NULL && i > 0)
        {
            Parent->Children[i - 1]->Next = Parent->Children[i + 1];
        }
        Limit = Parent->KeyNum;
        k = i + 1;
        while (k < Limit)
        {
            Parent->Children[k - 1] = Parent->Children[k];
            Parent->Key[k - 1] = Parent->Key[k];
            k++;
        }

        Parent->Children[Parent->KeyNum - 1] = NULL;
        Parent->Key[Parent->KeyNum - 1] = Unavailable;

        Parent->KeyNum--;
    }
    return X;
}

/* Src和Dst是两个相邻的节点，i是Src在Parent中的位置；
 将Src的元素移动到Dst中 ,n是移动元素的个数*/
static Position MoveElement(Position Src, Position Dst, Position Parent, int i, int n)
{
    KeyType TmpKey;
    Position Child;
    int j, SrcInFront;

    SrcInFront = 0;

    if ((unsigned)Src->Key[0]->z32 < (unsigned)Dst->Key[0]->z32)
    {
        SrcInFront = 1;
    }
    // cout<<"Infront:"<<SrcInFront<<endl;
    j = 0;
    /* 节点Src在Dst前面 */
    if (SrcInFront)
    {
        if (Src->Children[0] != NULL)
        {
            while (j < n)
            {
                Child = Src->Children[Src->KeyNum - 1];
                RemoveElement(0, Src, Child, Src->KeyNum - 1, 0);
                InsertElement(0, Dst, Child, Unavailable, 0, 0);
                j++;
            }
        }
        else
        {
            while (j < n)
            {
                TmpKey = Src->Key[Src->KeyNum - 1];
                RemoveElement(1, Parent, Src, i, Src->KeyNum - 1);
                InsertElement(1, Parent, Dst, TmpKey, i + 1, 0);
                j++;
            }
        }

        Parent->Key[i + 1] = Dst->Key[0];
        /* 将树叶节点重新连接 */
        if (Src->KeyNum > 0)
            FindMostRight(Src)->Next = FindMostLeft(Dst);
    }
    else
    {
        // cout << "PK:" << Parent->KeyNum << endl;
        if (Src->Children[0] != NULL)
        {
            while (j < n)
            {
                Child = Src->Children[0];
                RemoveElement(0, Src, Child, 0, 0);
                InsertElement(0, Dst, Child, Unavailable, Dst->KeyNum, 0);
                j++;
            }
        }
        else
        {
            while (j < n)
            {
                TmpKey = Src->Key[0];
                // cout << "Src:" << Src << " Dst: " << Dst << endl;

                // cout << "PK1:" << Parent->KeyNum << endl;
                RemoveElement(1, Parent, Src, i, 0);
                // cout << "PK2:" << Parent->KeyNum << endl;
                InsertElement(1, Parent, Dst, TmpKey, i - 1, Dst->KeyNum);
                // cout << "PK3:" << Parent->KeyNum << endl;
                j++;
            }
        }
        // cout << "PK:" << Parent->KeyNum << endl;
        Parent->Key[i] = Src->Key[0];
        if (Src->KeyNum > 0)
            FindMostRight(Dst)->Next = FindMostLeft(Src);
    }

    return Parent;
}

static BPlusTree SplitNode(Position Parent, Position X, int i)
{
    int j, k, Limit;
    Position NewNode;

    NewNode = MallocNewNode();

    k = 0;
    j = X->KeyNum / 2;
    Limit = X->KeyNum;
    // cout << "split detail:" << j << " " << Limit << " parent" << Parent << endl;
    while (j < Limit)
    {
        if (X->Children[0] != NULL)
        {
            NewNode->Children[k] = X->Children[j];
            X->Children[j] = NULL;
        }
        NewNode->Key[k] = X->Key[j];
        X->Key[j] = Unavailable;
        NewNode->KeyNum++;
        X->KeyNum--;
        j++;
        k++;
    }

    if (Parent != NULL)
        InsertElement(0, Parent, NewNode, Unavailable, i + 1, 0);
    else
    {
        /* 如果是X是根，那么创建新的根并返回 */
        Parent = MallocNewNode();
        InsertElement(0, Parent, X, Unavailable, 0, 0);
        InsertElement(0, Parent, NewNode, Unavailable, 1, 0);

        return Parent;
    }

    return X;
}

/* 合并节点,X少于M/2关键字，S有大于或等于M/2个关键字*/
static Position MergeNode(Position Parent, Position X, Position S, int i)
{
    int Limit;

    /* S的关键字数目大于M/2 */
    if (S->KeyNum > LIMIT_M_2)
    {
        /* 从S中移动一个元素到X中 */
        MoveElement(S, X, Parent, i, 1);
    }
    else
    {
        /* 将X全部元素移动到S中，并把X删除 */
        Limit = X->KeyNum;
        MoveElement(X, S, Parent, i, Limit);
        RemoveElement(0, Parent, X, i, 0);

        free(X);
        X = NULL;
    }

    return Parent;
}
/* (改动)当要对X插入Key的时候，i是X在Parent的位置，j是Key要插入的位置
   当要对Parent插入X节点的时候，i是要插入的位置，Key和j的值没有用
 */
static Position InsertBulk(int isKey, Position Parent, Position X, KeyType Key, int i, int j)
{

    int k;
    if (isKey)
    {
        /* 插入key */
        k = X->KeyNum - 1;
        while (k >= j)
        {
            X->Key[k + 1] = X->Key[k];
            k--;
        }
        X->Key[j] = Key;
        if (Parent != NULL)
        {
            // cout << "PKa:" << Parent->KeyNum <<" i: "<<i<< endl;
            Parent->Key[i] = X->Key[0];
            // cout << "PKa:" << Parent->KeyNum << endl;
        }
        X->KeyNum++;
    }
    else
    {
        /* 插入节点 */

        /* 对树叶节点进行连接 */
        if (X->Children[0] == NULL)
        {
            if (i > 0)
                Parent->Children[i - 1]->Next = X;
            X->Next = Parent->Children[i];
        }

        k = Parent->KeyNum - 1;
        while (k >= i)
        {
            Parent->Children[k + 1] = Parent->Children[k];
            Parent->Key[k + 1] = Parent->Key[k];
            k--;
        }
        Parent->Key[i] = X->Key[0];
        Parent->Children[i] = X;

        Parent->KeyNum++;
    }
    return X;
}

static BPlusTree RecursiveInsert(BPlusTree T, KeyType Key, int i, BPlusTree Parent)
{
    int j, Limit;
    Position Sibling;
    /* 查找分支 */
    j = 0;
    while (j < T->KeyNum && (unsigned)Key->z32 >= (unsigned)T->Key[j]->z32)
    {
        // if (Key->z32 == T->Key[j]->z32)
        //     return T;
        // cout<<(unsigned)Key->z32<<" "<<(unsigned)T->Key[j]->z32<<endl;
        j++;
        // if(T->Key[j]==NULL){
        //     break;
        // }
    }
    // cout << "Insert Middle" << endl;
    if (j != 0 && T->Children[0] != NULL)
    {
        j--;
    }
    /* 树叶 */
    if (T->Children[0] == NULL)
    {

        T = InsertElement(1, Parent, T, Key, i, j);
        // cout << T->KeyNum << endl;
    }
    /* 内部节点 */
    else
    {
        T->Children[j] = RecursiveInsert(T->Children[j], Key, j, T);
        // cout << T->KeyNum << endl;
    }
    // cout << T->KeyNum << endl;
    /* 调整节点 */

    Limit = M;

    if (T->KeyNum > Limit)
    {
        /* 根 */
        if (Parent == NULL)
        {
            /* 分裂节点 */
            // cout << "Spliting: " << T->KeyNum << endl;
            T = SplitNode(Parent, T, i);
        }
        else
        {
            // cout << "Trans2slib" << endl;
            Sibling = FindSibling(Parent, i);
            // cout << "slib " << Sibling << "Par" << Parent->KeyNum << endl;
            if (Sibling != NULL)
            {
                /* 将T的一个元素（Key或者Child）移动的Sibing中 */
                MoveElement(T, Sibling, Parent, i, 1);
            }
            else
            {
                /* 分裂节点 */
                T = SplitNode(Parent, T, i);
            }
        }
    }
    if (Parent != NULL)
    {
        Parent->Key[i] = T->Key[0];
        // cout << "Par " << Parent->KeyNum << endl;
    }

    // cout << "Insert Last:" << T->KeyNum << endl;
    return T;
}
/* 插入 */
extern BPlusTree Insert(BPlusTree T, KeyType Key)
{
    return RecursiveInsert(T, Key, 0, NULL);
}
BPlusTree bulkInit(vector<ZTuple *> &VZ)
{

    int outflow = 0;
    BPlusTree T = MallocNewNode();
    if (VZ.size() <= M)
    {
        for (int i = 0; i < VZ.size(); i++)
        {
            T->Key[i] = VZ[i];
        }
        T->KeyNum = VZ.size();
        T->Next = NULL;
        return T;
    }
    else
    {
        outflow = 1;
        // 构造链表
        BPlusNode *childHead = T;
        for (int i = 0; i < VZ.size(); i++)
        {
            if (T->KeyNum < M)
            {
                T->Key[T->KeyNum] = VZ[i];
                T->KeyNum++;
            }
            else
            {
                BPlusNode *nextNode = MallocNewNode();
                T->Next = nextNode;
                T = nextNode;
                T->Key[T->KeyNum] = VZ[i];
                T->KeyNum++;
            }
        }
        BPlusNode *fatherhead;
        while (outflow)
        {
            // 构造父亲们
            T = MallocNewNode();
            fatherhead = T;
            outflow = 0;
            int debugv1 = 0, debugv2 = 0;
            for (BPlusNode *cptr = childHead; cptr != NULL; cptr = cptr->Next)
            {
                debugv1++;
                if (T->KeyNum < M)
                {
                    T->Key[T->KeyNum] = cptr->Key[0];
                    T->Children[T->KeyNum] = cptr;
                    T->KeyNum++;
                }
                else
                {
                    outflow = 1;
                    BPlusNode *nextNode = MallocNewNode();
                    debugv2++;
                    T->Next = nextNode;
                    T = nextNode;
                    T->Key[T->KeyNum] = cptr->Key[0];
                    T->Children[T->KeyNum] = cptr;
                    T->KeyNum++;
                }
            }
            // cout << "ckpt arrived" <<" Num leafs:"<<debugv1<<" "<<" L3: "<<debugv2<< endl;
            // exit(1);
            // 更新变量维护
            childHead = fatherhead;
        }
        return fatherhead;
        // cout<<"I'm free!"<<endl;
        // cout<<fatherhead->KeyNum<<endl;
    }
    return NULL;
}
BPlusNode *fastBulkMerge(BPlusNode *curptr, vector<ZTuple *> &VZ)
{
    cout << "in FastBM" << endl;
    BPlusNode *oldhead = curptr;
    BPlusNode *newhead = MallocNewNode();
    BPlusNode *workingNode = newhead;
    int curidx = 0;
    int vzidx = 0;
    int skipNum=0;
    while (curptr != NULL && vzidx < VZ.size())
    {

        KeyType vzkey = VZ[vzidx];
        KeyType oldBTKey = curptr->Key[curidx];

        // if (vzidx >= 615000)
        // {
        //     cout  << vzidx << " " << curidx << " KN<" << curptr->KeyNum<<"> "<<endl;
        //     cout<< ((unsigned)vzkey->z32 )<<" "<<oldBTKey<<" "<<((unsigned)oldBTKey->z32)<<endl;
        // }
        if(curidx == 0 && ((unsigned)vzkey->z32 > (unsigned)curptr->Key[curptr->KeyNum-1]->z32 ) ){
            //skip 
            skipNum++;
            if(workingNode->KeyNum ==0){
                workingNode->KeyNum = curptr->KeyNum;
                workingNode->Key= curptr->Key;
                workingNode->Children = curptr->Children;
                workingNode->Next = MallocNewNode();
                workingNode = workingNode->Next;
                curptr = curptr->Next;

            }
            else{
                workingNode->Next = MallocNewNode();
                workingNode = workingNode->Next;
                workingNode->KeyNum = curptr->KeyNum;
                workingNode->Key= curptr->Key;
                workingNode->Children = curptr->Children;
                workingNode->Next = MallocNewNode();
                workingNode = workingNode->Next;
            }
            continue;
        }

        if ((unsigned)vzkey->z32 < (unsigned)oldBTKey->z32)
        {

            if (workingNode->KeyNum < M)
            {
                workingNode->Key[workingNode->KeyNum] = vzkey;
                workingNode->KeyNum++;
            }
            else
            {
                workingNode->Next = MallocNewNode();
                workingNode = workingNode->Next;
                workingNode->Key[workingNode->KeyNum] = vzkey;
                workingNode->KeyNum++;
            }
            vzidx += 1;
        }
        else
        {
            if (workingNode->KeyNum < M)
            {
                workingNode->Key[workingNode->KeyNum] = oldBTKey;
                workingNode->KeyNum++;
            }
            else
            {
                workingNode->Next = MallocNewNode();
                workingNode = workingNode->Next;
                workingNode->Key[workingNode->KeyNum] = oldBTKey;
                workingNode->KeyNum++;
            }
            curidx++;
            if (curidx >= curptr->KeyNum)
            {
                // cout << '\r' << vzidx << " " << curidx << " " << curptr->KeyNum<<" "<<curptr->Next;
                // if ( curptr->Next!=NULL){
                //     cout<< " KN:"<<curptr->Next->KeyNum<<"<<              ";
                // }
                curidx = 0;
                curptr = curptr->Next;
            }
        }
        // if (vzidx >= 615000)
        // {
        //     cout <<  vzidx << " " << curidx << " CP<" << curptr<<"> "<<endl;
        // }
    }
    // cout << "half way" << endl;
    cout<<"Skiped Number:"<<skipNum<<endl;
    if ((vzidx < VZ.size()))
    {
        cout << "bRANCH1" << endl;
    }
    else
    {
        cout << "bRANCH2" << endl;
    }
    while (vzidx < VZ.size())
    {
        KeyType vzkey = VZ[vzidx];
        if (workingNode->KeyNum < M)
        {
            workingNode->Key[workingNode->KeyNum] = vzkey;
            workingNode->KeyNum++;
        }
        else
        {
            workingNode->Next = MallocNewNode();
            workingNode = workingNode->Next;
            workingNode->Key[workingNode->KeyNum] = vzkey;
            workingNode->KeyNum++;
        }
        vzidx += 1;
    }
    while (curptr != NULL)
    {
        KeyType oldBTKey = curptr->Key[curidx];
        if (oldBTKey == NULL)
        {
            break;
        }
        if (workingNode->KeyNum < M)
        {
            workingNode->Key[workingNode->KeyNum] = oldBTKey;
            workingNode->KeyNum++;
        }
        else
        {
            workingNode->Next = MallocNewNode();
            workingNode = workingNode->Next;
            workingNode->Key[workingNode->KeyNum] = oldBTKey;
            workingNode->KeyNum++;
        }

        if (curidx < curptr->KeyNum)
        {
            curidx++;
        }
        else
        {
            curidx = 0;
            curptr = curptr->Next;
        }
    }
    workingNode = oldhead;
    // cout << "deleting" << endl;
    // while (workingNode != NULL)
    // {
    //     BPlusNode *pdelete;
    //     workingNode = workingNode->Next;
    //     delete pdelete;
    //     /* code */
    // }
    return newhead;
}

BPlusNode *bulkMerge(BPlusNode *curptr, vector<ZTuple *> &VZ)
{
    cout << "in BM" << endl;
    BPlusNode *oldhead = curptr;
    BPlusNode *newhead = MallocNewNode();
    BPlusNode *workingNode = newhead;
    int curidx = 0;
    int vzidx = 0;
    while (curptr != NULL && vzidx < VZ.size())
    {

        KeyType vzkey = VZ[vzidx];
        KeyType oldBTKey = curptr->Key[curidx];
        // if (vzidx >= 615000)
        // {
        //     cout  << vzidx << " " << curidx << " KN<" << curptr->KeyNum<<"> "<<endl;
        //     cout<< ((unsigned)vzkey->z32 )<<" "<<oldBTKey<<" "<<((unsigned)oldBTKey->z32)<<endl;
        // }
        if ((unsigned)vzkey->z32 < (unsigned)oldBTKey->z32)
        {

            if (workingNode->KeyNum < M)
            {
                workingNode->Key[workingNode->KeyNum] = vzkey;
                workingNode->KeyNum++;
            }
            else
            {
                workingNode->Next = MallocNewNode();
                workingNode = workingNode->Next;
                workingNode->Key[workingNode->KeyNum] = vzkey;
                workingNode->KeyNum++;
            }
            vzidx += 1;
        }
        else
        {
            if (workingNode->KeyNum < M)
            {
                workingNode->Key[workingNode->KeyNum] = oldBTKey;
                workingNode->KeyNum++;
            }
            else
            {
                workingNode->Next = MallocNewNode();
                workingNode = workingNode->Next;
                workingNode->Key[workingNode->KeyNum] = oldBTKey;
                workingNode->KeyNum++;
            }
            curidx++;
            if (curidx >= curptr->KeyNum)
            {
                // cout << '\r' << vzidx << " " << curidx << " " << curptr->KeyNum<<" "<<curptr->Next;
                // if ( curptr->Next!=NULL){
                //     cout<< " KN:"<<curptr->Next->KeyNum<<"<<              ";
                // }
                curidx = 0;
                curptr = curptr->Next;
            }
        }
        // if (vzidx >= 615000)
        // {
        //     cout <<  vzidx << " " << curidx << " CP<" << curptr<<"> "<<endl;
        // }
    }
    cout << "half way" << endl;
    if ((vzidx < VZ.size()))
    {
        cout << "bRANCH1" << endl;
    }
    else
    {
        cout << "bRANCH2" << endl;
    }
    while (vzidx < VZ.size())
    {
        KeyType vzkey = VZ[vzidx];
        if (workingNode->KeyNum < M)
        {
            workingNode->Key[workingNode->KeyNum] = vzkey;
            workingNode->KeyNum++;
        }
        else
        {
            workingNode->Next = MallocNewNode();
            workingNode = workingNode->Next;
            workingNode->Key[workingNode->KeyNum] = vzkey;
            workingNode->KeyNum++;
        }
        vzidx += 1;
    }
    while (curptr != NULL)
    {
        KeyType oldBTKey = curptr->Key[curidx];
        if (oldBTKey == NULL)
        {
            break;
        }
        if (workingNode->KeyNum < M)
        {
            workingNode->Key[workingNode->KeyNum] = oldBTKey;
            workingNode->KeyNum++;
        }
        else
        {
            workingNode->Next = MallocNewNode();
            workingNode = workingNode->Next;
            workingNode->Key[workingNode->KeyNum] = oldBTKey;
            workingNode->KeyNum++;
        }

        if (curidx < curptr->KeyNum)
        {
            curidx++;
        }
        else
        {
            curidx = 0;
            curptr = curptr->Next;
        }
    }
    workingNode = oldhead;
    // cout << "deleting" << endl;
    // while (workingNode != NULL)
    // {
    //     BPlusNode *pdelete;
    //     workingNode = workingNode->Next;
    //     delete pdelete;
    //     /* code */
    // }
    return newhead;
}
// CardIndex *C;
CardIndex *LinkedList2CardIndex(BPlusNode *Head, MADENet *Net)
{
    CardIndex *C = new CardIndex;
    C->Head = Head;
    BPlusNode *fatherhead;
    BPlusNode *childHead = Head;
    C->Mnet = Net;
    int curLinkTopLen = 0;
    while (true) // 如果不满足条件，索引增长树深度
    {
        curLinkTopLen = 0;
        BPlusNode *curptr = childHead;
        while (curptr != NULL)
        {
            curLinkTopLen += 1;
            curptr = curptr->Next;
        }
        cout << "curlen: " << curLinkTopLen << endl;
        if (curLinkTopLen < MAXL1CHILD)
        {
            break;
        }
        // 对树深度进行增长

        // 构造父亲们
        BPlusNode *T = MallocNewNode();
        fatherhead = T;
        for (BPlusNode *cptr = childHead; cptr != NULL; cptr = cptr->Next)
        {
            if (T->KeyNum < M)
            {
                T->Key[T->KeyNum] = cptr->Key[0];
                T->Children[T->KeyNum] = cptr;
                T->KeyNum++;
            }
            else
            {
                BPlusNode *nextNode = MallocNewNode();
                T->Next = nextNode;
                T = nextNode;
                T->Key[T->KeyNum] = cptr->Key[0];
                T->Children[T->KeyNum] = cptr;
                T->KeyNum++;
            }
        }
        // 更新变量维护
        childHead = fatherhead;
    }
    int i = -1;
    C->trans = new MiddleLayer;
    for (int ix = 0; ix < MAXL1CHILD; ix++)
    {
        C->trans->transferLayer[ix] = NULL;
        C->trans->Flag[ix] = 0;
    }
    // cout<<"starting 2 make somethnig new"<<endl;
    for (auto curptr = childHead; curptr != NULL; curptr = curptr->Next)
    {
        i++;
        float cdf0 = cdfCalculate(C->Mnet, curptr->Key[0]);
        int belong = int(cdf0 * MAXL1CHILD);
        if (belong < 0)
        {
            belong = 0;
        }
        if (belong >= MAXL1CHILD)
        {
            belong = MAXL1CHILD - 1;
        }
        if (C->trans->transferLayer[belong] == NULL)
        {
            C->trans->transferLayer[belong] = curptr;
        }
        else
        {
            BPlusNode *BPN = C->trans->transferLayer[belong];
            if (BPN->Next == (BPlusNode *)-1)
            { // 第2次以上的哈希冲突,extend,暂时不考虑满了的情况
                BPN->Key[BPN->KeyNum] = curptr->Key[0];
                BPN->Children[BPN->KeyNum] = curptr;
                BPN->KeyNum++;
            }
            else
            { // 第一次哈希冲突，将树的高度增大一层
                // cout<<"First Hash conflit"<<endl;
                BPlusNode *ReplaceN = MallocNewNode();
                ReplaceN->Next = (BPlusNode *)-1;
                ReplaceN->Key[0] = BPN->Key[0];
                ReplaceN->Children[0] = BPN;
                ReplaceN->Key[1] = curptr->Key[0];
                ReplaceN->Children[1] = curptr;
                ReplaceN->KeyNum = 2;
                C->trans->transferLayer[belong] = ReplaceN;
            }
        }
        // cout<<i<<"Bulk CDF: "<<cdf<<" IF CDF HASH:" << cdf * curLinkTopLen <<endl;
    }
    BPlusNode *firstNZ = NULL;
    for (int ix = 0; ix < MAXL1CHILD; ix++)
    {
        if (C->trans->transferLayer[ix] == NULL)
        {
            C->trans->transferLayer[ix] = firstNZ;
        }
        else
        {
            if (C->trans->transferLayer[ix] != NULL)
            {
                // 上一个相邻的会存在split的,因此添加标记
                C->trans->Flag[ix] = 1;
                firstNZ = C->trans->transferLayer[ix];
            }
        }
    }
    C->trans->curnum = MAXL1CHILD;
    return C;
}
BPlusNode *MergeLinkedList(BPlusNode *L1, ZTab *ZT)
{
    vector<ZTuple *> VZ = ZT->D;
    // sort(VZ.begin(), VZ.end(), ZTcmp);
    if (L1 == NULL)
    {
        BPlusNode *head;
        cout << "Cold Start" << endl;
        BPlusNode *T = MallocNewNode();
        head = T;
        for (int i = 0; i < VZ.size(); i++)
        {
            if (T->KeyNum < M)
            {
                T->Key[T->KeyNum] = VZ[i];
                T->KeyNum++;
            }
            else
            {
                BPlusNode *nextNode = MallocNewNode();
                T->Next = nextNode;
                T = nextNode;
                T->Key[T->KeyNum] = VZ[i];
                T->KeyNum++;
            }
        }
        return head;
    }
    else
    {
        BPlusNode *newLinklist = bulkMerge(L1, VZ);
        return newLinklist;
    }
}
BPlusTree bulkInsert(BPlusTree T, ZTab *ZT)
{
    // cout<<"Hi"<<endl;
    vector<ZTuple *> VZ = ZT->D;
    sort(VZ.begin(), VZ.end(), ZTcmp);
    if (T == NULL)
    {
        cout << "Cold Start" << endl;
        T = bulkInit(VZ);
        return T;
        /* code */
    }
    else
    {
        BPlusNode *curptr = T;
        while (curptr->Children[0] != NULL)
        {
            curptr = curptr->Children[0];
        }
        // MERGE
        typedef std::chrono::high_resolution_clock Clock;
        auto t3 = Clock::now(); // 计时开始
        BPlusNode *newLinklist = bulkMerge(curptr, VZ);

        auto t4 = Clock::now(); // 计时开始
        cout << "Merge Time:" << (std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count()) << " In average(ns/t):" << (std::chrono::duration_cast<std::chrono::nanoseconds>(t4 - t3).count()) / (ZT->r + 0.0) << endl;

        cout << "Bulk merged" << endl;
        BPlusNode *childHead = newLinklist;
        BPlusNode *fatherhead;
        int outflow = 1;
        while (outflow)
        {
            // 构造父亲们
            T = MallocNewNode();
            fatherhead = T;
            outflow = 0;
            int debugv1 = 0, debugv2 = 0;
            for (BPlusNode *cptr = childHead; cptr != NULL; cptr = cptr->Next)
            {
                debugv1++;
                if (T->KeyNum < M)
                {
                    T->Key[T->KeyNum] = cptr->Key[0];
                    T->Children[T->KeyNum] = cptr;
                    T->KeyNum++;
                }
                else
                {
                    outflow = 1;
                    BPlusNode *nextNode = MallocNewNode();
                    debugv2++;
                    T->Next = nextNode;
                    T = nextNode;
                    T->Key[T->KeyNum] = cptr->Key[0];
                    T->Children[T->KeyNum] = cptr;
                    T->KeyNum++;
                }
            }

            // cout << "ckpt arrived" <<" Num leafs:"<<debugv1<<" "<<" L3: "<<debugv2<< endl;
            // exit(1);
            // 更新变量维护
            childHead = fatherhead;
        }
        // auto tx = Clock::now();
        // int inc=0;
        // for (auto ptrx = fatherhead->Children[0]; ptrx != NULL; ptrx = ptrx->Next)
        // {
        //     inc++;
        //     int belong = getBelongNum(C, ptrx->Key[0]);
        // }
        // cout<<inc<<endl;
        // auto t6 = Clock::now(); // 计时开始
        // cout << "CDF Time:" << (std::chrono::duration_cast<std::chrono::nanoseconds>(tx - t6).count()) << " In average(ns/t):" << (std::chrono::duration_cast<std::chrono::nanoseconds>(tx - t6).count()) / (ZT->r + 0.0) << endl;
        auto t5 = Clock::now(); // 计时开始
        cout << "BT MAKE Time:" << (std::chrono::duration_cast<std::chrono::milliseconds>(t5 - t4).count()) << " In average(ns/t):" << (std::chrono::duration_cast<std::chrono::nanoseconds>(t5 - t4).count()) / (ZT->r + 0.0) << endl;
        return fatherhead;
    }
    return NULL;
}
static BPlusTree RecursiveRemove(BPlusTree T, KeyType Key, int i, BPlusTree Parent)
{
    cout << "shit in" << endl;
    exit(1);
    int j, NeedAdjust;
    Position Sibling, Tmp;

    Sibling = NULL;

    /* 查找分支 */
    j = 0;
    while (j < T->KeyNum && (unsigned)Key->z32 >= (unsigned)T->Key[j]->z32)
    {
        if (Key == T->Key[j])
            break;
        j++;
        if (T->Key[j] == NULL)
        {
            break;
        }
    }

    if (T->Children[0] == NULL)
    {
        /* 没找到 */
        if (Key->z32 != T->Key[j]->z32 || j == T->KeyNum)
            return T;
    }
    else if (j == T->KeyNum || Key->z32 < T->Key[j]->z32)
        j--;

    /* 树叶 */
    if (T->Children[0] == NULL)
    {
        T = RemoveElement(1, Parent, T, i, j);
    }
    else
    {
        T->Children[j] = RecursiveRemove(T->Children[j], Key, j, T);
    }

    NeedAdjust = 0;
    /* 树的根或者是一片树叶，或者其儿子数在2到M之间 */
    if (Parent == NULL && T->Children[0] != NULL && T->KeyNum < 2)
        NeedAdjust = 1;
    /* 除根外，所有非树叶节点的儿子数在[M/2]到M之间。(符号[]表示向上取整) */
    else if (Parent != NULL && T->Children[0] != NULL && T->KeyNum < LIMIT_M_2)
        NeedAdjust = 1;
    /* （非根）树叶中关键字的个数也在[M/2]和M之间 */
    else if (Parent != NULL && T->Children[0] == NULL && T->KeyNum < LIMIT_M_2)
        NeedAdjust = 1;

    /* 调整节点 */
    if (NeedAdjust)
    {
        /* 根 */
        if (Parent == NULL)
        {
            if (T->Children[0] != NULL && T->KeyNum < 2)
            {
                Tmp = T;
                T = T->Children[0];
                free(Tmp);
                return T;
            }
        }
        else
        {
            /* 查找兄弟节点，其关键字数目大于M/2 */
            Sibling = FindSiblingKeyNum_M_2(Parent, i, &j);
            if (Sibling != NULL)
            {
                MoveElement(Sibling, T, Parent, j, 1);
            }
            else
            {
                if (i == 0)
                    Sibling = Parent->Children[1];
                else
                    Sibling = Parent->Children[i - 1];

                Parent = MergeNode(Parent, T, Sibling, i);
                T = Parent->Children[i];
            }
        }
    }

    return T;
}

// /* 删除 */
// extern BPlusTree Remove(BPlusTree T,KeyType Key){
//     return RecursiveRemove(T, Key, 0, NULL);
// }

/* 销毁 */
extern BPlusTree Destroy(BPlusTree T)
{
    int i, j;
    if (T != NULL)
    {
        i = 0;
        while (i < T->KeyNum + 1)
        {
            Destroy(T->Children[i]);
            i++;
        }

        // printf("Destroy:(");
        j = 0;
        // while (j < T->KeyNum) /*  T->Key[i] != Unavailable*/
        //     printf("%d:", T->Key[j++]);
        // printf(") ");
        free(T);
        T = NULL;
    }

    return T;
}

int recnum = 0;
Position LastNode = NULL;
static void RecursiveTravel(BPlusTree T, int Level)
{
    int i;
    if (T != NULL)
    {
        // printf("  ");
        // printf("[Level:%d]-->", Level);
        // printf("(");
        i = 0;
        // while (i < T->KeyNum) /*  T->Key[i] != Unavailable*/
        //     printf("%d:", T->Key[i++]->z32);
        // printf(")");
        // return;
        Level++;
        if (T->Children[0] == NULL)
        {
            if (LastNode == NULL)
            {
                LastNode = T;
            }
            else
            {
                LastNode->Next = T;
                LastNode = T;
            }
            recnum += T->KeyNum;
        }
        i = 0;
        while (i <= T->KeyNum)
        {
            RecursiveTravel(T->Children[i], Level);
            i++;
        }
    }
}

// int TRAN;
/* 遍历修复指针顺序 */
extern void Travel(BPlusTree T)
{
    recnum = 0;
    RecursiveTravel(T, 0);
    LastNode->Next = NULL;
    // cout<<"TOLN:"<<recnum<<endl;
    // printf("\n");
}

/* 遍历树叶节点的数据 */
extern void TravelData(BPlusTree T)
{
    Position Tmp;
    int i;
    if (T == NULL)
        return;
    printf("All Data:");
    Tmp = T;
    while (Tmp->Children[0] != NULL)
        Tmp = Tmp->Children[0];
    /* 第一片树叶 */
    while (Tmp != NULL)
    {
        i = 0;
        while (i < Tmp->KeyNum)
            printf(" %d", Tmp->Key[i++]);
        Tmp = Tmp->Next;
    }
}
MADENet *loadMade(string filePath)
{
    ifstream infile(filePath);
    if (!infile.is_open())
    {
        cout << "Fail to Net load Tree" << endl;
        return NULL;
    }
    int bittol;
    // infile >> bittol;
    // cout<<"bt:"<<bittol<<endl;
    MADENet *ret = new MADENet;
    infile >> ret->zdr >> ret->zdc >> ret->connectlen >> ret->leafnums;
    bittol = ret->zdc;
    ret->diglen = bittol;
    ret->fc1w = new float[bittol * bittol];
    ret->fc2w = new float[bittol * bittol];
    ret->fc1b = new float[bittol];
    ret->fc2b = new float[bittol];
    for (int i = 0; i < bittol; i++)
    {
        for (int j = 0; j < bittol; j++)
        {
            infile >> ret->fc1w[i * bittol + j];
        }
    }
    for (int i = 0; i < bittol; i++)
    {
        infile >> ret->fc1b[i];
    }
    for (int i = 0; i < bittol; i++)
    {
        for (int j = 0; j < bittol; j++)
        {
            infile >> ret->fc2w[i * bittol + j];
        }
    }
    for (int i = 0; i < bittol; i++)
    {
        infile >> ret->fc2b[i];
    }
    int strcord = 0;
    infile.close();
    return ret;
}
void MadeIndexInfer(bool *xinput, float *out, int preLen, MADENet *net, float *middle)
{
    int winlen = net->connectlen;
    for (int i = 0; i < preLen; i++)
    {
        middle[i] = net->fc1b[i];
        // cout<<middle[i]<<endl;
        for (int j = max(i - winlen, 0); j < i; j++)
        {
            // cout<<"j"<<j<<endl;
            if (j >= i)
            {
                break;
            }
            // cout<<"xij: "<<xinput[j]<<endl;
            middle[i] += (xinput[j] * net->fc1w[i * net->diglen + j]);
        }
        if (middle[i] < 0)
        {
            middle[i] = 0;
        }
    }
    for (int i = 0; i < preLen; i++)
    {
        out[i] = net->fc2b[i];
        for (int j = max(i - winlen, 0); j < i; j++)
        {
            if (j >= i)
            {
                break;
            }
            out[i] += (middle[j] * net->fc2w[i * net->diglen + j]);
        }
        out[i] = (1.0) / (1.0 + exp(-out[i]));
    }
}
float cdfCalculate(MADENet *Mnet, ZTuple *ztup)
{
    float out[100];
    float mid[100];
    MadeIndexInfer(ztup->bin, out, 32, Mnet, mid);
    float cdf = 0;
    float acc = 1.0;
    for (int j = 0; j < 32; j++)
    {
        float onep = out[j];
        cdf += (acc * (1 - onep) * ztup->bin[j]);
        acc *= (onep * ztup->bin[j] + (1 - onep) * (1 - ztup->bin[j]));
    }
    return cdf;
}

CardIndex *InitCardIndex(ZTab *ZT, int L2num)
{
    Unavailable = new ZTuple;
    Unavailable->z32 = INT_MIN;
    CardIndex *C = new CardIndex;
    MADENet *Mnet = loadMade("./Model/MadeRootP0");
    C->Mnet = Mnet;
    string firstFilePath = "./data/ZD0.txt";
    MiddleLayer *MidL = new MiddleLayer;
    C->trans = MidL;
    C->trans->curnum = L2num;
    for (int i = 0; i < C->trans->curnum; i++)
    {
        C->trans->transferLayer[i] = Initialize();
    }
    return C;
    vector<ZTuple *> VZ = ZT->D;
    sort(VZ.begin(), VZ.end(), ZTcmp);
    int u;
    int batchN = 30;
    for (int i = 0; i < (ZT->r) / batchN; i++)
    {
        int ub = min((i + 1) * batchN, ZT->r);
        int lb = i * batchN;
        float cdfl = cdfCalculate(C->Mnet, VZ[lb]);
        float cdfu = cdfCalculate(C->Mnet, VZ[ub]);
        int belongl = cdfl / (1.0 / C->trans->curnum);
        int belongu = cdfu / (1.0 / C->trans->curnum);
        // cout<<belongl<<belongu<<endl;
        if (belongl == belongu)
        {
            for (int j = lb; j < ub; j++)
            {
                C->trans->transferLayer[belongl] = Insert(C->trans->transferLayer[belongl], VZ[j]);
                u = j;
            }
        }
        else
        {
            for (int j = lb; j < ub; j++)
            {
                float cdf0 = cdfCalculate(C->Mnet, VZ[j]);
                int belong = cdf0 / (1.0 / C->trans->curnum);
                int flag = 0;
                C->trans->transferLayer[belong] = Insert(C->trans->transferLayer[belong], VZ[j]);
                u = j;
            }
        }
        // float cdf0 = cdfCalculate(C->Mnet, VZ[i]);
        // int belong = cdf0 / (1.0 / C->trans->curnum);
        // int flag = 0;
        // C->trans->transferLayer[belong] = Insert(C->trans->transferLayer[belong],ZT->D[i]);
    }
    u++;
    while (u < ZT->r)
    {
        float cdf0 = cdfCalculate(C->Mnet, VZ[u]);
        int belong = cdf0 / (1.0 / C->trans->curnum);
        int flag = 0;
        C->trans->transferLayer[belong] = Insert(C->trans->transferLayer[belong], VZ[u]);
        u++;
    }
    cout << u << endl;
    LastNode = NULL;
    for (int i = 0; i < C->trans->curnum; i++)
    {
        BPlusTree tmp = C->trans->transferLayer[i];
        Travel(tmp);
        LastNode = NULL;
    }
    return C;
}

void StaticInsert(CardIndex *C, ZTab *ZT)
{
    vector<ZTuple *> VZ = ZT->D;
    sort(VZ.begin(), VZ.end(), ZTcmp);
    int u;
    int batchN = 30;
    for (int i = 0; i < (ZT->r) / batchN; i++)
    {
        int ub = min((i + 1) * batchN, ZT->r);
        int lb = i * batchN;
        float cdfl = cdfCalculate(C->Mnet, VZ[lb]);
        float cdfu = cdfCalculate(C->Mnet, VZ[ub]);
        int belongl = cdfl / (1.0 / C->trans->curnum);
        int belongu = cdfu / (1.0 / C->trans->curnum);
        // cout<<belongl<<belongu<<endl;
        if (belongl == belongu)
        {
            for (int j = lb; j < ub; j++)
            {
                C->trans->transferLayer[belongl] = Insert(C->trans->transferLayer[belongl], VZ[j]);
                u = j;
            }
        }
        else
        {
            for (int j = lb; j < ub; j++)
            {
                float cdf0 = cdfCalculate(C->Mnet, VZ[j]);
                int belong = cdf0 / (1.0 / C->trans->curnum);
                // int flag = 0;
                C->trans->transferLayer[belong] = Insert(C->trans->transferLayer[belong], VZ[j]);
                u = j;
            }
        }
        // float cdf0 = cdfCalculate(C->Mnet, VZ[i]);
        // int belong = cdf0 / (1.0 / C->trans->curnum);
        // int flag = 0;
        // C->trans->transferLayer[belong] = Insert(C->trans->transferLayer[belong],ZT->D[i]);
    }
    u++;

    while (u < ZT->r)
    {
        float cdf0 = cdfCalculate(C->Mnet, VZ[u]);
        int belong = cdf0 / (1.0 / C->trans->curnum);
        // int flag = 0;
        C->trans->transferLayer[belong] = Insert(C->trans->transferLayer[belong], VZ[u]);
        u++;
    }
    // LastNode = NULL;
    // for (int i = 0; i < C->trans->curnum; i++)
    // {
    //     BPlusTree tmp = C->trans->transferLayer[i];
    //     Travel(tmp);
    //     LastNode = NULL;
    // }
}
void CardIndexReport(CardIndex *C)
{
    int cap = 0;
    cout << "\n-----------------\n";
    for (int i = 0; i < C->trans->curnum; i++)
    {
        BPlusTree Ptemp = C->trans->transferLayer[i];
        Position Tmp;
        // cout << "Ci: " << i << " " << Ptemp->KeyNum << endl;
        Tmp = Ptemp;
        while (Tmp->Children[0] != NULL)
            Tmp = Tmp->Children[0];
        int tolL = 0;
        /* 逐一Insert */
        // cout<<"hell"<<endl;
        while (Tmp != NULL)
        {
            tolL += Tmp->KeyNum;
            Tmp = Tmp->Next;
        }
        // cout << i << " " << tolL << endl;
        cap += tolL;
    }
    cout << "Toltal records: " << cap << endl;
}

void reBalance(CardIndex *C, string MadeFilePath)
{
    // for (int i = 0; i < C->trans->curnum; i++)
    // {
    //     cout<<C->trans->transferLayer[i]->KeyNum<<" ";
    // }
    // CardIndexReport(C);

    LastNode = NULL;
    for (int i = 0; i < C->trans->curnum; i++)
    {
        BPlusTree tmp = C->trans->transferLayer[i];
        Travel(tmp);
        LastNode = NULL;
    }

    // CardIndexReport(C);
    C->Mnet = loadMade(MadeFilePath);
    int modifiedTrees = 0;
    for (int i = 0; i < C->trans->curnum; i++)
    {
        // cout<<i<<endl;
        if ((C->trans->transferLayer[i]->KeyNum) == 0)
        {
            continue;
        }
        ZTuple *l = C->trans->transferLayer[i]->Key[0];
        ZTuple *u = C->trans->transferLayer[i]->Key[(C->trans->transferLayer[i]->KeyNum) - 1];
        float cdfl = cdfCalculate(C->Mnet, l);
        float cdfu = cdfCalculate(C->Mnet, u);
        int belongl = cdfl / (1.0 / C->trans->curnum);
        int belongu = cdfu / (1.0 / C->trans->curnum);
        // cout << "SubT:" << i << " " << belongl << " " << belongu << endl;
        if (i != belongl || i != belongu)
        {
            // cout << "Imbalance " << i << endl;
            modifiedTrees++;
            BPlusTree Ptemp = C->trans->transferLayer[i];
            C->trans->transferLayer[i] = Initialize();
            Position Tmp;
            Tmp = Ptemp;
            while (Tmp->Children[0] != NULL)
                Tmp = Tmp->Children[0];
            /* 逐一Insert */
            while (Tmp != NULL)
            {
                float cdfcl = cdfCalculate(C->Mnet, Tmp->Key[0]);
                float cdfcu = cdfCalculate(C->Mnet, Tmp->Key[Tmp->KeyNum - 1]);
                int belongcl = cdfcl / (1.0 / C->trans->curnum);
                int belongcu = cdfcu / (1.0 / C->trans->curnum);
                if (belongcl == belongcu)
                {
                    for (int ki = 0; ki <= Tmp->KeyNum - 1; ki++)
                    {
                        C->trans->transferLayer[belongcl] = Insert(C->trans->transferLayer[belongcl], Tmp->Key[ki]);
                    }
                }
                else
                {
                    for (int ki = 0; ki <= Tmp->KeyNum - 1; ki++)
                    {
                        cdfcl = cdfCalculate(C->Mnet, Tmp->Key[ki]);
                        belongcl = cdfcl / (1.0 / C->trans->curnum);
                        C->trans->transferLayer[belongcl] = Insert(C->trans->transferLayer[belongcl], Tmp->Key[ki]);
                    }
                }
                Tmp = Tmp->Next;
            }
            Destroy(Ptemp);
            // Travel(C->trans->transferLayer[i]);
            LastNode = NULL;
        }
    }
    cout << "modifiedTrees:" << modifiedTrees << endl;
    LastNode = NULL;
    for (int i = 0; i < C->trans->curnum; i++)
    {
        BPlusTree tmp = C->trans->transferLayer[i];
        Travel(tmp);
        LastNode = NULL;
    }

    CardIndexReport(C);
}
void brief(CardIndex *C)
{
    int tol = 0;
    for (int i = 0; i < C->trans->curnum; i++)
    {
        Position Tmp = C->trans->transferLayer[i];
        while (Tmp->Children[0] != NULL)
        {
            Tmp = Tmp->Children[0];
        }
        while (Tmp != NULL)
        {
            tol += Tmp->KeyNum;
            Tmp = Tmp->Next;
        }
    }

    cout << "\nNum:" << tol << endl;
}
Querys *readQueryFile(string queryfilename)
{
    ifstream infile(queryfilename);
    if (!infile.is_open())
    {
        cout << queryfilename << endl;
        cout << "Fail to load" << endl;
        return NULL;
    }
    int colNumber, queryNumber;
    infile >> colNumber >> queryNumber;
    // cout<<colNumber<< "  "<<queryNumber<<endl;
    Querys *A = new Querys;
    A->queryNumber = queryNumber;
    A->Qs = new Query[queryNumber];
    int *binaryLength = new int[queryNumber];
    for (int i = 0; i < colNumber; i++)
    {
        infile >> binaryLength[i];
    }
    for (int i = 0; i < queryNumber; i++)
    {
        A->Qs[i].binaryLength = binaryLength;
        A->Qs[i].columnNumber = colNumber;
        A->Qs[i].queryid = i;
        A->Qs[i].leftupBound = new long long[colNumber];
        A->Qs[i].rightdownBound = new long long[colNumber];
        for (int j = 0; j < colNumber; j++)
        {
            infile >> A->Qs[i].leftupBound[j];
        }
        for (int j = 0; j < colNumber; j++)
        {
            infile >> A->Qs[i].rightdownBound[j];
        }
        long Tnumber;
        infile >> Tnumber;
        qid2TrueNumber[i] = Tnumber;
        // cout<<Tnumber<<endl;
    }
    // cout << "end" << endl;
    return A;
}
bool *QueryUp2Zvalue(Query Qi, int *tolbitsx, int rightupflag)
{
    // 将Qi的端点转Z值。同时首位用0填充,rightupflag=1：左上，=0,右下
    vector<vector<int>> bCs;
    // for(int i=0;i<45;i++){
    //     cout<<1;
    // }exit(1);
    for (int i = 0; i < Qi.columnNumber; i++)
    {
        vector<int> bvi;
        int digitTollen = Qi.binaryLength[i];
        int *vi = new int[digitTollen + 1];

        long long v;
        if (rightupflag == 0)
        {
            v = Qi.leftupBound[i];
        }
        else
        {
            v = Qi.rightdownBound[i];
        }
        longlong2digVec(v, vi, digitTollen);

        for (int ix = 0; ix < digitTollen; ix += 1)
        {
            bvi.push_back(vi[ix]);
        }
        // cout << endl
        //      << v << " " << digitTollen << endl;
        bCs.push_back(bvi);
    }
    int tolbits = 0;
    int maxbit = -1;
    for (int i = 0; i < Qi.columnNumber; i++)
    {
        if (Qi.binaryLength[i] > maxbit)
        {
            maxbit = Qi.binaryLength[i];
        }
        tolbits += Qi.binaryLength[i];
    }
    *tolbitsx = tolbits;
    bool *zencode = new bool[tolbits + 1];
    zencode[0] = 0;
    int cnt = 1;
    for (int i = 0; i < maxbit; i++)
    {
        for (int j = 0; j < Qi.columnNumber; j++)
        {
            if (i >= Qi.binaryLength[j])
            {
                continue;
            }
            else
            {
                zencode[cnt] = (bool)bCs[j][i];
                cnt += 1;
            }
        }
    }
    return zencode;
}

// 递归搜数据，返回叶节点指针
Position recrusiveFind(BPlusTree T, KeyType Key)
{
    if (T == NULL)
    {
        // cout << "NUL PTR" << endl;
        return T;
    }
    if (T->Children[0] == NULL)
    {
        return T;
    }
    // cout<<"Search begin: "<<Key->z32<<" "<<T->KeyNum<<endl;
    int j, Limit;
    Position Sibling;
    /* 查找分支 */
    j = 0;
    while (j < T->KeyNum && (unsigned)Key->z32 >= (unsigned)T->Key[j]->z32)
    {
        if ((unsigned)Key->z32 == (unsigned)T->Key[j]->z32)
        {
            if (T->Children[j] != NULL)
            {
                return recrusiveFind(T->Children[j], Key);
            }
            else
            {
                return T;
            }
        }
        j++;
    }
    // cout<<j<<endl;
    if (j != 0 && T->Children[0] != NULL)
    {
        j--;
    }
    return recrusiveFind(T->Children[j], Key);
}
void testBPlusPointQuery(BPlusTree T, string queryfilepath)
{
    typedef std::chrono::high_resolution_clock Clock;
    int NonleafCnt = 0;
    Querys *qs = readQueryFile(queryfilepath);
    cout << "Doing Point Q" << endl;
    int loopUp = 1;
    for (int loop = 0; loop < loopUp; loop++)
    {
        for (int i = 0; i < qs->queryNumber; i++)
        {
            // i = 67;
            // cout << "\rQid:" << i;
            int scanneditem = 0;
            Query qi = qs->Qs[i];
            bool *zencode0 = QueryUp2Zvalue(qi, tolbits, 1);
            // my turn
            ZTuple *ZT0 = makeZT(&zencode0[1], *tolbits);
            // cout<<ZT0->z32<<endl;exit(1);
            auto t1 = Clock::now(); // 计时开始
            Position p0 = recrusiveFind(T, ZT0);
            int flag = 0;
            for (int j = 0; j < p0->KeyNum; j++)
            {
                // cout<<p0->Key[j]->z32<<" "<<ZT0->z32<<endl;
                if (p0->Key[j]->z32 == ZT0->z32)
                {
                    cout << "Find" << endl;
                    flag = 1;
                    break;
                }

                // cout<<p0->Key[j]->z32<<" "<<ZT0->z32<<endl;
            }
            if (flag == 0)
            {
                cout << "nf" << endl;
                exit(1);
            }
            // exit(1);
            // int *blk0s = pointQueryTriple(M, qi, zencode0);
            auto t1d = Clock::now(); // 计时开始
            NonleafCnt += (std::chrono::duration_cast<std::chrono::nanoseconds>(t1d - t1).count());
            continue;
        }
    }
    cout << "Avg:" << NonleafCnt / (0.0 + loopUp * qs->queryNumber) << endl;
}
bool inboxZ(Query Qi, bool *zencode)
{
    int maxlen = 0;
    long long zdecode[20];
    for (int i = 0; i < Qi.columnNumber; i += 1)
    {
        zdecode[i] = 0;
        if (Qi.binaryLength[i] > maxlen)
        {
            maxlen = Qi.binaryLength[i];
        }
    }
    int idx = 0;
    for (int i = 0; i < maxlen; i += 1)
    {
        for (int j = 0; j < Qi.columnNumber; j++)
        {
            if (i >= Qi.binaryLength[j])
            {
                continue;
            }
            else
            {
                zdecode[j] *= 2;
                zdecode[j] += zencode[idx];
                idx += 1;
            }
        }
    }
    for (int i = 0; i < Qi.columnNumber; i++)
    {
        if (zdecode[i] < Qi.leftupBound[i] || zencode[i] > Qi.rightdownBound[i])
        {
            return false;
        }
    }
    return true;
}
bool inbox(Query *qi, KeyType Key)
{
    int c = qi->columnNumber;
    int preserve = 0;
    for (int i = 0; i < c; i++)
    {
        long long v = Key->values[i];
        // cout<<qi->leftupBound[i]<<" "<<v<<" "<<qi->rightdownBound[i]<<endl;
        if (v > qi->rightdownBound[i])
        {
            return false;
        }
        else if (v < qi->leftupBound[i])
        {
            return false;
        }
    }
    return true;
}
bool *getLITMAX(bool *minZ, bool *maxZ, bool *zvalue, int bitlength, int *colbitlength, int colNum)
{
    bool *Litmax = new bool[bitlength];
    bool *tmpmaxZ = new bool[bitlength];
    bool *tmpminZ = new bool[bitlength];
    for (int i = 0; i < bitlength; i += 1)
    {
        tmpminZ[i] = minZ[i];
        tmpmaxZ[i] = maxZ[i];
        Litmax[i] = 0;
    }
    int maxlen = 0;
    for (int i = 0; i < colNum; i += 1)
    {
        if (colbitlength[i] > maxlen)
        {
            maxlen = colbitlength[i];
        }
    }
    int idx = 0;
    for (int i = 0; i < maxlen; i += 1)
    {
        for (int j = 0; j < colNum; j++)
        {
            if (i >= colbitlength[j])
            {
                continue;
            }
            else
            {
                // cout<<i<<"th dig of col"<<j<<endl;
                int divnum = zvalue[idx];
                int minnum = tmpminZ[idx];
                int maxnum = tmpmaxZ[idx];
                // cout << "Idx" << idx << " div:" << divnum << " minnum:" << minnum << "  maxN:" << maxnum << endl;
                // cout << "Tminz:";
                // for (int i = 0; i < bitlength; i++)
                // {
                //     cout << tmpminZ[i] << " ";
                // }
                // cout << endl;
                // cout << "Tmaxz:";
                // for (int i = 0; i < bitlength; i++)
                // {
                //     cout << tmpmaxZ[i] << " ";
                // }
                // cout << endl;
                // cout << "Bigmi:";
                // for (int i = 0; i < bitlength; i++)
                // {
                //     cout << Bigmin[i] << " ";
                // }
                // cout << endl;

                if (divnum == 0 && minnum == 0 && maxnum == 0)
                {
                    idx += 1;
                    continue;
                }
                if (divnum == 1 && minnum == 1 && maxnum == 1)
                {
                    idx += 1;
                    continue;
                }
                if (divnum == 0 && minnum == 1 && maxnum == 1)
                {
                    // cout<<"code:011"<<endl;
                    return Litmax;
                }
                if (divnum == 1 && minnum == 0 && maxnum == 0)
                {
                    // cout<<"code100"<<endl;
                    return tmpmaxZ;
                }
                if (divnum == 0 && minnum == 1 && maxnum == 0)
                {
                    return zvalue;
                    cout << "LITMAXWRONG!" << endl;
                    exit(1);
                    // return Bigmin;
                }
                if (divnum == 1 && minnum == 1 && maxnum == 0)
                {
                    return zvalue;
                    cout << "LITMAXWRONG!" << endl;
                    exit(1);
                    // return Bigmin;
                }

                if (divnum == 0 && minnum == 0 && maxnum == 1)
                {
                    // cout << "CODE: 101" << endl;
                    // max = 1000000
                    int innercnt = 0;
                    for (int x0 = 0; x0 < maxlen; x0++)
                    {
                        for (int x1 = 0; x1 < colNum; x1 += 1)
                        {
                            if (x0 >= colbitlength[x1])
                            {
                                continue;
                            }
                            else
                            {
                                if (x0 < i)
                                {
                                    innercnt++;
                                    continue;
                                }
                                else if (x0 == i)
                                {
                                    if (x1 == j)
                                    {
                                        tmpmaxZ[innercnt] = 0;
                                    }
                                    innercnt += 1;
                                }
                                else
                                {
                                    if (x1 == j)
                                    {
                                        tmpminZ[innercnt] = 1;
                                    }
                                    innercnt += 1;
                                }
                            }
                        }
                    }
                    // Bigmin = tmpminZ;
                    idx += 1;
                    continue;
                }
                if (divnum == 1 && minnum == 0 && maxnum == 1)
                {
                    // cout << "CODE:001" << endl;
                    for (int x00 = 0; x00 < bitlength; x00++)
                    {
                        Litmax[x00] = tmpmaxZ[x00];
                    }
                    int innercnt = 0;
                    for (int x0 = 0; x0 < maxlen; x0++)
                    {
                        for (int x1 = 0; x1 < colNum; x1 += 1)
                        {
                            if (x0 >= colbitlength[x1])
                            {
                                continue;
                            }
                            else
                            {
                                if (x0 < i)
                                {
                                    innercnt++;
                                    continue;
                                }
                                else if (x0 == i)
                                {
                                    if (x1 == j)
                                    {
                                        Litmax[innercnt] = 0;
                                    }
                                    innercnt += 1;
                                }
                                else
                                {
                                    if (x1 == j)
                                    {
                                        Litmax[innercnt] = 1;
                                    }
                                    innercnt += 1;
                                }
                            }
                        }
                    }
                    innercnt = 0;
                    for (int x0 = 0; x0 < maxlen; x0++)
                    {
                        for (int x1 = 0; x1 < colNum; x1 += 1)
                        {
                            if (x0 >= colbitlength[x1])
                            {
                                continue;
                            }
                            else
                            {
                                if (x0 < i)
                                {
                                    innercnt++;
                                    continue;
                                }
                                else if (x0 == i)
                                {
                                    if (x1 == j)
                                    {
                                        tmpminZ[innercnt] = 1;
                                    }
                                    innercnt += 1;
                                }
                                else
                                {
                                    if (x1 == j)
                                    {
                                        tmpminZ[innercnt] = 0;
                                    }
                                    innercnt += 1;
                                }
                            }
                        }
                    }
                    // cout << "Tminz:";
                    // for (int i = 0; i < bitlength; i++)
                    // {
                    //     cout << tmpminZ[i];
                    // }
                    // cout << endl;
                    // cout << "Tmaxz:";
                    // for (int i = 0; i < bitlength; i++)
                    // {
                    //     cout << tmpmaxZ[i];
                    // }
                    // cout << endl;
                    // cout << "Bigmi:";
                    // for (int i = 0; i < bitlength; i++)
                    // {
                    //     cout << Bigmin[i];
                    // }
                    // cout << endl;
                    idx += 1;
                    continue;
                }
                idx += 1;
            }
        }
    }
    // cout<<"Normal Ret"<<endl;
    return Litmax;
}

bool *getBIGMIN(bool *minZ, bool *maxZ, bool *zvalue, int bitlength, int *colbitlength, int colNum)
{
    // 输入：查询框的左上minz，右下maxz，范围外的zvalue，返回zvalue的bigmin

    bool *Bigmin = new bool[bitlength];
    bool *tmpmaxZ = new bool[bitlength];
    bool *tmpminZ = new bool[bitlength];
    for (int i = 0; i < bitlength; i += 1)
    {
        tmpminZ[i] = minZ[i];
        tmpmaxZ[i] = maxZ[i];
        Bigmin[i] = 0;
    }
    // cout << "iptzv:";
    // for (int i = 0; i < bitlength - 1; i++)
    // {
    //     cout << zvalue[i];
    // }
    // cout << endl;
    // cout << "Tminz:";
    // for (int i = 0; i < bitlength - 1; i++)
    // {
    //     cout << tmpminZ[i];
    // }
    // cout << endl;
    // cout << "Tmaxz:";
    // for (int i = 0; i < bitlength - 1; i++)
    // {
    //     cout << tmpmaxZ[i];
    // }
    // cout << endl;

    int maxlen = 0;
    for (int i = 0; i < colNum; i += 1)
    {
        if (colbitlength[i] > maxlen)
        {
            maxlen = colbitlength[i];
        }
    }
    int idx = 0;
    for (int i = 0; i < maxlen; i += 1)
    {
        for (int j = 0; j < colNum; j++)
        {
            if (i >= colbitlength[j])
            {
                continue;
            }
            else
            {
                // cout<<i<<"th dig of col"<<j<<endl;
                int divnum = zvalue[idx];
                int minnum = tmpminZ[idx];
                int maxnum = tmpmaxZ[idx];
                // cout << "Idx" << idx << " div:" << divnum << " minnum:" << minnum << "  maxN:" << maxnum << endl;
                // cout << "Tminz:";
                // for (int i = 0; i < bitlength; i++)
                // {
                //     cout << tmpminZ[i] << " ";
                // }
                // cout << endl;
                // cout << "Tmaxz:";
                // for (int i = 0; i < bitlength; i++)
                // {
                //     cout << tmpmaxZ[i] << " ";
                // }
                // cout << endl;
                // cout << "Bigmi:";
                // for (int i = 0; i < bitlength; i++)
                // {
                //     cout << Bigmin[i] << " ";
                // }
                // cout << endl;

                if (divnum == 0 && minnum == 0 && maxnum == 0)
                {
                    idx += 1;
                    continue;
                }
                if (divnum == 1 && minnum == 1 && maxnum == 1)
                {
                    idx += 1;
                    continue;
                }
                if (divnum == 0 && minnum == 1 && maxnum == 1)
                {
                    // cout<<"code:011"<<endl;
                    return tmpminZ;
                }
                if (divnum == 1 && minnum == 0 && maxnum == 0)
                {
                    // cout<<"code100"<<endl;
                    return Bigmin;
                }
                if (divnum == 0 && minnum == 1 && maxnum == 0)
                {
                    return zvalue;
                    // cout << "BMWRONG!" << endl;
                    // exit(1);
                    // return Bigmin;
                }
                if (divnum == 1 && minnum == 1 && maxnum == 0)
                {
                    return zvalue;
                    // cout << "BMWRONG!" << endl;
                    // exit(1);
                    // return Bigmin;
                }

                if (divnum == 1 && minnum == 0 && maxnum == 1)
                {
                    // cout << "CODE: 101" << endl;
                    // minz = 1000000
                    int innercnt = 0;
                    for (int x0 = 0; x0 < maxlen; x0++)
                    {
                        for (int x1 = 0; x1 < colNum; x1 += 1)
                        {
                            if (x0 >= colbitlength[x1])
                            {
                                continue;
                            }
                            else
                            {
                                if (x0 < i)
                                {
                                    innercnt++;
                                    continue;
                                }
                                else if (x0 == i)
                                {
                                    if (x1 == j)
                                    {
                                        tmpminZ[innercnt] = 1;
                                    }
                                    innercnt += 1;
                                }
                                else
                                {
                                    if (x1 == j)
                                    {
                                        tmpminZ[innercnt] = 0;
                                    }
                                    innercnt += 1;
                                }
                            }
                        }
                    }
                    // Bigmin = tmpminZ;
                    idx += 1;
                    continue;
                }
                if (divnum == 0 && minnum == 0 && maxnum == 1)
                {
                    // cout << "CODE:001" << endl;
                    for (int x00 = 0; x00 < bitlength; x00++)
                    {
                        Bigmin[x00] = tmpminZ[x00];
                    }
                    int innercnt = 0;
                    for (int x0 = 0; x0 < maxlen; x0++)
                    {
                        for (int x1 = 0; x1 < colNum; x1 += 1)
                        {
                            if (x0 >= colbitlength[x1])
                            {
                                continue;
                            }
                            else
                            {
                                if (x0 < i)
                                {
                                    innercnt++;
                                    continue;
                                }
                                else if (x0 == i)
                                {
                                    if (x1 == j)
                                    {
                                        Bigmin[innercnt] = 1;
                                    }
                                    innercnt += 1;
                                }
                                else
                                {
                                    if (x1 == j)
                                    {
                                        Bigmin[innercnt] = 0;
                                    }
                                    innercnt += 1;
                                }
                            }
                        }
                    }
                    innercnt = 0;
                    for (int x0 = 0; x0 < maxlen; x0++)
                    {
                        for (int x1 = 0; x1 < colNum; x1 += 1)
                        {
                            if (x0 >= colbitlength[x1])
                            {
                                continue;
                            }
                            else
                            {
                                if (x0 < i)
                                {
                                    innercnt++;
                                    continue;
                                }
                                else if (x0 == i)
                                {
                                    if (x1 == j)
                                    {
                                        tmpmaxZ[innercnt] = 0;
                                    }
                                    innercnt += 1;
                                }
                                else
                                {
                                    if (x1 == j)
                                    {
                                        tmpmaxZ[innercnt] = 1;
                                    }
                                    innercnt += 1;
                                }
                            }
                        }
                    }
                    // cout << "Tminz:";
                    // for (int i = 0; i < bitlength; i++)
                    // {
                    //     cout << tmpminZ[i];
                    // }
                    // cout << endl;
                    // cout << "Tmaxz:";
                    // for (int i = 0; i < bitlength; i++)
                    // {
                    //     cout << tmpmaxZ[i];
                    // }
                    // cout << endl;
                    // cout << "Bigmi:";
                    // for (int i = 0; i < bitlength; i++)
                    // {
                    //     cout << Bigmin[i];
                    // }
                    // cout << endl;
                    idx += 1;
                    continue;
                }
                idx += 1;
            }
        }
    }
    // cout<<"Normal Ret"<<endl;
    return Bigmin;
}
// int rangeQueryExceute(CardIndex *C, Query qi)
// {
//     int scanneditem = 0;
//     bool *zencode0 = QueryUp2Zvalue(qi, tolbits, 0);
//     bool *zencode1 = QueryUp2Zvalue(qi, tolbits, 1);
//     ZTuple *ZT0 = makeZT(&zencode0[1], *tolbits);
//     ZTuple *ZT1 = makeZT(&zencode1[1], *tolbits);
// }
int rangeQueryExceute(BPlusTree T, Query qi)
{
    int scanneditem = 0;
    bool *zencode0 = QueryUp2Zvalue(qi, tolbits, 0);
    bool *zencode1 = QueryUp2Zvalue(qi, tolbits, 1);
    ZTuple *ZT0 = makeZT(&zencode0[1], *tolbits);
    ZTuple *ZT1 = makeZT(&zencode1[1], *tolbits);
    Position p0 = recrusiveFind(T, ZT0);
    Position p1 = recrusiveFind(T, ZT1);
    if (p0 == NULL && p1 == NULL)
    {
        return 0;
    }
    int estcard = 0;
    for (Position tmp = p0; (tmp != p1->Next) && (tmp != NULL); tmp = tmp)
    {
        int flag = 0;
        for (int ti = 0; ti < tmp->KeyNum; ti++)
        {
            if (inbox(&qi, tmp->Key[ti]))
            {
                estcard += 1;
                flag = 1;
            }
        }
        if (flag == 0)
        {
            bool *bigmin = getBIGMIN(ZT0->bin, ZT1->bin, tmp->Key[tmp->KeyNum - 1]->bin, *tolbits, qi.binaryLength, qi.columnNumber);
            ZTuple *ZTX = makeZT(bigmin, *tolbits);
            Position pt = recrusiveFind(T, ZTX);
            // cout<<pt->Key[0]->z32<<" "<<tmp->Key[0]->z32<<endl;
            if (pt->Key[0]->z32 <= tmp->Key[0]->z32)
            {
                tmp = tmp->Next;
            }
            else
            {
                tmp = pt;
            }
        }
        else
        {
            tmp = tmp->Next;
        }
        // if (tmp->Next == NULL || tmp->Next->Key[0]->z32 > p1->Key[0]->z32)
        // {
        //     break;
        // }
    }
    return estcard;
}

void testBPlusRangeQuery(BPlusTree T, string queryfilepath)
{
    typedef std::chrono::high_resolution_clock Clock;
    long long NonleafCnt = 0;
    Querys *qs = readQueryFile(queryfilepath);
    cout << "Doing Range Q" << endl;
    int loopUp = 1;
    for (int loop = 0; loop < loopUp; loop++)
    {
        for (int i = 0; i < qs->queryNumber; i++)
        {
            // i = 1;
            cout << "Qid:" << i << endl;
            int scanneditem = 0;
            Query qi = qs->Qs[i];
            bool *zencode0 = QueryUp2Zvalue(qi, tolbits, 0);
            bool *zencode1 = QueryUp2Zvalue(qi, tolbits, 1);
            // my turn
            ZTuple *ZT0 = makeZT(&zencode0[1], *tolbits);
            ZTuple *ZT1 = makeZT(&zencode1[1], *tolbits);
            // cout << ZT0->z32 << endl;
            // cout << ZT1->z32 << endl;
            auto t1 = Clock::now(); // 计时开始
            Position p0 = recrusiveFind(T, ZT0);
            // cout << ZT0->z32 << ": ";
            // for (int j = 0; j < qi.columnNumber; j++)
            // {
            //     cout << qi.leftupBound[j] << " ";
            // }
            // cout << endl;
            Position p1 = recrusiveFind(T, ZT1);
            int estcard = 0;
            int truecard = qid2TrueNumber[i];
            // cout << ZT1->z32 << ": ";
            // for (int j = 0; j < qi.columnNumber; j++)
            // {
            //     cout << qi.rightdownBound[j] << " ";
            // }
            // cout << endl;
            // cout << "D------------------" << endl;
            int tolnum = 0;
            for (Position tmp = p0; (tmp != p1->Next) && (tmp != NULL); tmp = tmp)
            {
                // cout<<tmp->Key[0]->z32<<endl;
                int flag = 0;
                for (int ti = 0; ti < tmp->KeyNum; ti++)
                {
                    tolnum++;
                    // cout<<tmp->Key[ti]->z32<< ": ";
                    // for (int j = 0; j < qi.columnNumber; j++)
                    // {
                    //     cout << tmp->Key[ti]->values[j] << " ";
                    // }
                    // cout << endl;
                    if (inbox(&qi, tmp->Key[ti]))
                    {
                        estcard += 1;
                        flag = 1;
                    }
                    // exit(1);
                }
                if (flag == 0)
                {
                    bool *bigmin = getBIGMIN(ZT0->bin, ZT1->bin, tmp->Key[tmp->KeyNum - 1]->bin, *tolbits, qi.binaryLength, qi.columnNumber);
                    ZTuple *ZTX = makeZT(bigmin, *tolbits);
                    Position pt = recrusiveFind(T, ZTX);
                    // cout<<pt->Key[0]->z32<<" "<<tmp->Key[0]->z32<<endl;
                    if (pt->Key[0]->z32 <= tmp->Key[0]->z32)
                    {
                        tmp = tmp->Next;
                    }
                    else
                    {
                        tmp = pt;
                    }
                }
                else
                {
                    tmp = tmp->Next;
                }
                // if (tmp->Next == NULL || tmp->Next->Key[0]->z32 > p1->Key[0]->z32)
                // {
                //     break;
                // }
            }
            cout << estcard << " " << truecard << " scanned:" << tolnum << endl;
            // exit(1);
            // int *blk0s = pointQueryTriple(M, qi, zencode0);
            auto t1d = Clock::now(); // 计时开始
            NonleafCnt += (std::chrono::duration_cast<std::chrono::nanoseconds>(t1d - t1).count());
            continue;
        }
        // exit(1);
    }
    cout << "Avg:" << NonleafCnt / (0.0 + loopUp * qs->queryNumber) << endl;
}

float pErrorcalculate(int est, int gt)
{
    if (est < gt)
    {
        int t = gt;
        gt = est;
        est = t;
    }
    if (gt == 0)
    {
        gt = 1;
    }
    return (est + 0.0) / (gt + 0.0);
}
void MadeIndexInferDig(int *xinput, float *out, int startidx, int endidx, MADENet *net, float *middle)
{
    int winlen = net->connectlen;
    for (int i = startidx; i <= endidx; i++)
    {
        middle[i] = net->fc1b[i];
        for (int j = max(i - winlen, 0); j < i; j++)
        {
            if (j >= i)
            {
                break;
            }
            middle[i] += (xinput[j] * net->fc1w[i * net->diglen + j]);
        }
        if (middle[i] < 0)
        {
            middle[i] = 0;
        }
    }
    // for (int i=0;i<5;i++){
    //     cout<<middle[i]<<" ";
    // }cout<<endl;
    for (int i = startidx; i <= endidx; i++)
    {
        out[i] = net->fc2b[i];
        // cout<<"oi"<<out[i]<<endl;
        for (int j = max(i - winlen, 0); j < i; j++)
        {
            if (j >= i)
            {
                break;
            }
            out[i] += (middle[j] * net->fc2w[i * net->diglen + j]);
        }
        // cout<<out[i]<<endl;
        out[i] = (1.0) / (1.0 + exp(-out[i]));
        // cout<<out[i]<<endl;
    }
    // for(int i=0;i<5;i++){
    //     cout<<out[i]<<endl;
    // }
    // exit(1);
}

int lrpermitCheck(long long minv, long long maxv, int encodeLength, int position, long long *rec, int demo)
{
    // cout<<"I'm in"<<endl;
    if (demo == 1)
    {
        cout << "MIN: " << minv << " MAX: " << maxv << " LEN:" << encodeLength << endl;
    }
    //
    bool tmpminv[200];
    bool tmpmaxv[200];
    long long tminv = minv;
    long long tmaxv = maxv;
    for (int i = 0; i < encodeLength; i++)
    {
        tmpminv[encodeLength - i - 1] = tminv % 2;
        tmpmaxv[encodeLength - i - 1] = tmaxv % 2;
        // cout<<tmaxv<<" "<< encodeLength-i<<" "<<tmpmaxv[encodeLength-i-1] <<endl;
        tminv = tminv >> 1;
        tmaxv = tmaxv >> 1;
    }
    if (tmaxv != 0)
    {
        for (int i = 0; i < encodeLength; i++)
        {
            tmpmaxv[encodeLength - i - 1] = 1;
        }
    }
    // cout<<tmaxv<<endl;
    // cout<<"\n";
    if (demo == 1)
    {
        for (int i = 0; i < encodeLength; i++)
        {
            cout << tmpmaxv[i] << " ";
        }
        cout << endl;
        for (int i = 0; i < encodeLength; i++)
        {
            cout << tmpminv[i] << " ";
        }
        cout << endl;
        for (int i = 0; i < position; i++)
        {
            cout << rec[i] << " ";
        }
        cout << endl;
    }
    //
    int leftp = 0; // 0:Nan,1:leftPermit
    int flagx0 = 1;
    for (int i = 0; i < position; i++)
    {
        if (rec[i] > tmpminv[i])
        {
            leftp = 1;
            flagx0 = 0;
            break;
        }
        else if (rec[i] < tmpminv[i])
        {
            leftp = 0;
            flagx0 = 0;
            break;
        }
    }
    if (flagx0 == 1)
    {
        if (tmpminv[position] == 0)
        {
            leftp = 1;
        }
    }
    flagx0 = 1;     // reset
    int rightp = 0; // 0:Nan,1:rightPermit
    for (int i = 0; i < position; i++)
    {
        if (rec[i] > tmpmaxv[i])
        {
            rightp = 0;
            flagx0 = 0;
            break;
        }
        else if (rec[i] < tmpmaxv[i])
        {
            rightp = 1;
            flagx0 = 0;
            break;
        }
    }
    if (flagx0 == 1)
    {
        if (tmpmaxv[position] == 1)
        {
            rightp = 1;
        }
    }
    int retv = 0; // 0:AllNon , 1: Left P,2: rightP,3:AllP
    if (leftp == 0 && rightp == 0)
    {
        retv = 0;
    }
    if (leftp == 1 && rightp == 0)
    {
        retv = 1;
    }
    if (leftp == 0 && rightp == 1)
    {
        retv = 2;
    }
    if (leftp == 1 && rightp == 1)
    {
        retv = 3;
    }

    // cout<<retv<<endl;
    return retv;
}

float drawZ(MADENet *root, Query Qi, int demo)
{
    float out[150];
    float mid[150];
    float p = 1.0;
    int *binaryList = Qi.binaryLength;
    int binaryAllLen = 0;
    int maxBinecn = -1;
    for (int i = 0; i < Qi.columnNumber; i++)
    {
        if (binaryList[i] > maxBinecn)
        {
            maxBinecn = binaryList[i];
        }
        if (binaryList[i] == 0)
        {
            binaryAllLen += 1;
            continue;
        }
        binaryAllLen += binaryList[i];
    }
    int currentdepth = 0;
    long long *searchState[15];
    for (int i = 0; i < Qi.columnNumber; i++)
    {
        int binlen = binaryList[i] + 1;
        searchState[i] = new long long[binlen];
    }
    int flag = 0;
    int samplePoint[150];
    for (int i = 0; i < binaryAllLen; i++)
    {
        samplePoint[i] = 0;
    }
    float OneProbpath[150];
    int innerloopCounter = 0;
    int layer = 0;
    // cout<<maxBinecn<<Qi.columnNumber<<endl;
    for (int i = 0; i < Qi.columnNumber; i++)
    {
        // cout<< binaryList[i]<<" ";
        if (binaryList[i] == 0)
        {
            binaryList[i] = 1;
        }
    }

    for (int i = 0; i < 100; i++)
    {
        samplePoint[i] = 0;
        out[i] = 0;
        mid[i] = 0;
    }
    for (int ix = 0; ix < maxBinecn; ix++)
    {
        for (int j = 0; j < Qi.columnNumber; j++)
        {

            if (ix >= binaryList[j])
            {
                continue;
            }
            else
            {
                // cout<<currentdepth<<endl;
                // currentdepth+=1;
                // continue;
                // eval

                // cout << samplePoint << endl;
                if (demo == 1)
                {
                    cout << ix << " th Dig of col" << j << endl;
                    cout << "Layer:" << layer << endl;
                }
                // cout<<ix<<" th Dig of col"<<j<<endl;
                float OneProb;

                // cout << currentdepth<<endl;
                //  cout<<"curDep"<<currentdepth<<" defaultDepth:"<<defaultDepth<<endl;
                currentdepth += 1;

                MadeIndexInferDig(samplePoint, out, innerloopCounter, innerloopCounter, root, mid);
                OneProb = out[innerloopCounter];
                // cout<<innerloopCounter <<" "<<OneProb<<endl;
                // exit(1);
                OneProbpath[innerloopCounter] = OneProb;
                // if (innerloopCounter > 80)
                // {
                //     return p;
                // }
                if (demo == 1)
                {
                    cout << "ilc" << innerloopCounter << " NetInput:" << samplePoint[innerloopCounter] << endl;
                    cout << "OneProb: " << OneProb << " P: " << p << endl;
                    // if (innerloopCounter ==3){
                    //     exit(1);
                    // }
                }
                // cout<<"OneProb: "<<OneProb<<" P: "<<p<<endl;
                // 根据查询剪枝
                int ff = lrpermitCheck(Qi.leftupBound[j], Qi.rightdownBound[j], Qi.binaryLength[j], ix, searchState[j], demo);
                if (demo == 1)
                {
                    cout << "PermitState:" << ff << endl;
                }
                //

                if (ff == 0)
                {
                    p = 0;
                    cout << "p0" << endl;
                    exit(1);
                    // return 0;
                }
                if (ff == 1)
                {
                    samplePoint[innerloopCounter] = 0;
                    searchState[j][ix] = 0;
                    p = p * (1 - OneProb);
                }
                if (ff == 2)
                {
                    samplePoint[innerloopCounter] = 1;
                    searchState[j][ix] = 1;
                    p = p * OneProb;
                }
                if (ff == 3)
                { // 生成采样点
                    samplePoint[innerloopCounter] = randG(OneProb);
                    searchState[j][ix] = samplePoint[innerloopCounter];
                }
                innerloopCounter += 1;
            }
        }
    }
    // cout<<"est done"<<endl;
    // for (int i = 0; i < 20; i++)
    // {
    //     cout << samplePoint[i] << " ";
    // }
    // for (int i = 0; i < 20; i++)
    // {
    //     cout << out[i] << " ";
    // }

    // cout<<"Over"<<endl;
    // cout<<p<<endl;
    // exit(1) ;
    for (int i = 0; i < Qi.columnNumber; i++)
    {
        delete searchState[i];
    }
    return p;

    return 0.0;
}

int cardEstimate(MADENet *root, Query Qi, int sampleNumber)
{
    float p = 0;
    int demo = 0;
    for (int i = 0; i < sampleNumber; i++)
    {
        // if (i==1){
        //     demo=1;

        // }
        // cout<<i<<endl;
        p += drawZ(root, Qi, demo);
        if (demo == 1)
        {
            cout << p << endl;
            exit(1);
        }
        /* code */
    }
    p = p / sampleNumber;
    return int(root->zdr * (p));
}
float point2cdfest(MADENet *root, Query Qi, bool *zencode)
{
    float cdf = 0;
    float acc = 1.0;
    float OneProb;
    int belong;
    float minnimumsep;
    float out[35];
    MadeIndexInfer(zencode, out, infL, root, midlle);
    for (int i = 0; i < infL; i++)
    {

        OneProb = out[i];
        cdf += (acc * (1 - OneProb) * zencode[i]);
        acc *= (OneProb * zencode[i] + (1 - OneProb) * (1 - zencode[i]));
        // cout<<i<<OneProb<<" "<<cdf<<" "<<acc<<endl;
    }
    return cdf;
}

int getBelongNum(CardIndex *C, ZTuple *zti)
{
    float cdf = cdfCalculate(C->Mnet, zti);
    if (cdf >= 1)
    {
        cdf = 1;
    }
    int belong = cdf / (1.0 / C->trans->curnum);
    if (belong <= 0)
    {
        belong = 0;
    }
    return belong;
}
void ZADD(bool *zencode0, bool *zencode1, bool *ans, int len)
{
    int next = 0;
    // for(int i=0;i<len;i++){
    //     cout<<zencode0[i];
    // }cout<<endl;
    // for(int i=0;i<len;i++){
    //     cout<<zencode1[i];
    // }cout<<endl;

    for (int i = len - 1; i >= 0; i--)
    {
        int val = zencode0[i] + zencode1[i] + next;
        if (val == 2)
        {
            next = 1;
            ans[i] = 0;
        }
        else if (val == 1)
        {
            next = 0;
            ans[i] = 1;
        }
        else if (val == 3)
        {
            next = 1;
            ans[i] = 1;
        }
        else
        {
            next = 0;
            ans[i] = 0;
        }
    }
    if (next == 1)
    {
        ans[-1] = next;
    }
    // for(int i=0;i<len;i++){
    //     cout<<ans[i];
    // }cout<<endl;
}
float probeCDF(bool *zencode0, bool *zencode1, int diglen, MADENet *Mnet, Query Qi, int depth)
{
    bool *mid = new bool[diglen + 10];
    float cdfl = point2cdfest(Mnet, Qi, zencode0);
    float cdfu = point2cdfest(Mnet, Qi, zencode1);
    // return cdfu-cdfl;
    // cout<<cdfl<<" "<<cdfu<<endl;
    if (cdfl > cdfu)
    {
        return 0;
    }
    if (depth == 0)
    {
        // cout << "Dep" << depth << " L:" << cdfl << " R:" << cdfu <<" Delta:" <<   cdfu - cdfl<< endl;
        return cdfu - cdfl;
    }

    if ((cdfu - cdfl) < 0.00001)
    {
        return cdfu - cdfl;
    }

    mid[0] = 0;
    ZADD(zencode0, zencode1, &mid[1], diglen);
    // cout<<"MIN:";
    // for(int i=0;i<*tolbits;i++){
    //     cout<<zencode0[i];
    // }cout<<endl;
    // cout<<"MAX:";
    // for(int i=0;i<*tolbits;i++){
    //     cout<<zencode1[i];
    // }cout<<endl;
    // cout<<"MID:";
    // for(int i=0;i<*tolbits;i++){
    //     cout<<mid[i];
    // }cout<<endl;
    float cdfm = point2cdfest(Mnet, Qi, mid);
    // cout << "Dep" << depth << " L:" << cdfl << " R:" << cdfu << " mid:" << cdfm << endl;
    // ZTuple *Zt = makeZT(mid,diglen);
    bool flag = inboxZ(Qi, mid);
    // cout<< inboxZ(Qi,zencode0) <<" "<< inboxZ(Qi,zencode1)<<" "<<inboxZ(Qi,mid);
    // exit(1);
    // delete(Zt);
    // cout << "flag" << flag << endl;
    if (flag == true)
    {
        // cout<<"inbox"<<endl;
        // bool lm[120];
        // bool mu[120];
        // lm[0] = 0;
        // mu[0] = 0;
        // ZADD(zencode0, mid, &lm[1], diglen);
        // ZADD(zencode1, mid, &mu[1], diglen);
        float l = probeCDF(zencode0, mid, diglen, Mnet, Qi, depth - 1);
        float u = probeCDF(mid, zencode1, diglen, Mnet, Qi, depth - 1);
        return l + u;
    }
    else
    {
        // cout<<"ob"<<endl;
        bool minb[120];
        bool maxb[120];
        bool midb[120];
        bool tmpbmi[120];
        bool tmpmai[120];
        for (int i = 0; i < diglen; i++)
        {
            minb[i] = zencode0[i];
            maxb[i] = zencode1[i];
            midb[i] = mid[i];
        }
        bool *bigmin = getBIGMIN(minb, maxb, midb, diglen, Qi.binaryLength, Qi.columnNumber);
        bool *litmax = getLITMAX(minb, maxb, midb, diglen, Qi.binaryLength, Qi.columnNumber);
        for (int i = 0; i < diglen; i++)
        {

            tmpbmi[i] = bigmin[i];
            tmpmai[i] = litmax[i];
        }
        // tmpbmi[0] = 0;
        // tmpmai[0] = 0;
        // cout << cdfl << " " << point2cdfest(Mnet, Qi, tmpmai) << " " << point2cdfest(Mnet, Qi, tmpbmi) << " " << cdfu << endl;
        float ret = 0;
        ret += probeCDF(zencode0, tmpmai, diglen, Mnet, Qi, depth - 1);
        ret += probeCDF(tmpbmi, zencode1, diglen, Mnet, Qi, depth - 1);
        // cout<<abs( cdfl - point2cdfest(M,Qi,tmpmai)  )<<endl;
        // if(abs( cdfl - point2cdfest(M,Qi,tmpmai)  )>= 0.000001){
        //     ret += probeCDF(zencode0,tmpbmi,diglen,M,Qi,depth-1);
        // }
        // if(abs(cdfu -point2cdfest(M,Qi,tmpbmi)) >=0.000001){
        //     ret+= probeCDF(tmpmai,zencode1,diglen,M,Qi,depth-1);
        // }
        if (ret < 0)
        {
            return 0;
        }
        return ret;
        // exit(1);
    }
    // cout << cdfm << endl;
}
int singleRangeQ(CardIndex *C, Query qi, ZTuple *ZT0, ZTuple *ZT1)
{
    // cout << "Detect small card APROX-CDF: ";
    // cout << cdfapx << endl;
    int belong0 = getBelongNum(C, ZT0);
    int belong1 = getBelongNum(C, ZT1);
    int estcard = 0;
    // if (belong0 > 0)
    // {
    //     belong0--;
    // }
    // if (belong1 < C->trans->curnum)
    // {
    //     belong1++;
    // }
    for (int xi = belong0; xi <= belong1; xi++)
    {
        estcard += rangeQueryExceute(C->trans->transferLayer[xi], qi);
    }
    return estcard;
}
bool showSubNodePerformance(BPlusNode *subN, ZTuple *ZT0)
{
    BPlusNode *leaf = recrusiveFind(subN, ZT0);
    int flag = 0;
    for (int j = 0; j < leaf->KeyNum; j++)
    {
        // cout<<p0->Key[j]->z32<<" "<<ZT0->z32<<endl;
        if (leaf->Key[j]->z32 == ZT0->z32)
        {
            // cout << "Find" << endl;
            flag = 1;
            return true;
            break;
        }

        // cout<<p0->Key[j]->z32<<" "<<ZT0->z32<<endl;
    }
    if (flag == 0)
    {
        // cout << "nf" << endl;
        return false;
        // exit(1);
    }
    return true;
}

void testCardIndexRangeQuery(CardIndex *C, string queryfilepath)
{
    Querys *qs = readQueryFile(queryfilepath);
    cout << "Query loaded" << endl;
    vector<float> ABSL;
    typedef std::chrono::high_resolution_clock Clock;
    long long timesum = 0, NonleafCnt = 0;
    long long queryestTime = 0;
    cout << "Query Num:" << qs->queryNumber << endl;
    int up = qs->queryNumber;
    int estCard;
    for (int i = 0; i < up; i++)
    {
        estCard = 0;
        // i = 11;
        Query qi = qs->Qs[i];
        // cout << "QID:" << i << " ";
        bool *zencode0 = QueryUp2Zvalue(qi, tolbits, 0);
        ZTuple *ZT0 = makeZT(&zencode0[1], *tolbits);
        bool *zencode1 = QueryUp2Zvalue(qi, tolbits, 1);
        ZTuple *ZT1 = makeZT(&zencode1[1], *tolbits);
        //

        // int estCard = rangeQueryExceute(C, qi);

        int subModelBelong0 = getBelongNum(C, ZT0);
        bool flag0 = C->trans->Flag[subModelBelong0];
        int subModelBelong1 = getBelongNum(C, ZT1);
        if (subModelBelong1 == MAXL1CHILD)
        {
            subModelBelong1--;
        }
        // bool flag1 = C->trans->Flag[subModelBelong1];
        // 左侧可能出现左移的情况，右侧不可能
        BPlusNode *subLeft;
        // cout<<subModelBelong0<<" "<<subModelBelong1<<endl;

        BPlusNode *subRight = C->trans->transferLayer[subModelBelong1];
        if (flag0 == 0)
        {
            subLeft = C->trans->transferLayer[subModelBelong0];
        }
        else
        {
            KeyType SepKey = C->trans->transferLayer[subModelBelong0]->Key[0];
            // cout<<(unsigned)SepKey->z32 <<" "<< (unsigned)ZT0->z32<<endl;
            if ((unsigned)SepKey->z32 < (unsigned)ZT0->z32) // this Node
            {
                subLeft = C->trans->transferLayer[subModelBelong0];
            }
            else
            {
                if (subModelBelong0 == 0) // not find, too small Z value
                {
                    subLeft = C->Head;
                }
                else
                {
                    subLeft = C->trans->transferLayer[subModelBelong0 - 1];
                }
            }
        }
        Position Ps0 = recrusiveFind(subLeft, ZT0);
        Position Ps1 = recrusiveFind(subRight, ZT1);
        // cout<<Ps0->Key[0]->z32<<" "<<Ps1->Key[0]->z32<<endl;
        for (Position tmp = Ps0; (tmp != Ps1->Next) && (tmp != NULL); tmp = tmp)
        {
            int flag = 0;
            for (int ti = 0; ti < tmp->KeyNum; ti++)
            {
                if (inbox(&qi, tmp->Key[ti]))
                {
                    estCard += 1;
                    flag = 1;
                }
            }
            // tmp = tmp->Next;
            // continue;
            if (flag == 0)
            {
                bool *bigmin = getBIGMIN(ZT0->bin, ZT1->bin, tmp->Key[tmp->KeyNum - 1]->bin, *tolbits, qi.binaryLength, qi.columnNumber);
                ZTuple *ZTX = makeZT(bigmin, *tolbits);
                int subModelBelongX = getBelongNum(C, ZTX);
                bool flagX = C->trans->Flag[subModelBelongX];
                Position nextPos;
                if (flagX == 0)
                {
                    nextPos = C->trans->transferLayer[subModelBelongX];
                }
                else
                {
                    KeyType SepKeyX = C->trans->transferLayer[subModelBelongX]->Key[0];
                    if ((unsigned)SepKeyX->z32 < (unsigned)ZTX->z32) // this Node
                    {
                        nextPos = C->trans->transferLayer[subModelBelongX];
                    }
                    else
                    {
                        if (subModelBelongX == 0) // not find, too small Z value
                        {
                            nextPos = C->Head;
                        }
                        else
                        {
                            nextPos = C->trans->transferLayer[subModelBelongX - 1];
                        }
                    }
                }
                Position pt = recrusiveFind(nextPos, ZTX);
                // cout<<(unsigned)pt->Key[0]->z32<<" "<<(unsigned)tmp->Key[0]->z32<<endl;
                if ((unsigned)pt->Key[0]->z32 <= (unsigned)tmp->Key[0]->z32)
                {
                    tmp = tmp->Next;
                }
                else
                {
                    tmp = pt;
                }
            }
            else
            {
                tmp = tmp->Next;
            }
        }

        if (estCard != qid2TrueNumber[i])
        {
            cout << "QID:" << i << "EstCard: " << estCard << " TrueCard: " << qid2TrueNumber[i] << endl;
            // cout<<"QID WRONG: "<<i<<endl;exit(1);
        }
        // exit(1);
    }
    // exit(1);
}

int CardIndexRangeExceute(CardIndex *C, Query qi)
{
    int estCard = 0;
    bool *zencode0 = QueryUp2Zvalue(qi, tolbits, 0);
    ZTuple *ZT0 = makeZT(&zencode0[1], *tolbits);
    bool *zencode1 = QueryUp2Zvalue(qi, tolbits, 1);
    ZTuple *ZT1 = makeZT(&zencode1[1], *tolbits);

    int subModelBelong0 = getBelongNum(C, ZT0);
    bool flag0 = C->trans->Flag[subModelBelong0];
    int subModelBelong1 = getBelongNum(C, ZT1);
    if (subModelBelong1 == MAXL1CHILD)
    {
        subModelBelong1--;
    }
    // bool flag1 = C->trans->Flag[subModelBelong1];
    // 左侧可能出现左移的情况，右侧不可能
    BPlusNode *subLeft;
    // cout<<subModelBelong0<<" "<<subModelBelong1<<endl;

    BPlusNode *subRight = C->trans->transferLayer[subModelBelong1];
    if (flag0 == 0)
    {
        subLeft = C->trans->transferLayer[subModelBelong0];
    }
    else
    {
        KeyType SepKey = C->trans->transferLayer[subModelBelong0]->Key[0];
        // cout<<(unsigned)SepKey->z32 <<" "<< (unsigned)ZT0->z32<<endl;
        if ((unsigned)SepKey->z32 < (unsigned)ZT0->z32) // this Node
        {
            subLeft = C->trans->transferLayer[subModelBelong0];
        }
        else
        {
            if (subModelBelong0 == 0) // not find, too small Z value
            {
                subLeft = C->Head;
            }
            else
            {
                subLeft = C->trans->transferLayer[subModelBelong0 - 1];
            }
        }
    }
    Position Ps0 = recrusiveFind(subLeft, ZT0);
    Position Ps1 = recrusiveFind(subRight, ZT1);
    // cout<<Ps0->Key[0]->z32<<" "<<Ps1->Key[0]->z32<<endl;
    for (Position tmp = Ps0; (tmp != Ps1->Next) && (tmp != NULL); tmp = tmp)
    {
        int flag = 0;
        for (int ti = 0; ti < tmp->KeyNum; ti++)
        {
            if (inbox(&qi, tmp->Key[ti]))
            {
                estCard += 1;
                flag = 1;
            }
        }
        // tmp = tmp->Next;
        // continue;
        if (flag == 0)
        {
            bool *bigmin = getBIGMIN(ZT0->bin, ZT1->bin, tmp->Key[tmp->KeyNum - 1]->bin, *tolbits, qi.binaryLength, qi.columnNumber);
            ZTuple *ZTX = makeZT(bigmin, *tolbits);
            int subModelBelongX = getBelongNum(C, ZTX);
            bool flagX = C->trans->Flag[subModelBelongX];
            Position nextPos;
            if (flagX == 0)
            {
                nextPos = C->trans->transferLayer[subModelBelongX];
            }
            else
            {
                KeyType SepKeyX = C->trans->transferLayer[subModelBelongX]->Key[0];
                if ((unsigned)SepKeyX->z32 < (unsigned)ZTX->z32) // this Node
                {
                    nextPos = C->trans->transferLayer[subModelBelongX];
                }
                else
                {
                    if (subModelBelongX == 0) // not find, too small Z value
                    {
                        nextPos = C->Head;
                    }
                    else
                    {
                        nextPos = C->trans->transferLayer[subModelBelongX - 1];
                    }
                }
            }
            Position pt = recrusiveFind(nextPos, ZTX);
            // cout<<(unsigned)pt->Key[0]->z32<<" "<<(unsigned)tmp->Key[0]->z32<<endl;
            if ((unsigned)pt->Key[0]->z32 <= (unsigned)tmp->Key[0]->z32)
            {
                tmp = tmp->Next;
            }
            else
            {
                tmp = pt;
            }
        }
        else
        {
            tmp = tmp->Next;
        }
    }
    return estCard;
}

void testCardIndexPointQuery(CardIndex *C, string queryfilepath)
{
    Querys *qs = readQueryFile(queryfilepath);
    cout << "Query loaded" << endl;
    vector<float> ABSL;
    typedef std::chrono::high_resolution_clock Clock;
    long long timesum = 0, NonleafCnt = 0;
    long long queryestTime = 0;
    cout << "Query Num:" << qs->queryNumber << endl;
    int up = qs->queryNumber;
    // up = 100;
    for (int j = 0; j <= 100; j++)
    {
        for (int i = 0; i < up; i++)
        {
            // i = 5;
            Query qi = qs->Qs[i];
            // cout << "QID:" << i;
            bool *zencode0 = QueryUp2Zvalue(qi, tolbits, 0);
            ZTuple *ZT0 = makeZT(&zencode0[1], *tolbits);
            auto t0 = Clock::now(); // 计时开始;
            int subModelBelong = getBelongNum(C, ZT0);
            // cout << subModelBelong << endl;
            // cout << "Reached CKPT" << endl;
            bool flag = C->trans->Flag[subModelBelong];
            // cout << "Flag:" << flag << endl;
            if (flag == 0)
            { // 不可能出现split
                BPlusNode *subN = C->trans->transferLayer[subModelBelong];
                showSubNodePerformance(subN, ZT0);
            }
            else
            {
                KeyType SepKey = C->trans->transferLayer[subModelBelong]->Key[0];
                // cout<<(unsigned)SepKey->z32 <<" "<< (unsigned)ZT0->z32<<endl;
                if ((unsigned)SepKey->z32 < (unsigned)ZT0->z32) // this Node
                {
                    BPlusNode *subN = C->trans->transferLayer[subModelBelong];
                    bool fla = showSubNodePerformance(subN, ZT0);
                    if (fla == false)
                    {
                        cout << "NF!!" << endl;
                        exit(1);
                    }
                }
                else if ((unsigned)SepKey->z32 > (unsigned)ZT0->z32)
                {
                    if (subModelBelong == 0)
                    {
                        cout << "0nf" << endl;
                        exit(1);
                    }
                    BPlusNode *subN = C->trans->transferLayer[subModelBelong - 1];
                    bool fla = showSubNodePerformance(subN, ZT0);
                    if (fla == false)
                    {
                        cout << "NF!!" << endl;
                        exit(1);
                    }
                }
                else
                { // 真实情况会更麻烦，首先检查前一个，确定起点在哪。
                    int a = 1;
                    // cout << "find" << endl;
                    // BPlusNode *subN = C->trans->transferLayer[subModelBelong-1];
                    // bool previousAns= showSubNodePerformance(subN, ZT0);
                }
            }
            auto t1 = Clock::now(); // 计时开始;
            timesum += (std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
            // exit(1);
        }
    }
    cout << "ex time:" << timesum / (0.0 + qs->queryNumber * 100);
}

void testCardPerformance(CardIndex *C, string queryfilepath)
{
    vector<float> pdist;
    // Querys* qs = readQueryFile("./data/osmQuerys.txt");
    Querys *qs = readQueryFile(queryfilepath);
    cout << "Query loaded" << endl;
    int sampleN = 2000;
    float p50, p95, p90, p99;
    vector<float> ABSL;
    typedef std::chrono::high_resolution_clock Clock;
    long long timesum = 0, NonleafCnt = 0;
    long long queryestTime = 0;
    cout << qs->queryNumber << endl;
    int up = qs->queryNumber;
    up = 100;
    for (int i = 0; i < up; i++)
    {
        // i = 61;
        // i=568;
        Query qi = qs->Qs[i];
        // cout<<"QID"<<i<<endl;
        auto queryfirst = Clock::now(); // 计时开始
        int realcard, estcard;
        realcard = qid2TrueNumber[i];
        auto t3 = Clock::now(); // 计时开始
        bool *zencode0 = QueryUp2Zvalue(qi, tolbits, 0);
        ZTuple *ZT0 = makeZT(&zencode0[1], *tolbits);
        bool *zencode1 = QueryUp2Zvalue(qi, tolbits, 1);
        ZTuple *ZT1 = makeZT(&zencode1[1], *tolbits);
        float cdfapx = probeCDF(zencode0, zencode1, *tolbits, C->Mnet, qi, 16);
        // cout << i << " " << cdfapx << " " << realcard / (C->Mnet->zdr + 0.0) << endl;
        // exit(1);
        // continue;
        // cout<<"pass"<<endl;
        // int belong0 = getBelongNum(C, ZT0);
        // int belong1 = getBelongNum(C, ZT1);
        // cout <<"bl0 bl1: "<< belong0 << " " << belong1 << endl;
        // cout << cdfapx <<" "<<(realcard+0.0)/C->Mnet->zdr << endl;
        // exit(1);
        // cdfapx = 0;
        if (cdfapx < 0.01) // potential bug exists
        {
            estcard = CardIndexRangeExceute(C, qi);
            // cout << "Detect small card APROX-CDF: ";
            // cout << cdfapx << endl;
            // int belong0 = getBelongNum(C, ZT0);
            // int belong1 = getBelongNum(C, ZT1);
            // estcard = singleRangeQ(C, qi, ZT0, ZT1);
            // int estcardx = cardEstimate(C->Mnet, qi, sampleN);
            // cout << "If not, then QError:" << pErrorcalculate(estcardx, realcard) << endl;
        }
        else
        {
            estcard = cardEstimate(C->Mnet, qi, sampleN);
        }

        auto t3d = Clock::now(); // 计时end
        long delta = (std::chrono::duration_cast<std::chrono::nanoseconds>(t3d - t3).count());
        // cout << "Card Est time:" << delta << endl;
        // cout << estcard << endl;
        timesum += (std::chrono::duration_cast<std::chrono::nanoseconds>(t3d - t3).count());
        // }
        auto queryend = Clock::now(); // 计时开始
        cout << "Qid" << qi.queryid << "\tRealCard:" << realcard << "\tEstCard:" << estcard << " P :" << pErrorcalculate(estcard, realcard) << endl;
        queryestTime += (std::chrono::duration_cast<std::chrono::nanoseconds>(queryend - queryfirst).count());
        // cout << "Query est time:" << to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(queryend - queryfirst).count()) << "ns" << endl;
        // ofs << "Qid" << qi.queryid << "\tRealCard:" << realcard << "\tEstCard:" << estcard << " P :" << pErrorcalculate(estcard, realcard) << endl;
        pdist.push_back(pErrorcalculate(estcard, realcard));
        // exit(1);
    }
    cout << timesum / qs->queryNumber << endl;
    sort(pdist.begin(), pdist.end());
    p50 = pdist[((int)(0.5 * pdist.size()))];
    p95 = pdist[((int)(0.95 * pdist.size()))];
    p99 = pdist[((int)(0.99 * pdist.size()))];
    // p99 = pdist[((int)(0.99 * pdist.size()))];
    float pmax = pdist[pdist.size() - 1];
    cout << "P50\tP95\tP99\tPmax\tAvgT" << endl;
    ofs << p50 << ", " << p95 << ", " << p99 << ", " << pmax << ", " << endl;
    cout << p50 << "\t" << p95 << '\t' << p99 << '\t' << pmax << '\t' << queryestTime / qs->queryNumber << endl;
}

void testBTBulkInsert()
{
    BPlusTree root = NULL;
    typedef std::chrono::high_resolution_clock Clock;
    for (int i = 0; i < 19; i++)
    {
        string filePathi = "./data/ZD" + to_string(i) + ".txt";
        cout << "------------------------\n";
        ZTab *ZT = loadZD(filePathi);
        cout << filePathi << " Loaded" << endl;
        auto t3 = Clock::now(); // 计时开始

        root = bulkInsert(root, ZT);
        cout << "Ins done" << endl;
        auto thalf = Clock::now();
        cout << "Static Ins" << (std::chrono::duration_cast<std::chrono::milliseconds>(thalf - t3).count()) << " In average(ns/t):" << (std::chrono::duration_cast<std::chrono::nanoseconds>(thalf - t3).count()) / (ZT->r + 0.0) << endl;
        cout << "checking correctness" << endl;
        BPlusNode *curptr = root;
        while (curptr->Children[0] != NULL)
        {
            curptr = curptr->Children[0];
        }
        int aggre = 0;
        while (curptr != NULL)
        {
            aggre += curptr->KeyNum;
            curptr = curptr->Next;
        }
        cout << "Toltal elements: " << aggre << endl;
    }
}
void testCardInsert()
{
    // BPlusTree root = NULL;
    BPlusNode *LinkEDList = NULL;
    ofs << "MergeTime; MaintainTime" << endl;
    typedef std::chrono::high_resolution_clock Clock;
    for (int i = 0; i <= 10; i++)
    {
        string filePathi = "./data/ZD" + to_string(i) + ".txt";
        cout << "------------------------\n";
        ZTab *ZT = loadZD(filePathi);
        cout << filePathi << " Loaded" << endl;
        MADENet *Net = loadMade("./Model/MadeRootP" + to_string(i));
        auto t3 = Clock::now(); // 计时开始
        LinkEDList = MergeLinkedList(LinkEDList, ZT);
        auto thalf = Clock::now();
        cout << "Merge done" << endl;
        cout << "AllTime: " << (std::chrono::duration_cast<std::chrono::milliseconds>(thalf - t3).count()) << " In average(ns/t):" << (std::chrono::duration_cast<std::chrono::nanoseconds>(thalf - t3).count()) / (ZT->r + 0.0) << endl;
        ofs << (std::chrono::duration_cast<std::chrono::nanoseconds>(thalf - t3).count()) / (ZT->r + 0.0) << "; ";
        t3 = Clock::now(); // 计时开始
        CardIndex *CI = LinkedList2CardIndex(LinkEDList, Net);
        thalf = Clock::now();
        cout << "AllTime: " << (std::chrono::duration_cast<std::chrono::milliseconds>(thalf - t3).count()) << " In average(ns/t):" << (std::chrono::duration_cast<std::chrono::nanoseconds>(thalf - t3).count()) / (ZT->r + 0.0) << endl;
        ofs << (std::chrono::duration_cast<std::chrono::nanoseconds>(thalf - t3).count()) / (ZT->r + 0.0) << endl;
        cout << "checking correctness" << endl;
        BPlusNode *curptr = CI->Head;
        string queryFilePath = "./query/PQ" + to_string(i) + ".txt";
        cout << queryFilePath << endl;
        // testCardIndexPointQuery(CI, queryFilePath);

    }
}

void testRangeQuery()
{
    BPlusNode *LinkEDList = NULL;
    ofs << "MergeTime; MaintainTime" << endl;
    typedef std::chrono::high_resolution_clock Clock;
    for (int i = 0; i <= 4; i++)
    {
        string filePathi = "./data/ZD" + to_string(i) + ".txt";
        cout << "------------------------\n";
        ZTab *ZT = loadZD(filePathi);
        cout << filePathi << " Loaded" << endl;
        MADENet *Net = loadMade("./Model/MadeRootP" + to_string(i));
        auto t3 = Clock::now(); // 计时开始
        LinkEDList = MergeLinkedList(LinkEDList, ZT);
        auto thalf = Clock::now();
        cout << "Merge done" << endl;
        cout << "AllTime: " << (std::chrono::duration_cast<std::chrono::milliseconds>(thalf - t3).count()) << " In average(ns/t):" << (std::chrono::duration_cast<std::chrono::nanoseconds>(thalf - t3).count()) / (ZT->r + 0.0) << endl;
        ofs << (std::chrono::duration_cast<std::chrono::nanoseconds>(thalf - t3).count()) / (ZT->r + 0.0) << "; ";
        t3 = Clock::now(); // 计时开始
        CardIndex *CI = LinkedList2CardIndex(LinkEDList, Net);
        thalf = Clock::now();
        cout << "AllTime: " << (std::chrono::duration_cast<std::chrono::milliseconds>(thalf - t3).count()) << " In average(ns/t):" << (std::chrono::duration_cast<std::chrono::nanoseconds>(thalf - t3).count()) / (ZT->r + 0.0) << endl;
        ofs << (std::chrono::duration_cast<std::chrono::nanoseconds>(thalf - t3).count()) / (ZT->r + 0.0) << endl;
        cout << "checking correctness via rqnge query" << endl;
        BPlusNode *curptr = CI->Head;
        string queryFilePath = "./query/Q" + to_string(i) + ".txt";
        cout << queryFilePath << endl;
        testCardIndexRangeQuery(CI, queryFilePath);
    }
}
void testInsertedCardPerformance()
{
    BPlusNode *LinkEDList = NULL;
    // ofs << "MergeTime; MaintainTime" << endl;
    typedef std::chrono::high_resolution_clock Clock;
    for (int i = 0; i <= 4; i++)
    {
        string filePathi = "./data/ZD" + to_string(i) + ".txt";
        cout << "------------------------\n";
        ZTab *ZT = loadZD(filePathi);
        cout << filePathi << " Loaded" << endl;
        MADENet *Net = loadMade("./Model/MadeRootP" + to_string(i));
        auto t3 = Clock::now(); // 计时开始
        // cout<<Net
        LinkEDList = MergeLinkedList(LinkEDList, ZT);
        auto thalf = Clock::now();
        cout << "Merge done" << endl;
        cout << "AllTime: " << (std::chrono::duration_cast<std::chrono::milliseconds>(thalf - t3).count()) << " In average(ns/t):" << (std::chrono::duration_cast<std::chrono::nanoseconds>(thalf - t3).count()) / (ZT->r + 0.0) << endl;
        // ofs << (std::chrono::duration_cast<std::chrono::nanoseconds>(thalf - t3).count()) / (ZT->r + 0.0) << "; ";
        t3 = Clock::now(); // 计时开始
        CardIndex *CI = LinkedList2CardIndex(LinkEDList, Net);
        thalf = Clock::now();
        cout << "AllTime: " << (std::chrono::duration_cast<std::chrono::milliseconds>(thalf - t3).count()) << " In average(ns/t):" << (std::chrono::duration_cast<std::chrono::nanoseconds>(thalf - t3).count()) / (ZT->r + 0.0) << endl;
        // ofs << (std::chrono::duration_cast<std::chrono::nanoseconds>(thalf - t3).count()) / (ZT->r + 0.0) << endl;
        cout << "checking correctness via rqnge query" << endl;
        BPlusNode *curptr = CI->Head;
        string queryFilePath = "./query/Q" + to_string(i) + ".txt";
        cout << queryFilePath << endl;
        testCardPerformance(CI, queryFilePath);
        // exit(1);
    }
}
int main(int argc, const char *argv[])
{
    // 思路：首先将树的合并开销降低到小于1微s的级别(已完成)
    //  然后将树的维护开销进一步的减少（已完成）
    //  最后对查询准确度进行调试（范围查询已完成，CE调试结束）
    // testInsertedCardPerformance();
    // system("E:\\Anaconda\\envs\\torch\\python.exe ./FancyTrain.py");
    // cout<<"I'm back 2   C++"<<endl;
    testInsertedCardPerformance();


}