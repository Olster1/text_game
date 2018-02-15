#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <OpenGl/gl.h>


#include <SDL2/sdl.h>
#include "easy.h"

static char* globalExeBasePath;
static Arena arena;

static float fontHeight = 32;
static float bufferHeight;
static float bufferWidth; 
static float globalSpacing = 20;

static float compassX;
static float compassY;

#include "sdl_audio.h"
#include "easy_opengl.h"
#include "easy_font.h"
#include "main.h"

static Font mainFont;
static Font menuFont;

static WavFile footstepSound;

#define STB_IMAGE_IMPLEMENTATION 
#include "stb_image.h"

GLuint loadImage(char *fileName) {
    GLuint resultId = 0;
    
    int w;
    int h;
    int comp = 4;
    unsigned char* image = stbi_load(fileName, &w, &h, &comp, STBI_rgb_alpha);
    
    if(image) {
        
        glGenTextures(1, &resultId);
        
        glBindTexture(GL_TEXTURE_2D, resultId);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        
        if(comp == 3) {
            image = stbi_load(fileName, &w, &h, &comp, STBI_rgb);
            assert(image);
            assert(comp == 3);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
        } else if(comp == 4) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
        } else {
            assert(!"Channel number not handled!");
        }
        
        glBindTexture(GL_TEXTURE_2D, 0);
        
        stbi_image_free(image);
    } else { 
        assert(!"no image found");
    }
    return resultId;
}

TextAnimationInfo *pushAnimInfo(PrintInfo *printInfo, 
                                V4 startColor, 
                                V4 finishColor,
                                float period) {
    assert(printInfo);
    TextAnimationInfo *animInfo = printInfo->animInfos + printInfo->animCount++;
    animInfo->startColor = startColor;
    animInfo->finishColor = finishColor;
    animInfo->timer.period = period;
    animInfo->useOnce = false;
    return animInfo;
}

void pushAnimInfosInOut(PrintInfo *printInfo, 
                        V4 startColor, 
                        V4 midColor,
                        float period) {
    
    V4 finishColor = midColor;
    finishColor.w = 0;
    startColor.w = 0;
    pushAnimInfo(printInfo, startColor, midColor, 1);
    pushAnimInfo(printInfo, midColor, midColor, period);
    pushAnimInfo(printInfo, midColor, finishColor, 1);
}

V2 getBounds(char *string, Rect2f margin, Font *font) {
    Rect2f bounds  = outputText(font, margin.minX, margin.minY, bufferWidth, bufferHeight, string, margin, v4(0, 0, 0, 1), 1, false);
    
    V2 result = getDim(bounds);
    return result;
}

PrintInfo *pushPrintStruct_(GameState *state, char *string, float minX, float maxX, float minY, float size, V4 startColor, V4 finishColor, float period, LifeSpan lifeSpan, RenderType renderType, V2 begin, V2 end, GLuint texId, Rect2f *rect) {
    PrintInfo *result = 0;
    if(state->printInfoCount < arrayCount(state->printInfos)) {
        result = state->printInfos + state->printInfoCount++; 
        zeroStruct(result, PrintInfo);
        
        result->type = renderType;
        if(renderType == STRING) {
            result->string = string;
            result->size = size;
        } else if(renderType == LINE) {
            result->begin = begin;
            result->end = end;
        } else if(renderType == TEXTURE) {
            result->texId = texId;
            result->texRect = rect;
        }
        
        zeroStruct(&result->margin, Rect2f);
        result->margin.minX = minX;
        result->margin.maxX = maxX;
        result->margin.minY = minY;
        result->margin.maxY = bufferHeight*0.9f;
        result->lifeSpan = lifeSpan;
        
        result->isDynamic = false;
        
        switch(lifeSpan) {
            case BRIEF: {
                pushAnimInfosInOut(result, startColor, finishColor, period);
            } break;
            case DELAYED: {
                pushAnimInfo(result, startColor, finishColor, period);
                result->lifeSpan = BRIEF; // We set the lifespan back to brief for delayed entities since we want them to only cycle through the animations once. 
            } break;
            default: {
                V4 tempColor = startColor;
                tempColor.w = 0;
                TextAnimationInfo *txtInf = pushAnimInfo(result, tempColor, finishColor, 1);
                txtInf->useOnce = true; //just fade in but don't cycle that animation
                pushAnimInfo(result, startColor, finishColor, period);
            }
        }
        
        result->active = true;
    }
    
    return result;
}

PrintInfo *pushPrintStruct(GameState *state, char *string, float minX, float maxX, float minY, float size, V4 startColor, V4 finishColor, float period, LifeSpan lifeSpan) {
    
    return pushPrintStruct_(state, string, minX, maxX, minY, size, startColor, finishColor, period, lifeSpan, STRING, v2(0, 0), v2(0, 0), 0, 0); // NOTE(Oliver): These two V2(0, 0) are for the begin and end string, but aren't used in this case. 
}

PrintInfo *pushLineStruct(GameState *state, V2 begin, V2 end, V4 startColor, V4 finishColor, float period, LifeSpan lifeSpan) {
    return pushPrintStruct_(state, 0, 0, 0, 0, 1, startColor, finishColor, period, lifeSpan, LINE, begin, end, 0, 0); 
}

// TODO(Oliver): Change all the push PrintStruct_ methods to alter the specific info in the wrapper. 
// Instead of sending all the data through to the end function!! 8/2/18
PrintInfo *pushTexStruct(GameState *state, GLuint textureID, Rect2f *rect, V4 startColor, V4 finishColor, float period, LifeSpan lifeSpan, Matrix4 offsetTransform) {
    PrintInfo *printInfo = pushPrintStruct_(state, 0, 0, 0, 0, 1, startColor, finishColor, period, lifeSpan, TEXTURE, v2(0, 0), v2(0, 0), textureID, rect); 
    
    printInfo->offsetTransform = offsetTransform;
    return printInfo;
}

PrintInfo *pushRectOutlineStruct(GameState *state, Rect2f *rect, V4 *color, V4 startColor, V4 finishColor, float period, LifeSpan lifeSpan, Matrix4 offsetTransform) {
    PrintInfo *printInfo = pushPrintStruct_(state, 0, 0, 0, 0, 1, startColor, finishColor, period, lifeSpan, RECT_OUTLINE, v2(0, 0), v2(0, 0), 0, rect); 
    
    printInfo->offsetTransform = offsetTransform;
    printInfo->color = color;
    printInfo->texRect = rect;
    
    return printInfo;
}

PrintInfo *pushPrintStructDefault(GameState *state, char *string, LifeSpan lifeSpan) {
    
    float period = 2;
    PrintInfo *result = pushPrintStruct(state, string, state->posAt.x, state->bounds.maxX, state->posAt.y, 1, v4(0, 0, 0, 1), v4(0, 0, 0, 1), period, lifeSpan);
    
    Rect2f margin = result->margin;
    
    margin.minX *= bufferWidth;
    margin.minY *= bufferHeight;
    margin.maxX *= bufferWidth;
    margin.maxY *= bufferHeight;
    
    V2 stringDim = getBounds(string, margin, &mainFont);
    
    state->posAt.y += (stringDim.y + globalSpacing) / bufferHeight;
    return result;
}

void seperateSentence(char *inputBuffer, String *sentenceParts, int lengthSentenceParts, int *stringCount) {
    
    char *at = inputBuffer;
    
    
    while(*at == ' ') { //trim input buffer of spaces at start
        at++;
    }
    
    String *currentString = 0;
    bool valid = *at;
    while(valid) {
        
        if(*stringCount < lengthSentenceParts) {
            currentString = sentenceParts + *stringCount;
            (*stringCount)++;
            currentString->begin = at;
        } else {
            valid = false;
            break;
        }
        
        while(*at && *at != ' ' && *at != '\n' && *at != '\r') {
            at++;
        } 
        
        if(currentString) { //end current string 
            currentString->length = (int)(at - currentString->begin);
        } else {
            invalidCodePathStr("Error: no current string");
        }
        
        assert(*at == ' ' || *at == '\0' || *at == '\n' || *at == '\r');
        
        while(*at == ' ') {
            at++;
        }
        
        if(*at == '\0' || *at == '\n' || *at == '\r') { valid = false;
            break;
        } 
    }
}


bool stringsMatch(String *a, String *b) {
    bool result = stringsMatchN(a->begin, a->length, b->begin, b->length);
    return result;
}

bool cmpString(char *a, String *b) {
    bool result = stringsMatchN(a, strlen(a), b->begin, b->length);
    return result;
}

bool cmpStringValue(char *a, String b) {
    bool result = stringsMatchN(a, strlen(a), b.begin, b.length);
    return result;
}

char *concat(Arena *arena, char *a, char *b) {
    int lenA = strlen(a);
    int lenB = strlen(b);
    char *result = pushArray(arena, lenA + lenB, char);
    int atIndex = 0;
    
    for(int i = 0; i < lenA; ++i) {
        result[atIndex++] = a[i];
    }
    
    for(int i = 0; i < lenB; ++i) {
        result[atIndex++] = b[i];
    }
    
    result[atIndex] = '\0';
    return result;
}

char *nullTerminate(String str) {
    size_t bytes = (str.length + 1) * sizeof(char);
    char *result = (char *)pushSize(&arena, bytes);
    
    for(int i = 0; i < str.length; ++i) {
        result[i] = str.begin[i];
    }
    result[str.length] = '\0';
    return result;
}

PrintInfoDelayed *pushDelayedPrintStructDefault(PrintInfoDelayedArray *array , GameState *state, char *string, LifeSpan lifeSpan) {
    
    // TODO(Oliver): To we want to turn this into a dynamic array? 
    assert(array->count < arrayCount(array->E));
    
    PrintInfoDelayed *item = array->E + array->count++;
    
    item->state = state;
    item->string = string;
    item->lifeSpan = lifeSpan;
    
    return item;
}

typedef enum {
    NULL_KEYWORD, 
    ROOM, 
    OBJECT, 
    INTERACTION, 
    ATTACKER_UPDATE, 
    CONNECT_ROOMS, 
    SET_FLAG
} LoadFileKeyword;

typedef struct {
    String name;
    Room *room;
} RoomVar;

typedef struct {
    String name;
    Object *object;
} ObjectVar;

bool isSet(Object *obj, ObjectFlag flag) {
    bool result = (obj->flags & flag);
    return result;
}

void setFlag(Object *obj, ObjectFlag flag, bool value) {
    if(value) {
        obj->flags |= flag;
    } else {
        obj->flags &= ~flag;
    }
}

Object *isInRoom(Room *room, String *object) {
    Object *result = 0;
    
    for(Object *roomObj = room->objects;
        roomObj; 
        roomObj = roomObj->next) {
        int cmpValue = cmpString(roomObj->name, object);
        if(cmpValue && roomObj->room == room) {
            result = roomObj;
            break;
        }
    }
    
    return result; 
}

