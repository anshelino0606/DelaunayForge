include_guard(GLOBAL)

function(fem_add_thread_pool_tests)
  add_executable(thread_pool_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/threading/thread_pool_tests.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/threading/thread_pool.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/threading/work_stealing_deque.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/threading/bounded_mpsc_queue.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/threading/spin_rw_lock.cc
  )

  fem_target_common_includes(thread_pool_tests)
  target_link_libraries(thread_pool_tests PRIVATE pthread)

  set(thread_pool_test_cases
    schedule_and_wait_idle
    submit_returns_values
    parallel_for_covers_all_ranges
    nested_scheduling
    spin_rw_lock_parallel_readers
    stress_many_external_producers
    stress_parallel_for_large_range
    stress_recursive_fan_out
  )

  foreach(test_case IN LISTS thread_pool_test_cases)
    add_test(NAME thread_pool_tests.${test_case} COMMAND thread_pool_tests ${test_case})
  endforeach()
endfunction()