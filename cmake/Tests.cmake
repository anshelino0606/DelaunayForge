include_guard(GLOBAL)

function(fem_add_thread_pool_tests)
  find_package(Threads REQUIRED)

  add_executable(thread_pool_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/threading/thread_pool_tests.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/threading/thread_pool.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/threading/task_pool_allocator.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/threading/work_stealing_deque.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/threading/mpsc_queue.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/threading/spin_rw_lock.cc
  )

  fem_target_common_includes(thread_pool_tests)
  target_link_libraries(thread_pool_tests PRIVATE Threads::Threads)

  set(thread_pool_test_cases
    schedule_and_wait_idle
    submit_returns_values
    submit_result_preserves_status_after_take
    schedule_reports_shutdown_failure
    submit_reports_shutdown_failure
    parallel_for_covers_all_ranges
    parallel_for_move_only_body
    parallel_for_keeps_pool_reusable
    nested_scheduling
    spin_rw_lock_parallel_readers
    small_pool_processes_detached_tasks
    large_callable_fallback_allocation
    stress_many_external_producers
    stress_parallel_for_large_range
    stress_recursive_fan_out
  )

  foreach(test_case IN LISTS thread_pool_test_cases)
    add_test(NAME thread_pool_tests.${test_case} COMMAND thread_pool_tests ${test_case})
  endforeach()

  add_executable(thread_pool_benchmarks
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/threading/thread_pool_benchmarks.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/threading/thread_pool.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/threading/task_pool_allocator.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/threading/work_stealing_deque.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/threading/mpsc_queue.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/threading/spin_rw_lock.cc
  )

  fem_target_common_includes(thread_pool_benchmarks)
  target_link_libraries(thread_pool_benchmarks PRIVATE Threads::Threads)

  add_test(
    NAME thread_pool_perf_smoke
    COMMAND thread_pool_benchmarks --smoke
  )
endfunction()
