#include "Connection.hpp"

Connection::Connection(int _playerId)
{
    playerId = _playerId;
}

Connection::~Connection()
{
    recv_mutex.lock();
    for(EventRequest *r = pop_incoming_request(); r != NULL; r = pop_incoming_request())
    {
        delete r;
    }
    recv_mutex.unlock();
    send_mutex.lock();
    for(string *s = pop_outgoing_message(); s != NULL; s = pop_outgoing_message())
    {
        delete s;
    }
    send_mutex.unlock();
}

void Connection::submit_incoming_message(string &message)
{
    // TODO string -> JSON
}

void Connection::submit_outgoing_event(Event &event)
{
    // TODO JSON -> string
}

EventRequest *Connection::pop_incoming_request()
{
    recv_mutex.lock();
    if(recv_queue.empty())
    {
        recv_mutex.unlock();
        return NULL;
    }
    EventRequest *r = recv_queue.front();
    recv_queue.pop();
    recv_mutex.unlock();
    return r;
}

string *Connection::pop_outgoing_message()
{
    send_mutex.lock();
    if(send_queue.empty())
    {
        send_mutex.unlock();
        return NULL;
    }
    string *r = send_queue.front();
    send_queue.pop();
    send_mutex.unlock();
    return r;
}