Object *isInInvetory(Player *player, String *object) {
    Object *result = 0;
    for(Object *obj = player->invetory;
        obj; 
        obj= obj->next) {
        if(cmpString(obj->name, object)) {
            result = obj;
            break;
        }
    }
    return result; 
}

CallbackInfo initOpenCallbackInfo() {
    CallbackInfo result = {};
    result.type = OPEN;
    return result;
}


CallbackInfo initCallbackInfoType(CallbackType type) {
    CallbackInfo result = {};
    result.type = type;
    return result;
}


CallbackInfo initHealthCallbackInfo(int value) {
    CallbackInfo result = {};
    result.type = HEALTH;
    result.healthValue = value;
    
    return result;
}


CallbackInfo initMoveDirection(Direction direction) {
    CallbackInfo result = {};
    result.type = MOVE;
    result.direction = direction;
    
    return result;
}


CallbackInfo initInvetoryCallbackInfo(Object *obj, CallbackType type) {
    CallbackInfo result = {};
    result.type = type;
    if(type == INVENTORY_ADD) {
        result.objectToAdd = obj;
    } else if(type == INVENTORY_DROP) {
        result.objectToDrop = obj;
    } else {
        invalidCodePathStr("Error: Wrong type supplied to init invetory callback");
    }
    
    return result;
}

#define addToList(ptr, listPtr) ptr->next = *listPtr; *listPtr = ptr;
static Object *freeObjectList = 0;
void removeFromRoom(Object *objToDrop, Room *roomWeAreIn) {
    assert(objToDrop->room == roomWeAreIn);
    bool found = false;
    //remove from inventory
    for(Object **objPtr = &roomWeAreIn->objects; 
        *objPtr; 
        objPtr = &((*objPtr)->next)) {
        
        Object *obj = *objPtr;
        if(obj == objToDrop) {
            *objPtr = obj->next;
            found = true;
            break;
        }
    }
    assert(found);
    addToList(objToDrop, &freeObjectList);
}

void removeFromInventory(Player *player, Object *objToDrop, Room *roomWeAreIn) {
    
    bool found = false;
    //remove from inventory
    for(Object **objPtr = &player->invetory; 
        *objPtr; 
        objPtr = &((*objPtr)->next)) {
        
        Object *obj = *objPtr;
        if(obj == objToDrop) {
            *objPtr = obj->next;
            found = true;
            break;
        }
    }
    assert(found);
    
    if(roomWeAreIn) {
        //add back to the room object list. 
        addToList(objToDrop, &roomWeAreIn->objects);
        objToDrop->room = roomWeAreIn;
    } else {
        //completely remove from game
        addToList(objToDrop, &freeObjectList);
    }
}

void addToInventory(Player *player, Object *objToDrop) {
    
    bool found = false;
    
    //just a check to see that it was in the room
    for(Object **objPtr = &objToDrop->room->objects; 
        *objPtr; 
        objPtr = &((*objPtr)->next)) {
        
        Object *obj = *objPtr;
        if(obj == objToDrop) {
            *objPtr = obj->next;
            found = true;
            break;
        }
    }
    assert(found);
    //
    
    objToDrop->room = 0;
    addToList(objToDrop, &player->invetory);
    
}

void moveDirection(PrintInfoDelayedArray *delayedArray, GameState *currentState, Room **currentRoomPtr, Direction dir) {
    Room *currentRoom = *currentRoomPtr;
    
    RoomConnection *connection = currentRoom->connections + dir;
    
    Room *newRoom = connection->room;
    
    if(connection->blocked && isSet(connection->blocked, IS_SHUT)) {
        char *bufferStr = pushArray(&arena, 256, char);
        sprintf(bufferStr, "The way seems to be blocked. %s\n", connection->blockedDescr);
        pushDelayedPrintStructDefault(delayedArray, currentState, bufferStr, BRIEF);
        
    } else if(newRoom) {
        newRoom->discovered = true;
        *currentRoomPtr = newRoom;
    } else {
        pushDelayedPrintStructDefault(delayedArray, currentState, "You can't seem to move that way\n", BRIEF);
    }
}


void addAttackerUpdate(Object *object, int beginAttackCount, char *attackString, int attackValue) {
    object->type = ATTACKER;
    object->beginAttackCount = beginAttackCount;
    object->attackCount = beginAttackCount; 
    object->attackString = attackString;
    object->attackValue = attackValue;
    
}

void updateObject(PrintInfoDelayedArray *delayedArray, GameState *currentState, Player *player, Object *object) {
    switch (object->type) {
        case ATTACKER: {
            object->attackCount--;
            if(object->attackCount == 0) {
                object->attackCount = object->beginAttackCount;
                player->health -= object->attackValue;
                char *charBuffer = pushArray(&arena, 256, char);
                sprintf(charBuffer, "%s\n", object->attackString);
                pushDelayedPrintStructDefault(delayedArray, currentState, charBuffer, BRIEF);
            }
        } break;
        default: {
            //printf("no update for object\n");
        }
    }
}

void actionCallBack(PrintInfoDelayedArray *delayedArray, GameState *currentState, Player *player, Interaction *interaction, Room *roomWeAreIn, GameMode *gameMode) {
    CallbackInfo *info = &interaction->callbackInfo;
    Object **objs = interaction->objects;
    Object *obj = objs[0];
    switch (info->type) {
        case HEALTH: {
            assert(interaction->objectCount);
            player->health += info->healthValue;
            if(obj->room == roomWeAreIn) { //remove from room (and world completely!) 
                removeFromRoom(obj, roomWeAreIn);
            } else { //remove from invetory (and world completely!)
                removeFromInventory(player, obj, 0);
            }
        } break;
        case INVENTORY_ADD: {
            assert(info->objectToAdd == objs[0]);
            addToInventory(player, info->objectToAdd);
        } break;
        case INVENTORY_DROP: {
            assert(info->objectToAdd == objs[0]);
            removeFromInventory(player, info->objectToDrop, roomWeAreIn);
        } break;
        case OPEN: {
            assert(interaction->objectCount);
            setFlag(objs[1], IS_SHUT, false);
        } break;
        case MOVE: {
            moveDirection(delayedArray, currentState, interaction->currentRoom, info->direction);
            playSound(&arena, &footstepSound, false);
        } break;
        case QUIT: {
            *gameMode = QUIT_MODE;
        } break;
        case CHECK_INVENTORY: {
            for(Object *obj = player->invetory;
                obj; 
                obj= obj->next) {
                char *charBuffer = pushArray(&arena, 256, char);
                sprintf(charBuffer, "%s\n", obj->description);
                pushDelayedPrintStructDefault(delayedArray, currentState, charBuffer, BRIEF);
                
            }
        } break;
        case SHOW_DIRECTIONS: {
            for(int i = 0; i < arrayCount(roomWeAreIn->connections); ++i) {
                RoomConnection *con = roomWeAreIn->connections + i;
                //Search other rooms for connections -> does it want a different data structure for storing connections per room? Ol 21/1/18
                
                if(con->room) {
                    char *charBuffer = pushArray(&arena, 256, char);
                    sprintf(charBuffer, "To your %s %s\n", directionStrings[i], con->transition);
                    pushDelayedPrintStructDefault(delayedArray, currentState, charBuffer, BRIEF);
                }
            }
        } break;
        case CHECK_HEALTH: {
            char *charBuffer = pushArray(&arena, 256, char);
            sprintf(charBuffer, "Your health is %d\n", player->health);
            pushDelayedPrintStructDefault(delayedArray, currentState, charBuffer, BRIEF);
        } break;
        default: {
            printf("WARNING: case not handle at line %d", __LINE__);
        }
    }
}

Object *initObject(char *name, char *description) {
    Object *o = (Object *)pushSize(&arena, sizeof(Object));
    
    o->name = name;
    o->description = description;
    
    return o;
}

Object *addObject(Room *room, char *name, char *description) {
    Object *o = initObject(name, description);
    o->next = room->objects;
    room->objects = o;
    
    o->room = room;
    return o;
}

Interaction *addInteraction_(InteractionTable *table, SentencePattern pattern,  Object **objects, int objectCount, ActionType *verbs, int verbCount, NounInfo *nouns, int nounInfoCount, char *response, CallbackInfo info) {
    
    Interaction *interaction = table->interactions + table->count++;
    
    interaction->pattern = pattern;
    
    for(int i = 0; i < objectCount; ++i) {
        interaction->objects[interaction->objectCount++] = objects[i];
    }
    
    for(int i = 0; i < verbCount; ++i) {
        interaction->verbs[interaction->verbCount++] = verbs[i];
    }
    
    
    for(int i = 0; i < nounInfoCount; ++i) {
        interaction->nounSearchInfo[interaction->nounInfoCount++] = nouns[i];
    }
    
    
    interaction->response = response;
    interaction->callbackInfo = info;
    return interaction;
}

void addInteractionVO(InteractionTable *table, Object *obj, ActionType type, char *response, CallbackInfo info) {
    assert(obj);
    assert(type);
    assert(info.type);
    Object *objects[1] = {obj};
    ActionType verbs[1] = {type};
    addInteraction_(table, VN, 
                    objects, 1, 
                    verbs, 1, 0, 0, response, info);
}

NounInfo initNounInfo(char *name) {
    NounInfo result = {};
    result.name.begin = name;
    result.name.length = strlen(name);
    return result;
}

void addInteractionVN(InteractionTable *table, char *noun, ActionType type, char *response, CallbackInfo info) {
    
    NounInfo nounInfo[1] = {initNounInfo(noun)};
    ActionType verbs[1] = {type};
    addInteraction_(table, VN, 
                    0, 0, 
                    verbs, 1, 
                    nounInfo, 1, response, info);
}


void addInteractionN(InteractionTable *table, char *noun, char *response, CallbackInfo info) {
    
    NounInfo nounInfo[1] = {initNounInfo(noun)};
    addInteraction_(table, N, 
                    0, 0, 
                    0, 0, 
                    nounInfo, 1, response, info);
}

void addInteractionVOVO(InteractionTable *table, Object *obj1, Object *obj2, ActionType type1, ActionType type2,  char *response, CallbackInfo info, char *soundToPlay) {
    
    Object *objects[2] = {obj1, obj2};
    ActionType verbs[2] = {type1, type2};
    Interaction *inter = addInteraction_(table, VNVN, 
                                         objects, 2, 
                                         verbs, 2, 0, 0, response, info);
    inter->soundResponse = soundToPlay;
}

Interaction *findInteraction(InteractionTable *table,
                             Interaction *info) { //use the same type of interaction for the search info -> maybe we fill out the info?
    
    Interaction *result = 0;
    
    bool found = false;
    for(int tableIndex = 0; tableIndex < table->count && !found; ++tableIndex) {
        Interaction *interaction = table->interactions + tableIndex;
        bool valid = true;
        //initial search space
        
        if(interaction->pattern == info->pattern &&
           interaction->objectCount == info->objectCount &&
           interaction->verbCount == info->verbCount &&
           interaction->nounInfoCount == info->nounInfoCount) {
            
            for(int i = 0; i < info->verbCount; ++i) {
                valid &= (interaction->verbs[i] == info->verbs[i]);
            }
            
            for(int i = 0; i < info->objectCount; ++i) {
                valid &= (interaction->objects[i] == info->objects[i]);
            }
            
            for(int i = 0; i < info->nounInfoCount; ++i) {
                
                valid &= (stringsMatch(&interaction->nounSearchInfo[i].name, &info->nounSearchInfo[i].name));
            }
            
            if(valid) {
                result = interaction;
                found = true;
                break;
            }
        }
    }
    
    return result;
}

