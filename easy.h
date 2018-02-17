/*
New File
*/

#if !defined arrayCount
#define arrayCount(array1) (sizeof(array1) / sizeof(array1[0]))
#endif 

#define invalidCodePathStr(msg) { printf(msg); exit(0); }

#if 1 //turn off for crash assert
#define assert(statement) if(!(statement)) {printf("Something went wrong at %d", __LINE__); exit(0);}
#else
#define assert(statement) if(!(statement)) { int *i_ = 0; *i_ = 0; }
#endif

#include <limits.h>

#define PI32 3.14159265359
#define COLOR_NULL v4(0, 0, 0, 0)
#define COLOR_BLACK v4(0, 0, 0, 1)
#define COLOR_WHITE v4(1, 1, 1, 1)
#define COLOR_RED v4(1, 0, 0, 1)
#define COLOR_GREEN v4(0, 1, 0, 1)
#define COLOR_BLUE v4(0, 0, 1, 1)
#define COLOR_YELLOW v4(1, 1, 0, 1)
#define COLOR_PINK v4(0, 1, 1, 1)


#define zeroStruct(memory, type) zeroSize(memory, sizeof(type))
#define zeroArray(array) zeroSize(array, sizeof(array))

void zeroSize(void *memory, size_t bytes) {
    char *at = (char *)memory;
    for(int i = 0; i < bytes; i++) {
        *at = 0;
        at++;
    }
}

typedef struct {
    void *memory;
    int currentSize;
    int totalSize;
} Arena;

#define pushStruct(arena, type) (type *)pushSize(arena, sizeof(type))

#define pushArray(arena, size, type) (type *)pushSize(arena, sizeof(type)*size)

void *pushSize(Arena *arena, size_t size) {
    void *result = ((char *)arena->memory) + arena->currentSize;
    arena->currentSize += size;
    
    zeroSize(result, size);
    return result;
}


bool stringsMatchN(char *a, int aLength, char *b, int bLength) {
    bool result = true;
    
    int indexCount = 0;
    while(indexCount < aLength && indexCount < bLength) {
        indexCount++;
        result &= (*a == *b);
        a++;
        b++;
    }
    result &= (indexCount == bLength && indexCount == aLength);
    
    return result;
} 


bool cmpStrNull(char *a, char *b) {
    bool result = stringsMatchN(a, strlen(a), b, strlen(b));
    return result;
}


size_t getFileSize(FILE *handle) {
    size_t result = 0;
    if(fseek(handle, 0, SEEK_END) == 0) {
        result = ftell(handle);
    }
    
    fseek(handle, 0, SEEK_SET);
    return result;
}
