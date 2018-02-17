/*
New File
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#if !defined arrayCount
#define arrayCount(array1) (sizeof(array1) / sizeof(array1[0]))
#endif 

#if !defined invalidCodePathStr
#define invalidCodePathStr(msg) { printf(msg); exit(0); }
#endif

#if !defined assert 
#if 1 //easy assert
#define assert(statement) if(!(statement)) {printf("Something went wrong at %d", __LINE__); exit(0);}
#else 
#define assert(statement) if(!(statement)) { int *i_ = 0; *i_ = 0; }
#endif
#endif

typedef struct Pool Pool;
typedef struct Pool {
    unsigned char *memory;
    unsigned int count;
    unsigned int indexAt;
    
    unsigned long inValid; //size of pool can only be max 64 slots 
    unsigned int id;  //unique to this array. Just an index;
    Pool *next;
} Pool;

typedef struct ValidIndex ValidIndex; 
typedef struct ValidIndex {
    int index;
    Pool *pool; 
    
    ValidIndex *next;
} Valid;

#define INCREMENT_COUNT 64
typedef struct {
    size_t sizeofType;
    Pool *poolHash[1096]; //should this be a hash table;
    Pool *latestPool; 
    unsigned int poolIdAt;
    
    int count;
    
    ValidIndex *freeIndexes;
    ValidIndex *freeList;
} Array_Dynamic;

Pool *initPool(Array_Dynamic *array, size_t sizeofType, unsigned int id) {
    Pool *pool = calloc(sizeof(Pool), 1);
    
    pool->count = INCREMENT_COUNT;
    size_t initialSizeBytes = sizeofType*pool->count;
    pool->memory = (unsigned char *)malloc(initialSizeBytes);
    memset(pool->memory, 0, initialSizeBytes);
    pool->id = id;
    
    //add it to the the hash table
    Pool **poolHashPtr = array->poolHash + id;
    while(*poolHashPtr) {
        poolHashPtr = &((*poolHashPtr)->next);
    } 
    assert(!(*poolHashPtr));
    
    *poolHashPtr = pool;
    //
    
    return pool;
}

#define initArray(type) initArray_(sizeof(type))
Array_Dynamic initArray_(size_t sizeofType) {
    Array_Dynamic result = {}; 
    result.sizeofType = sizeofType;
    result.latestPool = initPool(&result, sizeofType, result.poolIdAt++);
    return result;
}

bool isElmValid(Pool *pool, int index) {
    bool result = !(pool->inValid & (1 << index));
    return result;
}

#define addElement(array, data) addElement_(array, &data, sizeof(data))

int addElement_(Array_Dynamic *array, void *elmData, size_t sizeofData) {
    
    assert(sizeofData == array->sizeofType);
    
    Pool *pool = array->latestPool;
    int indexAt;
    if(array->freeIndexes) {
        ValidIndex *validIndex = array->freeIndexes; 
        indexAt = validIndex->index;
        pool = validIndex->pool;
        array->freeIndexes = validIndex->next;
        
        assert(pool);
        //put on free list
        validIndex->next = array->freeList;
        array->freeList = validIndex;
        assert(!isElmValid(pool, indexAt));
    } else {
        if(pool->indexAt >= pool->count) {
            //add new pool 
            pool = array->latestPool = initPool(array, array->sizeofType, array->poolIdAt++);
        } 
        //add element
        assert(pool->indexAt < pool->count);
        
        indexAt = pool->indexAt++;
        array->count++;
    }
    
    void *at = ((unsigned char *)pool->memory) + (array->sizeofType*indexAt);
    
    memcpy(at, elmData, array->sizeofType);
    pool->inValid &= (~(1 << indexAt));
    
    int absIndex = (pool->id*INCREMENT_COUNT) + indexAt;
    
    return absIndex;
}

Pool *getPool(Array_Dynamic *array, int poolIndex) {
    //hash table would avoid looking at all the arrays
    Pool *result = array->poolHash[poolIndex % arrayCount(array->poolHash)];
    while(result) {
        if(result->id == poolIndex) {
            break;
        }
        result = result->next;
    }
    
    return result;
}

typedef struct {
    Pool *pool;
    int indexAt;
} PoolInfo;

PoolInfo getPoolInfo(Array_Dynamic *array, int absIndex) {
    int poolAt = absIndex / INCREMENT_COUNT;
    
    PoolInfo result = {};
    result.pool = getPool(array, poolAt);
    result.indexAt = absIndex - (poolAt*INCREMENT_COUNT);
    
    return result;
}

void *getElement(Array_Dynamic *array, unsigned int absIndex) {
    
    void *elm = 0;
    PoolInfo info = getPoolInfo(array, absIndex);
    if(info.pool && info.indexAt < (info.pool->indexAt) && index >= 0 && isElmValid(info.pool, info.indexAt)) {
        
        elm = info.pool->memory + (info.indexAt*array->sizeofType);
    }
    return elm;
}

void removeElement_unordered(Array_Dynamic *array, int absIndex) {
    
    PoolInfo info = getPoolInfo(array, absIndex);
    
    Pool *pool = info.pool;
    int index = info.indexAt;
    if(pool && index < pool->indexAt && index >= 0 && isElmValid(info.pool, info.indexAt)) {
        
        void *elm = pool->memory + (index*array->sizeofType);
        unsigned int lastIndex = --pool->indexAt;
        if(lastIndex != index) {
            assert(index < lastIndex);
            void *elm2 = pool->memory + (lastIndex*array->sizeofType);
            //get element from the end. 
            memcpy(elm, elm2, array->sizeofType);
        }
        pool->inValid |= (1 << info.indexAt);
        array->count--;
    } else {
        invalidCodePathStr("index not valid");
    }
    
}

void removeElement_ordered(Array_Dynamic *array, int absIndex) {
    PoolInfo info = getPoolInfo(array, absIndex);
    if(info.pool && info.indexAt < info.pool->indexAt && info.indexAt >= 0 && isElmValid(info.pool, info.indexAt)) {
        
        ValidIndex *validInd = 0;
        if(array->freeList) {
            //get off free list
            validInd = array->freeList;
            array->freeList = validInd->next;
        } else {
            //alloc new valid index
            validInd = (ValidIndex *)calloc(sizeof(ValidIndex), 1);
        }
        assert(validInd);
        
        //add to valid indexes
        validInd->next = array->freeIndexes;
        array->freeIndexes = validInd;
        
        //assgin info
        validInd->index = info.indexAt;
        validInd->pool = info.pool;
        validInd->pool->inValid |= (1 << info.indexAt);
    }
}

void freeArray(Array_Dynamic *array) {
    //also free all the free lists and other lists.
    assert(!"not implemented");
}