Direction getOpposite(Direction direction) {
    Direction direction1 = NORTH;
    if(direction == NORTH) { direction1 = SOUTH; } else if(direction == SOUTH) { direction1 = NORTH; }else if(direction == EAST) { direction1 = WEST; }else if(direction == WEST) { direction1 = EAST; }
    return direction1;
}

bool connectRooms(Room *room1, char *room2ToRoom1, Room *room2, char *room1ToRoom2, Direction direction, Object *blocker, char *blockedDescrip) {
    assert(!blocker || isSet(blocker, IS_SHUT));
    
    bool connectedRooms = false;
    RoomConnection *con1 = room1->connections + direction;
    if(!con1->room) {
        connectedRooms = true;
        con1->transition = room2ToRoom1;
        con1->room = room2;
        con1->blocked = blocker;
        con1->blockedDescr = blockedDescrip;
        
        Direction oppCon = getOpposite(direction);
        
        con1 = room2->connections + oppCon;
        con1->transition = room1ToRoom2;
        con1->room = room1;
        con1->blocked = blocker;
        con1->blockedDescr = blockedDescrip;
    }
    return connectedRooms;
}

Room *addRoom(char *name, char *description) {
    
    Room *room = (Room *)pushSize(&arena, sizeof(Room));
    room->description = description;
    room->name = name;
    room->objects = 0;
    return room;
    
}

void addVerb(Interaction *info, ActionType type) {
    info->verbs[info->verbCount++] = type;
}


void addObjectToInfo(Interaction *info, Object *object) {
    info->objects[info->objectCount++] = object;
}


void addNounInfo(Interaction *info, String name, bool inBackpack, bool inRoom) {
    NounInfo *nounInfo  = info->nounSearchInfo + info->nounInfoCount++;
    nounInfo->name = name;
    nounInfo->inRoom = inRoom;
    nounInfo->inBackpack = inBackpack;
}

typedef struct {
    int verbCount;
    int nounCount;
    SentencePattern pattern;
    
} MatchItem;

SentencePattern matchPattern(Interaction *info) {
    MatchItem matches[] = {
        {0, 1, N}, 
        {1, 1, VN},
        {2, 2, VNVN},
        {3, 3, VNVNVN},
        {1, 0, V},
    };
    
    SentencePattern patternFound = NULL_PATTERN;
    for(int i = 0; i < arrayCount(matches); ++i) {
        MatchItem *item = matches + i;
        
        if(item->verbCount == info->verbCount &&
           item->nounCount == info->nounInfoCount) {
            patternFound = item->pattern;
        }
    }
    
    return patternFound;
}

bool parseString(bool inQuotation, char *at) {
    bool result = true;
    if(!inQuotation) {
        if(*at == '\n' || *at == ' ') {
            result = false;
        }
    } 
    
    return result;
}

String copyString(String *str) {
    String result = {};
    
    size_t bytes = str->length*sizeof(char);
    result.begin = (char *)pushSize(&arena, bytes);
    
    for(int i = 0; i < str->length; ++i) {
        result.begin[i] = str->begin[i];
    }
    
    result.length = str->length;
    return result;
}

void savePlayerData(InteractionTable *table, char *fileName, GameState *currentState) {
    char dataBuffer[2056] = {};
    char *at = dataBuffer;
    
    for(Object *obj = table->player->invetory;
        obj; 
        obj= obj->next) {
        
        at += sprintf(at, "%s\n", obj->description);
        assert(at <= (dataBuffer + sizeof(dataBuffer)));
    }
    
    printf("SAVING FILE\n");
    FILE *fileHandle = fopen(fileName, "w+");
    fwrite(dataBuffer, 1, (int)(at - dataBuffer), fileHandle);
    
}

bool isCommentToken(char *at) {
    bool result = at[0] == '/' && at[1] == '/';
    return result;
}

void loadGameDataFile(InteractionTable *table, char *fileName) {
    FILE *fileHandle = fopen(fileName, "r+");
    
    printf("LOADING FILE\n");
    size_t fileSize = getFileSize(fileHandle);
    char *memory = (char *)malloc(fileSize + 2); //plus 2 for null terminator and colon to parse correctly
    //printf("filesize = %zu\n", fileSize);
    memory[fileSize] = ':';
    memory[fileSize + 1] = '\0';
    
    int lineNumber = 0;
    if(fread(memory, 1, fileSize, fileHandle) == fileSize) {
        bool parsing = true;
        char *at = memory;
        
        
        String commandStrings[32] = {};
        int stringCount = 0;
        ObjHolder holder = {}; 
        RoomVar roomVars[16] = {};
        int roomVarCount = 0;
        
        Object*latestObject = 0;
        ObjectVar objectVars[16] = {};
        int objectVarCount = 0;
        
        while(parsing) {
            //make comments cleaner
            bool inComment = isCommentToken(at);
            
            while(*at != '\0' && (*at == '\n' || *at == ' ' || inComment || (isCommentToken(at)))) {
                if(*at == '\n') {
                    inComment = false;
                    lineNumber++;
                }
                
                if(!inComment) {
                    inComment = isCommentToken(at);
                }
                
                at++;
            }
            if(*at == '\0') {
                parsing = false;
                break;
            } else {
                if(*at == ':') {
                    at++;
                    //act on collected data
                    LoadFileKeyword keyWord = NULL_KEYWORD;
                    bool valid = true; //this is to say keep searching through the strings ------ most things will turn the valid to false and pass the rest of the string.   
                    for(int strIndex = 0; strIndex < stringCount && valid; ++strIndex) {
                        String *str = commandStrings + strIndex;
#if 0
                        
                        for(int strIndex = 0; strIndex < stringCount; ++strIndex) {
                            
                            String *str1 = commandStrings + strIndex;
                            printf("%.*s \n", str1->length, str1->begin);
                            
                        }
                        printf("\n");
#endif
                        
                        if(cmpString("room", str)) {
                            bool roomIsVar = false;
                            if(stringCount == 4) {
                                roomIsVar = true;
                            }
                            char *name = nullTerminate(commandStrings[2]);
                            char *descrip = nullTerminate(commandStrings[3]);
                            holder.room = addRoom(name, descrip);
                            holder.type = HOLDER_ROOM;
                            if(!table->firstRoom) {
                                table->firstRoom = holder.room;
                            }
                            if(roomIsVar) {
                                RoomVar *var = roomVars + roomVarCount++;
                                var->name = copyString(&commandStrings[1]);
                                var->room = holder.room;
                            }
                            valid = false;
                            break;
                            
                        } else if(cmpString("player", str)) {
                            
                            int health = atoi(nullTerminate(commandStrings[1]));
                            table->player->health = health;
                            
                            holder.type = HOLDER_PLAYER;
                            holder.player = table->player;
                            valid = false;
                            break;
                            
                        } else if(cmpString("currentRoom", str)) {
                            Room *newRoom = 0;
                            char *roomVarName = nullTerminate(commandStrings[1]);
                            for(int varIndex = 0; varIndex < roomVarCount; ++varIndex) {
                                RoomVar *roomVar = roomVars + varIndex;
                                if(cmpStringValue(roomVarName,  roomVar->name)) {
                                    newRoom= roomVar->room;
                                    break;
                                } 
                            }
                            if(newRoom) {
                                table->firstRoom = newRoom;
                            } else {
                                printf("No room variable named \"%s\". Please specify one above. At line number: %d\n", roomVarName, lineNumber);
                            }
                            valid = false;
                            break;
                            
                        } else if(cmpString("object", str)) {
                            bool objectIsVar = false;
                            int descripIndex = 2;
                            int nameIndex = 1;
                            if(stringCount == 4) {
                                objectIsVar = true;
                                descripIndex++;
                                nameIndex++;
                            }
                            
                            char *descrip = nullTerminate(commandStrings[descripIndex]);
                            char *name = nullTerminate(commandStrings[nameIndex]);
                            
                            if(!holder.room) {
                                printf("No room set. Add room in above lines. At line number: %d\n", lineNumber);
                            } else {
                                if(holder.type == HOLDER_PLAYER) {
                                    
                                    latestObject = initObject(name, descrip);
                                    
                                    addToList(latestObject, &table->player->invetory);
                                    
                                } else if(holder.type == HOLDER_ROOM) {
                                    latestObject = addObject(holder.room, name, descrip);
                                }
                            }
                            if(objectIsVar) {
                                ObjectVar *var = objectVars + objectVarCount++;
                                var->name = copyString(&commandStrings[1]);
                                var->object = latestObject;
                            }
                            valid = false;
                            break;
                        } else if(cmpString("interaction", str)) {
                            
                            if(stringCount < 3) {
                                printf("not enough parameters passed to interaction. At line number: %d\n", lineNumber);
                            } else {
                                char *descrip = nullTerminate(commandStrings[2]);
                                bool VO = false;
                                if(!latestObject) {
                                    printf("Not object set. Add object in above lines. At line number: %d\n", lineNumber);
                                } else {
                                    ActionType interactionType = NULL_ACTION;
                                    CallbackInfo callBackInfo = {};
                                    if(cmpString("TAKE", &commandStrings[1])) {
                                        callBackInfo = initInvetoryCallbackInfo(latestObject, INVENTORY_ADD);
                                        VO = true;
                                        interactionType = TAKE;
                                    } else if(cmpString("DROP", &commandStrings[1])) {
                                        callBackInfo = initInvetoryCallbackInfo(latestObject, INVENTORY_DROP);
                                        VO = true;
                                        interactionType = DROP;
                                    } else if(cmpString("EAT", &commandStrings[1])) {
                                        if(stringCount < 4) {
                                            printf("not enough paramters passed for health interaction, At line number: %d\n", lineNumber);
                                        } else {
                                            callBackInfo = initHealthCallbackInfo(atoi(nullTerminate(commandStrings[3])));
                                            VO = true;
                                            interactionType = EAT;
                                        }
                                    } 
                                    
                                    if(VO) {
                                        addInteractionVO(table, latestObject, interactionType, descrip, callBackInfo);
                                    } else {
                                        CallbackInfo callBackInfo = {}; 
                                        char *soundToPlay = 0;
                                        if(stringCount > 7) {
                                            soundToPlay = nullTerminate(commandStrings[7]);
                                        }
                                        if(cmpString("OPEN", &commandStrings[6])) {
                                            callBackInfo = initOpenCallbackInfo();
                                        }
                                        ActionType actionType1, actionType2 = NULL_ACTION;
                                        if(cmpString("USE", &commandStrings[1])) {
                                            actionType1 = USE;
                                        }
                                        if(cmpString("ON", &commandStrings[2])) {
                                            actionType2 = ON;
                                        }
                                        Object *obj1 = 0;
                                        Object *obj2 = 0;
                                        for(int varIndex = 0; varIndex < objectVarCount; ++varIndex) {
                                            ObjectVar *objVar = objectVars + varIndex;
                                            if(cmpStringValue(nullTerminate(commandStrings[3]),  objVar->name)) {
                                                obj1 = objVar->object;
                                            }
                                            if(cmpStringValue(nullTerminate(commandStrings[4]),  objVar->name)) {
                                                obj2 = objVar->object;
                                            }
                                        }
                                        
                                        if(stringCount > 5) {
                                            char *descrip = nullTerminate(commandStrings[5]);
                                            
                                            
                                            addInteractionVOVO(table, obj1, obj2, actionType1, actionType2, descrip, callBackInfo, soundToPlay);
                                        } else {
                                            printf("not enough parameters passed to extensive interaction. At line number: %d\n", lineNumber);
                                        } 
                                    };
                                }
                                valid = false;
                                break;
                            }
                        } else if(cmpString("attacker", str)) {
                            if(!latestObject) {
                                printf("Not object set. Add object in above lines. At line number: %d\n", lineNumber);
                            } else {
                                int turnCount = atoi(nullTerminate(commandStrings[1]));
                                char *attackString = nullTerminate(commandStrings[2]);
                                int damage = atoi(nullTerminate(commandStrings[3]));
                                addAttackerUpdate(latestObject, turnCount, attackString, damage);
                            }
                            valid = false; //don't parse rest of strings
                        } else if(cmpString("connectRooms", str)) {
                            valid = false;
                            Room *rm1 = 0;
                            Room *rm2 = 0;
                            
                            for(int varIndex = 0; varIndex < roomVarCount && (!rm2 || !rm1); ++varIndex) {
                                RoomVar *roomVar = roomVars + varIndex;
                                if(cmpStringValue(nullTerminate(commandStrings[1]),  roomVar->name)) {
                                    rm1 = roomVar->room;
                                } else if(cmpStringValue(nullTerminate(commandStrings[2]),  roomVar->name)) {
                                    rm2 = roomVar->room;
                                }
                            }
                            
                            if(rm1 && rm2) {
                                bool directionValid = true;
                                char *descrip1 = nullTerminate(commandStrings[3]);
                                char *descrip2 = nullTerminate(commandStrings[4]);
                                Direction direction = NULL_DIRECTION;
                                if(cmpString("NORTH", &commandStrings[5])) {
                                    direction = NORTH;
                                } else if(cmpString("EAST", &commandStrings[5])) {
                                    direction = EAST;
                                } else if(cmpString("WEST", &commandStrings[5])) {
                                    direction = WEST;
                                } else if(cmpString("SOUTH", &commandStrings[5])) {
                                    direction = SOUTH;
                                } else {
                                    printf("no direction passed to connect rooms. Did you misspell it?. At line number: %d\n", lineNumber);
                                    directionValid = false;
                                }
                                
                                if(directionValid) {
                                    Object *blocker = 0;
                                    char *blockerString = 0;
                                    if(stringCount > 6) {
                                        for(int varIndex = 0; varIndex < objectVarCount; ++varIndex) {
                                            ObjectVar *objVar = objectVars + varIndex;
                                            if(cmpStringValue(nullTerminate(commandStrings[6]),  objVar->name)) {
                                                blocker = objVar->object;
                                                assert(blocker);
                                            }
                                        }
                                        blockerString = nullTerminate(commandStrings[7]);
                                    } 
                                    connectRooms(rm1, descrip1, rm2, descrip2, direction, blocker, blockerString);
                                } else {
                                    printf("direction not valid. At line number: %d\n", lineNumber);
                                }
                            } else {
                                printf("couldn't find rooms. At line number: %d\n", lineNumber);
                            }
                        } else if(cmpString("setFlag", str)) {
                            valid = false;
                            if(stringCount < 4) {
                                printf("not enough flag paramters passed. At line number: %d\n", lineNumber);
                            } else {
                                ObjectFlag flag = NULL_FLAG;
                                Object *obj = 0;
                                if(cmpString("IS_SHUT", &commandStrings[2])) {
                                    flag = IS_SHUT;
                                }
                                for(int varIndex = 0; varIndex < objectVarCount; ++varIndex) {
                                    ObjectVar *objVar = objectVars + varIndex;
                                    if(cmpStringValue(nullTerminate(commandStrings[1]),  objVar->name)) {
                                        obj = objVar->object;
                                    }
                                }
                                if(!obj) {
                                    obj = latestObject;
                                }
                                bool value = (bool)atoi(nullTerminate(commandStrings[3]));
                                
                                setFlag(obj, flag, value);
                            }
                        } else {
                            
                            printf("couldn't match '%.*s' at line %d\n", str->length, str->begin, lineNumber);
                        }
                    }
                    stringCount = 0;
                } else if(stringCount < arrayCount(commandStrings)) {
                    String *str = commandStrings + stringCount++;
                    str->begin = (*at == '\"') ? at + 1 : at;
                    bool inQuotation = false;
                    while(*at != '\0' && *at != ':' && parseString(inQuotation, at)) {
                        if(*at == '\"') {
                            inQuotation = !inQuotation;
                        }
                        at++;
                    }
                    str->length = (int)(at - str->begin);
                    if(str->begin[str->length - 1] == '\"') {
                        str->length--;
                    }
                } else {
                    printf("Too many strings on line\n");
                }
                
            }
        }
    } else {
        printf("Couldn't read file \"%s\"\n", fileName);
    }
    
    fclose(fileHandle);
    
}

