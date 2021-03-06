#include "GenericResponses.hpp"
#include "PlayingState.hpp"
#include "RequestVerification.hpp"

PlayingState::PlayingState(LogWriter *log, int _game_id, Player *_player_one, Player *_player_two,
        vector<Unit*> _units_one, vector<Unit*> _units_two, MapInfo *_map) :
    GameState(log, _game_id, _player_one, _player_two)
{
    units_one = _units_one;
    units_two = _units_two;
    map = _map;
    state_name = PLAYING;
    active_player = player_two;
    handle_turn_change();
}

PlayingState::~PlayingState()
{
    for(Unit *u : units_one)
    {
        delete u;
    }
    for(Unit *u : units_two)
    {
        delete u;
    }
}

void PlayingState::handle_request(Player *p, EventRequest *r)
{
    // Regardless of the type of Request, if it's not the Player's turn, it's illegal.
    if(p != active_player)
    {
        notify_illegal_request(p->get_connection(), r);
        return;
    }

    // Depending on the type, we do different things.
    string type = (*r)["type"].asString();
    if(type.compare("UnitMoveRequest") == 0)
    {
        handle_unit_move(p, r);
    }
    else if(type.compare("UnitInteractRequest") == 0)
    {
        handle_unit_interact(p, r);
    }
    else
    {
        notify_invalid_request(p->get_connection(), r);
    }
}

bool PlayingState::tick(double time)
{
    // If the turn is over, change turns.
    bool turn_complete = true;
    if(active_player == player_one)
    {
        for(Unit *u : units_one)
        {
            if(u->is_alive() && !u->has_interacted())
            {
                turn_complete = false;
            }
        }
    }
    else
    {
        for(Unit *u : units_two)
        {
            if(u->is_alive() && !u->has_interacted())
            {
                turn_complete = false;
            }
        }
    }
    if(turn_complete)
    {
        handle_turn_change();
    }

    // If one of the Players has no living Units, then we're done!
    bool should_continue = false;
    for(Unit *u : units_one)
    {
        if(u->is_alive())
        {
            should_continue = true;
        }
    }
    if(!should_continue)
    {
        return false;
    }
    for(Unit *u : units_two)
    {
        if(u->is_alive())
        {
            should_continue = true;
        }
    }
    if(!should_continue)
    {
        return false;
    }

    // Otherwise return TRUE.
    return true;
}

void PlayingState::handle_turn_change()
{
    // Set the active Player.
    if(active_player == player_one)
    {
        active_player = player_two;
    }
    else
    {
        active_player = player_one;
    }

    // Notify all units that they can move and interact again.
    for(Unit *u : units_one)
    {
        u->new_turn();
    }
    for(Unit *u : units_two)
    {
        u->new_turn();
    }

    // Notify players of the turn change.
    notify_turn_change();
}

void PlayingState::handle_unit_interact(Player *p, EventRequest *r)
{
    // Verify that this request is okay.
    if(!verify_unit_interact(r))
    {
        notify_invalid_request(p->get_connection(), r);
        return;
    }

    // Determine which set of Units we're working with.
    vector<Unit*> *friendly_units = NULL;
    vector<Unit*> *enemy_units = NULL;
    if(p == player_one)
    {
        friendly_units = &units_one;
        enemy_units = &units_two;
    }
    else if(p == player_two)
    {
        friendly_units = &units_two;
        enemy_units = &units_one;
    }

    // Verify that the attacking Unit exists.
    int unit_id = (*r)["unit_id"].asInt();
    if(unit_id >= friendly_units->size())
    {
        notify_illegal_request(p->get_connection(), r);
        return;
    }
    Unit *unit = (*friendly_units)[unit_id];

    // If the targetId is empty, don't do anything.
    int target_id = (*r)["target_id"].asInt();
    if(target_id == -1)
    {
        unit->set_interacted();
        notify_unit_interact(r, unit, NULL);
        return;
    }

    // Otherwise, go ahead and try to do stuff.
    // Verify that the target Unit exists.
    if(unit_id >= friendly_units->size())
    {
        notify_illegal_request(p->get_connection(), r);
        return;
    }

    // If the primary Unit is a healer, then the target Unit is friendly. Otherwise, enemy.
    Unit *target = NULL;
    if(unit->get_type() == HEALER)
    {
        target = (*friendly_units)[target_id];
    }
    else
    {
        target = (*enemy_units)[target_id];
    }

    // If it exists, get it. Also verify that both Units are alive.
    if(!unit->is_alive() || !target->is_alive())
    {
        notify_illegal_request(p->get_connection(), r);
        return;
    }

    // Cause the interaction and send off an event.
    if(!unit->interact(target))
    {
        notify_illegal_request(p->get_connection(), r);
        return;
    }

    // Go ahead and notify everyone.
    notify_unit_interact(r, unit, target);
}

