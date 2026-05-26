#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* ========== 常量定义 ========== */
#define SCR_WIDTH       800
#define SCR_HEIGHT      600
#define PI              3.14159265358979323846f

/* 花朵参数 */
#define PETAL_COUNT     6
#define PETAL_DETAIL    60
#define PETAL_SIZE      0.38f
#define PETAL_WIDTH     2.2f
#define CENTER_RADIUS   0.10f
#define CENTER_DETAIL   40

/* 粒子参数 */
#define PARTICLE_COUNT  120
#define PARTICLE_LIFE   3.5f

/* 顶点结构: 位置 + 颜色 */
typedef struct {
    float x, y;
    float r, g, b;
} Vertex;

/* 绘制单元 */
typedef struct {
    unsigned int offset;
    unsigned int count;
    unsigned int mode;
} DrawCmd;

/* 粒子 */
typedef struct {
    float x, y;
    float vx, vy;
    float life;
    float r, g, b;
} Particle;

/* 配色方案 */
typedef struct {
    float petal_cr, petal_cg, petal_cb;  /* 花瓣中心 */
    float petal_tr, petal_tg, petal_tb;  /* 花瓣尖端 */
    float ctr_cr,  ctr_cg,  ctr_cb;     /* 花心中心 */
    float ctr_er,  ctr_eg,  ctr_eb;     /* 花心边缘 */
    float stem_r,  stem_g,  stem_b;     /* 茎 */
    float leaf_dr, leaf_dg, leaf_db;    /* 叶子深色 */
    float leaf_lr, leaf_lg, leaf_lb;    /* 叶子亮色 */
} Palette;

/* ========== 全局数据 ========== */
static Vertex  g_vertices[2048];
static int     g_vertCount = 0;
static DrawCmd g_drawCmds[16];
static int     g_cmdCount  = 0;

static Particle g_particles[PARTICLE_COUNT];

static float    g_time       = 0.0f;
static float    g_bloomT     = 0.0f;   /* 绽放计时器 */
static int      g_paletteIdx = 0;
static double   g_mouseX     = 0.0;
static double   g_mouseY     = 0.0;
static int      g_winW       = SCR_WIDTH;
static int      g_winH       = SCR_HEIGHT;
static unsigned int g_flowerVAO = 0;
static unsigned int g_flowerVBO = 0;

/* 随机数 (简单 LCG) */
static unsigned int g_rand = 0xDEADBEEF;
static float randf(void) {
    g_rand = g_rand * 1103515245u + 12345u;
    return (float)((g_rand >> 16) & 0x7FFF) / 32767.0f;
}

/* ========== 预设配色 ========== */
static const Palette g_palettes[] = {
    /* 0: 经典玫瑰 */
    { 0.75f,0.15f,0.25f,  0.95f,0.50f,0.60f,
      0.95f,0.70f,0.10f,  1.00f,0.85f,0.20f,
      0.15f,0.65f,0.15f,
      0.10f,0.50f,0.10f,  0.20f,0.75f,0.25f },
    /* 1: 向日葵 */
    { 0.90f,0.65f,0.10f,  1.00f,0.85f,0.20f,
      0.50f,0.30f,0.05f,  0.70f,0.50f,0.10f,
      0.15f,0.55f,0.10f,
      0.10f,0.45f,0.10f,  0.20f,0.65f,0.20f },
    /* 2: 薰衣草 */
    { 0.55f,0.30f,0.65f,  0.80f,0.60f,0.95f,
      0.90f,0.70f,0.20f,  1.00f,0.85f,0.40f,
      0.20f,0.55f,0.25f,
      0.15f,0.45f,0.20f,  0.25f,0.65f,0.35f },
    /* 3: 樱花 */
    { 0.90f,0.65f,0.70f,  1.00f,0.85f,0.88f,
      0.95f,0.60f,0.40f,  1.00f,0.80f,0.60f,
      0.25f,0.55f,0.30f,
      0.20f,0.45f,0.25f,  0.30f,0.65f,0.40f },
};

