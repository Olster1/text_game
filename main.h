/*
TODO: 
- think about lerp to new position on the scroll window
- save level using the same format as the game data.txt
- write audio mixer

- change menu to use fade in and out menu items. 

 */

typedef enum {
    BUTTON_NULL,
    BUTTON_LEFT,
    BUTTON_RIGHT,
    BUTTON_UP,
    BUTTON_DOWN,
    BUTTON_SPACE,
    BUTTON_ENTER,
    BUTTON_BACKSPACE,
    BUTTON_ESCAPE,
    BUTTON_LEFT_MOUSE,
    //
    BUTTON_COUNT
} ButtonType;

typedef struct {
    bool isDown;
    int transitionCount;
} GameButton;

#define wasPressed(buttonArray, index) (buttonArray[index].isDown && buttonArray[index].transitionCount != 0)  

#define wasReleased(buttonArray, index) (!buttonArray[index].isDown && buttonArray[index].transitionCount != 0)  

#define isDown(buttonArray, index) (buttonArray[index].isDown)  


typedef enum {
    MENU_MODE,
    PAUSE_MODE,
    PLAY_MODE,
    LOAD_MODE,
    SAVE_MODE,
    QUIT_MODE,
    DIED_MODE,
    SETTINGS_MODE
} GameMode;

typedef enum {
    NULL_ACTION,
    TAKE,
    WALK,
    ON,
    EAT, 
    USE,
    TALK,
    DROP,
} ActionType;

typedef enum {
    NULL_CALLBACK,
    HEALTH, 
    INVENTORY_ADD,
    INVENTORY_DROP,
    OPEN,
    MOVE,
    QUIT, 
    CHECK_INVENTORY, 
    CHECK_HEALTH,
    SHOW_DIRECTIONS,
} CallbackType;

static char *directionStrings[5] = {"NULL_DIRECTION", "North", "East", "South", "West"};

typedef enum {
    NULL_DIRECTION,
    NORTH,
    EAST, 
    SOUTH,
    WEST,
    //
    DIRECTION_COUNT,
} Direction;

typedef struct Object Object;
typedef struct {
    CallbackType type;
    union {
        struct {
            int healthValue;
        };
        struct {
            Direction direction;
        };
        struct {
            Object *objectToAdd;
        }; 
        struct {
            Object *objectToDrop;
        };
    };
    
} CallbackInfo;

typedef enum {
    NULL_WORD_TYPE,
    NOUN,
    VERB,
} WordType;

typedef enum {
    NULL_PATTERN,
    N, 
    VN, 
    VNVN, 
    VNVNVN, 
    V,
    
} SentencePattern;

typedef struct {
    char *begin;
    int length;
} String;

typedef struct {
    String name;
    bool inRoom;
    bool inBackpack;
} NounInfo;

struct Room;
typedef struct {
    SentencePattern pattern;
    
    NounInfo nounSearchInfo[16];
    int nounInfoCount;
    
    Object *objects[16];
    int objectCount;
    
    ActionType verbs[16];
    int verbCount;
    
    CallbackInfo callbackInfo;
    char *response;
    char *soundResponse;
    
    struct Room **currentRoom;
} Interaction;

typedef struct {
    struct Object *invetory; 
    int health;
} Player;

typedef struct {
    Interaction interactions[64];
    int count;
    
    Player *player;
    struct Room *firstRoom;
} InteractionTable;

typedef enum {
    NULL_FLAG = 0,
    IS_SHUT = 1 << 1,
} ObjectFlag; 

typedef enum {
    NULL_TYPE,
    ATTACKER,
} ObjectType;

typedef struct Object {
    char *name;
    char *description;
    
    ObjectType type;
    
    union {
        struct {
            int beginAttackCount;
            int attackCount; 
            char *attackString;
            int attackValue;
        };
    };
    
    struct Room *room; //if valid in a room. 
    int flags; // is open etc.
    
    Object *next; //for linked list
} Object;

