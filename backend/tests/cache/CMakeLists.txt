add_executable(yarda_hierarchy_analysis_tests
    streaming_hierarchy_test.cpp
    streaming_hierarchy_differential_test.cpp
    streaming_hierarchy_events_test.cpp
    streaming_hierarchy_budget_test.cpp
    streaming_hierarchy_work_limits_test.cpp
    streaming_hierarchy_default_limits_test.cpp
    streaming_hierarchy_failure_test.cpp
    streaming_hierarchy_summary_test.cpp
    streaming_hierarchy_task_test.cpp
    exact_csrd_test.cpp
    exact_csrd_differential_test.cpp
    recency_index_test.cpp
    exact_csrd_oracle.cpp
    exact_csrd_oracle_test.cpp
    hierarchy_lru_oracle.cpp
    hierarchy_lru_oracle_contract_test.cpp
    hierarchy_lru_oracle_differential_support.cpp
    hierarchy_lru_oracle_differential_support_test.cpp
    hierarchy_lru_oracle_differential_test.cpp
    hierarchy_lru_oracle_operation_test.cpp
    hierarchy_lru_oracle_test.cpp
    hierarchy_model_test.cpp
    hierarchy_model_validation_test.cpp
    hierarchy_analysis_validation_test.cpp
    hierarchy_analysis_l1_test.cpp
    hierarchy_analysis_llc_test.cpp
    hierarchy_analysis_differential_test.cpp
    hierarchy_level_analysis_test.cpp
    hierarchy_aggregation_test.cpp
    hierarchy_aggregation_validation_test.cpp
    hierarchy_service_summary_test.cpp
)
target_include_directories(yarda_hierarchy_analysis_tests PRIVATE "${YARDA_BACKEND_SOURCE_DIR}/src/cache")
target_link_libraries(yarda_hierarchy_analysis_tests PRIVATE
    yarda_hierarchy_analysis GTest::gtest_main
)
target_compile_definitions(yarda_hierarchy_analysis_tests PRIVATE
    YARDA_CACHE_CONFIG_32B_PATH="${YARDA_BACKEND_SOURCE_DIR}/config/cache.32b.yaml"
    YARDA_CACHE_CONFIG_EXAMPLE_PATH="${YARDA_BACKEND_SOURCE_DIR}/config/cache.example.yaml"
)
gtest_discover_tests(yarda_hierarchy_analysis_tests
    WORKING_DIRECTORY "${YARDA_BACKEND_BINARY_DIR}"
)

add_executable(yarda_cache_config_tests
    cache_config_test.cpp
    yaml_config_parser_test.cpp
)
target_link_libraries(yarda_cache_config_tests PRIVATE
    yarda_cache_config GTest::gtest_main
)
target_compile_definitions(yarda_cache_config_tests PRIVATE
    YARDA_CACHE_CONFIG_EXAMPLE_PATH="${YARDA_BACKEND_SOURCE_DIR}/config/cache.example.yaml"
)
gtest_discover_tests(yarda_cache_config_tests
    WORKING_DIRECTORY "${YARDA_BACKEND_BINARY_DIR}"
)
