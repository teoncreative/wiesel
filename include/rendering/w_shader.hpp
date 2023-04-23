
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

namespace Wiesel {
	// todo
	enum ShaderType {
		ShaderTypeVertex,
		ShaderTypeFragment
	};
	enum ShaderSource {
		ShaderSourcePrecompiled,
		ShaderSourceSource
	};

	enum ShaderLang {
		ShaderLangGLSL,
		ShaderLangHLSL
	};

	struct ShaderProperties {
		ShaderType Type;
		ShaderLang Lang;
		std::string Main;
		ShaderSource Source;
		std::string Path;
	};

	struct Shader {
		Shader(ShaderProperties properties);
		~Shader();

		VkShaderModule ShaderModule;
		ShaderProperties Properties;
	};

}