#if !defined EASY_MATH_H
/*
New File
*/

#define sqr(a) (a*a)

float max(float a, float b) {
    float result = (a < b) ? b : a;
    return result;
}

float min(float a, float b) {
    float result = (a > b) ? b : a;
    return result;
}

float safeRatio0(float a, float b) {
    float result = 0;
    if(b != 0) {
        result = a / b;
    }
    return result;
}

typedef union {
    struct {
        float x, y;
    };
} V2;

typedef union {
    struct {
        float x, y, z;
    };
    struct {
        V2 xy;
        float _ignore1;
    };
} V3;

typedef union {
    struct {
        float x, y, z, w;
    }; 
    struct {
        V2 xy;
        float _ignore1;
        float _ignore2;
    };
    struct {
        V3 xyz;
        float _ignore3;
    };
} V4;

V2 v2(float x, float y) {
    V2 result = {};
    result.x = x;
    result.y = y;
    return result;
}

V4 v4(float x, float y, float z, float w) {
    V4 result = {};
    result.x = x;
    result.y = y;
    result.z = z;
    result.w = w;
    return result;
}

V3 v3(float x, float y, float z) {
    V3 result = {};
    result.x = x;
    result.y = y;
    result.z = z;
    
    return result;
}

V2 v2_minus(V2 a, V2 b) {
    V2 result = {a.x - b.x, a.y - b.y};
    return result;
}

V2 v2_plus(V2 a, V2 b) {
    V2 result = {a.x + b.x, a.y + b.y};
    return result;
}

V2 v2_scale(float a, V2 b) {
    V2 result = v2(a*b.x, a*b.y);
    return result;
}

V2 v2_hadamard(V2 a, V2 b) {
    V2 result = v2(a.x*b.x, a.y*b.y);
    return result;
}

V2 v2_inverseHadamard(V2 a, V2 b) {
    //Change to safe ratio
    V2 result = v2(safeRatio0(a.x, b.x), safeRatio0(a.y, b.y));
    return result;
}

float getLength(V2 a) {
    float result = sqrt(sqr(a.x) + sqr(a.y));
    return result;
}

V2 normalize_(V2 a, float len) {
    V2 result = v2(a.x / len, a.y / len);
    return result;
}

