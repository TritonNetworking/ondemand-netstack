// Reads config from yaml file

#include <map>
#include <string>
#include <vector>
#include "yaml-cpp/yaml.h"

#include "utils.h"

struct yaml_config {
    uint64_t total_period_ns;
    uint64_t guard_time_ns;
    uint64_t recovery_delay_ns;
    uint64_t prealloc_delay_ns;
    int warmup_iters;

    uint dummy_magic;
    uint endhost_magic;
    uint done_magic;

    uint link_rate_gbps;
    int num_hosts;
    int num_rotors;

    std::map<int, std::vector<int>> id_to_rank;
    std::map<int, int> rank_to_id;
    std::map<int, int> rank_to_rotor;
    int control_rank;
    int dummy_rank;

    std::vector<uint> timeslot_order;
    std::map<int, std::map<std::string, int>> timeslots;
    std::map<int, std::map<int, std::vector<int>>> mappings;

    uint64_t exp_duration_ms;
    uint64_t min_exp_delay_ns;

    bool send_bulk_data;
};

struct yaml_config load_config(char *config_file) {
    struct yaml_config config;
    YAML::Node bulk_config = YAML::LoadFile(config_file);

    config.total_period_ns = load_or_abort(bulk_config, "total_period_ns").as<uint64_t>();
    config.guard_time_ns = load_or_abort(bulk_config, "guard_time_ns").as<uint64_t>();
    config.recovery_delay_ns = load_or_abort(bulk_config, "recovery_delay_ns").as<uint64_t>();
    config.prealloc_delay_ns = load_or_abort(bulk_config, "prealloc_delay_ns").as<uint64_t>();
    config.warmup_iters = load_or_abort(bulk_config, "warmup_iters").as<int>();

    config.dummy_magic = load_or_abort(bulk_config, "dummy_magic").as<uint>();
    config.endhost_magic = load_or_abort(bulk_config, "endhost_magic").as<uint>();
    config.done_magic = load_or_abort(bulk_config, "done_magic").as<uint>();

    config.link_rate_gbps = load_or_abort(bulk_config, "link_rate_gbps").as<uint>();
    config.num_hosts = load_or_abort(bulk_config, "num_hosts").as<int>();
    config.num_rotors = load_or_abort(bulk_config, "num_rotors").as<int>();

    YAML::Node id_to_rank = load_or_abort(bulk_config, "id_to_rank");
    for(int i = 0; i < config.num_hosts; i++) {
        std::vector<int> ranks = id_to_rank[i].as<std::vector<int>>();
        config.id_to_rank.insert({i, ranks});
        for (auto &rank : ranks)
            config.rank_to_id.insert({rank, i});
    }

    YAML::Node rank_to_rotor = load_or_abort(bulk_config, "rank_to_rotor");
    for (auto &pair : config.rank_to_id) {
        int rank = pair.first;
        int rotor = rank_to_rotor[rank].as<int>();
        config.rank_to_rotor.insert({rank, rotor});
    }

    config.control_rank = load_or_abort(id_to_rank, "control").as<int>();
    config.dummy_rank = load_or_abort(id_to_rank, "dummy").as<int>();

    config.timeslot_order = load_or_abort(bulk_config, "timeslot_order").as<std::vector<uint>>();
    config.timeslots = load_or_abort(bulk_config, "timeslots").as<std::map<int, std::map<std::string, int>>>();
    config.mappings = load_or_abort(bulk_config, "mappings").as<std::map<int, std::map<int, std::vector<int>>>>();

    config.exp_duration_ms = load_or_abort(bulk_config, "exp_duration_ms").as<uint64_t>() * 1000000ul;
    config.min_exp_delay_ns = load_or_abort(bulk_config, "min_exp_delay_ns").as<uint64_t>();
}