typedef struct RoomConnection {
    char *transition;
    struct Room *room;
    Object *blocked;
    char *blockedDescr;
} RoomConnection;

typedef struct Room {
    Object *objects;
    
    char *name;
    char *description;
    
    int conCount;
    RoomConnection connections[DIRECTION_COUNT];
    
    // TODO(Oliver): Maybe use flags if there are more flags! 1/2/18
    bool discovered; 
} Room;

typedef struct {
    float value;
    float period;
} Timer;

Timer initTimer(float period) {
    Timer result = {};
    result.period = period;
    return result;
}

float getTimerValue01(Timer *timer) {
    float value = timer->value / timer->period;
    return value;
}
typedef struct {
    bool finished; 
    float canonicalVal;
} TimerReturnInfo;

TimerReturnInfo updateTimer(Timer *timer, float dt) {
    
    TimerReturnInfo returnInfo = {};
    
    timer->value += dt;
    if(timer->period == 0) {
        float defaultPeriod = 1; 
        timer->period = defaultPeriod;
    }
    if((timer->value / timer->period) >= 1.0f) {
        timer->value = 0;
        returnInfo.canonicalVal = 1; //anyone using this value afterwards wants to know that it finished
        returnInfo.finished = true;
    } else {
        returnInfo.canonicalVal = getTimerValue01(timer);
    }
    return returnInfo; 
}

typedef struct {
    V4 startColor;
    V4 finishColor;
    Timer timer;
    bool useOnce;
} TextAnimationInfo;

typedef enum {
    STRING, 
    LINE,
    TEXTURE,
    RECT_OUTLINE,
} RenderType;

typedef enum {
    BRIEF, 
    DELAYED,
    TURN_BASED,
    FOREVER, 
} LifeSpan;

typedef struct {
    RenderType type;
    union {
        struct {
            char *string;
            float size;
        };
        struct {
            V2 begin;
            V2 end;
        };
        struct {
            GLuint texId;
            Rect2f *texRect; //pointer so it changes if we cahnge the rect size
            Matrix4 offsetTransform;
            V4 *color;
        };
    };
    
    Rect2f margin;
    
    TextAnimationInfo animInfos[32];
    int animAt;
    int animCount;
    
    float zIndex;
    
    LifeSpan lifeSpan;
    
    bool isDynamic;
    
    bool active;
} PrintInfo;

typedef enum {
    INPUT_ACTIVE = 1 << 1,
} GameStateFlag;

typedef struct GameState {
    unsigned int flags;
    
    PrintInfo printInfos[256];
    int printInfoCount;
    
    Rect2f bounds;
    V2 posAt; 
    
    V2 turnResetPosAt;
    
    struct GameState *next;
} GameState;


bool getGameStateFlag(unsigned int flags, GameStateFlag flagToGet) {
    bool result = flags & flagToGet;
    return result;
}


void setGameStateFlag(unsigned int *flags, GameStateFlag flagToSet, bool value) {
    if(value) {
        *flags |= flagToSet;
    } else {
        *flags &= ~flagToSet;
    }
}

typedef struct 
{
    bool IsDown;
    bool FrameCount;
} game_button;

#define INPUT_BUFFER_SIZE 1028
typedef struct {
    char chars[INPUT_BUFFER_SIZE];
    unsigned int length;
    int cursorAt;
} InputBuffer;


typedef struct {
    char *string;
    LifeSpan lifeSpan;
    GameState *state;
    
} PrintInfoDelayed;

// TODO(Oliver): Can we make this into a generic array class with preproccesor? 4/2/18
typedef struct {
    int count;
    PrintInfoDelayed E[32];
} PrintInfoDelayedArray;

typedef struct {
    char *options[32];
    int count;
} MenuOptions;

typedef enum {
    HOLDER_NULL,
    HOLDER_PLAYER,
    HOLDER_ROOM,
} ObjHolderType;

typedef struct {
    ObjHolderType type;
    union {
        struct {
            Player *player;
        };
        struct {
            Room *room;
        };
    };
} ObjHolder;

