#pragma once
#include "gm_effects.hpp"
#include "gm_state.hpp"
#include "expected.hpp"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

// GM Adjudicator — v0 serialization bridge (JSON <-> contract structs).
//
// This is the *only* boundary that turns the human GM's (later: the model's)
// textual work order into typed effects, and the engine's state into text the
// GM can read. It is PURE TRANSLATION — no game rules, no validation. Every
// acceptance check (ids exist, preconditions, magnitude) belongs to the
// validator gauntlet, which runs AFTER this bridge produces a WorkOrder.
//
// Dual-reader (spec §2): the JSON schema below is exactly what a human types
// in Experiment 1 and what a grammar-constrained Gemma 4 E4B emits in
// Experiment 2. One parser, two producers.
//
// Closed-vocabulary guarantee: the vocabulary is enforced HERE, at parse time.
// An effect whose "type" is not one of the seven primitives is rejected
// immediately — parse_work_order/parse_world return Expected::err(...), so an
// invented verb never reaches the gauntlet. (Mapping novel *intent* onto
// existing primitives remains the GM's job upstream of the JSON.)
//
// No C++ exceptions cross this header's public API (parse_work_order,
// parse_world) — see coding_style.md. Internally, individual from_json
// overloads below still `throw` on a bad enum-like string or the closed-
// vocabulary gate: nlohmann's own recursive ADL dispatch (.at()/.get<T>())
// has no non-throwing path, so a contained throw is the only way a nested
// from_json call can signal failure back up through it. parse_work_order/
// parse_world are the one place that throw is ever caught, converting it to
// an Expected — nothing upstream of this header ever sees an exception.
//
// JSON shapes
// -----------
// effect : { "type": "<primitive>", <fields…> }
//   damage             : { source, target, via, amount }
//   modify_disposition : { entity, delta }
//   transfer_item      : { item, from, to }          // to == "__void__" ⇒ consumed
//   modify_resource    : { actor, resource: "gold"|"hp", delta }
//   set_object_state   : { object, property, value } // value: bool|int|string
//   set_flag           : { key, value }              // value: bool|int|string
//
// work order : {
//   interpretation, ruling: "allow"|"partial"|"deny",
//   modifiers: [ {label, value} ],                    // optional; omit/[] ⇒ no roll
//   effects_on_success: [ effect… ], narration_on_success,
//   effects_on_failure: [ effect… ], narration_on_failure   // optional when no roll
// }
//
// state : { room, objects, entities, player }  (see gm-adjudicator.md §4)
//   entity/player also carry: max_hp (ceiling), vital: "alive"|"unconscious"|"dead"
//   — both tolerant of omission (default: max_hp = current hp, vital = "alive")
//   so fixtures predating this field still parse.

// ---------------------------------------------------------------------------
// PropertyValue is std::variant<bool,int64_t,std::string>. Its associated
// namespace is std, not gm, so plain gm::to_json/from_json are invisible to
// nlohmann's ADL. A serializer specialization is the correct hook for a type
// you cannot place functions beside.
// ---------------------------------------------------------------------------
namespace nlohmann {
template <>
struct adl_serializer<gm::PropertyValue> {
    static void to_json(json& j, const gm::PropertyValue& v) {
        std::visit([&](auto&& x) { j = x; }, v);
    }
    static void from_json(const json& j, gm::PropertyValue& v) {
        if (j.is_boolean())             v = j.get<bool>();
        else if (j.is_number_integer()) v = j.get<int64_t>();
        else if (j.is_string())         v = j.get<std::string>();
        else throw std::runtime_error("PropertyValue must be bool, integer, or string");
    }
};
}  // namespace nlohmann

