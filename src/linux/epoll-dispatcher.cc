#include "epoll-dispatcher.h"
#include "../stl-helpers.h"
#include "../event-scheduler.h"

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <memory>

EpollDispatcher::EpollDispatcher() 
    : poll_fd_(-1) {
}

EpollDispatcher::~EpollDispatcher() {
  if (poll_fd_ >= 0)
    close(poll_fd_);
}

bool EpollDispatcher::Init() {
  poll_fd_ = epoll_create(kQueueLength);
  if (poll_fd_ < 0)
    // FIXME: error!
    return false;
  return true;
}

bool EpollDispatcher::SetFd(
    int fd, event_mask_t set, event_mask_t clear,
    const event_handler_t* readh, const event_handler_t* writeh) {
  RUNTIME_FATAL_UNLESS(poll_fd_ >= 0)("must first call Init()");

  EpollEvent* data(StlMapGet(fd_events_, fd));
  if (!data)
    return AddFd(fd, set, readh, writeh);
  return ModFd(fd, set, clear, data, readh, writeh);
}

bool EpollDispatcher::AddFd(
    int fd, event_mask_t events,
    const event_handler_t* readh, const event_handler_t* writeh) {
  RUNTIME_FATAL_UNLESS(poll_fd_ >= 0)("must first call Init()");

  LOG_DEBUG("for %d, mask %08x, readh 0x%p, writeh 0x%p",
	    fd, events, (void*)readh, (void*)writeh);

  auto_ptr<EpollEvent> data(new EpollEvent(fd, readh, writeh));
  if (events & READ)
    data->epoll.events |= EPOLLIN;
  if (events & WRITE)
    data->epoll.events |= EPOLLOUT;

  LOG_DEBUG("for %d, setting events to %08x", fd, data->epoll.events);
  if (epoll_ctl(poll_fd_, EPOLL_CTL_ADD, fd, &(data->epoll)) != 0) {
    LOG_ERROR("epoll_ctl error, %s", strerror(errno));
    // FIXME: handle error!
    return false;
  }

  fd_events_[fd] = data.release();
  return true;
}

bool EpollDispatcher::DelFd(int fd) {
  RUNTIME_FATAL_UNLESS(poll_fd_ >= 0)("must first call Init()");
  LOG_DEBUG("marking fd %d for deletion", fd);

  FdMap::iterator data(fd_events_.find(fd));
  if (data == fd_events_.end()) {
    LOG_DEBUG("removing unknown file descriptor");
    return true;
  }

  EpollEvent* event(data->second);

  event->deleted = true;
  fd_events_.erase(data);
  DeleteLater(event);

  if (epoll_ctl(poll_fd_, EPOLL_CTL_DEL, fd, &(event->epoll)) != 0) {
    LOG_ERROR("epoll_ctl error, %s", strerror(errno));
    // FIXME: handle error!
    return false;
  }

  return true;
}

bool EpollDispatcher::GetFd(
    int fd, int* events,
    const event_handler_t** readh, const event_handler_t** writeh) {
  LOG_FATAL("NOT IMPLEMENTED");
  return false;
}

bool EpollDispatcher::ModFd(
    int fd, event_mask_t set, event_mask_t clear, EpollEvent* data,
    const event_handler_t* readh, const event_handler_t* writeh) {
  RUNTIME_FATAL_UNLESS(poll_fd_ >= 0)("must first call Init()");

  LOG_DEBUG("for %d, set %08x, clear %08x, readh 0x%p, writeh 0x%p",
	    fd, set, clear, (void*)readh, (void*)writeh);

  uint32_t events(data->epoll.events);
  if (set & READ) {
    data->read_handler = readh;
    data->epoll.events |= EPOLLIN;
  }
  if (set & WRITE) {
    data->write_handler = writeh;
    data->epoll.events |= EPOLLOUT;
  }
  if (clear & READ)
    data->epoll.events &= ~EPOLLIN;
  if (clear & WRITE)
    data->epoll.events &= ~EPOLLOUT;

  LOG_DEBUG("for %d, setting events to %08x", fd, data->epoll.events);
  if (events == data->epoll.events) {
    LOG_DEBUG("no changes to event set, returning true");
    return true;
  }

  if (epoll_ctl(poll_fd_, EPOLL_CTL_MOD, fd, &(data->epoll)) != 0) {
    LOG_ERROR("epoll_ctl error %s", strerror(errno));
    data->epoll.events = events;
    // FIXME: handle error!
    return false;
  }
  return true;
}

bool EpollDispatcher::Start(EventScheduler* scheduler) {
  RUNTIME_FATAL_UNLESS(poll_fd_ >= 0)("must first call Init()");

  epoll_event events[kQueueLength];
  while (true) {
    LOG_DEBUG("waiting for events.");

    for (DeletionsSet::const_iterator it(pending_deletions_.begin());
	 it != pending_deletions_.end(); ++it) {
      LOG_DEBUG("deleting pointer %08x", (unsigned int)*it);
      delete *it;
    }
    pending_deletions_.clear();

    int timeout = -1;
    if (scheduler)
      timeout = scheduler->GetTimeUntilNextEvent();

    // FIXME: add support for handling signals.
    int found = epoll_wait(poll_fd_, events, kQueueLength, timeout);
    if (found < 0) {
      LOG_DEBUG("epoll error: %d - %s", errno, strerror(errno));
      // FIXME: handle errors.
      continue;
    }

    if (scheduler)
      scheduler->ProcessPendingEvents();

    LOG_DEBUG("epoll events %d", found);

    for (int i = 0; i < found; i++) {
      EpollEvent* event = EpollEvent::FromEpoll(events[i]);
      LOG_DEBUG("received event %08x for fd %d", events[i].events, event->fd);

      if (event->deleted) {
	LOG_DEBUG("but handler has been deleted");
	continue;
      }

      if (events[i].events & (EPOLLIN | EPOLLPRI | EPOLLERR | EPOLLHUP))
        (*(event->read_handler))();
      if (events[i].events & EPOLLOUT)
        (*(event->write_handler))();
    }
  }
  return false;
}
