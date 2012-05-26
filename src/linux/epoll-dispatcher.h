#ifndef LINUX_EPOLL_DISPATCHER_H
# define LINUX_EPOLL_DISPATCHER_H

# include "../base.h"
# include "../errors.h"
# include "../macros.h"
# include "../stl-helpers.h"

# include <sys/epoll.h>
# include <list>

class EpollDispatcher;

class EpollDispatcher {
 public:
  static const int kQueueLength = 10;

  enum event_mask_e {
    NONE = 0,
    READ = BIT(0),
    WRITE = BIT(1)
  };

  typedef int event_mask_t;

  typedef function<void ()> event_handler_t;

  EpollDispatcher();
  ~EpollDispatcher();

  bool Start();
  void Stop();

  bool Init();

  bool AddFd(int fd, event_mask_t events,
	     const event_handler_t* readh, const event_handler_t* writeh);
  bool SetFd(int fd, event_mask_t set, event_mask_t clear,
	     const event_handler_t* readh, const event_handler_t* writeh);
  bool DelFd(int fd);

  bool GetFd(int fd, event_mask_t* events,
	     const event_handler_t** readh, const event_handler_t** writeh);

  template<typename TYPE>
  void DeleteLater(TYPE* todelete);

 private:
  int poll_fd_;

  struct EpollEvent {
    EpollEvent(
        int fd, const event_handler_t* readh, const event_handler_t* writeh)
        : fd(fd), deleted(false), read_handler(readh), write_handler(writeh) {
      epoll.events = 0;
      epoll.data.ptr = this;
    }
    static EpollEvent* FromEpoll(const epoll_event& epoll) {
      return reinterpret_cast<EpollEvent*>(epoll.data.ptr);
    }

    int fd;
    uint32_t deleted : 1;
    const event_handler_t* read_handler;
    const event_handler_t* write_handler;
    struct epoll_event epoll;
  };

  typedef unordered_map<int, EpollEvent*> FdMap;
  FdMap fd_events_;

  typedef list<Holder*> DeletionsSet;
  DeletionsSet pending_deletions_;

  bool ModFd(int fd, event_mask_t set, event_mask_t clear, EpollEvent* data,
	     const event_handler_t* readh, const event_handler_t* writeh);
};

template<typename TYPE>
void EpollDispatcher::DeleteLater(TYPE* todelete) {
  LOG_DEBUG("scheduling deletion of %08x", (unsigned int)todelete);
  pending_deletions_.push_back(new TypedHolder<TYPE>(todelete));
}

#endif /* LINUX_EPOLL_DISPATCHER_H */
