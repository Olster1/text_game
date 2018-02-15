/*
File for easy opengl drawing functions for game mock ups or quick drawing, using the fixed function openGl pipeline. You have to link with opengl and include opengl header file in your project. 
*/

#define PI32 3.14159265359
#define HALF_PI32 0.5f*PI32

#if !defined arrayCount
#define arrayCount(arg) (sizeof(arg) / sizeof(arg[0])) 
#endif

#if !defined EASY_MATH_H
#include "easy_math.h"
#endif

GLuint openGlLoadTexture(int width, int height, void *imageData) {
    GLuint resultId;
    glGenTextures(1, &resultId);
    
    glBindTexture(GL_TEXTURE_2D, resultId);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageData);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    return resultId;
}

void openGlDrawLine(V2 begin, V2 end, V4 color, float thickness, Matrix4 offsetTransform) {
    glDisable(GL_TEXTURE_2D);
    glMatrixMode(GL_MODELVIEW);
    
    begin = V4MultMat4(v2ToV4Homogenous(begin), offsetTransform).xy; 
    
    end = V4MultMat4(v2ToV4Homogenous(end), offsetTransform).xy; 
    
    V2 rel = v2_minus(end, begin);
    float len = getLength(rel);
    rel = normalize_(rel, len);
    
    float a1 = rel.x;
    float a2 = rel.y;
    float b1 = -rel.y;
    float b2 = rel.x;
    
    float rotationMat[] = {
        a1,  a2,  0,  0,
        b1,  b2,  0,  0,
        0,  0,  1,  0,
        begin.x, begin.y, 0,  1
    };
    
    glLoadMatrixf(rotationMat);
    
    glColor4f(color.x, color.y, color.z, color.w);
    float halfThickness = 0.5f*thickness;
    
    glBegin(GL_TRIANGLES);
    glTexCoord2f(0, 0);
    glVertex2f(0, -halfThickness);
    glTexCoord2f(0, 1);
    glVertex2f(0, halfThickness);
    glTexCoord2f(1, 1);
    glVertex2f(len, halfThickness);
    
    glTexCoord2f(0, 0);
    glVertex2f(0, -halfThickness);
    glTexCoord2f(1, 0);
    glVertex2f(len, -halfThickness);
    glTexCoord2f(1, 1);
    glVertex2f(len, halfThickness);
    glEnd();
    
    glLoadIdentity();
    glEnable(GL_TEXTURE_2D);
}

void openGlDrawRectOutlineCenterDim(V2 center, V2 dim, V4 color, float rot, Matrix4 offsetTransform) {
    glDisable(GL_TEXTURE_2D);
    
    glMatrixMode(GL_MODELVIEW);
    
    V2 deltaP = V4MultMat4(v2ToV4Homogenous(center), offsetTransform).xy; 
    
    float a1 = cos(rot);
    float a2 = sin(rot);
    float b1 = cos(rot + HALF_PI32);
    float b2 = sin(rot + HALF_PI32);
    
    float rotationMat[] = {
        a1,  a2,  0,  0,
        b1,  b2,  0,  0,
        0,  0,  1,  0,
        deltaP.x, deltaP.y, 0,  1
    };
    
    glColor4f(color.x, color.y, color.z, color.w);
    
    V2 halfDim = v2(0.5f*dim.x, 0.5f*dim.y);
    
    float rotations[4] = {
        0,
        PI32 + HALF_PI32,
        PI32,
        HALF_PI32
    };
    
    float lengths[4] = {
        dim.y,
        dim.x, 
        dim.y,
        dim.x,
    };
    
    V2 offsets[4] = {
        v2(-halfDim.x, 0),
        v2(0, halfDim.y),
        v2(halfDim.x, 0),
        v2(0, -halfDim.y),
    };
    
    float thickness = 4;
    
    for(int i = 0; i < 4; ++i) {
        
        glLoadMatrixf(rotationMat);
        
        float rotat = rotations[i];
        float halfLen = 0.5f*lengths[i];
        V2 offset = offsets[i];
        
        float rotationMat1[] = {
            cos(rotat),  sin(rotat),  0,  0,
            -sin(rotat),  cos(rotat),  0,  0,
            0,  0,  1,  0,
            offset.x, offset.y, 0,  1
        };
        glMultMatrixf(rotationMat1);
        
        glBegin(GL_TRIANGLES);
        glTexCoord2f(0, 0);
        glVertex2f(0, -halfLen);
        glTexCoord2f(0, 1);
        glVertex2f(0, halfLen);
        glTexCoord2f(1, 1);
        glVertex2f(thickness, halfLen);
        
        glTexCoord2f(0, 0);
        glVertex2f(0, -halfLen);
        glTexCoord2f(1, 0);
        glVertex2f(thickness, -halfLen);
        glTexCoord2f(1, 1);
        glVertex2f(thickness, halfLen);
        glEnd();
    }
    glLoadIdentity();
    glEnable(GL_TEXTURE_2D);
    
}