void processTurn(GameState *currentGameState, InteractionTable *table, Room **currentRoom, InputBuffer *inputBuffer, Player *player, GameMode *gameMode, 
                 PrintInfoDelayedArray *delayedArray) {
    
    String sentenceParts[32] = {};
    int stringCount = 0;
    
    seperateSentence(inputBuffer->chars, sentenceParts, arrayCount(sentenceParts), &stringCount);
    
#define PRINT_WORDS 0
#if PRINT_WORDS
    printf("%d\n", stringCount);
    for(int i = 0; i < stringCount; ++i) {
        printf("String: %.*s\n", sentenceParts[i].length, sentenceParts[i].begin);
    }
#endif
    
    bool inRoom = false;
    bool inBackpack = false;
    
    Interaction interactionInfo = {};
    
    //Find Pattern
    for(int wordAt = 0; wordAt < stringCount; ++wordAt) { //parse sentence
        
        String *currentWord = sentenceParts + wordAt;
        
        if(cmpString("take", currentWord)) {
            addVerb(&interactionInfo, TAKE);
            inRoom = true;
            inBackpack = false;
        } else if(cmpString("drop", currentWord)) {
            addVerb(&interactionInfo, DROP);
            inBackpack = true;
            inRoom = false;
        } else if(cmpString("talk", currentWord)) {
            addVerb(&interactionInfo, TALK);
            inRoom = true;
            inBackpack = false;
        } else if(cmpString("walk", currentWord)) {
            addVerb(&interactionInfo, WALK);
            inRoom = false;
            inBackpack = false;
        } else if(cmpString("use", currentWord)) {
            addVerb(&interactionInfo, USE);
            inBackpack = true;
            inRoom = true;
        } else if(cmpString("eat", currentWord)) {
            addVerb(&interactionInfo, EAT);
            inBackpack = true;
            inRoom = true;
        } else if(cmpString("on", currentWord)) {
            addVerb(&interactionInfo, ON);
            inBackpack = true;
            inRoom = true;
            printf("ON\n");
        } else { //Not a verb
            addNounInfo(&interactionInfo, *currentWord, inBackpack, inRoom);
        }
    }
    
    interactionInfo.pattern = matchPattern(&interactionInfo);
    
    Interaction tempInteraction = {};
    
    for(int nounIndex = 0; nounIndex < interactionInfo.nounInfoCount; ++nounIndex) {
        
        NounInfo nounInfo = interactionInfo.nounSearchInfo[nounIndex];
        
        Object *objToActOn = 0;
        if(nounInfo.inRoom) { //Search room
            objToActOn = isInRoom(*currentRoom, &nounInfo.name);
        } 
        
        if(!objToActOn && nounInfo.inBackpack) { //Search backpack
            objToActOn = isInInvetory(player, &nounInfo.name);
        }
        
        if(objToActOn) {
            addObjectToInfo(&interactionInfo, objToActOn);
        } else {
            tempInteraction.nounSearchInfo[tempInteraction.nounInfoCount++] = nounInfo;
        }
        
    }
    
    for(int nounIndex = 0; nounIndex < interactionInfo.nounInfoCount; ++nounIndex) {
        
        NounInfo nounInfo = interactionInfo.nounSearchInfo[nounIndex];
        
        interactionInfo.nounSearchInfo[nounIndex] = tempInteraction.nounSearchInfo[nounIndex];
        
    }
    interactionInfo.nounInfoCount = tempInteraction.nounInfoCount;
    
    Interaction *thisInteraction = findInteraction(table, &interactionInfo);
    
    if(thisInteraction) {
        
        thisInteraction->currentRoom = currentRoom;
        
        if(thisInteraction->response) {
            char *charBuffer = pushArray(&arena, 256, char);
            sprintf(charBuffer, "%s\n", thisInteraction->response);
            pushDelayedPrintStructDefault(delayedArray, currentGameState, charBuffer, BRIEF);
        }
        
        if(thisInteraction->soundResponse) {
            WavFile *sound = findSound(thisInteraction->soundResponse);
            if(sound) {
                playSound(&arena, sound, false);
            } else {
                printf("couldn't find sound named %s. Is it loaded?", thisInteraction->soundResponse);
            }
        }
        
        
        actionCallBack(delayedArray, currentGameState, player, thisInteraction, *currentRoom, gameMode);
        
        for(Object *roomObj = (*currentRoom)->objects;
            roomObj; 
            roomObj = roomObj->next) {
            
            updateObject(delayedArray, currentGameState, player, roomObj);
        }
        
        if(player->health <= 0) {pushDelayedPrintStructDefault(delayedArray, currentGameState, "You have lost all your health\n", BRIEF);
            *gameMode= DIED_MODE;
        }
        
    } else {
        pushDelayedPrintStructDefault(delayedArray, currentGameState, "No Interaction Found\n", BRIEF);
    }
}

void OpenGlAdjustScreenDim(int width, int height) {
    glMatrixMode(GL_PROJECTION); 
    
    float a = 2.0f / (float)width; 
    float b = 2.0f / (float)height;
    float ProjMat[] = {
        a,  0,  0,  0,
        0,  b,  0,  0,
        0,  0,  1,  0,
        -1, -1, 0,  1
    };
    
    glLoadMatrixf(ProjMat);
    
}

void enableOpenGl(int width, int height) {
    glViewport(0, 0, width, height);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);//Non premultiplied alpha textures! 
    //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);//Premultiplied alpha textures! 
    
    
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW); 
    glLoadIdentity();
    OpenGlAdjustScreenDim(width, height);
    
}

