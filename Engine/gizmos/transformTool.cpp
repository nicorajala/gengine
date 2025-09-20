#include "Engine/gizmos/transformTool.hpp"
#include "Engine/objects/shapegen.hpp"
#include <vector>

void TransformTool::drawGizmo(const Vec3d& objPosition, int grabbedAxisIndex, GLuint shaderProgram, const Mat4& view, const Mat4& projection) {
    // Query attribute/uniform locations from the supplied program (doesn't require the program to be bound for glGet*).
    GLint locPos = glGetAttribLocation(shaderProgram, "aPos");
    GLint locColor = glGetAttribLocation(shaderProgram, "aColor");
    GLint locNormal = glGetAttribLocation(shaderProgram, "aNormal");
    GLint locModel = glGetUniformLocation(shaderProgram, "model");
    GLint locView = glGetUniformLocation(shaderProgram, "view");
    GLint locProj = glGetUniformLocation(shaderProgram, "projection");
    GLint locUseOverride = glGetUniformLocation(shaderProgram, "useOverrideColor");
    GLint locOverrideColor = glGetUniformLocation(shaderProgram, "overrideColor");

    // It's expected the caller has bound shaderProgram before calling this function.
    // Compute camera pos (for potential scaling) and local transform
    Vec3d camPos = Vec3d(view.inverse().m[3][0], view.inverse().m[3][1], view.inverse().m[3][2]);
    float dist = (objPosition - camPos).length();

    // Parameters
    float scale = 1.1f;
    float shaftThickness = 0.15f; // diameter; radius = thickness * 0.5
    float arrowSize = 0.225f;
    int segments = 16; // tessellation

    Mat4 model = translate(Mat4(1.0f), objPosition);

    struct Axis { Vec3d dir; Vec3d color; };
    std::vector<Axis> axes(3);
    axes[0].dir = Vec3d(1, 0, 0); axes[0].color = Vec3d(1, 0, 0);
    axes[1].dir = Vec3d(0, 1, 0); axes[1].color = Vec3d(0, 1, 0);
    axes[2].dir = Vec3d(0, 0, 1); axes[2].color = Vec3d(0, 0, 1);

    for (size_t i = 0; i < axes.size(); i++) {
        Vec3d start(0, 0, 0);
        Vec3d end = axes[i].dir * scale;
        Vec3d color = ((int)i == grabbedAxisIndex) ? Vec3d(1, 1, 0) : axes[i].color;

        float shaftRadius = shaftThickness * 0.5f;
        Vec3d shaftStart = start;
        Vec3d shaftEnd = end - axes[i].dir.normalized() * (arrowSize * 0.2f);

        std::vector<Vertex> cylVerts;
        std::vector<unsigned int> cylIndices;
        ShapeGenerator::createCylinder(shaftStart, shaftEnd, shaftRadius, segments, cylVerts, cylIndices);

        if (!cylVerts.empty()) {
            GLuint cylVAO = 0, cylVBO = 0, cylEBO = 0;
            glGenVertexArrays(1, &cylVAO);
            glGenBuffers(1, &cylVBO);
            glGenBuffers(1, &cylEBO);

            glBindVertexArray(cylVAO);

            glBindBuffer(GL_ARRAY_BUFFER, cylVBO);
            glBufferData(GL_ARRAY_BUFFER, cylVerts.size() * sizeof(Vertex), &cylVerts[0], GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cylEBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, cylIndices.size() * sizeof(unsigned int), &cylIndices[0], GL_STATIC_DRAW);

            // position
            if (locPos >= 0) {
                glEnableVertexAttribArray((GLuint)locPos);
                glVertexAttribPointer((GLuint)locPos, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
            }
            // color
            if (locColor >= 0) {
                glEnableVertexAttribArray((GLuint)locColor);
                glVertexAttribPointer((GLuint)locColor, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
            }
            // normal
            if (locNormal >= 0) {
                glEnableVertexAttribArray((GLuint)locNormal);
                glVertexAttribPointer((GLuint)locNormal, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
            }

            // Set mats & color uniforms (caller must have bound shaderProgram)
            if (locModel >= 0) glUniformMatrix4fv(locModel, 1, GL_FALSE, model.value_ptr());
            if (locView >= 0) glUniformMatrix4fv(locView, 1, GL_FALSE, view.value_ptr());
            if (locProj >= 0) glUniformMatrix4fv(locProj, 1, GL_FALSE, projection.value_ptr());
            if (locUseOverride >= 0) glUniform1i(locUseOverride, 1);
            if (locOverrideColor >= 0) glUniform3fv(locOverrideColor, 1, &color[0]);

            glDrawElements(GL_TRIANGLES, (GLsizei)cylIndices.size(), GL_UNSIGNED_INT, (void*)0);

            // cleanup
            if (locPos >= 0) glDisableVertexAttribArray((GLuint)locPos);
            if (locColor >= 0) glDisableVertexAttribArray((GLuint)locColor);
            if (locNormal >= 0) glDisableVertexAttribArray((GLuint)locNormal);
            glBindVertexArray(0);
            glDeleteBuffers(1, &cylVBO);
            glDeleteBuffers(1, &cylEBO);
            glDeleteVertexArrays(1, &cylVAO);
        }

        Vec3d dirNorm = axes[i].dir.normalized();
        Vec3d coneBaseCenter = shaftEnd;
        float coneBaseRadius = arrowSize * 0.5f;
        Vec3d tip = coneBaseCenter + dirNorm * arrowSize;

        std::vector<Vertex> coneVerts;
        std::vector<unsigned int> coneIndices;

        ShapeGenerator::createCone(coneBaseCenter, tip, coneBaseRadius, segments, coneVerts, coneIndices);

        if (!coneVerts.empty()) {
            GLuint coneVAO = 0, coneVBO = 0, coneEBO = 0;
            glGenVertexArrays(1, &coneVAO);
            glGenBuffers(1, &coneVBO);
            glGenBuffers(1, &coneEBO);

            glBindVertexArray(coneVAO);

            glBindBuffer(GL_ARRAY_BUFFER, coneVBO);
            glBufferData(GL_ARRAY_BUFFER, coneVerts.size() * sizeof(Vertex), &coneVerts[0], GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, coneEBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, coneIndices.size() * sizeof(unsigned int), &coneIndices[0], GL_STATIC_DRAW);

            // position
            if (locPos >= 0) {
                glEnableVertexAttribArray((GLuint)locPos);
                glVertexAttribPointer((GLuint)locPos, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
            }
            // color
            if (locColor >= 0) {
                glEnableVertexAttribArray((GLuint)locColor);
                glVertexAttribPointer((GLuint)locColor, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
            }
            // normal
            if (locNormal >= 0) {
                glEnableVertexAttribArray((GLuint)locNormal);
                glVertexAttribPointer((GLuint)locNormal, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
            }

            if (locModel >= 0) glUniformMatrix4fv(locModel, 1, GL_FALSE, model.value_ptr());
            if (locView >= 0) glUniformMatrix4fv(locView, 1, GL_FALSE, view.value_ptr());
            if (locProj >= 0) glUniformMatrix4fv(locProj, 1, GL_FALSE, projection.value_ptr());
            if (locUseOverride >= 0) glUniform1i(locUseOverride, 1);
            if (locOverrideColor >= 0) glUniform3fv(locOverrideColor, 1, &color[0]);

            glDrawElements(GL_TRIANGLES, (GLsizei)coneIndices.size(), GL_UNSIGNED_INT, (void*)0);

            if (locPos >= 0) glDisableVertexAttribArray((GLuint)locPos);
            if (locColor >= 0) glDisableVertexAttribArray((GLuint)locColor);
            if (locNormal >= 0) glDisableVertexAttribArray((GLuint)locNormal);
            glBindVertexArray(0);
            glDeleteBuffers(1, &coneVBO);
            glDeleteBuffers(1, &coneEBO);
            glDeleteVertexArrays(1, &coneVAO);
        }
    }

    // restore override uniform off (assumes shader bound by caller)
    if (locUseOverride >= 0) glUniform1i(locUseOverride, 0);
    // done
}