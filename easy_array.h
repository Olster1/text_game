/*
New File
*/
typedef struct {
    size_t sizeofType;
    void *memory;
    unsigned int count;
} Array_Dynamic;

#define initArray(type) initArray(sizeof(type))

Array_Dynamic initArray(size_t sizeofType) {
    Array 
        memory = calloc(sizeofType)
} 