void resetPrintStrctures(GameState *state) {
    for(int printStructCount = 0;
        printStructCount < state->printInfoCount;
        printStructCount++) {
        PrintInfo *info = state->printInfos + printStructCount++;
        info->active = true;
        info->animAt = 0;
    }
}

void splice(InputBuffer *buffer, char *string, int addOrRemove) {
    char tempChars[INPUT_BUFFER_SIZE] = {};
    int tempCharCount = 0;
    
    char *at = string;
    
    //copy characters
    for(int i = buffer->cursorAt; i < buffer->length; ++i) {
        tempChars[tempCharCount++] = buffer->chars[i]; 
    }
    
    while(*at) {
        
        if(addOrRemove > 0) { //adding
            
            if(buffer->length < (sizeof(buffer->chars) - 1)) {
                
                buffer->chars[buffer->cursorAt++] = *at;
                buffer->length++;
            }
            
        } else {
            if(buffer->length) {
                buffer->chars[buffer->cursorAt--] = *at;
                buffer->length--;
            }
        }
        
        at++;
    }
    //replace characters
    for(int i = 0; i < tempCharCount; ++i) {
        buffer->chars[buffer->cursorAt + i] = tempChars[i]; 
    }
    assert(buffer->length < arrayCount(buffer->chars));
    buffer->chars[buffer->length] = '\0'; //null terminate buffer
}


typedef struct {
    char *name;
    V2 pos;
    Direction direction;
} CompassInfo;

V2 drawCompass(GameState *state, Room *currentRoom, float x, float y, Rect2f margin,  float compassSideLen, bool pushRoomInfo) { 
    
    margin.minX *= bufferWidth;
    margin.minY *= bufferHeight;
    margin.maxX *= bufferWidth;
    margin.maxY *= bufferHeight;
    
    // NOTE(Oliver): x & y are the top left corner. 
    compassX = x;
    compassY = y;
    V2 beginA = v2(x, y + compassSideLen); //WEST 
    V2 endA = v2(x + 2*compassSideLen, y + compassSideLen); //EAST 
    
    V2 beginB = v2(x + compassSideLen, y); //NORTH 
    V2 endB = v2(x + compassSideLen, y + 2*compassSideLen); //SOUTH 
    
    V2 screenDim = v2(bufferWidth, bufferHeight);
    
    pushLineStruct(state, v2_inverseHadamard(beginA, screenDim), v2_inverseHadamard(endA, screenDim), COLOR_BLACK, COLOR_BLACK, 1, TURN_BASED);
    
    
    pushLineStruct(state, v2_inverseHadamard(beginB, screenDim), v2_inverseHadamard(endB, screenDim), COLOR_BLACK, COLOR_BLACK, 1, TURN_BASED);
    
    CompassInfo roomNames[4] = {
        {0, beginA, WEST},
        {0, beginB, NORTH},
        {0, endA, EAST},
        {0, endB, SOUTH},
    };
    
    // NOTE(Oliver): start at 1 to account for the null direction
    for(int i = 0; i < arrayCount(roomNames); ++i) {
        CompassInfo *compassInfo = roomNames + i;
        
        RoomConnection *con = currentRoom->connections + compassInfo->direction;
        
        compassInfo->name = 0;
        if(con->room) {
            if(con->room->discovered) {
                compassInfo->name = con->room->name;
            } else {
                compassInfo->name = "?";
            }
            
            //adjust for writing offset. 
            V2 bounds = getBounds(compassInfo->name, margin, &mainFont);
            switch(compassInfo->direction) {
                case NORTH: {
                    compassInfo->pos.x -= (bounds.x/2);
                    compassInfo->pos.y -= (bounds.y/2);
                } break;
                case EAST: {
                    compassInfo->pos.y += (bounds.y/2);
                    compassInfo->pos.x += 5;
                } break;
                case SOUTH: {
                    compassInfo->pos.y += (bounds.y);
                    compassInfo->pos.x -= (bounds.x/2); 
                    endB = v2(max(endB.x, compassInfo->pos.x), max(endB.y, compassInfo->pos.y) + 10);
                } break;
                case WEST: {
                    compassInfo->pos.x -= bounds.x + 10;
                    compassInfo->pos.y += (bounds.y/2); 
                } break;
                default: {
                    printf("ERROR: No Direction specified\n");
                }
            }
        }
    }
    
    if(pushRoomInfo) {
        for(int i = 0; i < arrayCount(roomNames); ++i) {
            CompassInfo *roomInfo = roomNames + i;
            if(roomInfo->name) {
                V2 at = roomInfo->pos;
                pushPrintStruct(state, roomInfo->name, at.x/bufferWidth, 1, at.y/bufferHeight, 1, COLOR_BLACK, COLOR_BLACK, 1, TURN_BASED);
            }
        }
    }
    V2 result = v2(x / bufferWidth, (endB.y + globalSpacing + 16) / bufferHeight);
    return result;
}

void setItemWindow(float yMin, Rect2f *objectsWindow) {
    objectsWindow->minY = yMin;
    objectsWindow->minX = 0.18f*bufferWidth;
    objectsWindow->maxX = 0.9f*bufferWidth;
    objectsWindow->maxY = objectsWindow->minY + 0.3f*bufferHeight;
}

static float globalCompassLen = 50;
void describeRoom(Room *currentRoom, GameState *currentGameState, Rect2f *objectsWindow, GLuint scrollTexId, Rect2f *scrollHandleRect, V4 *scrollHandleColor) {
    //describe room
    char *charBuffer = pushArray(&arena, 256, char);
    sprintf(charBuffer, "%s\n", currentRoom->description);
    pushPrintStructDefault(currentGameState, charBuffer, TURN_BASED);
    
    //printf("%f\n", currentGameState->bounds.maxX);
    V2 comPos = v2_hadamard(v2(bufferWidth, bufferHeight), currentGameState->posAt);
    currentGameState->posAt = drawCompass(currentGameState, currentRoom, comPos.x, comPos.y, currentGameState->bounds, globalCompassLen, true);
    
    setItemWindow((currentGameState->posAt.y * bufferHeight) - fontHeight, objectsWindow);
    
#if 0 // if we want a texture background for the window. 
    pushTexStruct(currentGameState, scrollTexId, objectsWindow, v4(1, 1, 1, 1), v4(1, 1, 1, 1), 4, TURN_BASED, mat4TopLeftToBottomLeft(bufferHeight)); 
#endif
#if 1 //if we want the scroll handle
    pushRectOutlineStruct(currentGameState, scrollHandleRect, scrollHandleColor, v4(1, 1, 1, 1), v4(1, 1, 1, 1), 4, TURN_BASED, mat4TopLeftToBottomLeft(bufferHeight)); 
#endif
    //printf("%s\n", charBuffer);
    //list all objects in the current room
    for(Object *obj = currentRoom->objects;
        obj; 
        obj = obj->next) {
        assert(obj->room == currentRoom);
        char *charBufferObj = pushArray(&arena, 256, char);
        sprintf(charBufferObj, "%s\n", obj->description);
        PrintInfo *printInfo = pushPrintStructDefault(currentGameState, charBufferObj, TURN_BASED);
        printInfo->isDynamic = true;
        
    }
    
    
}

void removePrintInfo(GameState *state, int index) {
    state->printInfos[index] = state->printInfos[--state->printInfoCount];
}

void setForRemoval(PrintInfo *info) {
    info->lifeSpan = BRIEF;
    pushAnimInfo(info, 
                 v4(0, 0, 0, 1), 
                 v4(0, 0, 0, 0),
                 1);
    info->animAt = info->animCount - 1; //set animation to final animation
}

void sdlProcessGameKey(GameButton *button, bool isDown, bool repeated) {
    button->isDown = isDown;
    if(!repeated) {
        button->transitionCount++;
    }
}

int updateMenu(MenuOptions *menuOptions, GameButton *gameButtons, int menuCursorAt) {
    if(wasPressed(gameButtons, BUTTON_DOWN)) {
        menuCursorAt++;
        if(menuCursorAt >= menuOptions->count) {
            menuCursorAt = 0;
        }
    } 
    
    if(wasPressed(gameButtons, BUTTON_UP)) {
        menuCursorAt--;
        if(menuCursorAt < 0) {
            menuCursorAt = menuOptions->count - 1;
        }
    }
    return menuCursorAt;
}

void renderMenu(GLuint backgroundTexId, MenuOptions *menuOptions, int menuCursorAt) {
    
    char *titleAt = menuOptions->options[menuCursorAt];
    
    openGlTextureCentreDim(backgroundTexId, v2(0.5f*bufferWidth, 0.5f*bufferHeight), v2(bufferWidth, bufferHeight), v4(1, 1, 1, 1), 0, mat4());
    float yIncrement = bufferHeight / (menuOptions->count + 1);
    Rect2f menuMargin = rect2f(0, 0, bufferWidth, bufferHeight);
    
    float xAt_ = (bufferWidth / 2);
    float yAt = yIncrement;
    for(int menuIndex = 0;
        menuIndex < menuOptions->count;
        ++menuIndex) {
        V4 menuItemColor = COLOR_BLUE;
        
        if(menuIndex == menuCursorAt) {
            menuItemColor = COLOR_RED;
        }
        
        char *title = menuOptions->options[menuIndex];
        float xAt = xAt_ - (getBounds(title, menuMargin, &menuFont).x / 2);
        outputText(&menuFont, xAt, yAt, bufferWidth, bufferHeight, title, menuMargin, menuItemColor, 1, true);
        yAt += yIncrement;
    }
}

PrintInfo *pushPrintStructWithDelay(GameState *state, char *string, float minX, float maxX, float minY, float size, V4 startEndColor, V4 MiddleColor, float delay, float period) {
    
    PrintInfo *printInfo = pushPrintStruct(state, string, minX, maxX, minY, size, COLOR_NULL, COLOR_NULL, delay, DELAYED);
    
    pushAnimInfo(printInfo, 
                 startEndColor, 
                 MiddleColor,
                 period);
    
    pushAnimInfo(printInfo, 
                 MiddleColor, 
                 MiddleColor,
                 period);
    
    pushAnimInfo(printInfo, 
                 MiddleColor,
                 startEndColor,
                 period);
    
    return printInfo;
}


