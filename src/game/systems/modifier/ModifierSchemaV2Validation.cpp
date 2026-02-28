#include "game/systems/modifier/ModifierSchemaV2Validation.hpp"

#include <nlohmann/json.hpp>

#include <stdexcept>

namespace NoMoreDay {

namespace {

using json = nlohmann::json;

[[noreturn]] void ThrowSchemaError(const std::string &message) {
  throw std::runtime_error("modifier schema invalid: " + message);
}

void RequireObject(const json &value, const std::string &path) {
  if (!value.is_object()) {
    ThrowSchemaError(path + " must be an object");
  }
}

void RequireArray(const json &value, const std::string &path) {
  if (!value.is_array()) {
    ThrowSchemaError(path + " must be an array");
  }
}

void RequireString(const json &value, const std::string &path) {
  if (!value.is_string()) {
    ThrowSchemaError(path + " must be a string");
  }
}

void RequireInteger(const json &value, const std::string &path) {
  if (!value.is_number_integer()) {
    ThrowSchemaError(path + " must be an integer");
  }
}

void RequireNumber(const json &value, const std::string &path) {
  if (!value.is_number()) {
    ThrowSchemaError(path + " must be a number");
  }
}

const json &RequireKey(const json &object, const char *key,
                       const std::string &path) {
  if (!object.contains(key)) {
    ThrowSchemaError(path + " missing '" + key + "'");
  }
  return object.at(key);
}

void ValidateU32Array(const json &array, const std::string &path) {
  RequireArray(array, path);
  for (size_t i = 0; i < array.size(); ++i) {
    RequireInteger(array[i], path + "[" + std::to_string(i) + "]");
  }
}

void ValidateFilters(const json &filters, const std::string &path) {
  RequireObject(filters, path);

  RequireInteger(RequireKey(filters, "profession_mask", path),
                 path + ".profession_mask");
  ValidateU32Array(RequireKey(filters, "skill_id_whitelist", path),
                   path + ".skill_id_whitelist");
  RequireInteger(RequireKey(filters, "required_skill_tags_all", path),
                 path + ".required_skill_tags_all");
  RequireInteger(RequireKey(filters, "forbidden_skill_tags_any", path),
                 path + ".forbidden_skill_tags_any");
  RequireInteger(RequireKey(filters, "weapon_class_mask", path),
                 path + ".weapon_class_mask");
  RequireInteger(RequireKey(filters, "equip_slot_mask", path),
                 path + ".equip_slot_mask");
  ValidateU32Array(RequireKey(filters, "node_id_whitelist", path),
                   path + ".node_id_whitelist");
}

void ValidateConstraints(const json &constraints, const std::string &path) {
  RequireObject(constraints, path);
  RequireInteger(RequireKey(constraints, "exclusive_group", path),
                 path + ".exclusive_group");
  RequireInteger(RequireKey(constraints, "max_active", path),
                 path + ".max_active");
}

void ValidateOps(const json &ops, const std::string &path) {
  RequireArray(ops, path);
  for (size_t i = 0; i < ops.size(); ++i) {
    const std::string opPath = path + "[" + std::to_string(i) + "]";
    const auto &op = ops[i];
    RequireObject(op, opPath);
    RequireString(RequireKey(op, "opcode", opPath), opPath + ".opcode");
    RequireString(RequireKey(op, "target", opPath), opPath + ".target");
    RequireInteger(RequireKey(op, "param_u32", opPath), opPath + ".param_u32");
    RequireNumber(RequireKey(op, "param_f32", opPath), opPath + ".param_f32");
  }
}

void ValidateRecord(const json &record, const std::string &path,
                    const std::string &rootDomain) {
  RequireObject(record, path);
  RequireInteger(RequireKey(record, "id", path), path + ".id");
  const auto &domain = RequireKey(record, "domain", path);
  RequireString(domain, path + ".domain");
  if (domain.get<std::string>() != rootDomain) {
    ThrowSchemaError(path + ".domain must match top-level domain");
  }
  RequireInteger(RequireKey(record, "priority", path), path + ".priority");
  ValidateFilters(RequireKey(record, "filters", path), path + ".filters");
  ValidateConstraints(RequireKey(record, "constraints", path),
                      path + ".constraints");
  ValidateOps(RequireKey(record, "ops", path), path + ".ops");
}

} // namespace

void ValidateModifierSchemaV2Json(const std::string_view text) {
  const auto root = json::parse(text);
  RequireObject(root, "root");

  if (!root.contains("schema_version")) {
    throw std::runtime_error("modifier schema missing schema_version");
  }
  const auto &schemaVersion = root.at("schema_version");
  RequireInteger(schemaVersion, "root.schema_version");
  if (schemaVersion.get<int>() != 2) {
    ThrowSchemaError("schema_version must be 2");
  }

  const auto &domain = RequireKey(root, "domain", "root");
  RequireString(domain, "root.domain");
  const std::string rootDomain = domain.get<std::string>();

  const auto &records = RequireKey(root, "records", "root");
  RequireArray(records, "root.records");
  for (size_t i = 0; i < records.size(); ++i) {
    ValidateRecord(records[i], "root.records[" + std::to_string(i) + "]",
                   rootDomain);
  }
}

} // namespace NoMoreDay
