#pragma once
#include "habit_model.hpp"
namespace el_sistema {
bool habit_json_encode(const HabitDocument &document, std::string &json, std::string &error);
bool habit_json_decode(const std::string &json, HabitDocument &document, std::string &error);
// Uses the already-mounted SPIFFS; never initializes NVS or formats storage.
class HabitRepository {
  public:
    explicit HabitRepository(std::string directory) : directory_(std::move(directory)) {}
    bool load(HabitDocument &document, std::string &message);
    bool save(const HabitDocument &document, std::string &error);
    bool warning() const;

  private:
    bool read(const std::string &path, HabitDocument &document, bool &missing, std::string &error);
    std::string directory_;
    bool primary_valid_ = false;
};
} // namespace el_sistema
