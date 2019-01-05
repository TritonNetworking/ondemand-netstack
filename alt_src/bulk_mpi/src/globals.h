#ifndef BULKMPI_GLOBALS_H_
#define BULKMPI_GLOBALS_H_

#include "yaml-cpp/yaml.h"
#include "yaml_config.h"

#define DATA_COLOR 0xDA1AC0D3
#define SYNC_COLOR 0xBA3BC0D3

#define SYNC_PKT_SIZE 9

extern char* config_file;
extern uint64_t base_time;
extern struct yaml_config config;

extern YAML::Node bulk_config;

#endif