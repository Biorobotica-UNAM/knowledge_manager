#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct RobotDescription {
  std::string name{};
  std::string room{};
  std::string left_hand{};
  std::string right_hand{};
  std::string safe_drop_mode{};
  std::string safe_drop_frame{};
  std::vector<float> safe_drop_pose{};
};

struct ObjectDescription {
  short int number{};
  std::string name{};
  std::vector<std::string> locations{};
  std::vector<float> navigation{};
  std::vector<float> manipulation{};
};

struct PersonDescription {
  short int number{};
  std::string name{"unknown"};
  std::vector<std::string> description{};
  std::string drink{"unknown"};
  std::string room{"unknown"};
  std::vector<std::string> accessories{};
  std::vector<std::string> order{};
  std::vector<float> navigation{};
};

struct AnalyticsCount {
  std::string target_class{};
  int32_t class_count{0};
  int32_t frame_count{0};
  int32_t cumulative{0};
};

struct AnalyticsSize {
  std::string target_class{};
  std::string filter_mode{};
  std::string best_match{};
  float confidence{0.0f};
};

struct AnalyticsColor {
  std::string target_class{};
  std::string color{};
  std::string best_match{};
};

struct AnalyticsCompare {
  std::string classes{};
  std::string winner{};
};

struct AnalyticsState {
  AnalyticsCount count{};
  AnalyticsSize size{};
  AnalyticsColor color{};
  AnalyticsCompare compare{};
};

struct WorldState {
  RobotDescription robot{};
  std::vector<std::string> known_locations{};
  std::unordered_map<std::string, std::vector<ObjectDescription>> objects;
  std::unordered_map<std::string, std::vector<PersonDescription>> people;
  AnalyticsState analytics{};
};
