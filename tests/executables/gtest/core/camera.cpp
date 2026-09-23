#include <gtest/gtest.h>

#include <core/camera.h>
#include <core/display/viewport.h>
#include <internals/exceptions.h>
#include <ext/matrix_transform.hpp>

#include <cmath>

TEST(camera_2d, uses_framebuffer_pixels_and_tracks_view) {
    // Map the two framebuffer corners into clip space to establish the 2D
    // projection's pixel coordinate convention.
    CE::Camera2D camera;
    camera.set_framebuffer_size({800, 600});
    const auto& projection = camera.projection_matrix();
    const auto lower_left = projection * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    const auto upper_right = projection * glm::vec4(800.0f, 600.0f, 0.0f, 1.0f);
    EXPECT_NEAR(lower_left.x, -1.0f, 1e-5f);
    EXPECT_NEAR(lower_left.y, -1.0f, 1e-5f);
    EXPECT_NEAR(upper_right.x, 1.0f, 1e-5f);
    EXPECT_NEAR(upper_right.y, 1.0f, 1e-5f);

    // Changing the view advances its revision. Setting that same view again
    // leaves the revision alone so consumers can skip an unnecessary update.
    const auto view = glm::translate(glm::mat4(1.0f), glm::vec3(-40.0f, -20.0f, 0.0f));
    const auto original_revision = camera.revision();
    camera.set_view_matrix(view);
    EXPECT_EQ(camera.view_matrix(), view);
    EXPECT_GT(camera.revision(), original_revision);
    const auto updated_revision = camera.revision();
    camera.set_view_matrix(view);
    EXPECT_EQ(camera.revision(), updated_revision);
}

TEST(camera_3d, uses_degree_fov_and_framebuffer_aspect_ratio) {
    // Check the default vertical field of view in degrees and the horizontal
    // scaling implied by a framebuffer twice as wide as it is high.
    CE::Camera3D camera;
    const CE::CameraBase& base = camera;
    EXPECT_EQ(base.mode(), CE::Enum::gfx_mode::R3D);
    camera.set_framebuffer_size({1600, 800});
    const auto wide = camera.projection_matrix();
    EXPECT_NEAR(wide[1][1], 1.0f / std::tan(glm::radians(22.5f)), 1e-5f);
    EXPECT_NEAR(wide[0][0], wide[1][1] / 2.0f, 1e-5f);

    // A square framebuffer removes aspect scaling; an explicit 90-degree
    // perspective then changes the vertical matrix scale to one.
    camera.set_framebuffer_size({800, 800});
    EXPECT_NEAR(camera.projection_matrix()[0][0], camera.projection_matrix()[1][1], 1e-5f);
    camera.set_perspective(90.0f, 0.5f, 500.0f);
    EXPECT_NEAR(camera.projection_matrix()[1][1], 1.0f, 1e-5f);
}

TEST(camera, tolerates_minimized_framebuffer_and_rejects_invalid_settings) {
    // A minimized window has no drawable pixels but should leave both
    // projections finite rather than producing NaNs.
    CE::Camera2D two_d;
    CE::Camera3D three_d;
    two_d.set_framebuffer_size({0, 0});
    three_d.set_framebuffer_size({0, 0});
    EXPECT_EQ(two_d.framebuffer_size(), (CE::FramebufferSize{0, 0}));
    EXPECT_TRUE(std::isfinite(two_d.projection_matrix()[0][0]));
    EXPECT_TRUE(std::isfinite(three_d.projection_matrix()[0][0]));

    // Reject negative dimensions and nonsensical perspective parameters.
    EXPECT_THROW(two_d.set_framebuffer_size({-1, 10}), CE::Exceptions::invalid_args);
    EXPECT_THROW(three_d.set_perspective(180.0f, 0.1f, 10.0f), CE::Exceptions::invalid_args);
    EXPECT_THROW(three_d.set_perspective(45.0f, 10.0f, 1.0f), CE::Exceptions::invalid_args);
}

TEST(viewport, copied_sizes_are_independent) {
    // Mutating the source after copying must leave the copy's dimensions intact.
    CE::ViewPort<int> original(800, 600);
    auto copy = original;
    original.width = 1024;
    original.height = 768;
    EXPECT_EQ(copy.width, 800);
    EXPECT_EQ(copy.height, 600);
}
