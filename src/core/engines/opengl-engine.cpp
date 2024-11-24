#include <cgl.h>
#include <core/engines/opengl-engine.h>
#include <internals.h>
#include <glm.hpp>
#include "ext/matrix_clip_space.hpp"
#include <mutex>

template<typename P>
void fn_update_matrices(const P &p) {
	if constexpr (std::is_same_v<P,CE::Enum::gfx_mode>) {

	}
}

namespace CE::Engine {
	void glEngine::update_matrices() {
		switch(m_gMode.get()) {
			case Enum::gfx_mode::R2D: {
				/// Disable Depth Testing for 2D!
				glDisable( GL_DEPTH_TEST );

				///2d orthographic projection
				m_projectionMatrix.set(glm::mat4( 1.f )
				* glm::ortho( 0.f,static_cast<float>(display.active->width),
					0.f,static_cast<float>(display.active->height),
					0.f, 1.f)); //2D was using 0,1 for near,far
				break;
			}
			case Enum::gfx_mode::R3D: {
				/// Enable Depth Testing for 3D!
				glEnable( GL_DEPTH_TEST );

				///3D perspective projection
				m_projectionMatrix.set(glm::mat4( 1.f )
				* glm::perspective( 45.0f,
					static_cast<float>(display.active->width) / static_cast<float>(display.active->height),
					m_nearplane.get(), m_farplane.get()));
				break;
			}
		}
	}

    glEngine::glEngine():
	m_gMode({},{}),
	m_viewMatrix(glm::mat4{1.f}, {}), // todo: move to camera controller
	m_projectionMatrix(glm::mat4{}, {}),
	m_nearplane(0.1f, {}),
	m_farplane(10000.f, {}) {
	    std::once_flag flag;
	    std::call_once(flag, []() {
		    gladLoadGL(glfwGetProcAddress);
	    });
    }

    void glEngine::init() {
        /// We need our GLFW function pointers to be assigned, if this process fails we cannot continue
		if (!glfwInit()) {
			return;
		}
    	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		/// Here we query how much sampling is possible and set that to be used if possible
		GLint samples = 8;
		glGetIntegerv(GL_SAMPLES, &samples);
		if (samples) {
			glEnable(GL_MULTISAMPLE);
		}
		glfwWindowHint(GLFW_SAMPLES, samples);

    	auto pm = display.primary_monitor;
    	display.create_window(pm, Enum::window_mode::FULLSCREEN, pm.width, pm.height)->activate();
		/// If creating the window failed we need to terminate
		if (!display.active || !display.active->glfw_window) {
			glfwTerminate();
			return;
		}
		set_mode(Enum::gfx_mode::R2D);

    	// where are we? (part 1)
		glEnable( GL_CULL_FACE );
		glCullFace( GL_BACK );
		glFrontFace( GL_CCW );

    	// what are we doing? (part 2)
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

    	// it's dark here. (part 3)
		// glClearColor(
		// 	clear_colour.colour.r,
		// 	clear_colour.colour.g,
		// 	clear_colour.colour.b,
		// 	clear_colour.colour.a);
		glfwSwapInterval(1);
    }

    void glEngine::deinit() {
#ifdef POSH_OS_LINUX
#else
    	glMakeCurrent( 0, 0 );
#endif
    }

    void glEngine::pre_draw() {
    	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void glEngine::post_draw() {
    	glfwSwapBuffers(display.active->glfw_window);
    }

    void glEngine::set_mode(Enum::gfx_mode mode) {
    	m_gMode.set(mode);
    }

    void glEngine::set_mode(Enum::window_mode mode) {
    	display.active->set_mode(mode);
    }

    void glEngine::set_clear_colour( float r, float g, float b, float a ) {
    	glClearColor(r,g,b,a);
    }

    void glEngine::hide_cursor(bool hide) {
    	display.active->hide_cursor(hide);
    }
}
