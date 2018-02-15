
#define SDL_AUDIO_CALLBACK(name) void name(void *udata, unsigned char *stream, int len)
typedef SDL_AUDIO_CALLBACK(sdl_audio_callback);

typedef struct {
    unsigned int size;
    unsigned char *data;
    char *fileName;
} WavFile;

typedef struct WavFilePtr WavFilePtr;
typedef struct WavFilePtr {
    WavFile *file;
    
    char *name;
    
    WavFilePtr *next;
} WavFilePtr;


typedef struct PlayingSound{
    WavFile *wavFile;
    unsigned int bytesAt;
    
    bool loop;
    
    struct PlayingSound *next;
} PlayingSound;

static PlayingSound *playingSoundsFreeList;
static PlayingSound *playingSounds;

WavFilePtr *sounds[4096];

int getSoundHashKey(char *at, int maxSize) {
    int hashKey = 0;
    while(*at) {
        //Make the has look up different prime numbers. 
        hashKey += (*at)*19;
        at++;
    }
    hashKey %= maxSize;
    return hashKey;
}

WavFile *findSound(char *fileName) {
    int hashKey = getSoundHashKey(fileName, arrayCount(sounds));
    
    WavFilePtr *file = sounds[hashKey];
    WavFile *result = 0;
    
    bool found = false;
    
    while(!found) {
        if(!file) {
            found = true; 
        } else {
            assert(file->file);
            assert(file->name);
            if(cmpStrNull(fileName, file->name)) {
                result = file->file;
                found = true;
            } else {
                file = file->next;
            }
        }
    }
    return result;
}

char *lastFilePortion(char *at) {
    // TODO(Oliver): Make this more robust
    char *recent = at;
    while(*at) {
        if(*at == '/' && at[1] != '\0') { 
            recent = (at + 1); //plus 1 to pass the slash
        }
        at++;
    }
    
    int length = (int)(at - recent);
    char *result = (char *)calloc(length, 1);
    
    memcpy(result, recent, length);
    
    return result;
}

void addSound(WavFile *sound) {
    char *truncName = lastFilePortion(sound->fileName);
    int hashKey =getSoundHashKey(truncName, arrayCount(sounds));
    
    WavFilePtr **filePtr = sounds + hashKey;
    
    bool found = false; 
    while(!found) {
        WavFilePtr *file = *filePtr;
        if(!file) {
            file = calloc(sizeof(WavFilePtr), 1);
            file->file = sound;
            file->name = truncName;
            *filePtr = file;
            found = true;
        } else {
            filePtr = &file->next;
        }
    }
    assert(found);
    
}

PlayingSound *playSound(Arena *arena, WavFile *wavFile, bool loop) {
    PlayingSound *result = playingSoundsFreeList;
    if(result) {
        playingSoundsFreeList = result->next;
    } else {
        result = pushStruct(arena, PlayingSound);
    }
    assert(result);
    
    //add to playing sounds. 
    result->next = playingSounds;
    playingSounds = result;
    
    result->loop = loop;
    result->bytesAt = 0;
    result->wavFile = wavFile;
    return result;
}

void loadWavFile(WavFile *result, char *fileName, SDL_AudioSpec *audioSpec) {
    
    SDL_AudioSpec* outputSpec = SDL_LoadWAV(fileName, audioSpec, &result->data, &result->size);
    result->fileName = fileName;
    
    addSound(result);
    
    if(outputSpec) {
        assert(audioSpec->freq == outputSpec->freq);
        assert(audioSpec->format = outputSpec->format);
        assert(audioSpec->channels == outputSpec->channels);   
        assert(audioSpec->samples == outputSpec->samples);
    } else {
        fprintf(stderr, "Couldn't open wav file: %s\n", SDL_GetError());
    }
}

SDL_AudioSpec initAudioSpec(int frequency, sdl_audio_callback *callback) {
    SDL_AudioSpec audioSpec = {};
    /* Set the audio format */
    audioSpec.freq = frequency;
    audioSpec.format = AUDIO_S16;
    audioSpec.channels = 2;    /* 1 = mono, 2 = stereo */
    audioSpec.samples = 1024;  /* Good low-latency value for callback */
    audioSpec.callback = callback;
    audioSpec.userdata = NULL;
    return audioSpec;
}

bool initAudio(SDL_AudioSpec *audioSpec) {
    bool successful = true;
    /* Open the audio device, forcing the desired format */
    if ( SDL_OpenAudio(audioSpec, NULL) < 0 ) {
        fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
        successful = false;
    }
    return successful;
}


SDL_AUDIO_CALLBACK(audioCallback) {
    SDL_memset(stream, 0, len);
    for(PlayingSound **soundPrt = &playingSounds;
        *soundPrt; 
        ) {
        bool advancePtr = true;
        PlayingSound *sound = *soundPrt;
        unsigned char *samples = sound->wavFile->data + sound->bytesAt;
        int remainingBytes = sound->wavFile->size - sound->bytesAt;
        
        assert(remainingBytes >= 0);
        
        unsigned int bytesToWrite = (remainingBytes < len) ? remainingBytes: len;
        
        SDL_MixAudio(stream, samples, bytesToWrite, SDL_MIX_MAXVOLUME);
        
        sound->bytesAt += bytesToWrite;
        
        if(sound->bytesAt >= sound->wavFile->size) {
            assert(sound->bytesAt == sound->wavFile->size);
            if(sound->loop) {
                sound->bytesAt = 0;
            } else {
                //remove from linked list
                advancePtr = false;
                *soundPrt = sound->next;
                sound->next = playingSoundsFreeList;
                playingSoundsFreeList = sound;
            }
        }
        
        if(advancePtr) {
            soundPrt = &((*soundPrt)->next);
        }
    }
}
