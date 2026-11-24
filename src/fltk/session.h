#pragma once
#include "core.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

struct pg_conn;
namespace pgjr
{
struct Event
{
    enum Kind
    {
        Connected,
        Batch,
        Message,
        Finished,
        ConnectionError
    } kind = Message;
    Section section;
    std::size_t index = 0;
    std::string text, database;
    bool success = true, transaction = false, needs_password = false;
    int backend = 0;
    std::uint64_t request = 0;
};
// Own one PGconn on one worker. All UI interactions travel through the bounded event queue.
// A session accepts one operation at a time; destruction requests cancellation, joins and closes PGconn.
class Session
{
  public:
    explicit Session(std::function<void()> wake = {});
    ~Session();
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;
    bool connect(Profile, std::string database, std::string password);
    bool execute(std::string sql, bool rollback_explain = false, std::filesystem::path full_csv = {},
                 std::vector<int> widths = {});
    void cancel();
    bool busy() const
    {
        return busy_;
    }
    bool connected() const
    {
        return connected_;
    }
    bool inTransaction() const
    {
        return transaction_;
    }
    std::vector<Event> poll();
    std::uint64_t request() const
    {
        return latest_;
    }

  private:
    struct Work
    {
        bool connect = false;
        Profile profile;
        std::string database, password, sql;
        bool rollback = false;
        std::filesystem::path csv;
        std::vector<int> widths;
        std::uint64_t request = 0;
    };
    void run();
    void open(const Work &);
    void script(const Work &);
    bool command(const std::string &, const std::function<void(void *)> &result, bool cleanup = false);
    void post(Event);
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<Event> events_;
    std::unique_ptr<Work> work_;
    std::atomic_bool stopping_{false}, cancelled_{false}, busy_{false}, connected_{false},
        transaction_{false};
    std::function<void()> wake_;
    pg_conn *connection_ = nullptr;
    std::atomic_uint64_t latest_{0};
    std::uint64_t active_ = 0;
    std::thread thread_;
};
} // namespace pgjr
