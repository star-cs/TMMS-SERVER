/*
 * @Author: star-cs
 * @Date: 2025-07-26 21:36:43
 * @LastEditTime: 2025-07-26 21:36:49
 * @FilePath: /TMMS-SERVER/tmms/coro_uring_net/io/base_io_type.hpp
 * @Description: 
 */

#include "coro_uring_net/core/engine.hpp"
namespace tmms::net
{

// If you need to use the feature "IOSQE_FIXED_FILE", just include fixed_fds as member varaible,
// this class will automatically fetch fixed fds from local engine's io_uring
struct fixed_fds
{
    fixed_fds() noexcept
    {
        // borrow fixed fd from uring
        item = local_engine().get_uring().get_fixed_fd();
    }

    ~fixed_fds() noexcept { return_back(); }

    inline auto assign(int& fd, int& flag) noexcept -> void
    {
        if (!item.valid())
        {
            return;
        }
        *(item.ptr) = fd;
        fd          = item.idx;
        flag |= IOSQE_FIXED_FILE;
        local_engine().get_uring().update_register_fixed_fds(item.idx);
    }

    inline auto return_back() noexcept -> void
    {
        // return back fixed fd to uring
        if (item.valid())
        {
            local_engine().get_uring().back_fixed_fd(item);
            item.set_invalid();
        }
    }

    uring_fds_item item;
};
}; // namespace tmms::net
