#pragma once

#include <cheryl/export.h>
#include <cheryl/assets/types/2d/unicode-text.h>

#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <string>

namespace CE::Runtime {
    class Context;
}

namespace CE::Text::Rendering {
    /** Owned desired text and retained font/material handles, compared by content/options and snapshot identity. */
    struct Request {
        Font font; ///< Empty selects the default captured when this updater was created.
        std::string text; ///< Owned UTF-8 source; malformed sequences use the layout replacement policy.
        LayoutOptions layout; ///< CPU shaping, style and wrapping options for this request.
        RasterOptions raster; ///< CPU atlas and positioned-geometry preparation options.
        std::shared_ptr<const CE::Rendering::Resources::Material> material; ///< Optional retained override; empty uses the provider's default text material.
    };

    /** Stage whose submission or execution failed; the previous complete generation remains current. */
    enum class FailureStage { Preparation /**< CPU worker submission or shaping/rasterization. */, Upload /**< Platform submission, cancellation or graphics upload. */ };

    /** Owned asynchronous error observation; revision identifies the failed request event. */
    struct Failure {
        FailureStage stage; ///< Preparation or platform upload, including failure to submit that stage.
        std::exception_ptr error; ///< Retained exception for reporting/rethrow; no borrowed worker state.
        std::uint64_t revision; ///< Updater-local request revision, not a stable content identifier.
    };

    /** Retains the last complete text while replacing it asynchronously. Construct
     * on platform during initialization, then use from the simulation owner. Calls
     * require serialization; destruction after quiescence may run on platform.
     * CPU work uses a Context-tracked worker group; uploads use its dispatcher.
     * No job borrows this updater or the game. Destruction never waits for jobs.
     */
    class Updater final {
        struct State;
        std::unique_ptr<State> state_;

    public:
        CE_API explicit Updater(CE::Runtime::Context& engine);
        CE_API Updater(CE::Runtime::Context& engine, Request initial);
        CE_API ~Updater();
        Updater(const Updater&) = delete;
        Updater& operator=(const Updater&) = delete;

        CE_API void request(Request next);
        CE_API bool update();
        CE_API void retry();
        [[nodiscard]] CE_API bool pending() const noexcept;
        [[nodiscard]] CE_API std::shared_ptr<const Generation> current() const noexcept;
        [[nodiscard]] CE_API std::optional<Failure> failure() const noexcept;
    };
}

/** @fn CE::Text::Rendering::Updater::Updater(CE::Runtime::Context& engine)
 * Construct on the platform owner with no current generation. Capture the current default Font, a Context-tracked worker group and its platform submission endpoint. First request()/update() publishes asynchronously; construction/allocation failures propagate.
 */

/** @fn CE::Text::Rendering::Updater::Updater(CE::Runtime::Context& engine, Request initial)
 * Synchronously prepare/upload the initial request on the graphics owner/current context. An empty font uses the captured engine default. Initial errors propagate from construction; later asynchronous errors are retained by failure().
 * @see [Retained updates](cheryl-guide:assets/text-layout#retained-text-updates)
 */

/** @fn CE::Text::Rendering::Updater::~Updater()
 * Destroy after serialized callers quiesce; platform teardown is allowed after simulation joins. Destruction never waits for jobs: submitted work owns its request and borrows neither updater nor game. Retained current generations remain independent; native use still needs their backend domain.
 */

/** @fn void CE::Text::Rendering::Updater::request(Request next)
 * Own the newest desired request. Equal content/options and font/material snapshot identities do not resubmit or retry. A distinct request clears the previous failure; empty font selects the captured default. Revision exhaustion throws failed_operation. No preparation/upload is started until update().
 */

/** @fn bool CE::Text::Rendering::Updater::update()
 * Advance without waiting, adopting a ready complete generation and starting the newest desired work when idle. At most one active replacement plus one newest desired request is retained; an older active success is adopted before its successor starts. Submission/preparation/upload failures preserve current() and are observed through failure().
 * @return Whether current() changed during this call.
 */

/** @fn void CE::Text::Rendering::Updater::retry()
 * Requeue an unchanged failed request explicitly, clearing its failure. No effect while pending or after success; revision exhaustion throws. Call update() to start the retry; there is no automatic busy retry.
 */

/** @fn bool CE::Text::Rendering::Updater::pending() const noexcept
 * Return whether preparation/upload or an unsettled desired revision exists. This is a serialized observation, not permission for concurrent calls.
 */

/** @fn std::shared_ptr<const CE::Text::Rendering::Generation> CE::Text::Rendering::Updater::current() const noexcept
 * Return a retained immutable complete generation, initially null for the empty constructor. Failure never replaces it; retaining the handle does not retain a live graphics context.
 */

/** @fn std::optional<CE::Text::Rendering::Failure> CE::Text::Rendering::Updater::failure() const noexcept
 * Copy the last failure since a distinct request/retry. A newer already-queued success need not clear it; use revision to identify the event. No failure is reported for synchronous construction errors, which propagate.
 */