/* ========== 辅助函数 ========== */
static void addVertex(float x, float y, float r, float g, float b)
{
    Vertex* v = &g_vertices[g_vertCount++];
    v->x = x;  v->y = y;
    v->r = r;  v->g = g;  v->b = b;
}

static void startFan(void)
{
    g_drawCmds[g_cmdCount].offset = (unsigned int)g_vertCount;
    g_drawCmds[g_cmdCount].mode   = GL_TRIANGLE_FAN;
    g_drawCmds[g_cmdCount].count  = 0;
}

static void endFan(void)
{
    DrawCmd* cmd = &g_drawCmds[g_cmdCount];
    cmd->count = (unsigned int)g_vertCount - cmd->offset;
    g_cmdCount++;
}

static void startStrip(void)
{
    g_drawCmds[g_cmdCount].offset = (unsigned int)g_vertCount;
    g_drawCmds[g_cmdCount].mode   = GL_TRIANGLE_STRIP;
    g_drawCmds[g_cmdCount].count  = 0;
}

static void endStrip(void)
{
    DrawCmd* cmd = &g_drawCmds[g_cmdCount];
    cmd->count = (unsigned int)g_vertCount - cmd->offset;
    g_cmdCount++;
}

/* ========== 生成花瓣 ========== */
static void generatePetal(float baseAngle,
                          float cr, float cg, float cb,
                          float tr, float tg, float tb)
{
    float cosB = cosf(baseAngle);
    float sinB = sinf(baseAngle);
    float centerOffset = 0.06f;
    float halfAngle = PI / (2.0f * PETAL_WIDTH);

    startFan();
    float cx = centerOffset * cosB;
    float cy = centerOffset * sinB;
    addVertex(cx, cy, cr, cg, cb);

    for (int s = 0; s <= PETAL_DETAIL; s++) {
        float t = -halfAngle + 2.0f * halfAngle * (float)s / (float)PETAL_DETAIL;
        float r = PETAL_SIZE * cosf(PETAL_WIDTH * t);
        float lx = r * sinf(t);
        float ly = centerOffset + r * cosf(t);
        float wx = lx * cosB - ly * sinB;
        float wy = lx * sinB + ly * cosB;
        float tipFactor = cosf(PETAL_WIDTH * t);
        float rr = cr + (tr - cr) * tipFactor;
        float gg = cg + (tg - cg) * tipFactor;
        float bb = cb + (tb - cb) * tipFactor;
        addVertex(wx, wy, rr, gg, bb);
    }
    endFan();
}

/* ========== 生成花心 ========== */
static void generateCenter(float cr, float cg, float cb,
                           float er, float eg, float eb)
{
    startFan();
    addVertex(0.0f, 0.0f, cr, cg, cb);
    for (int s = 0; s <= CENTER_DETAIL; s++) {
        float angle = 2.0f * PI * (float)s / (float)CENTER_DETAIL;
        addVertex(CENTER_RADIUS * cosf(angle),
                  CENTER_RADIUS * sinf(angle),
                  er, eg, eb);
    }
    endFan();
}

/* ========== 生成茎 ========== */
static void generateStem(float topY, float bottomY, float width,
                         float r, float g, float b)
{
    startStrip();
    addVertex(-width, topY,    r, g, b);
    addVertex( width, topY,    r, g, b);
    addVertex(-width, bottomY, r * 0.6f, g * 0.6f, b * 0.6f);
    addVertex( width, bottomY, r * 0.6f, g * 0.6f, b * 0.6f);
    endStrip();
}

/* ========== 生成叶子 ========== */
static void generateLeaf(float stemX, float stemY,
                         float leafAngle, float leafSize,
                         float dr, float dg, float db,
                         float lr, float lg, float lb)
{
    float cosB = cosf(leafAngle);
    float sinB = sinf(leafAngle);
    float halfAngle = PI / 2.2f;

    startFan();
    addVertex(stemX, stemY, dr, dg, db);
    for (int s = 0; s <= PETAL_DETAIL; s++) {
        float t = -halfAngle + 2.0f * halfAngle * (float)s / (float)PETAL_DETAIL;
        float radius = leafSize * cosf(2.2f * t);
        float lx = radius * sinf(t);
        float ly = radius * cosf(t);
        float wx = stemX + lx * cosB - ly * sinB;
        float wy = stemY + lx * sinB + ly * cosB;
        float tipFactor = fabsf(cosf(2.2f * t));
        float rr = dr + (lr - dr) * tipFactor;
        float gg = dg + (lg - dg) * tipFactor;
        float bb = db + (lb - db) * tipFactor;
        addVertex(wx, wy, rr, gg, bb);
    }
    endFan();
}

