#pragma once
#ifndef RENDERER_H
#define RENDERER_H
#include <memory>
#include <filesystem>
#include <core/display.h>

namespace CE {
	namespace RenderAPIs {
		namespace fs = std::filesystem;
		using program_id = uint64_t;
		struct iRenderer {
			std::unique_ptr<DisplaySystem> display = nullptr;
			virtual ~iRenderer() = default; //todo: clion.. is this useful?

			virtual void initialize_libraries() = 0;
			virtual void initialize_rendering_context() = 0;
			virtual void deinitialize() = 0;
			virtual void clear() = 0;
			virtual void swap_buffer() = 0;
			virtual void draw() = 0;
			virtual program_id compile_shader(fs::path file) = 0;
		};
	}
	namespace R = RenderAPIs;
}


#endif //RENDERER_H
