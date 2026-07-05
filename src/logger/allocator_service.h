#ifndef ALLOCATOR_SERVICE
#define ALLOCATOR_SERVICE

#include "core/threading/mpsc_queue.h"

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <new>
#include <thread>
#include <vector>
#include <new>
#include <chrono>
#include "logger.h"
#include "logger_macros.h"

namespace logger::svc {

using CallerId  = uint32_t;
using RequestId = uint64_t;

struct Ticket {
    CallerId  caller{};
    RequestId id{};
};

enum class RespKind : uint8_t { AllocOk, FreeOk, Error };

struct Response {
    RespKind  kind{};
    Ticket    ticket{};
    void*     ptr{nullptr};
    std::size_t bytes{0};
    int       error_code{0};
};

enum class ReqKind : uint8_t { Alloc, Free, Stop };

struct Request {
    ReqKind   kind{};
    Ticket    ticket{};
    std::size_t bytes{0};
    std::size_t align{alignof(std::max_align_t)};
    void*     ptr{nullptr};
};


template <
    std::size_t ReqQCapPow2,
    std::size_t InboxCapPow2,
    std::size_t MaxCallers,
    std::size_t LogQCapPow2
>
class AllocatorService {
public:
    class CallerHandle {
    public:
        CallerHandle() = default;
        ~CallerHandle() { reset(); }

        CallerHandle(const CallerHandle&) = delete;
        CallerHandle& operator=(const CallerHandle&) = delete;

        CallerHandle(CallerHandle&& o) noexcept { move_from_(o); }
        CallerHandle& operator=(CallerHandle&& o) noexcept {
            if (this != &o) { reset(); move_from_(o); }
            return *this;
        }

        Ticket allocate(std::size_t bytes, std::size_t align = alignof(std::max_align_t)) {
            Ticket t{ id_, next_id_++ };
            Request r;
            r.kind = ReqKind::Alloc;
            r.ticket = t;
            r.bytes = bytes;
            r.align = align;
            service_->enqueue_request_with_backoff_(r);
            return t;
        }

        Ticket free(void* ptr, std::size_t align = alignof(std::max_align_t)) {
            Ticket t{ id_, next_id_++ };
            Request r;
            r.kind = ReqKind::Free;
            r.ticket = t;
            r.ptr = ptr;
            r.align = align;
            service_->enqueue_request_with_backoff_(r);
            return t;
        }


        bool poll(Response& out) {
            return inbox_.try_pop(out);
        }

        CallerId id() const { return id_; }

    private:
        friend class AllocatorService;

        void reset() {
            if (service_) {
                service_->unregister_caller_(id_, &inbox_);
                service_ = nullptr;
                id_ = 0;
            }
        }

        void move_from_(CallerHandle& o) {
            service_ = o.service_;
            id_      = o.id_;
            next_id_ = o.next_id_.load(std::memory_order_relaxed);
            // NOTE: we do not move the inbox queue object safely across threads
            o.service_ = nullptr;
            o.id_ = 0;
        }

        AllocatorService* service_{nullptr};
        CallerId id_{0};
        std::atomic<RequestId> next_id_{1};
        mpsc::BoundedMPSC<Response, InboxCapPow2> inbox_{};
    };

    AllocatorService()
        : running_(true),
          logger_(),
          th_([this]{ run(); })
    {
        caller_inboxes_.resize(MaxCallers);
        for (auto& a : caller_inboxes_) a.store(nullptr, std::memory_order_relaxed);
    }

    ~AllocatorService() {
        stop();
    }

    AllocatorService(const AllocatorService&) = delete;
    AllocatorService& operator=(const AllocatorService&) = delete;

    CallerHandle create_caller() {
        const CallerId id = allocate_caller_id_();
        CallerHandle h;
        h.service_ = this;
        h.id_ = id;
        register_caller_(id, &h.inbox_);
        return h;
    }

    void stop() {
        bool expected = true;
        if (!running_.compare_exchange_strong(expected, false)) return;

        Request r;
        r.kind = ReqKind::Stop;
        r.ticket = Ticket{0, 0};
        enqueue_request_with_backoff_(r);

        if (th_.joinable()) th_.join();
        logger_.stop();
    }

    logger::Logger<LogQCapPow2, 256, true>& logger() { return logger_; }

private:
    using Inbox = logger::mpsc::BoundedMPSC<Response, InboxCapPow2>;

    void enqueue_request_with_backoff_(const Request& r) {
        for (int spins = 0; spins < 500; ++spins) {
            if (req_q_.try_push(r)) return;
            logger::cpu::relax(spins);
        }
        // THINK: If we would want to never drop allocator requests, 
        // block on a semaphore/condvar (will context switch),
        // or shard the service (TODO below),
        // or apply caller-side backpressure policies.
    }

    CallerId allocate_caller_id_() {
        const CallerId id = next_caller_id_.fetch_add(1, std::memory_order_relaxed);
        return id;
    }

    void register_caller_(CallerId id, Inbox* inbox) {
        if (id >= MaxCallers) return; // production: handle error
        caller_inboxes_[id].store(inbox, std::memory_order_release);
    }

    void unregister_caller_(CallerId id, Inbox* inbox) {
        (void)inbox;
        if (id >= MaxCallers) return;
        caller_inboxes_[id].store(nullptr, std::memory_order_release);
    }

    Inbox* inbox_of_(CallerId id) {
        if (id >= MaxCallers) return nullptr;
        return caller_inboxes_[id].load(std::memory_order_acquire);
    }

    void run() {
        // TODO(per-caller allocator): Replace this single consumer loop with a routing layer:
        // either N allocator shards each with its own req queue and thread,
        // or one thread per caller (mapping caller_id -> shard),

        while (running_.load(std::memory_order_relaxed)) {
            Request req;
            if (!req_q_.try_pop(req)) {
                std::this_thread::yield();
                continue;
            }

            if (req.kind == ReqKind::Stop) break;

            Inbox* inbox = inbox_of_(req.ticket.caller);
            if (!inbox) {
                continue;
            }

            Response resp;
            resp.ticket = req.ticket;

            if (req.kind == ReqKind::Alloc) {
                void* p = nullptr;
                p = ::operator new(req.bytes, std::align_val_t(req.align), std::nothrow);
                if (p) {
                    resp.kind = RespKind::AllocOk;
                    resp.ptr = p;
                    resp.bytes = req.bytes;
                } else {
                    resp.kind = RespKind::Error;
                    resp.error_code = 1;
                }

                logger_.logf(logger::Level::Debug,
                logger::Site{"ALLOC", __FILE__, __func__, (uint32_t)__LINE__},
                            "alloc bytes=%zu align=%zu", req.bytes, req.align);
            } else if (req.kind == ReqKind::Free) {
                ::operator delete(req.ptr, std::align_val_t(req.align));
                resp.kind = RespKind::FreeOk;
            }

            bool sent = false;
            for (int spins = 0; spins < 300; ++spins) {
                if (inbox->try_push(resp)) { sent = true; break; }
                logger::cpu::relax(spins);
            }
            (void)sent;
        }

        // Optional drain.. idk may be smth smarter
        Request req;
        while (req_q_.try_pop(req)) { /* discard */ }
    }

    std::atomic<bool> running_{false};
    std::atomic<CallerId> next_caller_id_{1};

    logger::mpsc::BoundedMPSC<Request, ReqQCapPow2> req_q_{};
    
    std::vector<std::atomic<Inbox*>> caller_inboxes_;

    logger::Logger<LogQCapPow2, 256, true> logger_;
    std::thread th_;
};

} // namespace logger::svc

#endif