typedef enum {
    LINEAR, 
    SMOOTH_STEP_01,
    SMOOTH_STEP_00,
} LerpType;

typedef struct {
    float a;
    float b;
    
    float *val;
    Timer timer;
} Lerpf;

typedef struct {
    V4 a;
    V4 b;
    
    V4 *val;
    Timer timer;
} LerpV4;

typedef enum {
    VAR_V4,
    VAR_FLOAT,
} VarType;

float updateLerpf_(float tAt, Lerpf *f, LerpType lerpType, TimerReturnInfo timeInfo) {
    
    float lerpValue = 0;
    switch(lerpType) {
        case LINEAR: {
            lerpValue = lerp(f->a, tAt, f->b);
        } break;
        case SMOOTH_STEP_00: {
            lerpValue = smoothStep00(f->a, tAt, f->b);
        } break;
        case SMOOTH_STEP_01: {
            lerpValue = smoothStep01(f->a, tAt, f->b);
        } break;
        default: {
            invalidCodePathStr("Unhandled case in update lerp function\n");
        }
    }
    
    if(timeInfo.finished) {
        lerpValue = f->b;
        f->timer.value = -1;
    }
    return lerpValue;
}

V4 updateLerpV4_(float tAt, LerpV4 *f, LerpType lerpType, TimerReturnInfo timeInfo) {
    
    V4 lerpValue = {};
    switch(lerpType) {
        case LINEAR: {
            lerpValue = lerpV4(f->a, tAt, f->b);
        } break;
        case SMOOTH_STEP_00: {
            lerpValue = smoothStep00V4(f->a, tAt, f->b);
        } break;
        case SMOOTH_STEP_01: {
            lerpValue = smoothStep01V4(f->a, tAt, f->b);
        } break;
        default: {
            invalidCodePathStr("Unhandled case in update lerp function\n");
        }
    }
    
    if(timeInfo.finished) {
        lerpValue = f->b;
        f->timer.value = -1;
    }
    return lerpValue;
}

void updateLerpGeneral_(void *lerpStruct, Timer *timer, float dt, void *valPrt, LerpType lerpType, VarType varType) {
    if(timer->value >= 0 && valPrt) { 
        TimerReturnInfo timeInfo = updateTimer(timer, dt);
        
        switch(varType) {
            case VAR_FLOAT: {
                Lerpf *f = (Lerpf *)lerpStruct;
                *(f->val) = updateLerpf_(timeInfo.canonicalVal, f, lerpType, timeInfo);
            } break;
            case VAR_V4: {
                LerpV4 *f = (LerpV4 *)lerpStruct;
                *(f->val) = updateLerpV4_(timeInfo.canonicalVal, f, lerpType, timeInfo);
            } break;
        }
        
    }
}

void updateLerpf(Lerpf *f, float dt, LerpType lerpType) {
    updateLerpGeneral_(f, &f->timer, dt, f->val, lerpType, VAR_FLOAT);
}


void updateLerpV4(LerpV4 *f, float dt, LerpType lerpType) {
    updateLerpGeneral_(f, &f->timer, dt, f->val, lerpType, VAR_V4);
}

void setLerpInfof(Lerpf *f, float a, float b, float period, float *val) {
    f->a = a;
    f->b = b;
    f->timer = initTimer(period);
    f->val = val;
}


void setLerpInfof_s(Lerpf *f, float b, float period, float *val) {
    f->a = *val;
    f->b = b;
    f->timer = initTimer(period);
    f->val = val;
}


void setLerpInfoV4_s(LerpV4 *f, V4 b, float period, V4 *val) {
    f->a = *val;
    f->b = b;
    f->timer = initTimer(period);
    f->val = val;
}


bool isOn(Timer *timer) {
    bool result = timer->value >= 0;
    return result;
}

LerpV4 initLerpV4() {
    LerpV4 a = {};
    a.timer.value = -1; //turn off timer;
    return a;
}

Lerpf initLerpf() {
    Lerpf a = {};
    a.timer.value = -1; //turn off timer;
    return a;
}