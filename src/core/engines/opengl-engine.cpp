#include <cgl.h>
#include <core.h>
#include <internals.h>
#include <glm.hpp>
#include "ext/matrix_clip_space.hpp"
#include <mutex>

namespace CE::Engine {
	void glEngine::calculate_projection() {
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

	// when any one of these observed variables changes value we want to recalculate the projection matrix immediately
	// in the case of the projection matrix itself changing value, we want to dispatch an event for all the shaders listening
	// The observed variables and the event system use the publisher/subscriber paradigm to
	// ensure that these chains of dependencies are able to update as a chain.
    glEngine::glEngine():
	m_gMode(Enum::gfx_mode::R2D,{[this](const Enum::gfx_mode&){calculate_projection();}}),
	m_nearplane(0.1f, {[this](const float&) {
		if (m_gMode.get() == Enum::gfx_mode::R3D) {
			calculate_projection();
		}
	}}),
	m_farplane(10000.f, {[this](const float&){
		if (m_gMode.get() == Enum::gfx_mode::R3D) {
			calculate_projection();
		}
	}}),
	m_projectionMatrix(glm::mat4{}, {
	   [](const glm::mat4& mat) {
	       SubSystems::EventSystem::get().dispatch("projection-matrix-changed", mat);
	   }
	}) {
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

    	const auto pm = display.primary_monitor;
    	display.create_window(pm, Enum::window_mode::FULLSCREEN, pm.width, pm.height)->activate();
		/// If creating the window failed we need to terminate
		if (!display.active || !display.active->glfw_window) {
			glfwTerminate();
			return;
		}
		SubSystems::EventSystem::get().register_listener("window-resized",[this](std::any payload) {
			if (!payload.has_value()) {
				throw Exceptions::failed_operation(CE_HERE,"An event (\"window-resized\") was dispatched without a payload.");
			}
			try {
				const auto window = std::get<0>(std::any_cast<std::tuple<Window*,float,float>>(payload));
				if (window == display.active) {
					calculate_projection();
				}
			} catch (const std::bad_any_cast& e) {
				CELog::error("Event payload was illformed.\n{}", e.what());
			}
		});

    	// where are we? (part 1)
		glEnable( GL_CULL_FACE );
		glCullFace( GL_BACK );
		glFrontFace( GL_CCW );

    	// what are we doing? (part 2)
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

    	// it's dark here. (part 3)
		set_clear_colour(0.f,0.f,0.f,0.f); //white or black, dunno
		glfwSwapInterval(1);
    }

    void glEngine::deinit() {
#ifdef POSH_OS_LINUX
		//?
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

    void glEngine::set_clear_colour(float r, float g, float b, float a) {
    	glClearColor(r,g,b,a);
    }

    void glEngine::hide_cursor(bool hide) {
    	display.active->hide_cursor(hide);
    }
}
