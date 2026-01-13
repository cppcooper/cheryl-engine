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
				* glm::ortho( 0.f,static_cast<float>(renderer->display->active->width),
					0.f,static_cast<float>(renderer->display->active->height),
					0.f, 1.f)); //2D was using 0,1 for near,far
				break;
			}
			case Enum::gfx_mode::R3D: {
				/// Enable Depth Testing for 3D!
				glEnable( GL_DEPTH_TEST );

				///3D perspective projection
				m_projectionMatrix.set(glm::mat4( 1.f )
				* glm::perspective( 45.0f,
					static_cast<float>(renderer->display->active->width) / static_cast<float>(renderer->display->active->height),
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
		renderer = &Singleton_CTS<RenderAPIs::OpenGLRenderer>::get();
    }

    void glEngine::init() {
		renderer->initialize_libraries();
		renderer->initialize_rendering_context();
    }

    void glEngine::deinit() {
#ifdef POSH_OS_LINUX
		//?
#else
    	glMakeCurrent( 0, 0 );
#endif
    }

    void glEngine::pre_draw() {
		glfwPollEvents(); // OS Event Queue needs servicing
		set_clear_colour(0.4,0.2,0.8,1.0);
    	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void glEngine::post_draw() {
    	glfwSwapBuffers(renderer->display->active->glfw_window);
    }

    void glEngine::set_mode(Enum::gfx_mode mode) {
    	m_gMode.set(mode);
    }

    void glEngine::set_mode(Enum::window_mode mode) {
    	renderer->display->active->set_mode(mode);
    }

    void glEngine::set_clear_colour(float r, float g, float b, float a) {
    	glClearColor(r,g,b,a);
    }

    void glEngine::hide_cursor(bool hide) {
    	renderer->display->active->hide_cursor(hide);
    }
}