int main(int argc, char *args[]) {
    SDL_Window *windowHandle = 0;
    
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 2 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 3 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    
    //SDL_GL_CONTEXT_PROFILE_COMPATIBILITY
    
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    
    float scale = 1.5f;
    int width = (int)(scale*640);
    int height = (int)(scale*480);
    
    bufferWidth = width;
    bufferHeight = height;
    
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) == 0) {
        
        windowHandle = SDL_CreateWindow(
            "Adventure",
            SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED, 
            width, 
            height, 
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        
        SDL_DisplayMode mode;
        SDL_GetCurrentDisplayMode(0, &mode);
        
        SDL_GLContext renderContext = SDL_GL_CreateContext(windowHandle);
        if(renderContext) {
            
            if(SDL_GL_MakeCurrent(windowHandle, renderContext) == 0) {
                
                if(SDL_GL_SetSwapInterval(1) == 0) {
                    //Success
                } else {
                    printf("Couldn't set swap interval\n");
                }
            } else {
                printf("Couldn't make context current\n");
            }
        }
        
        //We move back one folder, out of the src file 
        char *globalExeBasePath_ = SDL_GetBasePath();
        unsigned int execPathLength = strlen(globalExeBasePath_);
        globalExeBasePath = calloc(execPathLength, 1);
        
        char *at = globalExeBasePath_;
        char *dest = globalExeBasePath;
        char *mostRecent = at;
        while(*at) {
            if(*at == '/' && at[1]) { //don't collect last slash
                mostRecent = at;
            }
            *dest = *at;
            at++;
            dest++;
        }
        int indexAt = (int)(mostRecent - globalExeBasePath_) + 1; //plus one to keep the slash
        globalExeBasePath[indexAt] = 'r';
        globalExeBasePath[indexAt + 1] = 'e';
        globalExeBasePath[indexAt + 2] = 's';
        globalExeBasePath[indexAt + 3] = '/';
        globalExeBasePath[indexAt + 4] = '\0';
        assert(strlen(globalExeBasePath) <= execPathLength);
        
        arena.currentSize = 0;
        arena.memory = calloc(1000000000, 1);
        InteractionTable interactionTable_ = {};
        
        InteractionTable *table = &interactionTable_;
        
        Player player = {};
        
        table->player = &player; 
        
        float h1FontSize = 4;
        float h2FontSize = 2;
        float h3FontSize = 1;
        
        float dt = 1 / (float)mode.refresh_rate; //use monitor refresh rate 
        
        SDL_AudioSpec audioSpec = initAudioSpec(48000, audioCallback);
        
        initAudio(&audioSpec);
        SDL_PauseAudio(0); //play audio
        
        WavFile backgroundSound;loadWavFile(&backgroundSound, (concat(&arena, globalExeBasePath,"CrystalWaters.wav")), &audioSpec);
        
        WavFile clankSound;
        loadWavFile(&clankSound, (concat(&arena, globalExeBasePath,"Clank1.wav")), &audioSpec);
        
        WavFile rain;
        loadWavFile(&rain, (concat(&arena, globalExeBasePath,"rain.wav")), &audioSpec);
        
        //WavFile *test = findSound("CrystalWaters.wav");
        playSound(&arena, &rain, true);
        
        loadWavFile(&footstepSound, (concat(&arena, globalExeBasePath, "foot_steps_sand1.wav")), &audioSpec);
        
        GLuint scrollTexId = loadImage(concat(&arena, globalExeBasePath, "parchment1.jpg"));
        GLuint doorTexId = loadImage(concat(&arena, globalExeBasePath, "door1.png"));
        GLuint backgroundTexId = loadImage(concat(&arena, globalExeBasePath, "scrollTexture2.jpg"));
        
        GameState *gameStateFullPage = pushStruct(&arena, GameState);
        
        GameState *gameStateTurn = pushStruct(&arena, GameState);
        
        setGameStateFlag(&gameStateTurn->flags, INPUT_ACTIVE, true);
        
        gameStateTurn->posAt = v2(0.2f, 0.1f);
        
        gameStateTurn->turnResetPosAt = gameStateTurn->posAt;
        
        gameStateTurn->bounds = rect2f(0.2f, 0.2f, 0.8f, 0.8f);
        
        //printf("%f\n", gameStateTurn->bounds.maxX);
        gameStateFullPage->next = gameStateTurn;
        
        gameStateTurn->next = gameStateTurn;
        
        GameState *gameStateFreeList = 0;
        GameState *currentGameState = gameStateFullPage;
        //GameState *currentGameState = gameStateTurn;
        
        mainFont = initFont(concat(&arena, globalExeBasePath, "Merriweather-Regular.ttf"));
        
        menuFont = initFont(concat(&arena, globalExeBasePath, "VT323-Regular.ttf"));
        
        loadGameDataFile(table, concat(&arena, globalExeBasePath, "gameData.txt"));
        
        addInteractionVN(table, "north", WALK, "you walk North", initMoveDirection(NORTH));
        
        
        addInteractionVN(table, "east", WALK, "you walk East", initMoveDirection(EAST));
        
        
        addInteractionVN(table, "south", WALK, "you walk South", initMoveDirection(SOUTH));
        
        
        addInteractionVN(table, "west", WALK, "you walk West", initMoveDirection(WEST));
        
        addInteractionN(table, "backpack", 0,  initCallbackInfoType(CHECK_INVENTORY));
        
        addInteractionN(table, "compass", 0, initCallbackInfoType(SHOW_DIRECTIONS));
        
        addInteractionN(table, "quit", 0, initCallbackInfoType(QUIT));
        
        addInteractionN(table, "health", 0, initCallbackInfoType(CHECK_HEALTH));
        
        Timer cursorTimer = initTimer(2.0f);
        
        GameMode gameMode = PLAY_MODE; 
        
        //sleep(1);
        bool running = true;
        Room *currentRoom = table->firstRoom;
        currentRoom->discovered = true;
        Room *lastRoom = 0;
        
        char *aString = "It's been five long years since Edward the thirds reign. The war of the roses has seen many bloody deaths.\n";
        
        char *bString = "You have seen your family die at the hands of Edwards army. Just before the family were killed, you were put into hiding.";
        
        char *cString = "You now live as a peasant, with a kind family. Word has got out that you are the descendant of King William.";
        
        char *dString = "Some one knocks at the door...";
        
        pushPrintStruct(gameStateFullPage, aString, 0.1f, 0.8f, 0.2, h3FontSize, v4(0, 0, 0, 0), v4(0, 0, 0, 1), 4, BRIEF);
        
#if 0
        pushPrintStruct(gameStateFullPage, bString, 0.1f, 0.8f, 0.2, h3FontSize, v4(0, 0, 0, 0), v4(0, 0, 0, 1), 4, BRIEF);
        
        
        pushPrintStruct(gameStateFullPage, cString, 0.1f, 0.8f, 0.2, h3FontSize, v4(0, 0, 0, 0), v4(0, 0, 0, 1), 4, BRIEF);
        
        
        pushPrintStruct(gameStateFullPage, dString, 0.1f, 0.8f, 0.2, h3FontSize, v4(0, 0, 0, 0), v4(0, 0, 0, 1), 4, BRIEF);
#else
        pushPrintStructWithDelay(gameStateFullPage, bString, 0.4f, 0.8f, 0.2, h3FontSize, COLOR_NULL, COLOR_BLACK, 4, 3);
        
        
        pushPrintStructWithDelay(gameStateFullPage, cString, 0.1f, 0.8f, 0.6, h3FontSize, COLOR_NULL, COLOR_BLACK, 7, 3);
        
        pushPrintStructWithDelay(gameStateFullPage, dString, 0.4f, 0.8f, 0.6, h3FontSize, COLOR_NULL, COLOR_BLACK, 9, 3);
#endif
        
        pushPrintStruct(gameStateTurn, "What do you do?\n", 0.1f, 0.8f, 0.8, h3FontSize, v4(0, 0, 0, 1), v4(0, 0, 0, 1), 4, FOREVER);
        
        InputBuffer inputBuffer = {};
        
        Rect2f roomItemsWindow = {};
        
        V4 scrollHandleColor = COLOR_BLACK;
        Rect2f scrollHandleRect = {};
        
        describeRoom(currentRoom, gameStateTurn, &roomItemsWindow, scrollTexId, &scrollHandleRect, &scrollHandleColor);
        
        int menuCursorAt = 0;
        
        GameButton gameButtons[BUTTON_COUNT] = {};
        
        GLuint frameBufferHandles[32] = {0};
        int frameBufferCount = 1;
        
        GLuint scrollFrameBufferTexture = openGlLoadTexture(bufferWidth, bufferHeight, 0);
        
        int frameBufferAt = frameBufferCount++;
        GLuint frameBufferId;
        glGenFramebuffers(1, &frameBufferHandles[frameBufferAt]);
        
        glBindFramebuffer(GL_FRAMEBUFFER, frameBufferHandles[frameBufferAt]);
        
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 
                               scrollFrameBufferTexture, 0);
        
        bool scrollIsHot = false;
        bool scrollIsInteracting = false;
        
        V2 scrollHandleAt = {};
        float scrHanVel = 0;
        
        GameMode lastMode = gameMode;
        
        float lastHeighestYValue = 0;
        bool setYValue = true;
        
        V4 scrollColorHandle = COLOR_BLACK;
        Lerpf scrAt = initLerpf();
        LerpV4 scrCol = initLerpV4();
        Lerpf windowH = initLerpf();
        //game loop
        while(running && currentRoom) {
            bool mouseWasDown = isDown(gameButtons, BUTTON_LEFT_MOUSE);
            zeroArray(gameButtons);
            
            
            bool drawInput = getGameStateFlag(currentGameState->flags, INPUT_ACTIVE);
            //ask player for new input
            SDL_Event event;
            bool submitTurn = false;
            
            enableOpenGl(width, height);
            float scrHanAccel = 0;
            
            while( SDL_PollEvent( &event ) != 0 ) {
                if (event.type == SDL_WINDOWEVENT) {
                    switch(event.window.event) {
                        case SDL_WINDOWEVENT_RESIZED: {
                            // TODO(Oliver): Work on this! Need to think harder about resizing the screen. 
                            height = bufferHeight = event.window.data2;
                            width = bufferWidth = event.window.data1;
                            
                            OpenGlAdjustScreenDim(width, height);
                            
                            setItemWindow(roomItemsWindow.minY, &roomItemsWindow);
                            
                            //glViewport(0, 0, event.window.data1, event.window.data2);
                        } break;
                        default: {
                        }
                    }
                } else if(event.type == SDL_MULTIGESTURE) {
                    if(event.mgesture.numFingers == 2) {
                        // TODO(Oliver): For iOS version do we want to do more here?
                        //scrHanAccel = 3000*event.mgesture.y - lastGestY;
                    }
                } else if(event.type == SDL_FINGERDOWN) {
                    scrHanVel = 0;
                } else if(event.type == SDL_MOUSEWHEEL) {
                    if(event.wheel.y > 0) { // scroll up
                        scrHanAccel = -3000;
                    } else if(event.wheel.y < 0) { // scroll down
                        scrHanAccel = 3000;
                    }
                } else if( event.type == SDL_QUIT ) {
                    running = false;
                } else if(event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                    
                    SDL_Keycode keyCode = event.key.keysym.sym;
                    
                    bool altKeyWasDown = (event.key.keysym.mod & KMOD_ALT);
                    bool shiftKeyWasDown = (event.key.keysym.mod & KMOD_SHIFT);
                    bool isDown = (event.key.state == SDL_PRESSED);
                    bool repeated = event.key.repeat;
                    ButtonType buttonType = BUTTON_NULL;
                    switch(keyCode) {
                        case SDLK_RETURN: {
                            buttonType = BUTTON_ENTER;
                            if(isDown) {
                                submitTurn = true;
                                cursorTimer.value = 0;
                            }
                        } break;
                        case SDLK_UP: {
                            buttonType = BUTTON_UP;
                        } break;
                        case SDLK_ESCAPE: {
                            buttonType = BUTTON_ESCAPE;
                        } break;
                        case SDLK_DOWN: {
                            buttonType = BUTTON_DOWN;
                        } break;
                        case SDLK_LEFT: {
                            if(isDown) {
                                if(inputBuffer.cursorAt > 0) {
                                    inputBuffer.cursorAt--;
                                    cursorTimer.value = 0;
                                }
                            }
                        } break;
                        case SDLK_RIGHT: {
                            if(isDown) {
                                if(inputBuffer.cursorAt < (inputBuffer.length)) {
                                    inputBuffer.cursorAt++;
                                    cursorTimer.value = 0;
                                }
                            }
                        } break;
                        case SDLK_BACKSPACE: {
                            if(isDown) {
                                splice(&inputBuffer, "a", -1);
                                cursorTimer.value = 0;
                            }
                        } break;
                    }
                    if(buttonType) {
                        sdlProcessGameKey(&gameButtons[buttonType], isDown, repeated);
                    }
                    
                } else if( event.type == SDL_TEXTINPUT ) {
                    if(drawInput) {
                        char *inputString = event.text.text;
                        splice(&inputBuffer, inputString, 1);
                        cursorTimer.value = 0;
                    }
                }
            }
            
            int mouseX, mouseY;
            
            unsigned int mouseState = SDL_GetMouseState(&mouseX, &mouseY);
            
            V2 mouseP = v2(mouseX, mouseY);
            
            
            bool leftMouseDown = mouseState & SDL_BUTTON_LMASK;
            sdlProcessGameKey(&gameButtons[BUTTON_LEFT_MOUSE], leftMouseDown, leftMouseDown && mouseWasDown);
            
            
            //clear all frame buffers
            for(int frameBufferIndex = 0; 
                frameBufferIndex < frameBufferCount;
                ++frameBufferIndex) {
                
                glBindFramebuffer(GL_FRAMEBUFFER, frameBufferHandles[frameBufferIndex]);
                
                glClearColor(1, 0, 1, 1);
                glClear(GL_COLOR_BUFFER_BIT);
            }
            
            int defaultFrameBufferHandle = 0;
            glBindFramebuffer(GL_FRAMEBUFFER, defaultFrameBufferHandle);
            switch(gameMode) {
                case LOAD_MODE:{
                    MenuOptions menuOptions = {};
                    menuOptions.options[menuOptions.count++] = "Go Back";
                    
                    menuCursorAt = updateMenu(&menuOptions, gameButtons, menuCursorAt);
                    
                    // NOTE(Oliver): Main Menu action options
                    if(wasPressed(gameButtons, BUTTON_ENTER)) {
                        switch (menuCursorAt) {
                            case 0: {
                                gameMode = lastMode;
                                menuCursorAt = 0;
                            } break;
                        }
                        lastMode = LOAD_MODE;
                    }
                    
                    renderMenu(backgroundTexId, &menuOptions, menuCursorAt);
                    
                } break;
                case QUIT_MODE:{
                    MenuOptions menuOptions = {};
                    menuOptions.options[menuOptions.count++] = "Really Quit?";
                    menuOptions.options[menuOptions.count++] = "Go Back";
                    
                    menuCursorAt = updateMenu(&menuOptions, gameButtons, menuCursorAt);
                    
                    // NOTE(Oliver): Main Menu action options
                    if(wasPressed(gameButtons, BUTTON_ENTER)) {
                        switch (menuCursorAt) {
                            case 0: {
                                running = false;
                            } break;
                            case 1: {
                                gameMode = lastMode;
                                menuCursorAt = 0;
                            } break;
                        }
                        lastMode = QUIT_MODE;
                    }
                    
                    renderMenu(backgroundTexId, &menuOptions, menuCursorAt);
                    
                } break;
                case DIED_MODE:{
                    MenuOptions menuOptions = {};
                    menuOptions.options[menuOptions.count++] = "Really Quit?";
                    menuOptions.options[menuOptions.count++] = "Go Back";
                    
                    menuCursorAt = updateMenu(&menuOptions, gameButtons, menuCursorAt);
                    
                    // NOTE(Oliver): Main Menu action options
                    if(wasPressed(gameButtons, BUTTON_ENTER)) {
                        switch (menuCursorAt) {
                            case 0: {
                                running = false;
                            } break;
                            case 1: {
                                gameMode = lastMode;
                                menuCursorAt = 0;
                            } break;
                        }
                        lastMode = DIED_MODE;
                    }
                    
                    renderMenu(backgroundTexId, &menuOptions, menuCursorAt);
                    
                } break;
                case SAVE_MODE:{
                    MenuOptions menuOptions = {};
                    menuOptions.options[menuOptions.count++] = "SAVE";
                    menuOptions.options[menuOptions.count++] = "Go Back";
                    
                    menuCursorAt = updateMenu(&menuOptions, gameButtons, menuCursorAt);
                    
                    // NOTE(Oliver): Main Menu action options
                    if(wasPressed(gameButtons, BUTTON_ENTER)) {
                        switch (menuCursorAt) {
                            case 0: {
                                savePlayerData(table, "playerData.txt", currentGameState);
                            } break;
                            case 1: {
                                gameMode = lastMode;
                                menuCursorAt = 1;
                            } break;
                        }
                        lastMode = SAVE_MODE;
                        
                    }
                    
                    renderMenu(backgroundTexId, &menuOptions, menuCursorAt);
                    
                } break;
                case PAUSE_MODE:{
                    MenuOptions menuOptions = {};
                    
                    menuOptions.options[menuOptions.count++] = "Resume";
                    menuOptions.options[menuOptions.count++] = "Save";
                    menuOptions.options[menuOptions.count++] = "Load";
                    menuOptions.options[menuOptions.count++] = "Settings";
                    menuOptions.options[menuOptions.count++] = "Quit";
                    
                    
                    menuCursorAt = updateMenu(&menuOptions, gameButtons, menuCursorAt);
                    
                    // NOTE(Oliver): Main Menu action options
                    if(wasPressed(gameButtons, BUTTON_ENTER)) {
                        switch (menuCursorAt) {
                            case 0: {
                                gameMode = PLAY_MODE;
                            } break;
                            case 1: {
                                gameMode = SAVE_MODE;
                            } break;
                            case 2: {
                                gameMode = LOAD_MODE;
                            } break;
                            case 3: {
                                gameMode = SETTINGS_MODE;
                            } break;
                            case 4: {
                                gameMode = QUIT_MODE;
                            } break;
                        }
                        menuCursorAt = 0;
                        lastMode = PAUSE_MODE;
                    }
                    
                    renderMenu(backgroundTexId, &menuOptions, menuCursorAt);
                    
                } break;
                case SETTINGS_MODE:{
                    MenuOptions menuOptions = {};
                    
                    unsigned int windowFlags = SDL_GetWindowFlags(windowHandle);
                    
                    bool isFullScreen = (windowFlags & SDL_WINDOW_FULLSCREEN);
                    char *fullScreenOption = isFullScreen ? "Exit Full Screen" : "Full Screen";
                    menuOptions.options[menuOptions.count++] = fullScreenOption;
                    menuOptions.options[menuOptions.count++] = "Go Back";
                    
                    menuCursorAt = updateMenu(&menuOptions, gameButtons, menuCursorAt);
                    
                    // NOTE(Oliver): Main Menu action options
                    if(wasPressed(gameButtons, BUTTON_ENTER)) {
                        switch (menuCursorAt) {
                            case 0: {
                                if(isFullScreen) {
                                    // TODO(Oliver): Change this to handle resolution change. Have to query the current window size to adjust screen size. 
                                    if(SDL_SetWindowFullscreen(windowHandle, false) < 0) {
                                        printf("couldn't un-set to full screen\n");
                                    }
                                } else {
                                    if(SDL_SetWindowFullscreen(windowHandle, SDL_WINDOW_FULLSCREEN) < 0) {
                                        printf("couldn't set to full screen\n");
                                    }
                                }
                            } break;
                            case 1: {
                                gameMode = lastMode;
                                lastMode = SETTINGS_MODE;
                            } break;
                        }
                        menuCursorAt = 0;
                    }
                    
                    renderMenu(backgroundTexId, &menuOptions, menuCursorAt);
                } break;
                case MENU_MODE:{
                    MenuOptions menuOptions = {};
                    
                    menuOptions.options[menuOptions.count++] = "Play";
                    menuOptions.options[menuOptions.count++] = "Load";
                    menuOptions.options[menuOptions.count++] = "Quit";
                    
                    
                    menuCursorAt = updateMenu(&menuOptions, gameButtons, menuCursorAt);
                    
                    // NOTE(Oliver): Main Menu action options
                    if(wasPressed(gameButtons, BUTTON_ENTER)) {
                        switch (menuCursorAt) {
                            case 0: {
                                gameMode = PLAY_MODE;
                            } break;
                            case 1: {
                                gameMode = LOAD_MODE;
                            } break;
                            case 2: {
                                gameMode = QUIT_MODE;
                            } break;
                        }
                        lastMode = MENU_MODE;
                        
                        menuCursorAt = 0;
                    }
                    
                    renderMenu(backgroundTexId, &menuOptions, menuCursorAt);
                } break;
                case PLAY_MODE: {
                    if(wasPressed(gameButtons, BUTTON_ESCAPE)) {
                        gameMode = PAUSE_MODE;
                        lastMode = PLAY_MODE;
                    }
                    
                    openGlTextureCentreDim(backgroundTexId, v2(0.5f*width, 0.5f*height), v2(width, height), v4(1, 1, 1, 1), 0, mat4());
                    
                    float marFacW = 0.8f;
                    float marFacH = 0.95f;
                    openGlTextureCentreDim(scrollTexId, v2(0.5f*width, 0.5f*height), v2(marFacW*width, marFacH*height), v4(1, 1, 1, 1), 0, mat4());
                    
                    //openGlTextureCentreDim(doorTexId, v2(0.5f*width, 0.5f*height), v2(0.2f*width, 0.2f*height), v4(1, 1, 1, 1), 0, mat4());
                    
                    if(submitTurn) {
                        for(int printStructCount = 0;
                            printStructCount < currentGameState->printInfoCount;
                            printStructCount++) {
                            PrintInfo *info = currentGameState->printInfos + printStructCount;
                            if(info->lifeSpan == TURN_BASED) {
                                setForRemoval(info);
                            } 
                        }
                        
                        PrintInfoDelayedArray delayed = {};
                        
                        processTurn(currentGameState, table, &currentRoom, &inputBuffer, &player, &gameMode, &delayed);
                        
                        //Clear input buffer
                        inputBuffer.length = 0;
                        inputBuffer.cursorAt = 0;
                        zeroArray(inputBuffer.chars);
                        //
                        
                        //Reset where we start drawing from
                        currentGameState->posAt = currentGameState->turnResetPosAt; 
                        //
                        
                        describeRoom(currentRoom, currentGameState, &roomItemsWindow, scrollTexId, &scrollHandleRect, &scrollHandleColor);
                        //Write out all the dealyed print structs
                        for(int delayedIndex = 0; delayedIndex < delayed.count; ++delayedIndex) {
                            PrintInfoDelayed *printInfo = delayed.E + delayedIndex;
                            
                            PrintInfo *tempInfo = pushPrintStructDefault(printInfo->state, printInfo->string, printInfo->lifeSpan);
                            tempInfo->isDynamic = true;
                        }
                        //
                    }
                    
                    if(drawInput) {
                        V4 colorA = v4(1, 1, 0, 1);
                        V4 colorB = v4(1, 0, 0, 1);
                        
                        updateTimer(&cursorTimer, dt);
                        
                        V2 startP = v2(0.2f*width, 0.9f*height);
                        
                        V4 cursorColor = smoothStep00V4(colorA, getTimerValue01(&cursorTimer), colorB);
                        
                        V4 textColor = v4(0, 0, 0, 1);
                        
                        Rect2f textInputMargin = {};
                        textInputMargin.minX = startP.x;
                        textInputMargin.minY = startP.y;
                        textInputMargin.maxX = width*0.8f;
                        textInputMargin.maxY = height*0.9f;
                        
                        outputText_with_cursor(&mainFont, startP.x, startP.y, (float)width, (float)height, inputBuffer.chars, textInputMargin, textColor, 1, inputBuffer.cursorAt, cursorColor, true);
                        
                    }
                    
                    float windowHeight = getDim(roomItemsWindow).y;
                    
                    
                    //update scroll handle based on input scrolling
                    if(inRect(mouseP, roomItemsWindow) && !scrollIsInteracting) { 
                        scrollHandleAt.y += (scrHanAccel*sqr(dt)) + scrHanVel*dt;
                        scrHanVel += dt*scrHanAccel;
                        float dragFactor = 0.7f;
                        scrHanVel -= dt*dragFactor*scrHanVel; 
                    } 
                    
                    V2 scrollDim = v2(10, 30); //change this so the handle expands to the space. 
                    
                    V2 scrollAbs = v2_plus(scrollHandleAt, v2(roomItemsWindow.minX, roomItemsWindow.minY)); 
                    
                    scrollHandleRect = rect2fCenterDim(scrollAbs.x, scrollAbs.y, scrollDim.x, scrollDim.y);
                    
                    scrollIsHot = false;
                    if(!scrollIsInteracting) {
                        if(inRect(mouseP, scrollHandleRect)) {
                            scrollIsHot = true;
                        } else {
                            //Work on this!!
                            setLerpInfoV4_s(&scrCol, COLOR_BLACK, 0.3f, &scrollHandleColor);
                        }
                    }
                    
                    if(scrollIsHot) {
                        setLerpInfoV4_s(&scrCol, COLOR_BLUE, 0.3f, &scrollHandleColor);
                        
                        if(wasPressed(gameButtons, BUTTON_LEFT_MOUSE)) {
                            scrollIsInteracting = true;
                        }
                    }
                    
                    if(scrollIsInteracting) {
                        if(wasReleased(gameButtons, BUTTON_LEFT_MOUSE)) {
                            scrollIsInteracting = false;
                            
                            setLerpInfoV4_s(&scrCol, COLOR_BLACK, 0.3f, &scrollHandleColor);
                            
                        } else {
                            setLerpInfoV4_s(&scrCol, COLOR_GREEN, 0.3f, &scrollHandleColor);
                            scrollHandleAt.y = mouseP.y - roomItemsWindow.minY;
                            
                        }
                    } 
                    
                    updateLerpf(&scrAt, dt, SMOOTH_STEP_01);
                    updateLerpV4(&scrCol, dt, SMOOTH_STEP_01);
                    updateLerpf(&windowH, dt, SMOOTH_STEP_01);
                    
                    //clamp scroll handle to the window bounds
                    if(scrollHandleAt.y < 0) {
                        scrollHandleAt.y = 0;
                        scrHanVel = 0;
                    }
                    if(scrollHandleAt.y > windowHeight) {
                        scrollHandleAt.y = windowHeight;
                        scrHanVel = 0;
                    }
                    //
                    
                    //openGlDrawRectOutlineRect2f(roomItemsWindow, COLOR_GREEN, 0, mat4TopLeftToBottomLeft(bufferHeight));
                    
                    float windowOverflow = (lastHeighestYValue - roomItemsWindow.minY);
                    windowOverflow -= windowHeight + fontHeight;
                    float scrollOffset = (scrollHandleAt.y / windowHeight) * windowOverflow; 
                    float heighestYValue_ = 0;
                    
                    //Set up scissors window
                    Rect2f transWin = roomItemsWindow;
                    
                    // Move rect to bottom left coordinate space
                    transWin.min = V4MultMat4(v2ToV4Homogenous(roomItemsWindow.min), mat4TopLeftToBottomLeft(bufferHeight)).xy; 
                    transWin.max = V4MultMat4(v2ToV4Homogenous(roomItemsWindow.max), mat4TopLeftToBottomLeft(bufferHeight)).xy; 
                    
                    //reset the min and max values of the rectangle
                    transWin = reevalRect2f(transWin);
                    //
                    
                    
                    //RENDER
                    int activeCount = currentGameState->printInfoCount;
                    for(int printStructCount = 0; printStructCount < currentGameState->printInfoCount; ) {
                        bool incrementIndex = true;
                        PrintInfo *info = currentGameState->printInfos + printStructCount;
                        
                        TextAnimationInfo *animInfo = info->animInfos + info->animAt;
                        assert(info->animCount);
                        bool wentIn = true;
                        if(info->active) {
                            wentIn = false;
                            TimerReturnInfo timerInfo = updateTimer(&animInfo->timer, dt);
                            
                            if(timerInfo.finished) {
                                if(animInfo->useOnce) {
                                    //remove animation and don't move to next one because we move all animations down. 
                                    for(int animIndex = info->animAt;
                                        animIndex < info->animCount;
                                        ++animIndex) {
                                        info->animInfos[animIndex] = info->animInfos[animIndex + 1]; //move everyone down one index - same as splicing with text. 
                                    }
                                    info->animCount--;
                                } else {
                                    //move to next animation
                                    info->animAt++;
                                }
                                animInfo->timer.value = 0; //clear value if this print struct is used again
                                if(info->animAt < info->animCount) {
                                    //alright to go to next animation
                                } else if(info->lifeSpan == BRIEF){
                                    removePrintInfo(currentGameState, printStructCount);
                                    incrementIndex = false;
                                    activeCount--;
                                    info->active = false;
                                } else { //start from the start again
                                    info->animAt = 0;
                                }
                            } 
                            
                            V4 color = smoothStep01V4(animInfo->startColor, timerInfo.canonicalVal, animInfo->finishColor);
                            
                            float xPos = info->margin.minX*bufferWidth; 
                            float yPos = info->margin.minY*bufferHeight;
                            Rect2f stringMargin = info->margin;
                            stringMargin.minX *= bufferWidth;
                            stringMargin.minY *= bufferHeight;
                            stringMargin.maxX *= bufferWidth;
                            stringMargin.maxY *= bufferHeight;
                            
                            if(info->isDynamic) {
                                glEnable(GL_SCISSOR_TEST);
                                
                                V2 a = transWin.min;
                                V2 b = transWin.max;
                                
                                int ww = b.x - a.x;
                                int wh = b.y - a.y;
                                
                                glScissor(a.x, a.y, ww, wh);
                                
                                if(yPos > heighestYValue_) {
                                    heighestYValue_ = yPos;
                                }
                                
                                yPos -= scrollOffset;
                            }
#if 1
                            switch(info->type) {
                                case STRING: {
                                    outputText(&mainFont, xPos, yPos, (float)bufferWidth, (float)bufferHeight, info->string, stringMargin, color, info->size, true);
                                } break;
                                case LINE: {
                                    float thickness = 4;
                                    V2 A = v2_hadamard(v2(bufferWidth, bufferHeight), info->begin);
                                    V2 B = v2_hadamard(v2(bufferWidth, bufferHeight), info->end);
                                    openGlDrawLine(A, B, color, thickness, mat4TopLeftToBottomLeft(bufferHeight));
                                } break;
                                case TEXTURE: {
                                    openGlTextureCentreDim(info->texId, getCenter(*info->texRect), getDim(*info->texRect), color, 0, info->offsetTransform);
                                } break;
                                case RECT_OUTLINE: {
                                    V4 rectColor = *info->color;
                                    rectColor.w = color.w;
                                    openGlDrawRectOutlineRect2f(*info->texRect, rectColor, 0, info->offsetTransform);
                                    
                                } break;
                                default: {
                                    invalidCodePathStr("case not handled");
                                }
                            }
#endif
                            glDisable(GL_SCISSOR_TEST);
                            
                        } else {
                            assert(wentIn);
                            assert(!info->active);
                            activeCount--;
                        }
                        
                        if(incrementIndex) {
                            printStructCount++;
                        }
                    }
                    
                    if(!activeCount) {
                        GameState *newState = currentGameState->next;
                        
                        if(newState == currentGameState) {
                            //don't remove - we are still using it. 
                            resetPrintStrctures(newState);
                        } else {
                            currentGameState->next = gameStateFreeList;
                            gameStateFreeList = currentGameState;
                            
                            currentGameState = newState;
                        }
                        assert(currentGameState);
                    }
                    float newHeighVal = heighestYValue_ + 1.5f*fontHeight;
                    if(!setYValue) {
                        if((newHeighVal != lastHeighestYValue && !isOn(&windowH.timer))) {
                            printf("hey tehre\n");
                            setLerpInfof_s(&windowH, newHeighVal, 0.3f, &lastHeighestYValue);
                            setLerpInfof_s(&scrAt, windowHeight, 0.3f, &scrollHandleAt.y);
                            
                            // TODO(Oliver): This is broken! we need to
                            // think about this more. There are different cases we need
                            // to handle. like when there is info at the bottom, but also when we go back to the top when we change room and when the window gets smaller. and we can't just use the print bounds, since there is the case when one line dissapears and one appears, it won't move. 
                        }
                    } else {
                        lastHeighestYValue = newHeighVal;
                        setYValue = false;
                    }
                    
#if 0 //draw white lines 
                    openGlDrawLine(v2(roomItemsWindow.minX, roomItemsWindow.minY), v2(roomItemsWindow.maxX, roomItemsWindow.minY), COLOR_WHITE, 3, mat4());
                    
                    openGlDrawLine(v2(roomItemsWindow.minX, roomItemsWindow.maxY), v2(roomItemsWindow.maxX, roomItemsWindow.maxY), COLOR_WHITE, 3, mat4());
                    
#endif
                } break;
            } 
            
            SDL_GL_SwapWindow(windowHandle);
        }
    }
    return 0;
}