/* ========== 构建全部几何体 (使用指定配色) ========== */
static void buildFlowerGeometry(const Palette* pal)
{
    g_vertCount = 0;
    g_cmdCount  = 0;

    for (int p = 0; p < PETAL_COUNT; p++) {
        float angle = 2.0f * PI * (float)p / (float)PETAL_COUNT;
        float hue   = (float)p / (float)PETAL_COUNT;

        float cr = pal->petal_cr + 0.25f * hue * (pal->petal_tr - pal->petal_cr);
        float cg = pal->petal_cg + 0.10f * hue;
        float cb = pal->petal_cb + 0.10f * (1.0f - hue);
        float tr = pal->petal_tr;
        float tg = pal->petal_tg + 0.25f * (1.0f - hue);
        float tb = pal->petal_tb + 0.30f * hue;

        generatePetal(angle, cr, cg, cb, tr, tg, tb);
    }

    generateCenter(pal->ctr_cr, pal->ctr_cg, pal->ctr_cb,
                   pal->ctr_er, pal->ctr_eg, pal->ctr_eb);
    generateStem(-0.01f, -0.80f, 0.025f,
                 pal->stem_r, pal->stem_g, pal->stem_b);

    generateLeaf(0.0f, -0.40f, PI * 0.75f, 0.18f,
                 pal->leaf_dr, pal->leaf_dg, pal->leaf_db,
                 pal->leaf_lr, pal->leaf_lg, pal->leaf_lb);
    generateLeaf(0.0f, -0.55f, PI * 0.25f, 0.15f,
                 pal->leaf_dr, pal->leaf_dg, pal->leaf_db,
                 pal->leaf_lr, pal->leaf_lg, pal->leaf_lb);
}

/* ========== 粒子系统 ========== */
static void resetParticle(Particle* p)
{
    /* 在花头周围生成 */
    float angle = randf() * 2.0f * PI;
    float dist  = randf() * 0.35f;
    p->x  = cosf(angle) * dist;
    p->y  = sinf(angle) * dist - 0.02f;
    p->vx = (randf() - 0.5f) * 0.12f;
    p->vy = randf() * 0.25f + 0.08f;
    p->life = PARTICLE_LIFE * (0.3f + randf() * 0.7f);
    /* 暖金色花粉 */
    p->r = 1.0f;
    p->g = 0.75f + randf() * 0.25f;
    p->b = 0.15f + randf() * 0.30f;
}

static void initParticles(void)
{
    for (int i = 0; i < PARTICLE_COUNT; i++) {
        resetParticle(&g_particles[i]);
        g_particles[i].life = randf() * PARTICLE_LIFE; /* 错开生命周期 */
    }
}

static void updateParticles(float dt)
{
    for (int i = 0; i < PARTICLE_COUNT; i++) {
        Particle* p = &g_particles[i];
        p->life -= dt;
        if (p->life <= 0.0f) resetParticle(p);
        p->x  += p->vx * dt;
        p->y  += p->vy * dt;
        p->vx += (randf() - 0.5f) * 0.04f * dt; /* 轻微随机漂移 */
    }
}

/* ========== 着色器编译 ========== */
static unsigned int compileShader(unsigned int type, const char* source)
{
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &source, NULL);
    glCompileShader(id);

    int success;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(id, 512, NULL, info);
        printf("Shader compile error: %s\n", info);
    }
    return id;
}

static unsigned int createProgram(const char* vsSrc, const char* fsSrc)
{
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);

    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info[512];
        glGetProgramInfoLog(program, 512, NULL, info);
        printf("Program link error: %s\n", info);
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

