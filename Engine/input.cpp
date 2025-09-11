#include "Engine/input.hpp"
#include <cmath>
#include "Engine/editor.hpp"
#include "GameMain.hpp"
#include "Engine/sceneManager.hpp"

EditorInput::EditorInput(SDL_Window* window) {
    cameraPos   = Vec3d(2.0f, 2.0f, 3.0f);
    cameraFront = Vec3d(-0.5f, -0.5f, -1.0f);
    cameraUp    = Vec3d(0.0f, 1.0f, 0.0f);

    yaw = -90.0f;
    pitch = 0.0f;
    speed = 5.f;
    sensitivity = 0.1f;

    // SDL_HideCursor();
    // SDL_SetWindowRelativeMouseMode(window, true);
}

void EditorInput::processKeyboard(float deltaTime) {
    const Uint8* state = SDL_GetKeyboardState(NULL);
    float cameraSpeed = speed * deltaTime;

    if (state[SDL_SCANCODE_W]) cameraPos += cameraFront * cameraSpeed;
    if (state[SDL_SCANCODE_S]) cameraPos -= cameraFront * cameraSpeed;
    if (state[SDL_SCANCODE_A]) cameraPos -= cameraFront.cross(cameraUp).normalized() * cameraSpeed;
    if (state[SDL_SCANCODE_D]) cameraPos += cameraFront.cross(cameraUp).normalized() * cameraSpeed;
    if (state[SDL_SCANCODE_SPACE]) cameraPos += cameraUp * cameraSpeed;
    if (state[SDL_SCANCODE_LSHIFT]) cameraPos -= cameraUp * cameraSpeed;
}

void EditorInput::handleEvent(SDL_Event& e, SDL_Window* window) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        return;
    }
    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT) {
        rightMouseHeld = true;
        SDL_ShowCursor(SDL_FALSE);
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }
    if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_RIGHT) {
        rightMouseHeld = false;
        SDL_ShowCursor(SDL_TRUE);
        SDL_SetRelativeMouseMode(SDL_FALSE);
    }
    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        leftMouseClicked = true;
        leftMouseHeld = true;
        SDL_GetMouseState(&mouseX, &mouseY);
    }
    if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
        leftMouseHeld = false;
        SDL_GetMouseState(&mouseX, &mouseY);
    }
    if (rightMouseHeld && e.type == SDL_MOUSEMOTION) {
        processMouse(e);
    }
    if ((rightMouseHeld && e.type == SDL_MOUSEMOTION) || (leftMouseHeld && e.type == SDL_MOUSEMOTION)) {
        // Update mouse coords for dragging or camera control
        SDL_GetMouseState(&mouseX, &mouseY);
    }
}

void EditorInput::processMouse(SDL_Event& e) {
    if (e.type == SDL_MOUSEMOTION) {
        float xpos = (float)e.motion.xrel;
        float ypos = (float)e.motion.yrel;

        yaw   += xpos * sensitivity;
        pitch -= ypos * sensitivity;

        if(pitch > 89.0f) pitch = 89.0f;
        if(pitch < -89.0f) pitch = -89.0f;

        Vec3d front;
        front.x = NMATH::cos(radians(yaw)) * NMATH::cos(radians(pitch));
        front.y = NMATH::sin(radians(pitch));
        front.z = NMATH::sin(radians(yaw)) * NMATH::cos(radians(pitch));
        cameraFront = front.normalized();
    }
}

