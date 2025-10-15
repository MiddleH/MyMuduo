#include "Poller.h"
#include "Channal.h"

Poller::Poller(EventLoop *loop)
    : ownerLoop_(loop)
{
}
Poller::~Poller()
{
}

bool Poller::hasChannal(Channal *channal) const
{
    auto it = channals_.find(channal->fd());
    return it != channals_.end() && it->second == channal;
}