/* ========== 回调 ========== */
static void cursorCallback(GLFWwindow* window, double x, double y)
{
    g_mouseX = x;
    g_mouseY = y;
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action != GLFW_PRESS) return;

    if (key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(window, 1);

    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_4) {
        g_paletteIdx = key - GLFW_KEY_1;
        const Palette* pal = &g_palettes[g_paletteIdx];
        buildFlowerGeometry(pal);
        /* 更新 VBO */
        glBindBuffer(GL_ARRAY_BUFFER, g_flowerVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        (long)(g_vertCount * sizeof(Vertex)), g_vertices);
        printf("Palette %d: %d vertices\n", g_paletteIdx + 1, g_vertCount);
    }
}

static void framebufferSizeCallback(GLFWwindow* window, int w, int h)
{
    g_winW = w;
    g_winH = h;
    glViewport(0, 0, w, h);
}

/* ========== 主函数 ========== */
int main(void)
{
    /* 1. 初始化 GLFW */
    if (!glfwInit()) {
        printf("Failed to initialize GLFW\n");
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
                                          "Little Flower - Press 1-4 to recolor!",
                                          NULL, NULL);
    if (!window) {
        printf("Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, cursorCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    /* 2. 初始化 GLAD */
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("Failed to initialize GLAD\n");
        glfwTerminate();
        return -1;
    }

    printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
    printf("GPU: %s\n", glGetString(GL_RENDERER));

    /* 3. 生成花朵几何体 */
    buildFlowerGeometry(&g_palettes[0]);
    printf("Generated %d vertices, %d draw commands\n", g_vertCount, g_cmdCount);

    /* 4. 初始化粒子 */
    initParticles();

    /* 5. VAO + VBO (花朵) */
    glGenVertexArrays(1, &g_flowerVAO);
    glGenBuffers(1, &g_flowerVBO);
    glBindVertexArray(g_flowerVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_flowerVBO);
    glBufferData(GL_ARRAY_BUFFER, (long)(g_vertCount * sizeof(Vertex)),
                 g_vertices, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    /* 6. VAO + VBO (粒子) */
    unsigned int partVAO, partVBO;
    glGenVertexArrays(1, &partVAO);
    glGenBuffers(1, &partVBO);
    glBindVertexArray(partVAO);
    glBindBuffer(GL_ARRAY_BUFFER, partVBO);
    glBufferData(GL_ARRAY_BUFFER, PARTICLE_COUNT * (long)sizeof(Vertex),
                 NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    /* 7. 花朵着色器 (带动画) */
    const char* flowerVS =
        "#version 330 core\n"
        "layout (location = 0) in vec2 aPos;\n"
        "layout (location = 1) in vec3 aColor;\n"
        "out vec3 vColor;\n"
        "uniform float uTime;\n"
        "uniform float uBloom;\n"
        "uniform vec2  uMouse;\n"
        "void main() {\n"
        "    vec2 pos = aPos;\n"
        "    float height = pos.y + 0.8;\n"           /* 0(茎底) ~ 0.8(花顶) */
        "    float sway = sin(uTime * 2.3 + pos.y * 3.5) * 0.018 * height;\n"
        "    float breathe = 1.0 + sin(uTime * 1.6) * 0.025;\n"
        "    float lean = uMouse.x * 0.04 * height;\n"
        "    pos.x += sway + lean;\n"
        "    pos *= breathe;\n"
        "    float bloomScale = 0.15 + 0.85 * smoothstep(0.0, 1.0, uBloom);\n"
        "    pos *= bloomScale;\n"
        "    gl_Position = vec4(pos, 0.0, 1.0);\n"
        "    vColor = aColor;\n"
        "}\n";

    const char* flowerFS =
        "#version 330 core\n"
        "in vec3 vColor;\n"
        "out vec4 FragColor;\n"
        "void main() { FragColor = vec4(vColor, 1.0); }\n";

    unsigned int flowerShader = createProgram(flowerVS, flowerFS);

    /* 8. 粒子着色器 */
    const char* partVS =
        "#version 330 core\n"
        "layout (location = 0) in vec2 aPos;\n"
        "layout (location = 1) in vec3 aColor;\n"
        "out vec3 vColor;\n"
        "void main() {\n"
        "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "    gl_PointSize = 5.0;\n"
        "    vColor = aColor;\n"
        "}\n";

    const char* partFS =
        "#version 330 core\n"
        "in vec3 vColor;\n"
        "out vec4 FragColor;\n"
        "void main() {\n"
        "    float d = length(gl_PointCoord - vec2(0.5));\n"
        "    float alpha = 1.0 - smoothstep(0.15, 0.5, d);\n"
        "    FragColor = vec4(vColor, alpha);\n"
        "}\n";

    unsigned int partShader = createProgram(partVS, partFS);

    /* 获取 uniform 位置 */
    int locTime  = (int)glGetUniformLocation(flowerShader, "uTime");
    int locBloom = (int)glGetUniformLocation(flowerShader, "uBloom");
    int locMouse = (int)glGetUniformLocation(flowerShader, "uMouse");

    /* 9. 渲染循环 */
    double lastFrame = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float  dt  = (float)(now - lastFrame);
        lastFrame  = now;

        g_time   += dt;
        if (g_bloomT < 1.0f) {
            g_bloomT += dt / 1.8f;  /* 1.8秒绽放 */
            if (g_bloomT > 1.0f) g_bloomT = 1.0f;
        }

        /* 归一化鼠标坐标 [-1, 1] */
        float mx = (float)(g_mouseX / (double)g_winW) * 2.0f - 1.0f;
        float my = 1.0f - (float)(g_mouseY / (double)g_winH) * 2.0f;

        /* 更新粒子 */
        updateParticles(dt);

        /* 构建粒子顶点数据 */
        {
            Vertex partVerts[PARTICLE_COUNT];
            for (int i = 0; i < PARTICLE_COUNT; i++) {
                Particle* p = &g_particles[i];
                float alpha = (p->life / PARTICLE_LIFE);
                if (alpha > 1.0f) alpha = 1.0f;
                partVerts[i].x = p->x;
                partVerts[i].y = p->y;
                partVerts[i].r = p->r * alpha;
                partVerts[i].g = p->g * alpha;
                partVerts[i].b = p->b * alpha;
            }
            glBindBuffer(GL_ARRAY_BUFFER, partVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0,
                            PARTICLE_COUNT * (long)sizeof(Vertex), partVerts);
        }

        /* 背景颜色随时间微调 */
        float bgR = 0.10f + 0.03f * sinf(g_time * 0.3f);
        float bgG = 0.13f + 0.03f * sinf(g_time * 0.3f + 1.5f);
        float bgB = 0.22f + 0.04f * sinf(g_time * 0.3f + 3.0f);
        glClearColor(bgR, bgG, bgB, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        /* ---- 绘制花朵 ---- */
        glUseProgram(flowerShader);
        glUniform1f(locTime,  g_time);
        glUniform1f(locBloom, g_bloomT);
        glUniform2f(locMouse, mx, my);
        glBindVertexArray(g_flowerVAO);
        for (int i = 0; i < g_cmdCount; i++) {
            glDrawArrays(g_drawCmds[i].mode,
                         (int)g_drawCmds[i].offset,
                         (int)g_drawCmds[i].count);
        }

        /* ---- 绘制粒子 (叠加混合) ---- */
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glUseProgram(partShader);
        glBindVertexArray(partVAO);
        glDrawArrays(GL_POINTS, 0, PARTICLE_COUNT);
        glDisable(GL_BLEND);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    /* 10. 清理 */
    glDeleteVertexArrays(1, &g_flowerVAO);
    glDeleteBuffers(1, &g_flowerVBO);
    glDeleteVertexArrays(1, &partVAO);
    glDeleteBuffers(1, &partVBO);
    glDeleteProgram(flowerShader);
    glDeleteProgram(partShader);
    glfwTerminate();
    return 0;
}