V4 v4_minus(V4 a, V4 b) {
    V4 result = {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
    return result;
}

V4 v4_plus(V4 a, V4 b) {
    V4 result = {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
    return result;
}

V4 v4_scale(float a, V4 b) {
    V4 result = v4(a*b.x, a*b.y, a*b.z, a*b.w);
    return result;
}

V4 v2ToV4Homogenous(V2 a) {
    V4 result = v4(a.x, a.y, 0, 1);
    return result;
}

typedef union {
    struct {
        float minX;
        float minY;
        float maxX;
        float maxY;
    };
    struct {
        V2 min;
        V2 max;
    };
} Rect2f;

Rect2f rect2f(float minX, 
              float minY, 
              float maxX, 
              float maxY) {
    
    Rect2f result = {};
    
    result.minX = minX;
    result.maxX = maxX;
    result.minY = minY;
    result.maxY = maxY;
    
    return result;
    
}

Rect2f rect2fNull() {
    Rect2f result = {};
    return result;
}

Rect2f rect2fMinDim(float minX, 
                    float minY, 
                    float dimX, 
                    float dimY) {
    
    Rect2f result = {};
    
    result.minX = minX;
    result.maxX = minX + dimX;
    result.minY = minY;
    result.maxY = minY + dimY;
    
    return result;
    
}

Rect2f rect2fCenterDim(float centerX, 
                       float centerY, 
                       float dimX, 
                       float dimY) {
    
    Rect2f result = {};
    
    float halfDimX = 0.5f*dimX;
    float halfDimY = 0.5f*dimY;
    
    result.minX = centerX - halfDimX;
    result.minY = centerY - halfDimY;
    result.maxX = centerX + halfDimX;
    result.maxY = centerY + halfDimY;
    
    return result;
    
}


Rect2f rect2fMinDimV2(V2 a, V2 b) {
    
    Rect2f result = rect2fMinDim(a.x, a.y, b.x, b.y);
    
    return result;
}

Rect2f reevalRect2f(Rect2f rect) {
    Rect2f result = {};
    result.min = v2(min(rect.minX, rect.maxX), min(rect.maxY, rect.minY));
    result.max = v2(max(rect.maxX, rect.minX), max(rect.maxY, rect.minY));
    return result;
}

#define INFINITY_VALUE 1000000
Rect2f InverseInfinityRect2f() {
    Rect2f result = {};
    
    result.minX = INFINITY_VALUE;
    result.maxX = -INFINITY_VALUE;
    result.minY = INFINITY_VALUE;
    result.maxY = -INFINITY_VALUE;
    
    return result;
}

Rect2f unionRect2f(Rect2f a, Rect2f b) {
    Rect2f result = {};
    
    result.minX = (a.minX > b.minX) ? b.minX : a.minX;
    result.minY = (a.minY > b.minY) ? b.minY : a.minY;
    result.maxX = (a.maxX < b.maxX) ? b.maxX : a.maxX;
    result.maxY = (a.maxY < b.maxY) ? b.maxY : a.maxY;
    
    return result;
    
}

bool inRect(V2 p, Rect2f rect) {
    bool result = (p.x >= rect.minX &&
                   p.y >= rect.minY &&
                   p.x < rect.maxX &&
                   p.y < rect.maxY);
    
    return result;
}

V2 getDim(Rect2f b) {
    V2 res = v2(b.maxX - b.minX, b.maxY - b.minY);
    return res;
}

V2 getCenter(Rect2f b) {
    V2 res = v2(b.minX + 0.5f*(b.maxX - b.minX), b.minY + 0.5f*(b.maxY - b.minY));
    return res;
}

//matrix operations
//matrix operations
typedef union {
    struct {
        float E[2][2];
    };
    struct {
        V2 a;
        V2 b;
    };
} Matrix2;

Matrix2 mat2() {
    Matrix2 result = {{
            1, 0,
            0, 1
        }};
    return result;
}

V2 mat2_project(Matrix2 a, V2 b) {
    V2 result = v2_plus(v2_scale(b.x, a.a),  v2_scale(b.y, a.b));
    return result;
}

typedef union {
    struct {
        float E[4][4];
    };
    struct {
        V4 a;
        V4 b;
        V4 c;
        V4 d;
    };
} Matrix4;

Matrix4 mat4() {
    Matrix4 result = {{
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        }};
    return result;
}

Matrix4 mat4_xyAxis(V2 xAxis, V2 yAxis) {
    Matrix4 result = {{
            xAxis.x, xAxis.y, 0, 0,
            yAxis.x, yAxis.y, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        }};
    return result;
}

Matrix4 mat4_xyzAxis(V3 xAxis, V3 yAxis, V3 zAxis) {
    Matrix4 result = {{
            xAxis.x, xAxis.y, xAxis.z, 0,
            yAxis.x, yAxis.y, yAxis.z, 0,
            zAxis.x, zAxis.y, zAxis.z, 0,
            0, 0, 0, 1
        }};
    return result;
}

Matrix4 Matrix4_translate(Matrix4 a, V3 b) {
    a.d.x += b.x;
    a.d.y += b.y;
    a.d.z += b.z;
    return a;
}

Matrix4 Mat4Mult(Matrix4 a, Matrix4 b) {
    //SIMD this. Can we use V4 instead of float 
    Matrix4 result = {};
    
    for(int i = 0; i < 4; ++i) {
        for(int j = 0; j < 4; ++j) {
            
            result.E[i][j] = 
                a.E[0][j] * b.E[i][0] + 
                a.E[1][j] * b.E[i][1] + 
                a.E[2][j] * b.E[i][2] + 
                a.E[3][j] * b.E[i][3];
            
        }
    }
    
    return result;
}

V4 V4MultMat4(V4 a, Matrix4 b) {
    V4 result = {};
    
    V4 x = v4_scale(a.x, b.a);
    V4 y = v4_scale(a.y, b.b);
    V4 z = v4_scale(a.z, b.c);
    V4 w = v4_scale(a.w, b.d);
    
    result = v4_plus(v4_plus(x, y), v4_plus(z, w));
    return result;
}

Matrix4 mat4TopLeftToBottomLeft(float bufferHeight) {
    Matrix4 transform = mat4_xyAxis(v2(1, 0), v2(0, -1));
    transform = Matrix4_translate(transform, v3(0, bufferHeight, 0));
    return transform;
}

float lerp(float a, float t, float b) {
    float value = a + t*(b - a);
    return value;
}


float smoothStep01(float a, float t, float b) {
    float mappedT = sin(t*PI32/2);
    float value = lerp(a, mappedT, b);
    return value;
}

float smoothStep00(float a, float t, float b) {
    float mappedT = sin(t*PI32);
    float value = lerp(a, mappedT, b);
    return value;
}


V4 lerpV4(V4 a, float t, V4 b) {
    V4 value = {};
    
    value.x = lerp(a.x, t, b.x);
    value.y = lerp(a.y, t, b.y);
    value.z = lerp(a.z, t, b.z);
    value.w = lerp(a.w, t, b.w);
    
    return value;
}


V4 smoothStep01V4(V4 a, float t, V4 b) {
    V4 value = {};
    
    value.x = smoothStep01(a.x, t, b.x);
    value.y = smoothStep01(a.y, t, b.y);
    value.z = smoothStep01(a.z, t, b.z);
    value.w = smoothStep01(a.w, t, b.w);
    
    return value;
}

V4 smoothStep00V4(V4 a, float t, V4 b) {
    V4 value = {};
    
    value.x = smoothStep00(a.x, t, b.x);
    value.y = smoothStep00(a.y, t, b.y);
    value.z = smoothStep00(a.z, t, b.z);
    value.w = smoothStep00(a.w, t, b.w);
    
    return value;
}


#define EASY_MATH_H 1
#endif