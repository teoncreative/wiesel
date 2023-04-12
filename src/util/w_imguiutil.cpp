//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "util/w_imguiutil.h"

namespace Wiesel {
	std::string PrefixLabel(const char* label) {
		float width = ImGui::CalcItemWidth();

		float x = ImGui::GetCursorPosX();
		ImGui::Text("%s", label);
		ImGui::SameLine();
		ImGui::SetCursorPosX(x + width * 0.5f + ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::SetNextItemWidth(-1);

		std::string labelID = "##";
		labelID += label;

		return labelID;
	}
}