void PlayingState::handle_unit_move(Player *p, EventRequest *r)
{
    // First verify that the event is valid.
    if(!verify_unit_move(r))
    {
        notify_invalid_request(p->get_connection(), r);
        return;
    }

    // Determine which set of Units we're working with.
    vector<Unit*> *units = NULL;
    if(p == player_one)
    {
        units = &units_one;
    }
    else if(p == player_two)
    {
        units = &units_two;
    }

    // Since it's valid, let's grab the Unit and its intended destination.
    int unit_id = (*r)["unit_id"].asInt();
    int x = (*r)["x"].asInt();
    int y = (*r)["y"].asInt();
    if(unit_id >= units->size())
    {
        notify_illegal_request(p->get_connection(), r);
        return;
    }
    Unit *unit = (*units)[unit_id];
    if(!unit->is_alive())
    {
        notify_illegal_request(p->get_connection(), r);
        return;
    }

    // Verify that the Unit has not moved yet.
    if(unit->has_moved())
    {
        notify_invalid_request(p->get_connection(), r);
        return;
    }

    // See if the Unit can move to that tile.
    // TODO - get this working again
    //if(!tile_reachable(unit->get_move_distance(), unit->get_x(), unit->get_y(), x, y))
    //{
        //notify_illegal_request(p->get_connection(), r);
        //return;
    //}

    // If we can move, go ahead annd move then!
    unit->set_position(x, y);
    notify_unit_move(r, unit);
}

void PlayingState::notify_turn_change()
{
    // Change the player_turn.
    int player_turn = 1;
    if(active_player == player_two)
    {
        player_turn = 2;
    }

    // Build the Event
    Event notify;
    notify["type"] = string("TurnChangeEvent");
    notify["game_id"] = game_id;
    notify["player_turn"] = player_turn;

    // Send to all connected players.
    send_all_players(notify);
}

void PlayingState::notify_unit_interact(EventRequest *r, Unit *first, Unit *second)
{
    // First get the Unit IDs
    int uid1 = -1;
    int uid2 = -1;
    if(first != NULL)
    {
        uid1 = first->get_unit_id();
    }
    if(second != NULL)
    {
        uid2 = second->get_unit_id();
    }

    // Build the Event
    Event notify;
    notify["type"] = string("UnitInteractEvent");
    notify["game_id"] = game_id;
    notify["request_id"] = (*r)["request_id"];
    notify["unit_id"] = uid1;
    notify["target_id"] = uid2;
    if(first != NULL)
    {
        notify["unit_hp"] = first->get_remaining_health();
    }
    if(second != NULL)
    {
        notify["target_hp"] = second->get_remaining_health();
    }

    // Send to all connected players.
    send_all_players(notify);
}

void PlayingState::notify_unit_move(EventRequest *r, Unit *target)
{
    // First get the Player IDs
    int uid = -1;
    if(target != NULL)
    {
        uid = target->get_unit_id();
    }

    // Build the Event
    Event notify;
    notify["type"] = string("UnitMoveEvent");
    notify["game_id"] = game_id;
    notify["request_id"] = (*r)["request_id"];
    notify["unit_id"] = uid;
    notify["unit_x"] = target->get_x();
    notify["unit_y"] = target->get_y();

    // Send to all connected players.
    send_all_players(notify);
}

bool PlayingState::tile_reachable(int distance, int x, int y, int to_x, int to_y)
{
    // If we can't go any farther, return false.
    if(distance < 0)
    {
        return  false;
    }

    // Verify that the current (x,y) are not outside the map.
    if(x < 0 || y < 0 || x >= map->get_width() || y >= map->get_height())
    {
        return false;
    }

    // Verify that this location isn't blocked.
    if(map->is_blocked(x, y) == true)
    {
        return false;
    }

    // If we're there, we're there!
    if(x == to_x && y == to_y)
    {
        return true;
    }

    // However, if we can go further, let's try.
    return tile_reachable(distance - 1, x + 1, y, to_x, to_y)
        || tile_reachable(distance - 1, x - 1, y, to_x, to_y)
        || tile_reachable(distance - 1, x, y + 1, to_x, to_y)
        || tile_reachable(distance - 1, x, y - 1, to_x, to_y);
}
