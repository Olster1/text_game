/*
File to enable writing with fonts easy to add to your project. Uses Sean Barret's stb_truetype file to load and write the fonts. Requires a current openGL context. Right now uses fixed function pipeline. 
TODO: 
Move to programmable pipeline. 
*/

#define STB_TRUETYPE_IMPLEMENTATION 
#include "stb_truetype.h"

typedef struct {
    int index;
    V4 color;
} CursorInfo;

typedef struct {
    stbtt_bakedchar *cdata;
    GLuint handle;
    
} Font;

Font initFont_(char *fileName, char firstChar, char numOfChars)
{
    Font result = {}; 
    int bitmapW = 512;
    int bitmapH = 512;
    int numOfPixels = bitmapH*bitmapW;
    unsigned char ttf_buffer[1<<20];
    unsigned char temp_bitmap[numOfPixels]; //bakfontbitmap is one byte per pixel. 
    
    FILE *fileHandle = fopen(fileName, "rb");
    size_t fileSize = getFileSize(fileHandle);
    assert(sizeof(ttf_buffer) > fileSize);
    
    fread(ttf_buffer, 1, fileSize, fileHandle);
    
    //TODO: do we want to use an arena for this?
    result.cdata = calloc(numOfChars*sizeof(stbtt_bakedchar), 1);
    //
    
    stbtt_BakeFontBitmap(ttf_buffer,0, fontHeight, temp_bitmap, bitmapW, bitmapH, firstChar, numOfChars, result.cdata);
    // no guarantee this fits!
    
    // NOTE(Oliver): We expand the the data out from 8 bits per pixel to 32 bits per pixel. It doens't matter that the char data is based on the smaller size since getBakedQuad divides by the width of original, smaller bitmap. 
    
    // TODO(Oliver): can i free this once its sent to the graphics card? 
    unsigned int *bitmapTexture = (unsigned int *)pushSize(&arena, sizeof(unsigned int)*numOfPixels);
    unsigned char *src = temp_bitmap;
    unsigned int *dest = bitmapTexture;
    for(int y = 0; y < bitmapH; ++y) {
        for(int x = 0; x < bitmapW; ++x) {
            unsigned int alpha = *src++;
            *dest = 
                alpha << 24 |
                alpha << 16 |
                alpha << 8 |
                alpha << 0;
            dest++;
        }
    }
    ////////////////
    glGenTextures(1, &result.handle);
    glBindTexture(GL_TEXTURE_2D, result.handle);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP); 
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE); 
    
    //Load texture to graphics card.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 512, 512, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, bitmapTexture);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    return result;
}

Font initFont(char *fileName) {
    Font result = initFont_(fileName, 32, 96); // ASCII 32..126 is 95 glyphs
    return result;
}

typedef struct {
    stbtt_aligned_quad q;
    int index;
    V2 lastXY;
} GlyphInfo;

