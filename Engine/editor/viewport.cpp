#include "Engine/editor/viewport.hpp"

void Viewport::DrawViewport(GLuint viewportTexture, int viewportTexW, int viewportTexH,
	bool& viewportHovered, float& viewportMouseU, float& viewportMouseV, bool& viewportMouseDown) {
	if (viewportTexture != 0) {
		ImVec2 avail = ImGui::GetContentRegionAvail();
		float aspect = (viewportTexW > 0 && viewportTexH > 0) ? (float)viewportTexW / (float)viewportTexH : (16.0f / 9.0f);
		float width = avail.x;
		float height = width / aspect;
		if (height > avail.y) { height = avail.y; width = height * aspect; }

		ImVec2 uv0 = ImVec2(0.0f, 1.0f);
		ImVec2 uv1 = ImVec2(1.0f, 0.0f);
		ImGui::Image((ImTextureID)(intptr_t)viewportTexture, ImVec2(width, height), uv0, uv1);

		ImGuiIO& io = ImGui::GetIO();
		ImVec2 imgMin = ImGui::GetItemRectMin();
		ImVec2 imgMax = ImGui::GetItemRectMax();
		ImVec2 mousePos = io.MousePos;

		// Check hover
		viewportHovered = (mousePos.x >= imgMin.x && mousePos.x <= imgMax.x && mousePos.y >= imgMin.y && mousePos.y <= imgMax.y);
		if (viewportHovered) {
			float localX = mousePos.x - imgMin.x;
			float localY = mousePos.y - imgMin.y;
			float w = imgMax.x - imgMin.x;
			float h = imgMax.y - imgMin.y;
			viewportMouseU = localX / w;
			viewportMouseV = 1.0f - (localY / h);
			viewportMouseDown = ImGui::IsMouseDown(0);
		} else {
			viewportMouseDown = false;
		}
	} else {
		ImGui::Text("No viewport texture available");
		viewportHovered = false;
		viewportMouseDown = false;
	}
}