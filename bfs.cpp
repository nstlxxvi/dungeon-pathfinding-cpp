#include <cassert>
 #include <iomanip>
 #include <cstdint>
 #include <iostream>
 #include <memory>
 #include <limits>
 #include <optional>
 #include <algorithm>
 #include <functional>
 #include <bitset>
 #include <list>
 #include <array>
 #include <vector>
 #include <deque>
 #include <unordered_set>
 #include <unordered_map>
 #include <set>
 #include <map>
 #include <stack>
 #include <queue>
 #include <random>
 #include <type_traits>
 #include <compare>
 #include <ranges>
 #include <optional>
 #include <variant>

 struct Item {
   enum Type : uint8_t {
     Weapon = 0,
     Armor = 1,
     RubberDuck = 2,
     TYPE_COUNT = 3,
   };

   std::string name;
   Type type;
   int hp = 0, off = 0, def = 0;
   int stacking_off = 0, stacking_def = 0;
   bool first_attack = false; // Hero attacks first.
   bool stealth = false; // Hero can sneak past monsters (but cannot loot items while sneaking).

   friend auto operator <=> (const Item&, const Item&) = default;
 };

 struct Monster {
   int hp = 0, off = 0, def = 0;
   int stacking_off = 0, stacking_def = 0;
 };

 using RoomId = size_t;
 using ItemId = size_t;

 struct Room {
   std::vector<RoomId> neighbors;
   std::optional<Monster> monster;
   std::vector<Item> items;
 };

 struct Move { RoomId room; };
 struct Pickup { ItemId item; };
 struct Drop { Item::Type type; };
 using Action = std::variant<Move, Pickup, Drop>;


 std::optional<int> turns_to_kill(int hp, int dmg, int stacking_dmg) {
   assert(hp > 0);

   if (stacking_dmg == 0) {
     if (dmg <= 0) return {};
     return (hp + dmg - 1) / dmg;
   }

   int i = 0;
   for (; hp > 0; i++) {
     if (dmg <= 0 && stacking_dmg < 0) return {};
     hp -= std::max(dmg, 0);
     dmg += stacking_dmg;
   }

   return i;
 }

 enum CombatResult {
   A_WINS, B_WINS, TIE
 };

 // Monster `a` attacks first
 CombatResult simulate_combat(Monster a, Monster b) {
   a.def += a.stacking_def;

   auto a_turns = turns_to_kill(b.hp, a.off - b.def, a.stacking_off - b.stacking_def);
   auto b_turns = turns_to_kill(a.hp, b.off - a.def, b.stacking_off - a.stacking_def);

   if (!a_turns && !b_turns) return TIE;
   if (!a_turns) return B_WINS;
   if (!b_turns) return A_WINS;
   return *a_turns <= *b_turns ? A_WINS : B_WINS;
 }

 struct State
 {
   RoomId room;
   std::array<std::optional<Item>, 3> equip;
   bool has_treasure = false;
   //bool can_stealth = false;
   bool can_pickup = true;
   bool operator<(const State& other) const
   {
     if (room != other.room)
     {
       return room < other.room;
     }
     if (equip != other.equip)
     {
       return equip < other.equip;
     }
     if (has_treasure != other.has_treasure)
     {
       return has_treasure < other.has_treasure;
     }

     return can_pickup < other.can_pickup;
   }
   bool operator==(const State& other) const
   {
     return room == other.room && equip == other.equip && has_treasure == other.has_treasure && can_pickup == other.can_pickup;
   }
 };
   struct StateHash {
     size_t operator()(const State& state_hash) const noexcept {
       size_t h = std::hash<size_t>()(state_hash.room);
       for (const auto& item : state_hash.equip) {
         if (item.has_value()) {
           h ^= std::hash<std::string>()(item->name)
                + 0x9e3779b9 + (h << 6) + (h >> 2);
         } else {
           h ^= 0x9e3779b9 + (h << 6) + (h >> 2);
         }
       }
       h ^= std::hash<bool>()(state_hash.has_treasure)
            + 0x9e3779b9 + (h << 6) + (h >> 2);
       h ^= std::hash<bool>()(state_hash.can_pickup)
            + 0x9e3779b9 + (h << 6) + (h >> 2);
       return h;
     }
   };
 bool monster_in_the_room(const Room& room)
 {
   if (room.monster.has_value())
   {
     return true;
   }
   return false;
 };
 bool has_stealth( const State& s)
 {
   for (const auto& item : s.equip)
   {
     if (item.has_value() && item->stealth)
       return true;
   }
   return false;
 };

 bool can_survive (const State& s, const Room& r)
 {
   // if monster is  not in the room - safe
   if (monster_in_the_room(r) == false)
   {
     return true;
   }

   Monster hero;
   hero.hp = 10000;
   hero.off = 3;
   hero.def = 2;
   hero.stacking_off = 0;
   hero.stacking_def = 0;

   //присваиваем герою фичи от айтемов
   for (const auto& item: s.equip)
   {
     if (item.has_value())
     {
       hero.hp += item->hp;
       hero.off += item->off;
       hero.def += item->def;
       hero.stacking_off += item->stacking_off;
       hero.stacking_def += item->stacking_def;
     }
   }

   if (hero.hp <= 0) hero.hp = 1;

   Monster enemy = *r.monster;

   bool hero_first = false;
   for (auto& item : s.equip)
   {
     if (item.has_value() && item -> first_attack) hero_first = true;
   }

   CombatResult result;
   if (hero_first)
   {
     result = simulate_combat(hero, enemy);
     return result == A_WINS;
   }
   else
   {
     result = simulate_combat(enemy, hero);
     return result == B_WINS;
   }
 }
   // check if the monster is in the room
   // bool for stealth
 std::vector<Action> find_shortest_path(
   const std::vector<Room>& rooms,
   const std::vector<RoomId>& entrances,
   RoomId treasure
 ) {
   std::deque<State> dq;
   std::unordered_map<State, bool, StateHash> visited;
   std::unordered_map<State, std::pair<State, Action>, StateHash> path;

   State target;
   bool found = false;

   std::unordered_set<RoomId> entrances_set(entrances.begin(), entrances.end());

   for (size_t start : entrances)
   {
     State init;
     init.room = start;
     init.has_treasure = (start == treasure);
     init.can_pickup = true;

     const Room& r = rooms[start];
     if (monster_in_the_room(r) == true)
     {
       if (has_stealth(init) == false && can_survive(init, r) == false)
         continue;
        if (has_stealth(init) == true && can_survive(init, r) == false)
        {
          init.can_pickup = false;
        }
     }
     if (!visited[init])
     {
       visited[init] = true;
       path[init] = {init, Move{start}};
       dq.push_back(init);
     }
     if (monster_in_the_room(r) && has_stealth(init))
     {
       State st = init;
       st.can_pickup = false;
       if (!visited[st])
       {

         visited[st] = true;
         path[st] = {init, Move{start}};
         dq.push_back(st);
       }
     }
   }


   while (!dq.empty())
   {
     State current_state = dq.front();
     dq.pop_front();

     const Room& r = rooms[current_state.room];

       // если у героя уже есть клад и он дошёл до входа — всё, конец
       if (current_state.has_treasure) {
         if (entrances_set.contains(current_state.room)) {
           target = current_state;
           found = true;
           break;
         }
       }

        for (RoomId neib : r.neighbors)
        {
          const Room& next_room = rooms[neib];

          if (!monster_in_the_room(next_room))
          {
            State new_state = current_state;
            new_state.room = neib;
            new_state.can_pickup = true;
            new_state.has_treasure = current_state.has_treasure || (neib == treasure);

            if (!visited[new_state])
            {

              visited[new_state] = true;
              path[new_state] = {current_state, Move{neib}};
              dq.push_back(new_state);
            }
          }

          else
          {
            if (can_survive(current_state, next_room))
            {
              State new_state = current_state;
              new_state.room = neib;
              new_state.can_pickup = true;
              new_state.has_treasure = current_state.has_treasure || (neib == treasure);

              if (!visited[new_state])
              {

                visited[new_state] = true;
                path[new_state] = {current_state, Move{neib}};
                dq.push_back(new_state);
              }
            }
          }
          if (has_stealth(current_state))
          {
            State new_state = current_state;
            new_state.room = neib;
            new_state.can_pickup = false;
            new_state.has_treasure = current_state.has_treasure; //|| (neib == treasure);
            if (!visited[new_state])
            {
              visited[new_state] = true;
              path[new_state] = {current_state, Move{neib}};
              dq.push_back(new_state);
            }
          }
        }

     //pickup
     if (current_state.can_pickup == true)
     {
       for (size_t i = 0; i < r.items.size(); i++)
       {
         const Item& item = r.items[i];

         if (!current_state.equip[item.type].has_value() || *(current_state.equip[item.type]) != item )
         {
           State new_state = current_state;
           new_state.equip[item.type] = item;

           if (!visited[new_state])
           {
             visited[new_state] = true;
             path[new_state] = {current_state, Pickup{i}};
             dq.push_front(new_state);
           }
         }
       }
     }

     for (int t = 0; t < 3; t++)
     {
       if (current_state.equip[t].has_value())
       {
         State new_state = current_state;
         new_state.equip[t].reset();
         if (!visited[new_state])
         {
           visited[new_state] = true;
           dq.push_front(new_state);
           path[new_state] = {current_state, Drop{(Item::Type)t}};
         }
       }
     }
   }
   if (!found) return {};

   std::vector<Action> result;
   State s = target;
   while (path.count(s))
   {
     result.push_back(path[s].second);
     if (s == path[s].first)
     {
       break;
     }
     s = path[s].first;
   }
   std::reverse(result.begin(), result.end());
   return result;
 }

int main() {
   Room r0, r1;

   r0.neighbors = {1};
   r1.neighbors = {0};

   r1.monster = Monster{100, 3, 2};

   std::vector<Room> rooms = {r0, r1};

   std::vector<RoomId> entrances = {0};
   
   RoomId treasure = 1;

   auto result = find_shortest_path(rooms, entrances, treasure);

   if (result.empty()) {
     std::cout << "No path found\n";
   } else {
     std::cout << "Path found! Actions:\n";

     for (const auto& action : result) {
       if (std::holds_alternative<Move>(action)) {
         std::cout << "Move to room "
                   << std::get<Move>(action).room << "\n";
       } else if (std::holds_alternative<Pickup>(action)) {
         std::cout << "Pickup item "
                   << std::get<Pickup>(action).item << "\n";
       } else if (std::holds_alternative<Drop>(action)) {
         std::cout << "Drop item type "
                   << std::get<Drop>(action).type << "\n";
       }
     }
   }

   return 0;
 }