{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
      ],
      # Defaults if the -DUSE_* flags are not passed to gyp is 0. You can set
      # the default value for the USE flag in the ebuild.
      'USE_hwid_override%': '0',
      'USE_power_management%': '0',
    },
    'cflags': [
      '-g',
      '-ffunction-sections',
      '-Wall',
      '-Wextra',
      '-Werror',
      '-Wno-unused-parameter',
      '-Wno-deprecated-register',
    ],
    'cflags_cc': [
      '-fno-strict-aliasing',
      '-std=gnu++11',
    ],
    'ldflags': [
      '-Wl,--gc-sections',
    ],
    'defines': [
      '__STDC_FORMAT_MACROS=1',
      '_FILE_OFFSET_BITS=64',
      '_POSIX_C_SOURCE=199309L',
      'USE_HWID_OVERRIDE=<(USE_hwid_override)',
      'USE_POWER_MANAGEMENT=<(USE_power_management)',
    ],
  },
  'targets': [
    # Protobufs.
    {
      'target_name': 'update_metadata-protos',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': '.',
        'proto_out_dir': 'include/update_engine',
        'exported_deps': [
          'protobuf',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        'update_metadata.proto'
      ],
      'includes': ['../common-mk/protoc.gypi'],
    },
    # D-Bus glib bindings.
    {
      'target_name': 'update_engine-dbus-client',
      'type': 'none',
      'variables': {
        'dbus_glib_type': 'client',
        'dbus_glib_out_dir': 'include/update_engine',
        'dbus_glib_prefix': 'update_engine_service',
      },
      'sources': [
        'update_engine.xml',
      ],
      'includes': ['../common-mk/dbus_glib.gypi'],
    },
    {
      'target_name': 'update_engine-dbus-server',
      'type': 'none',
      'variables': {
        'dbus_glib_type': 'server',
        'dbus_glib_out_dir': 'include/update_engine',
        'dbus_glib_prefix': 'update_engine_service',
      },
      'sources': [
        'update_engine.xml',
      ],
      'includes': ['../common-mk/dbus_glib.gypi'],
    },
    # The main static_library with all the code.
    {
      'target_name': 'libupdate_engine',
      'type': 'static_library',
      'dependencies': [
        'update_metadata-protos',
        'update_engine-dbus-client',
        'update_engine-dbus-server',
      ],
      'variables': {
        'exported_deps': [
          'dbus-1',
          'dbus-glib-1',
          'gio-2.0',
          'gio-unix-2.0',
          'glib-2.0',
          'gthread-2.0',
          'libchrome-<(libbase_ver)',
          'libchromeos-<(libbase_ver)',
          'libcrypto',
          'libcurl',
          'libmetrics-<(libbase_ver)',
          'libssl',
          'libxml-2.0',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'link_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
        'libraries': [
          '-lbz2',
          '-lgflags',
          '-lpolicy-<(libbase_ver)',
          '-lrootdev',
          '-lrt',
          '-lvboot_host',
        ],
      },
      'sources': [
        'action_processor.cc',
        'bzip.cc',
        'bzip_extent_writer.cc',
        'certificate_checker.cc',
        'chrome_browser_proxy_resolver.cc',
        'clock.cc',
        'connection_manager.cc',
        'constants.cc',
        'dbus_service.cc',
        'delta_performer.cc',
        'download_action.cc',
        'extent_ranges.cc',
        'extent_writer.cc',
        'file_descriptor.cc',
        'file_writer.cc',
        'filesystem_copier_action.cc',
        'hardware.cc',
        'http_common.cc',
        'http_fetcher.cc',
        'hwid_override.cc',
        'install_plan.cc',
        'libcurl_http_fetcher.cc',
        'metrics.cc',
        'multi_range_http_fetcher.cc',
        'omaha_hash_calculator.cc',
        'omaha_request_action.cc',
        'omaha_request_params.cc',
        'omaha_response_handler_action.cc',
        'p2p_manager.cc',
        'payload_constants.cc',
        'payload_state.cc',
        'payload_verifier.cc',
        'postinstall_runner_action.cc',
        'prefs.cc',
        'proxy_resolver.cc',
        'real_system_state.cc',
        'simple_key_value_store.cc',
        'subprocess.cc',
        'terminator.cc',
        'update_attempter.cc',
        'update_check_scheduler.cc',
        'update_manager/boxed_value.cc',
        'update_manager/chromeos_policy.cc',
        'update_manager/default_policy.cc',
        'update_manager/evaluation_context.cc',
        'update_manager/event_loop.cc',
        'update_manager/policy.cc',
        'update_manager/real_config_provider.cc',
        'update_manager/real_device_policy_provider.cc',
        'update_manager/real_random_provider.cc',
        'update_manager/real_shill_provider.cc',
        'update_manager/real_system_provider.cc',
        'update_manager/real_time_provider.cc',
        'update_manager/real_updater_provider.cc',
        'update_manager/state_factory.cc',
        'update_manager/update_manager.cc',
        'utils.cc',
      ],
    },
    # update_engine daemon.
    {
      'target_name': 'update_engine',
      'type': 'executable',
      'dependencies': ['libupdate_engine'],
      'sources': [
        'main.cc',
      ]
    },
    # update_engine console client.
    {
      'target_name': 'update_engine_client',
      'type': 'executable',
      'dependencies': ['libupdate_engine'],
      'sources': [
        'update_engine_client.cc',
      ]
    },
    # server-side code. This is used for delta_generator and unittests but not
    # for any client code.
    {
      'target_name': 'libpayload_generator',
      'type': 'static_library',
      'dependencies': [
        'update_metadata-protos',
      ],
      'variables': {
        'exported_deps': [
          'ext2fs',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'link_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        'payload_generator/cycle_breaker.cc',
        'payload_generator/delta_diff_generator.cc',
        'payload_generator/extent_mapper.cc',
        'payload_generator/filesystem_iterator.cc',
        'payload_generator/full_update_generator.cc',
        'payload_generator/graph_utils.cc',
        'payload_generator/metadata.cc',
        'payload_generator/payload_signer.cc',
        'payload_generator/tarjan.cc',
        'payload_generator/topological_sort.cc',
      ],
    },
    # server-side delta generator.
    {
      'target_name': 'delta_generator',
      'type': 'executable',
      'dependencies': [
        'libupdate_engine',
        'libpayload_generator',
      ],
      'link_settings': {
        'ldflags!': [
          '-pie',
        ],
      },
      'sources': [
        'payload_generator/generate_delta_main.cc',
      ]
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        # Public keys used for unit testing.
        {
          'target_name': 'update_engine-testkeys',
          'type': 'none',
          'variables': {
            'openssl_pem_in_dir': '.',
            'openssl_pem_out_dir': 'include/update_engine',
          },
          'sources': [
            'unittest_key.pem',
            'unittest_key2.pem',
          ],
          'includes': ['../common-mk/openssl_pem.gypi'],
        },
        # Test HTTP Server.
        {
          'target_name': 'test_http_server',
          'type': 'executable',
          'dependencies': ['libupdate_engine'],
          'sources': [
            'test_http_server.cc',
          ]
        },
        # Main unittest file.
        {
          'target_name': 'update_engine_unittests',
          'type': 'executable',
          'dependencies': [
            'libupdate_engine',
            'libpayload_generator',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'defines': [
            'SYSROOT="<(sysroot)"',
          ],
          'sources': [
            'action_pipe_unittest.cc',
            'action_processor_unittest.cc',
            'action_unittest.cc',
            'bzip_extent_writer_unittest.cc',
            'certificate_checker_unittest.cc',
            'chrome_browser_proxy_resolver_unittest.cc',
            'connection_manager_unittest.cc',
            'delta_performer_unittest.cc',
            'download_action_unittest.cc',
            'extent_ranges_unittest.cc',
            'extent_writer_unittest.cc',
            'fake_prefs.cc',
            'fake_system_state.cc',
            'file_writer_unittest.cc',
            'filesystem_copier_action_unittest.cc',
            'http_fetcher_unittest.cc',
            'hwid_override_unittest.cc',
            'mock_http_fetcher.cc',
            'omaha_hash_calculator_unittest.cc',
            'omaha_request_action_unittest.cc',
            'omaha_request_params_unittest.cc',
            'omaha_response_handler_action_unittest.cc',
            'p2p_manager_unittest.cc',
            'payload_generator/cycle_breaker_unittest.cc',
            'payload_generator/delta_diff_generator_unittest.cc',
            'payload_generator/extent_mapper_unittest.cc',
            'payload_generator/filesystem_iterator_unittest.cc',
            'payload_generator/full_update_generator_unittest.cc',
            'payload_generator/graph_utils_unittest.cc',
            'payload_generator/metadata_unittest.cc',
            'payload_generator/payload_signer_unittest.cc',
            'payload_generator/tarjan_unittest.cc',
            'payload_generator/topological_sort_unittest.cc',
            'payload_state_unittest.cc',
            'postinstall_runner_action_unittest.cc',
            'prefs_unittest.cc',
            'simple_key_value_store_unittest.cc',
            'subprocess_unittest.cc',
            'terminator_unittest.cc',
            'test_utils.cc',
            'update_attempter_unittest.cc',
            'update_check_scheduler_unittest.cc',
            'update_manager/boxed_value_unittest.cc',
            'update_manager/chromeos_policy_unittest.cc',
            'update_manager/evaluation_context_unittest.cc',
            'update_manager/event_loop_unittest.cc',
            'update_manager/generic_variables_unittest.cc',
            'update_manager/prng_unittest.cc',
            'update_manager/real_config_provider_unittest.cc',
            'update_manager/real_device_policy_provider_unittest.cc',
            'update_manager/real_random_provider_unittest.cc',
            'update_manager/real_shill_provider_unittest.cc',
            'update_manager/real_system_provider_unittest.cc',
            'update_manager/real_time_provider_unittest.cc',
            'update_manager/real_updater_provider_unittest.cc',
            'update_manager/umtest_utils.cc',
            'update_manager/update_manager_unittest.cc',
            'update_manager/variable_unittest.cc',
            'utils_unittest.cc',
            'zip_unittest.cc',
            # Main entry point for runnning tests.
            'testrunner.cc',
          ],
        },
      ],
    }],
  ],
}