void openGlDrawRectOutlineRect2f(Rect2f rect, V4 color, float rot, Matrix4 offsetTransform) {
    openGlDrawRectOutlineCenterDim(getCenter(rect), getDim(rect), color, rot, offsetTransform);
}

void openGlDrawRectCenterDim_(V2 center, V2 dim, V4 color, float rot, Matrix4 offsetTransform) {
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float a1 = cos(rot);
    float a2 = sin(rot);
    float b1 = cos(rot + HALF_PI32);
    float b2 = sin(rot + HALF_PI32);
    
    V4 centerV4 = v2ToV4Homogenous(center);
    
    V2 deltaP = V4MultMat4(centerV4, offsetTransform).xy; 
    float rotationMat[] = {
        a1,  a2,  0,  0,
        b1,  b2,  0,  0,
        0,  0,  1,  0,
        deltaP.x, deltaP.y, 0,  1
    };
    
    glMultMatrixf(rotationMat);
    
    glColor4f(color.x, color.y, color.z, color.w);
    
    
    V2 halfDim = v2(0.5f*dim.x, 0.5f*dim.y);
    
    glBegin(GL_TRIANGLES);
    glTexCoord2f(0, 0);
    glVertex2f(-halfDim.x, -halfDim.y);
    glTexCoord2f(0, 1);
    glVertex2f(-halfDim.x, halfDim.y);
    glTexCoord2f(1, 1);
    glVertex2f(halfDim.x, halfDim.y);
    
    glTexCoord2f(0, 0);
    glVertex2f(-halfDim.x, -halfDim.y);
    glTexCoord2f(1, 0);
    glVertex2f(halfDim.x, -halfDim.y);
    glTexCoord2f(1, 1);
    glVertex2f(halfDim.x, halfDim.y);
    glEnd();
    
}

void openGlDrawRectCenterDim(V2 center, V2 dim, V4 color, float rot, Matrix4 offsetTransform) {
    glDisable(GL_TEXTURE_2D);
    
    openGlDrawRectCenterDim_(center, dim, color, rot, offsetTransform);
    glLoadIdentity();
    glEnable(GL_TEXTURE_2D);
}


void openGlTextureCentreDim(GLuint textureId, V2 center, V2 dim, V4 color, float rot, Matrix4 offsetTransform) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureId); 
    openGlDrawRectCenterDim_(center, dim, color, rot, offsetTransform);
    glBindTexture(GL_TEXTURE_2D, 0); 
    glLoadIdentity();
}


void openGlDrawCircle(V2 center, V2 dim, V4 color) {
    glMatrixMode(GL_MODELVIEW);
    glDisable(GL_TEXTURE_2D);
    
    float rotationMat[] = {
        1,  0,  0,  0,
        0,  1,  0,  0,
        0,  0,  1,  0,
        center.x, center.y, 0,  1
    };
    
    glLoadMatrixf(rotationMat);
    
    V4 colors[8] = {
        v4(1, 0, 0, 1), 
        v4(1, 1, 0, 1), 
        v4(1, 1, 1, 1), 
        v4(1, 0, 1, 1), 
        v4(0, 0, 1, 1), 
        v4(0, 1, 1, 1), 
        v4(0, 1, 0, 1), 
        v4(0, 0, 0, 1), 
    };
    
    int colorAt = 0;
    
    glColor4f(color.x, color.y, color.z, color.w);
    
    glBegin(GL_TRIANGLES);
    
    float radAt = 0;
    int numOfTriangles = 20;
    float rad_dt = 2*PI32 / (float)numOfTriangles;
    
    V2 p1 = v2(dim.x*cos(radAt), dim.y*sin(radAt)); 
    V2 firstP = p1;
    for(int i = 0 ; i < numOfTriangles; ++i) {
        
        radAt += rad_dt;
        V2 p2 = v2(dim.x*cos(radAt), dim.y*sin(radAt));
        
        glTexCoord2f(0, 0);
        glVertex2f(0, 0);
        glTexCoord2f(0, 1);
        glVertex2f(p1.x, p1.y);
        glTexCoord2f(1, 1);
        glVertex2f(p2.x, p2.y);
        
        p1 = p2;
    }
    
    glEnd();
    
    glLoadIdentity();
    glEnable(GL_TEXTURE_2D);
    
}



