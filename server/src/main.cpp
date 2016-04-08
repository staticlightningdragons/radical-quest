#include "Connection.hpp"
#include "GameState.hpp"
#include "LogWriter.hpp"
#include "NetworkManager.hpp"
#include "Player.hpp"

#include <map>
#include <unistd.h>

void notify_invalid_request(Player *p, EventRequest *r)
{
    Event invalid_event;
    invalid_event["type"] = string("InvalidEvent");
    invalid_event["message_id"] = (*r)["message_id"];
    p->get_connection()->submit_outgoing_event(invalid_event);
    delete r;
}

void notify_illegal_request(Player *p, EventRequest *r)
{
    Event illegal_event;
    illegal_event["type"] = string("IllegalEvent");
    illegal_event["message_id"] = (*r)["message_id"];
    p->get_connection()->submit_outgoing_event(illegal_event);
    delete r;
}

void handle_assign_game_request(Player *p, EventRequest *r, map<int, GameState*> &games, int &highest_game_id)
{
    // First, let's make sure they aren't already in a game.
    if(p->get_game_id() != 0)
    {
        notify_illegal_request(p, r);
        return;
    }

    // So a game_id was provided! Make sure it's valid number.
    int game_id = (*r)["game_id"].asInt();
    if(game_id != -1 && game_id != -2 && games.find(game_id) == games.end())
    {
        notify_illegal_request(p, r);
        return;
    }

    // If game_id is -2, they want a new game to be created for them.
    if(game_id == -2)
    {
        ++highest_game_id;
        games[highest_game_id] = new GameState(highest_game_id, "test.map");
        game_id = highest_game_id;
        games[game_id]->add_player(p);
    }

    // If game_id is -1, they want a random existing game. Otherwise we'll make a game for them.
    else if(game_id == -1)
    {
        // Try to put them in an existing game.
        for(pair<int, GameState*> pi : games)
        {
            if(pi.second->needs_player())
            {
                pi.second->add_player(p);
                game_id = p->get_game_id();
                break;
            }
        }

        // If they didn't get put in an existing game, go ahead and make one.
        if(game_id == -1)
        {
            ++highest_game_id;
            games[highest_game_id] = new GameState(highest_game_id, "test.map");
            game_id = highest_game_id;
            games[game_id]->add_player(p);
        }
    }

    // If all else fails, they wanted a specific game.
    else
    {
        GameState *g = games[game_id];
        if(!g->needs_player())
        {
            notify_illegal_request(p, r);
            return;
        }
        g->add_player(p);
        game_id = p->get_game_id();
    }

    // Finally, now that they're in a game, notify them of this.
    Event assign_game;
    assign_game["type"] = string("AssignGameEvent");
    assign_game["request_id"] = (*r)["request_id"];
    assign_game["game_id"] = game_id;
    p->get_connection()->submit_outgoing_event(assign_game);

    // Go ahead and delete the request now that we've handled it.
    delete r;
}

int main(int argc, char **argv)
{
    // initialize core components.
    LogWriter log;
    log.write("[MAIN] INFO: Starting RQ server, logging system initialized.");
    NetworkManager nm;
    init_networking_thread(&nm, &log);

    // important maps
    map<int, Player*> players;
    map<int, GameState*> games;
    int highest_game_id = 0;

    // main event loop
    while(true)
    {
        // Used for any output messages...
        char buffer[2048];

        // If any new players connected, create a Player entity for them.
        for(Connection *c = nm.pop_new_connection(); c != NULL; c = nm.pop_new_connection())
        {
            players[c->get_id()] = new Player(c->get_id(), c);
            memset(buffer, 0, 2048);
            sprintf(buffer, "[MAIN] INFO: A new Player with ID %d has connected.", c->get_id());
            log.write(buffer);
        }

        // Let's go ahead and handle any outstanding event requests now.
        for(EventRequest *r = nm.pop_incoming_request(); r != NULL; r = nm.pop_incoming_request())
        {
            // Get the requesting player.
            Player *p = players[(*r)["player_id"].asInt()];
            string type = (*r)["type"].asString();

            // AssignGameRequest
            if(type.compare("AssignGameRequest") == 0)
            {
                handle_assign_game_request(p, r, games, highest_game_id);
            }

            // PlayerQuitRequest
            else if(type.compare("PlayerQuitRequest") == 0)
            {
                // First notify the GameState.
                games[p->get_game_id()]->handle_request(r);

                // Then disconnect/delete the Player.
                nm.kill_connection(p->get_player_id());
                players.erase(p->get_player_id());
                delete p;
            }

            // All other requests just go straight to the GameState.
            else
            {
                games[p->get_game_id()]->handle_request(r);
            }
        }

        // Sleep for a bit. Don't want our poor server to die.
        usleep(250);
    }

    // make sure to shutdown everything
    shutdown_networking_thread();

    // also need to delete used memory
    for(pair<int, Player*> p : players)
    {
        delete p.second;
    }
    for(pair<int, GameState*> p : games)
    {
        delete p.second;
    }
    return 0;
}
