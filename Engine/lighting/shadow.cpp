#include "Engine/lighting/shadow.hpp"
#include "Engine/sceneManager.hpp"
#include "Engine/util/shaderc.hpp"

Shadow::Shadow() {
	SHADOW_SIZE = 2048;
	depthMapFBO = 0;
	depthMapTex = 0;

    glGenFramebuffers(1, &depthMapFBO);
    glGenTextures(1, &depthMapTex);
    glBindTexture(GL_TEXTURE_2D, depthMapTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
        SHADOW_SIZE, SHADOW_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    std::cerr << "[Shadow] Created depth texture id=" << depthMapTex << " size=" << SHADOW_SIZE << "x" << SHADOW_SIZE << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMapTex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[Shadow] Shadow FBO incomplete: " << status << std::endl;
    } else {
        std::cerr << "[Shadow] Shadow FBO complete (fbo=" << depthMapFBO << ", tex=" << depthMapTex << ")" << std::endl;
    }

    // Check GL error
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) std::cerr << "[Shadow] GL error after FBO setup: 0x" << std::hex << err << std::dec << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // defaults
    lightPos = Vec3d(10.0f, 10.0f, 10.0f);
    lightDir = Vec3d(-1.0f, -1.0f, -1.0f).normalized();
    sz = 20.0f;
    nearP = 1.0f;
    farP = 60.0f;
}

Shadow::~Shadow() {
	if (depthMapTex) glDeleteTextures(1, &depthMapTex);
	if (depthMapFBO) glDeleteFramebuffers(1, &depthMapFBO);
}

// Render depth-only pass for this light. scene->render will draw objects with the given shader/program.
// depthProgram must be a depth-only shader that uses 'model','view','projection' uniforms.
Mat4 Shadow::renderDepth(SceneManager* scene, GLuint depthProgram) {
    // compute a scene-centered light view/proj for directional lights (orthographic)
    Vec3d sceneMin(1e9f, 1e9f, 1e9f);
    Vec3d sceneMax(-1e9f, -1e9f, -1e9f);
    bool hasObjects = false;

    for (size_t i = 0; i < scene->objects.size(); ++i) {
        Object* o = scene->objects[i];
        if (!o) continue;
        hasObjects = true;
        Vec3d p = o->position;
        sceneMin.x = std::min(sceneMin.x, p.x);
        sceneMin.y = std::min(sceneMin.y, p.y);
        sceneMin.z = std::min(sceneMin.z, p.z);
        sceneMax.x = std::max(sceneMax.x, p.x);
        sceneMax.y = std::max(sceneMax.y, p.y);
        sceneMax.z = std::max(sceneMax.z, p.z);
    }

    Vec3d center;
    float useSz = sz;
    if (hasObjects) {
        center = (sceneMin + sceneMax) * 0.5f;
        Vec3d ext = sceneMax - sceneMin;
        float maxDim = std::max(std::max(ext.x, ext.y), ext.z);
        if (maxDim * 0.6f > useSz) useSz = maxDim * 0.6f;
    } else {
        center = lightPos;
    }

    Vec3d lightCamPos = center - lightDir.normalized() * (useSz * 2.0f);

    Mat4 lightView = lookAt(lightCamPos, center, Vec3d(0, 1, 0));
    Mat4 lightProj = orthographic(-useSz, useSz, -useSz, useSz, nearP, farP);
    lastLightSpace = lightProj * lightView;

    // Save GL state we will modify
    GLint prevViewport[4]; glGetIntegerv(GL_VIEWPORT, prevViewport);
    GLint prevDrawFBO = 0; glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFBO);
    GLint prevReadFBO = 0; glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFBO);
    GLint prevProgram = 0; glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
    GLint prevDrawBuf = GL_BACK; glGetIntegerv(GL_DRAW_BUFFER, &prevDrawBuf);
    GLint prevReadBuf = GL_BACK; glGetIntegerv(GL_READ_BUFFER, &prevReadBuf);

    // Bind shadow FBO and set viewport to shadow map size
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, depthMapFBO);
    glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);

    // Ensure we won't write color to the shadow FBO
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);

    // Use provided depth program + set view/projection uniforms
    glUseProgram(depthProgram);
    GLint locV = glGetUniformLocation(depthProgram, "view");
    GLint locP = glGetUniformLocation(depthProgram, "projection");
    if (locV >= 0) glUniformMatrix4fv(locV, 1, GL_FALSE, lightView.value_ptr());
    if (locP >= 0) glUniformMatrix4fv(locP, 1, GL_FALSE, lightProj.value_ptr());

    // Draw scene using depth program; SceneManager::render will set 'model' per object.
    scene->render(depthProgram, lightView, lightProj);

    glDisable(GL_POLYGON_OFFSET_FILL);

    // Restore previous GL state
    // Bind previous draw/read framebuffer(s)
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDrawFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, prevReadFBO);

    // Restore draw/read buffers (in case default framebuffer was changed)
    glDrawBuffer(prevDrawBuf);
    glReadBuffer(prevReadBuf);

    // Restore viewport
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

    // Restore previously bound program
    glUseProgram((GLuint)prevProgram);

    return lastLightSpace;
}