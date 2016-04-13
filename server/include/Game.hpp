#ifndef GAMESTATE_H
#define GAMESTATE_H

#include "Event.hpp"
#include "LogWriter.hpp"
#include "Player.hpp"
#include "Unit.hpp"

#include <map>
#include <string>
#include <vector>
#include <utility>

#include "json/json.h"

using namespace std;

enum State
{
    WAITING_FOR_PLAYERS,
    UNIT_SELECTION,
    GAME_PLAYING,
    GAME_OVER
};

class Game
{
    public:
        // Constructor and Destructor
        Game(int _game_id, std::string _map_file);
        ~Game();

        // Getters
        int get_game_id() { return game_id; }
        int get_map_width() { return map_width; }
        int get_map_height() { return map_height; }
        vector<Unit*> &get_units() { return units; }
        bool needs_player();

        // Tick the Game. Return FALSE if game is over, TRUE otherwise.
        bool tick(double time_in_seconds);

        // Handle an EventRequest bound for this Game.
        void handle_request(Player *p, EventRequest *req);

    private:
        // Build the map (this->tiles) from a JSON file.
        void build_map_from_file(string &map_filename);

        // Check if a given location is within reach of a Unit.
        bool tile_reachable(int distance, int x, int y, int to_x, int to_y);

        // Handle EventRequests
        void handle_assign_game(Player *p, EventRequest *r);
        void handle_unit_interact(Player *p, EventRequest *r);
        void handle_unit_move(Player *p, EventRequest *r);
        void handle_unit_selection(Player *p, EventRequest *r);
        void handle_player_quit(Player *p, EventRequest *r);
        void handle_turn_change(Player *p, EventRequest *r);

        // Send notifications of Events to Players.
        void send_all_players(Event &e);
        void notify_assign_game(EventRequest *r);
        void notify_select_units(EventRequest *r, Player *p);
        void notify_state_change(EventRequest *r);
        void notify_turn_change(EventRequest *r);
        void notify_unit_interact(EventRequest *r, Unit *first, Unit *second);
        void notify_unit_move(EventRequest *r, Unit *target);

        int game_id;        // ID of the Game
        int map_width;      // width of the map in tiles
        int map_height;     // height of the map in tiles

        bool **blocked_tiles;   // an array storing all of the blocked tiles

        State current_gamestate;    // the current state of this Game

        vector<Unit*> units;        // all of the units currently in-play

        Player *player_one;     // the first Player in the game
        Player *player_two;     // the second Player in the game
        int player_turn;        // either 1 or 2

};

#endif