Rect2f my_stbtt_print_(Font *font, float x, float y, float bufferWidth, float bufferHeight, char *text_, Rect2f margin, V4 color, float size, CursorInfo *cursorInfo, bool display) {
    
    Rect2f bounds = InverseInfinityRect2f();
    glMatrixMode(GL_MODELVIEW);
    float modelMatrix[] = {
        size,  0,  0,  0, 
        0, size,  0,  0, 
        0, 0,  1,  0, 
        0, 0, 0,  1
    };
    glLoadMatrixf(modelMatrix); 
    
    glColor4f(color.x, color.y, color.z, color.w);
    float width = 0; 
    float height = 0;
    V2 pos = v2(0, 0);
    
    Rect2f points[512] = {};
    int pC = 0;
    
    V2 points1[512] = {};
    int pC1 = 0;
    
    GlyphInfo qs[256] = {};
    int quadCount = 0;
    
    char *text = text_;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, font->handle);
    glBegin(GL_QUADS);
    
    V2 lastXY = v2(x, y);
    
    bool inWord = false;
    bool hasBeenToNewLine = false;
    char *tempAt = text;
    while (*text) {
        bool increment = true;
        bool drawText = false;
        points1[pC1++] = v2(x, y);
        
        if (((int)(*text) >= 32 && (int)(*text) < 128)) {
            stbtt_aligned_quad q  = {};
            stbtt_GetBakedQuad(font->cdata, 512, 512, *text-32, &x,&y,&q, 1);
            GlyphInfo *glyph = qs + quadCount++; 
            glyph->q = q;
            glyph->index = (int)(text - text_);
            glyph->lastXY = lastXY;;
        }
        
        bool overflowed = (x > margin.maxX && inWord);
        
        if(*text == ' ' || *text == '\n') {
            inWord = false;
            hasBeenToNewLine = false;
            drawText = true;
        } else {
            inWord = true;
        }
        
        if(overflowed && !hasBeenToNewLine && inWord) {
            text = tempAt;
            increment = false;
            quadCount = 0;  //empty the quads. We lose the data because we broke the word.  
            hasBeenToNewLine = true;
        }
        
        if(overflowed || *text == '\n') {
            x = margin.minX;
            y += 32; 
        }
        
        bool lastCharacter = (*(text + 1) == '\0');
        
        if(drawText || lastCharacter) {
            for(int i = 0; i < quadCount; ++i) {
                GlyphInfo *glyph = qs + i;
                stbtt_aligned_quad q = glyph->q;
                Rect2f b = rect2f(q.x0, q.y0, q.x1, q.y1);
                bounds = unionRect2f(bounds, b);
                points[pC++] = b;
                
                if(display) {
                    glTexCoord2f(q.s0, q.t1); glVertex2f(q.x0, -1*q.y1 + bufferHeight);
                    glTexCoord2f(q.s1,q.t1); glVertex2f(q.x1,-1*q.y1 + bufferHeight);
                    glTexCoord2f(q.s1,q.t0); glVertex2f(q.x1,-1*q.y0 + bufferHeight);
                    glTexCoord2f(q.s0,q.t0); glVertex2f(q.x0,-1*q.y0 + bufferHeight);
                }
                if(cursorInfo && (glyph->index == cursorInfo->index)) {
                    width = q.x1 - q.x0;
                    height = 32;//q.y1 - q.y0;
                    pos = v2(glyph->lastXY.x + 0.5f*width, glyph->lastXY.y - 0.5f*height);
                }
                
            }
            
            quadCount = 0; //empty the quads now that we have finished drawing them
        }
        
        lastXY.x = x;
        lastXY.y = y;
        if(increment) {
            ++text;
        }
        if(!inWord) {
            tempAt = text;
        }
    }
    glEnd();
    
    glLoadIdentity();
    
    if(cursorInfo && (int)(text - text_) <= cursorInfo->index) {
        width = 16;
        height = 32;
        
        pos = v2(x + 0.5f*width, y - 0.5f*height);
    }
    
    if(cursorInfo) {
        openGlDrawRectCenterDim(pos, v2(width, height), cursorInfo->color, 0, mat4TopLeftToBottomLeft(bufferHeight));
    }
#if 0 //draw idvidual text boxes
    for(int i = 0; i < pC; ++i) {
        Rect2f b = points[i];
        V2 d = points1[i];
        
        float w = b.maxX - b.minX;
        float h = b.maxY - b.minY;
        V2 c = v2(b.minX + 0.5f*w, b.minY + 0.5f*h);
        openGlDrawRectOutlineCenterDim(c, v2(w, h), v4(1, 0, 0, 1), 0, v2(1, -1), v2(0, bufferHeight));
        
    }
#endif
#if 0 //draw text bounds
    openGlDrawRectOutlineCenterDim(getCenter(bounds), getDim(bounds), v4(1, 0, 0, 1), 0, v2(1, -1), v2(0, bufferHeight));
#endif
    
    V2 boundsDim = getDim(bounds);
    bounds = rect2f(0, 0, boundsDim.x, boundsDim.y);
    return bounds;
    
}

Rect2f outputText_with_cursor(Font *font, float x, float y, float bufferWidth, float bufferHeight, char *text, Rect2f margin, V4 color, float size, int cursorIndex, V4 cursorColor, bool display) {
    CursorInfo cursorInfo = {};
    cursorInfo.index = cursorIndex;
    cursorInfo.color = cursorColor;
    
    Rect2f result = my_stbtt_print_(font, x, y, bufferWidth, bufferHeight, text, margin, color, size, &cursorInfo, display);
    return result;
}

Rect2f outputText(Font *font, float x, float y, float bufferWidth, float bufferHeight, char *text, Rect2f margin, V4 color, float size, bool display) {
    Rect2f result = my_stbtt_print_(font, x, y, bufferWidth, bufferHeight, text, margin, color, size, 0, display);
    return result;
}



