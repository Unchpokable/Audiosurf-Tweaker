#pragma once

// Work that must not happen on the render thread: shader compiles, image decoding, atlas baking,
// and whatever a package's generator script decides to do.
//
// This is a thin wrapper over std::async, not a thread pool. MSVC's std::async already goes to the
// Windows thread pool - _Task_async_state hands the job to Concurrency::create_task, which schedules
// a PTP_WORK chore - so threads are reused and the OS decides how many run at once. Writing a pool
// on top of that would be a second scheduler competing with the first, and lanes that name their own
// callers ("the shader lane") put domain knowledge into infrastructure that should have none.
//
// # The one thing std::async gets wrong
//
// Its future's destructor **blocks**. Uniquely among futures: `~_Task_async_state()` calls `_Wait()`,
// so dropping a handle waits for the task to finish. That is the opposite of what abandoning should
// do, and it is a live path here rather than a theoretical one - sky_program::load_package_layer
// reuses an existing program object and overwrites `pending`, so re-selecting a sky while its shader
// is building would stall the render thread for the rest of a five-second compile.
//
// So the rule is simply: **the async future is never owned by the caller.** A handle that goes away
// hands its future to the retirement list below, and a later poll() destroys it once it has finished
// on its own. Nothing waits on the render thread, and nothing is detached into the void either.
//
// # Cancellation
//
// A stop flag, shared with the job. It is advisory and it has to be: D3DCompile takes no
// cancellation token and cannot be interrupted mid-compile. What the flag buys is real anyway - a
// job that has not started yet exits immediately, and a long loop that *can* check it (baking tiles,
// compositing images, a script's placement pass) bails at the next iteration instead of finishing
// work whose result is already superseded.
//
// # Rules for a job
//
//  - It must not touch the D3D9 device, or anything owned by it. Reading files, decoding images,
//    compiling shaders and computing pixels are the point; uploading the result is the caller's, on
//    the render thread, once the task reports ready.
//  - It must not throw. Project rule everywhere, and here there is nowhere for it to go.
//  - Anything it captures must outlive it, which in practice means capturing by value: by the time
//    it runs, the sky may have been switched and the device replaced.
//  - Callers needing two jobs not to overlap arrange that themselves, by keeping one handle and
//    abandoning the previous. That is one line where they are, and it does not require this file to
//    know what they do.
namespace tw::plugin::bg_work
{
// Shared between the caller and the job. Cheap to copy; the job takes it by value.
class stop_flag
{
public:
    stop_flag()
        : m_stopped { std::make_shared<std::atomic<bool>>(false) }
    {
    }

    void request_stop() const noexcept
    {
        m_stopped->store(true, std::memory_order_release);
    }

    // For the job to poll. False when the handle is empty, so a default-constructed flag reads as
    // "carry on" rather than as "stop immediately".
    [[nodiscard]] bool stop_requested() const noexcept
    {
        return m_stopped != nullptr && m_stopped->load(std::memory_order_acquire);
    }

private:
    std::shared_ptr<std::atomic<bool>> m_stopped;
};

namespace detail
{
// Takes ownership of a finished-or-not future so the caller does not have to wait for it. The
// predicate returns true once the future has completed and its state can be destroyed without
// blocking; poll() calls it and erases the entry when it does.
void retire(std::function<bool()> reap);

void note_started() noexcept;
void note_finished() noexcept;
} // namespace detail

// A handle to one task in flight. Non-blocking in every operation, destructor included - which is
// the entire reason this exists rather than a bare std::future.
template<typename T>
class task
{
public:
    task() = default;

    task(const task&) = delete;
    task& operator=(const task&) = delete;

    task(task&&) noexcept = default;

    task& operator=(task&& other) noexcept
    {
        if(this != &other) {
            abandon();
            m_future = std::move(other.m_future);
            m_stop = std::move(other.m_stop);
        }

        return *this;
    }

    ~task()
    {
        abandon();
    }

    [[nodiscard]] bool valid() const noexcept
    {
        return m_future.valid();
    }

    // Whether the result is there to be taken. A zero wait, safe to call every frame - which is what
    // collecting a finished compile amounts to.
    [[nodiscard]] bool ready() const noexcept
    {
        return m_future.valid() && m_future.wait_for(std::chrono::seconds { 0 }) == std::future_status::ready;
    }

    // Moves the result out, emptying the handle. Nothing when the task is still running, which the
    // caller should read as "ask again next frame" rather than as failure.
    [[nodiscard]] std::optional<T> take()
    {
        if(!ready()) {
            return std::nullopt;
        }

        std::optional<T> out = m_future.get();
        m_stop = {};

        return out;
    }

    // "Nobody wants this any more." Never blocks: the stop is requested (which a job that has not
    // started, or one that polls, will honour) and the future is retired to be destroyed later, off
    // this thread's critical path.
    void abandon() noexcept
    {
        if(!m_future.valid()) {
            m_stop = {};
            return;
        }

        m_stop.request_stop();

        detail::retire([future = std::make_shared<std::future<T>>(std::move(m_future))]() mutable {
            return future->wait_for(std::chrono::seconds { 0 }) == std::future_status::ready;
        });

        m_stop = {};
    }

    // The flag the job sees, so a caller can ask it to stop without giving up the result.
    [[nodiscard]] const stop_flag& stop() const noexcept
    {
        return m_stop;
    }

private:
    template<typename U, typename TWork>
    friend task<U> run(TWork);

    std::future<T> m_future;
    stop_flag m_stop;
};

// Queues `work` and hands back a handle to its result.
//
// The callable is invoked with the task's stop_flag when it accepts one, and with no arguments when
// it does not - so a job that cannot be interrupted anyway (a D3DCompile call) stays a plain lambda.
template<typename T, typename TWork>
[[nodiscard]] task<T> run(TWork work)
{
    task<T> handle;

    detail::note_started();

    handle.m_future = std::async(std::launch::async, [work = std::move(work), stop = handle.m_stop]() mutable -> T {
        struct scope {
            ~scope()
            {
                detail::note_finished();
            }
        } counted;

        // Nothing has looked at the result yet and the caller is already gone: this is the superseded
        // rebake, and it costs a branch instead of a core.
        if constexpr(std::is_invocable_v<TWork, const stop_flag&>) {
            return work(stop);
        }
        else {
            return work();
        }
    });

    return handle;
}

// Destroys the futures of abandoned tasks that have since finished. Call once per frame from the
// render thread; it never blocks and does nothing when the list is empty, which is almost always.
void poll() noexcept;

// Waits for everything still outstanding. Must complete before the DLL could be unmapped - a worker
// running plugin code through an unmapped image is not a crash anyone can debug. The plugin has no
// teardown path today, which makes this theoretical; the cost of having it right now is nothing.
void shutdown() noexcept;

// How many tasks are running or awaiting retirement. For the overlay, so "nothing is happening" and
// "three things are happening slowly" stop looking the same.
[[nodiscard]] int pending() noexcept;
} // namespace tw::plugin::bg_work