namespace gm {

using json = nlohmann::json;

// ---- ids: plain JSON strings (the GM only ever sees ids as strings) --------
// Id<Tag> lives in gm, so these are found by ADL.
template <typename Tag> void to_json(json& j, const Id<Tag>& id) { j = id.value; }
template <typename Tag> void from_json(const json& j, Id<Tag>& id) { j.get_to(id.value); }

// ---- effects ---------------------------------------------------------------
inline void to_json(json& j, const Damage& e) {
    j = json{{"type","damage"}, {"source",e.source}, {"target",e.target}, {"via",e.via}, {"amount",e.amount}};
}
inline void from_json(const json& j, Damage& e) {
    j.at("source").get_to(e.source); j.at("target").get_to(e.target); j.at("via").get_to(e.via); j.at("amount").get_to(e.amount);
}

inline void to_json(json& j, const ModifyDisposition& e) {
    j = json{{"type","modify_disposition"}, {"entity",e.entity}, {"delta",e.delta}};
}
inline void from_json(const json& j, ModifyDisposition& e) {
    j.at("entity").get_to(e.entity); j.at("delta").get_to(e.delta);
}

inline void to_json(json& j, const TransferItem& e) {
    j = json{{"type","transfer_item"}, {"item",e.item}, {"from",e.from}, {"to",e.to}};
}
inline void from_json(const json& j, TransferItem& e) {
    j.at("item").get_to(e.item); j.at("from").get_to(e.from); j.at("to").get_to(e.to);
}

inline void to_json(json& j, const ModifyResource& e) {
    j = json{{"type","modify_resource"}, {"actor",e.actor},
             {"resource", e.resource == ModifyResource::Resource::Gold ? "gold" : "hp"},
             {"delta",e.delta}};
}
inline void from_json(const json& j, ModifyResource& e) {
    j.at("actor").get_to(e.actor);
    const auto r = j.at("resource").get<std::string>();
    if (r == "gold")    e.resource = ModifyResource::Resource::Gold;
    else if (r == "hp") e.resource = ModifyResource::Resource::Hp;
    else throw std::runtime_error("modify_resource: unknown resource '" + r + "'");
    j.at("delta").get_to(e.delta);
}

inline void to_json(json& j, const SetObjectState& e) {
    j = json::object();
    j["type"] = "set_object_state";
    j["object"] = e.object;
    j["property"] = e.property;
    j["value"] = e.value;  // via adl_serializer<PropertyValue>
}
inline void from_json(const json& j, SetObjectState& e) {
    j.at("object").get_to(e.object); j.at("property").get_to(e.property); j.at("value").get_to(e.value);
}

inline void to_json(json& j, const SetFlag& e) {
    j = json::object();
    j["type"] = "set_flag";
    j["key"] = e.key;
    j["value"] = e.value;
}
inline void from_json(const json& j, SetFlag& e) {
    j.at("key").get_to(e.key); j.at("value").get_to(e.value);
}

// ---- the Effect variant: dispatch on "type" (closed vocabulary gate) -------
// Effect = std::variant<gm::Damage, …>; gm IS associated via the args, so ADL
// finds these.
inline void to_json(json& j, const Effect& e) { std::visit([&](auto&& x) { to_json(j, x); }, e); }
inline void from_json(const json& j, Effect& e) {
    const auto t = j.at("type").get<std::string>();
    if      (t == "damage")             e = j.get<Damage>();
    else if (t == "modify_disposition") e = j.get<ModifyDisposition>();
    else if (t == "transfer_item")      e = j.get<TransferItem>();
    else if (t == "modify_resource")    e = j.get<ModifyResource>();
    else if (t == "set_object_state")   e = j.get<SetObjectState>();
    else if (t == "set_flag")           e = j.get<SetFlag>();
    else throw std::runtime_error("unknown effect type '" + t + "' (not in the closed v0 vocabulary)");
}

// ---- ruling, modifier, work order ------------------------------------------
inline void to_json(json& j, const Ruling& r) {
    switch (r) {
        case Ruling::Allow: j = "allow"; break;
        case Ruling::Partial: j = "partial"; break;
        case Ruling::Deny: j = "deny"; break;
    }
}
inline void from_json(const json& j, Ruling& r) {
    const auto s = j.get<std::string>();
    if      (s == "allow")   r = Ruling::Allow;
    else if (s == "partial") r = Ruling::Partial;
    else if (s == "deny")    r = Ruling::Deny;
    else throw std::runtime_error("unknown ruling '" + s + "'");
}

inline void to_json(json& j, const Modifier& m) { j = json{{"label",m.label}, {"value",m.value}}; }
inline void from_json(const json& j, Modifier& m) { j.at("label").get_to(m.label); j.at("value").get_to(m.value); }

inline void to_json(json& j, const WorkOrder& w) {
    j = json::object();
    j["interpretation"]       = w.interpretation;
    j["ruling"]               = w.ruling;
    j["modifiers"]            = w.modifiers;
    j["effects_on_success"]   = w.effects_on_success;
    j["narration_on_success"] = w.narration_on_success;
    j["effects_on_failure"]   = w.effects_on_failure;
    j["narration_on_failure"] = w.narration_on_failure;
}
inline void from_json(const json& j, WorkOrder& w) {
    j.at("interpretation").get_to(w.interpretation);
    j.at("ruling").get_to(w.ruling);
    // Tolerant of omissions so the human types less and the model has fewer required fields:
    w.modifiers            = j.value("modifiers", std::vector<Modifier>{});
    w.effects_on_success   = j.value("effects_on_success", std::vector<Effect>{});
    w.narration_on_success = j.value("narration_on_success", std::string{});
    w.effects_on_failure   = j.value("effects_on_failure", std::vector<Effect>{});
    w.narration_on_failure = j.value("narration_on_failure", std::string{});
}

// ---- state -----------------------------------------------------------------
// disposition serializes as a readable label (state dumps are shown to the GM);
// from_json accepts either the label or a raw ordinal int.
inline std::string disposition_label(int32_t d) {
    switch (d) {
        case disposition::Hostile:    return "hostile";
        case disposition::Unfriendly: return "unfriendly";
        case disposition::Neutral:    return "neutral";
        case disposition::Friendly:   return "friendly";
        case disposition::Allied:     return "allied";
        default:                      return std::to_string(d);
    }
}
inline int32_t disposition_from(const json& j) {
    if (j.is_number_integer()) return j.get<int32_t>();
    const auto s = j.get<std::string>();
    if (s == "hostile")    return disposition::Hostile;
    if (s == "unfriendly") return disposition::Unfriendly;
    if (s == "neutral")    return disposition::Neutral;
    if (s == "friendly")   return disposition::Friendly;
    if (s == "allied")     return disposition::Allied;
    throw std::runtime_error("unknown disposition '" + s + "'");
}

// vital serializes as a readable label, same posture as disposition above.
inline std::string vital_state_label(VitalState v) {
    switch (v) {
        case VitalState::Alive:       return "alive";
        case VitalState::Unconscious: return "unconscious";
        case VitalState::Dead:        return "dead";
    }
    return "unknown";
}
inline VitalState vital_state_from(const json& j) {
    const auto s = j.get<std::string>();
    if (s == "alive")       return VitalState::Alive;
    if (s == "unconscious") return VitalState::Unconscious;
    if (s == "dead")        return VitalState::Dead;
    throw std::runtime_error("unknown vital state '" + s + "'");
}

inline void to_json(json& j, const Exit& e) { j = json{{"object", e.object}}; }
inline void from_json(const json& j, Exit& e) { j.at("object").get_to(e.object); }

inline void to_json(json& j, const ObjectState& o) {
    j = json::object();
    j["id"] = o.id;
    j["properties"] = o.properties;  // map<string, PropertyValue>
}
inline void from_json(const json& j, ObjectState& o) {
    j.at("id").get_to(o.id);
    o.properties = j.value("properties", std::map<std::string, PropertyValue>{});
}

inline void to_json(json& j, const Entity& e) {
    j = json::object();
    j["id"] = e.id;
    j["hp"] = e.hp;
    j["max_hp"] = e.max_hp;
    j["vital"] = vital_state_label(e.vital);
    j["disposition"] = disposition_label(e.disposition);
    j["holds"] = e.holds;
    j["persona"] = e.persona;
}
inline void from_json(const json& j, Entity& e) {
    j.at("id").get_to(e.id); j.at("hp").get_to(e.hp);
    // Tolerant of omission, same reasoning as World::flags below: fixtures
    // predating this field shouldn't need editing just to keep parsing.
    e.max_hp = j.value("max_hp", e.hp);
    e.vital = j.contains("vital") ? vital_state_from(j.at("vital")) : VitalState::Alive;
    e.disposition = disposition_from(j.at("disposition"));
    e.holds = j.value("holds", std::vector<ItemId>{});
    e.persona = j.value("persona", std::string{});
}

inline void to_json(json& j, const Player& p) {
    j = json::object();
    j["id"] = p.id; j["hp"] = p.hp; j["max_hp"] = p.max_hp;
    j["vital"] = vital_state_label(p.vital);
    j["gold"] = p.gold; j["inventory"] = p.inventory;
}
inline void from_json(const json& j, Player& p) {
    j.at("id").get_to(p.id); j.at("hp").get_to(p.hp); j.at("gold").get_to(p.gold);
    p.max_hp = j.value("max_hp", p.hp);
    p.vital = j.contains("vital") ? vital_state_from(j.at("vital")) : VitalState::Alive;
    p.inventory = j.value("inventory", std::vector<ItemId>{});
}

inline void to_json(json& j, const Room& r) {
    j = json::object();
    j["id"] = r.id; j["exits"] = r.exits;
}
inline void from_json(const json& j, Room& r) {
    j.at("id").get_to(r.id);
    r.exits = j.value("exits", std::map<std::string, Exit>{});
}

inline void to_json(json& j, const World& w) {
    j = json::object();
    j["room"] = w.room; j["objects"] = w.objects; j["entities"] = w.entities; j["player"] = w.player;
    j["flags"] = w.flags;
}
inline void from_json(const json& j, World& w) {
    j.at("room").get_to(w.room);
    w.objects  = j.value("objects", std::vector<ObjectState>{});
    w.entities = j.value("entities", std::vector<Entity>{});
    j.at("player").get_to(w.player);
    // Tolerant of omission, same reasoning as WorkOrder's optional fields:
    // existing room fixtures (room_guardroom.json) predate this field and
    // shouldn't need editing just to keep parsing.
    w.flags = j.value("flags", std::map<std::string, PropertyValue>{});
}

// ---- convenience -----------------------------------------------------------
inline Expected<WorkOrder, std::string> parse_work_order(const std::string& s) {
    try {
        return Expected<WorkOrder, std::string>::ok(json::parse(s).get<WorkOrder>());
    } catch (const std::exception& ex) {
        return Expected<WorkOrder, std::string>::err(ex.what());
    }
}
inline Expected<World, std::string> parse_world(const std::string& s) {
    try {
        return Expected<World, std::string>::ok(json::parse(s).get<World>());
    } catch (const std::exception& ex) {
        return Expected<World, std::string>::err(ex.what());
    }
}
inline std::string dump(const WorkOrder& w, int indent = 2) { return json(w).dump(indent); }
inline std::string dump(const World& w, int indent = 2)     { return json(w).dump(indent); }

}  // namespace gm