void EditorInput::handleViewportInput(Editor* editor, GameMain& game,
                                      const Mat4& view, const Mat4& projection,
                                      int viewportX, int viewportY, int viewportW, int viewportH) {
    if (!editor) return;

    ImGuiIO& io = ImGui::GetIO();
    bool hovered = editor->isViewportHovered();
    float u=0.0f, v=0.0f; editor->getViewportMouseUV(u, v);

    Mat4 invPV = (projection * view).inverse();

    // defined inline to avoid exposing in header
    struct ClipToWorldHelper {
        static Vec3d convert(const Mat4& inv, float ndcX, float ndcY, float ndcZ) {
            float tx = inv.m[0][0]*ndcX + inv.m[0][1]*ndcY + inv.m[0][2]*ndcZ + inv.m[0][3]*1.0f;
            float ty = inv.m[1][0]*ndcX + inv.m[1][1]*ndcY + inv.m[1][2]*ndcZ + inv.m[1][3]*1.0f;
            float tz = inv.m[2][0]*ndcX + inv.m[2][1]*ndcY + inv.m[2][2]*ndcZ + inv.m[2][3]*1.0f;
            float tw = inv.m[3][0]*ndcX + inv.m[3][1]*ndcY + inv.m[3][2]*ndcZ + inv.m[3][3]*1.0f;
            if (std::fabs(tw) < 1e-8f) tw = 1e-8f;
            return Vec3d(tx/tw, ty/tw, tz/tw);
        }
    };

    if (hovered && editor->isViewportMouseDown() && !prevViewportMouseDown) {
        float ndcX = u * 2.0f - 1.0f;
        float ndcY = v * 2.0f - 1.0f;

        Vec3d nearP = ClipToWorldHelper::convert(invPV, ndcX, ndcY, -1.0f);
        Vec3d farP  = ClipToWorldHelper::convert(invPV, ndcX, ndcY,  1.0f);
        Vec3d rayDir = (farP - nearP).normalized();

        // Try gizmo axis pick first
        GizmoAxis axis;
        if (game.scene->pickGizmoAxis(nearP, rayDir, axis)) {
            game.scene->grabbedAxis = axis;
            game.scene->axisGrabbed = true;
        } else {
            game.scene->axisGrabbed = false;
            game.scene->grabbedAxisIndex = -1;
            // Pick object
            Object* picked = game.scene->pickObject(nearP, rayDir);
            if (picked) {
                game.scene->selectedObject = picked;
            } else {
                // pick light
                int li = -1;
                if (game.scene->pickLight(nearP, rayDir, li)) {
                    game.scene->selectedLightIndex = li;
                } else {
                    game.scene->selectedObject = nullptr;
                    game.scene->selectedLightIndex = -1;
                }
            }
        }
    }

    // While left mouse is down and an axis is grabbed, drag the selected object
    if (hovered && editor->isViewportMouseDown() && game.scene->axisGrabbed) {
        float ndcX = u * 2.0f - 1.0f;
        float ndcY = v * 2.0f - 1.0f;
        Vec3d nearP = ClipToWorldHelper::convert(invPV, ndcX, ndcY, -1.0f);
        Vec3d farP  = ClipToWorldHelper::convert(invPV, ndcX, ndcY,  1.0f);
        Vec3d rayDir = (farP - nearP).normalized();
        game.scene->dragSelectedObject(nearP, rayDir);
    }

    // Camera rotate (right mouse) and pan (middle mouse) while over viewport
    if (hovered) {
        // rotate
        if (ImGui::IsMouseDown(1)) {
            // Only rotate if there is actual mouse movement
            if (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f) {
                float dx = -io.MouseDelta.x;
                float dy = io.MouseDelta.y;
                const float sens = 0.005f;
                Vec3d f = cameraFront;
                double pitch = std::asin((double)f.y);
                double yaw = std::atan2((double)f.z, (double)f.x);
                yaw += -dx * sens;
                pitch += -dy * sens;
                const double limit = 1.5707963267948966 - 0.01;
                if (pitch > limit) pitch = limit;
                if (pitch < -limit) pitch = -limit;
                Vec3d newFront = Vec3d((float)(cos(pitch)*cos(yaw)), (float)sin(pitch), (float)(cos(pitch)*sin(yaw))).normalized();
                cameraFront = newFront;

                Vec3d worldUp(0.0f, 1.0f, 0.0f);
                Vec3d right = cameraFront.cross(worldUp).normalized();
                cameraUp = right.cross(cameraFront).normalized();
            }
            SDL_ShowCursor(SDL_FALSE);
        } else { SDL_ShowCursor(SDL_TRUE); }

        // pan
        if (ImGui::IsMouseDown(2)) {
            float px = -io.MouseDelta.x * 0.01f;
            float py = io.MouseDelta.y * 0.01f;
            Vec3d right = cameraFront.cross(cameraUp).normalized();
            Vec3d up = right.cross(cameraFront).normalized();
            cameraPos = cameraPos + right * px + up * py;
        }
    }

    prevViewportMouseDown = editor->isViewportMouseDown();
}