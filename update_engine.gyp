#
# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
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
      '-Wnon-virtual-dtor',
    ],
    'ldflags': [
      '-Wl,--gc-sections',
    ],
    'defines': [
      '_POSIX_C_SOURCE=199309L',
      'USE_HWID_OVERRIDE=<(USE_hwid_override)',
      'USE_MTD=<(USE_mtd)',
      'USE_POWER_MANAGEMENT=<(USE_power_management)',
    ],
    'include_dirs': [
      # We need this include dir because we include all the local code as
      # "update_engine/...".
      '<(platform2_root)/../aosp/system',
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
          'protobuf-lite',
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
      'includes': ['../../../platform2/common-mk/protoc.gypi'],
    },
    # Chrome D-Bus bindings.
    {
      'target_name': 'update_engine-dbus-adaptor',
      'type': 'none',
      'variables': {
        'dbus_adaptors_out_dir': 'include/update_engine/dbus_adaptor',
        'dbus_xml_extension': 'dbus-xml',
      },
      'sources': [
        'dbus_bindings/org.chromium.UpdateEngineInterface.dbus-xml',
      ],
      'includes': ['../../../platform2/common-mk/generate-dbus-adaptors.gypi'],
    },
    {
      'target_name': 'update_engine-other-dbus-proxies',
      'type': 'none',
      'actions': [
        {
          'action_name': 'update_engine-dbus-shill-client',
          'variables': {
            'mock_output_file': 'include/shill/dbus-proxy-mocks.h',
            'proxy_output_file': 'include/shill/dbus-proxies.h'
          },
          'sources': [
            '<(platform2_root)/shill/dbus_bindings/org.chromium.flimflam.Manager.xml',
            '<(platform2_root)/shill/dbus_bindings/org.chromium.flimflam.Service.xml',
          ],
          'includes': ['../../../platform2/common-mk/generate-dbus-proxies.gypi'],
        },
        {
          'action_name': 'update_engine-dbus-debugd-client',
          'variables': {
            'mock_output_file': 'include/debugd/dbus-proxy-mocks.h',
            'proxy_output_file': 'include/debugd/dbus-proxies.h'
          },
          'sources': [
            '<(platform2_root)/debugd/share/org.chromium.debugd.xml',
          ],
          'includes': ['../../../platform2/common-mk/generate-dbus-proxies.gypi'],
        },
        {
          'action_name': 'update_engine-dbus-login_manager-client',
          'variables': {
            'mock_output_file': 'include/login_manager/dbus-proxy-mocks.h',
            'proxy_output_file': 'include/login_manager/dbus-proxies.h'
          },
          'sources': [
            '<(platform2_root)/login_manager/org.chromium.SessionManagerInterface.xml',
          ],
          'includes': ['../../../platform2/common-mk/generate-dbus-proxies.gypi'],
        },
        {
          'action_name': 'update_engine-dbus-power_manager-client',
          'variables': {
            'mock_output_file': 'include/power_manager/dbus-proxy-mocks.h',
            'proxy_output_file': 'include/power_manager/dbus-proxies.h'
          },
          'sources': [
            '<(platform2_root)/power_manager/dbus_bindings/org.chromium.PowerManager.xml',
          ],
          'includes': ['../../../platform2/common-mk/generate-dbus-proxies.gypi'],
        },
        {
          'action_name': 'update_engine-dbus-libcros-client',
          'variables': {
            'mock_output_file': 'include/libcros/dbus-proxy-mocks.h',
            'proxy_output_file': 'include/libcros/dbus-proxies.h'
          },
          'sources': [
            'dbus_bindings/org.chromium.LibCrosService.xml',
          ],
          'includes': ['../../../platform2/common-mk/generate-dbus-proxies.gypi'],
        },
      ],
    },
    # The main static_library with all the code.
    {
      'target_name': 'libupdate_engine',
      'type': 'static_library',
      'dependencies': [
        'update_metadata-protos',
        'update_engine-dbus-adaptor',
        'update_engine-other-dbus-proxies',
      ],
      'variables': {
        'exported_deps': [
          'dbus-1',
          'libchrome-<(libbase_ver)',
          'libchromeos-<(libbase_ver)',
          'libcrypto',
          'libcurl',
          'libmetrics-<(libbase_ver)',
          'libssl',
          'expat'
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
          '-lpolicy-<(libbase_ver)',
          '-lrootdev',
          '-lrt',
          '-lvboot_host',
        ],
      },
      'sources': [
        'action_processor.cc',
        'boot_control_chromeos.cc',
        'bzip.cc',
        'bzip_extent_writer.cc',
        'certificate_checker.cc',
        'chrome_browser_proxy_resolver.cc',
        'clock.cc',
        'connection_manager.cc',
        'constants.cc',
        'daemon.cc',
        'dbus_service.cc',
        'delta_performer.cc',
        'download_action.cc',
        'extent_writer.cc',
        'file_descriptor.cc',
        'file_writer.cc',
        'filesystem_verifier_action.cc',
        'hardware.cc',
        'http_common.cc',
        'http_fetcher.cc',
        'hwid_override.cc',
        'install_plan.cc',
        'libcros_proxy.cc',
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
        'shill_proxy.cc',
        'subprocess.cc',
        'terminator.cc',
        'update_attempter.cc',
        'update_manager/boxed_value.cc',
        'update_manager/chromeos_policy.cc',
        'update_manager/default_policy.cc',
        'update_manager/evaluation_context.cc',
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
      'conditions': [
        ['USE_mtd == 1', {
          'sources': [
            'mtd_file_descriptor.cc',
          ],
          'link_settings': {
            'libraries': [
              '-lmtdutils',
            ],
          },
        }],
      ],
    },
    # update_engine daemon.
    {
      'target_name': 'update_engine',
      'type': 'executable',
      'dependencies': [
        'libupdate_engine',
      ],
      'sources': [
        'main.cc',
      ]
    },
    # update_engine console client.
    {
      'target_name': 'update_engine_client',
      'type': 'executable',
      'dependencies': [
        'libupdate_engine-client-headers',
      ],
      'variables': {
        'exported_deps': [
          'libchrome-<(libbase_ver)',
          'libchromeos-<(libbase_ver)',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'link_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        'update_engine_client.cc',
      ],
    },
    # update_engine client library generated headers. Used by other daemons and
    # by the update_engine_client console program to interact with
    # update_engine.
    {
      'target_name': 'libupdate_engine-client-headers',
      'type': 'none',
      'actions': [
        {
          'action_name': 'update_engine_client-dbus-proxies',
          'variables': {
            'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
            'proxy_output_file': 'include/update_engine/dbus-proxies.h',
            'mock_output_file': 'include/update_engine/dbus-proxy-mocks.h',
          },
          'sources': [
            'dbus_bindings/org.chromium.UpdateEngineInterface.dbus-xml',
          ],
          'includes': ['../../../platform2/common-mk/generate-dbus-proxies.gypi'],
        },
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
        'libraries': [
          '-lvboot_host',
        ],
      },
      'sources': [
        'payload_generator/ab_generator.cc',
        'payload_generator/annotated_operation.cc',
        'payload_generator/blob_file_writer.cc',
        'payload_generator/block_mapping.cc',
        'payload_generator/cycle_breaker.cc',
        'payload_generator/delta_diff_generator.cc',
        'payload_generator/delta_diff_utils.cc',
        'payload_generator/ext2_filesystem.cc',
        'payload_generator/extent_ranges.cc',
        'payload_generator/extent_utils.cc',
        'payload_generator/full_update_generator.cc',
        'payload_generator/graph_types.cc',
        'payload_generator/graph_utils.cc',
        'payload_generator/inplace_generator.cc',
        'payload_generator/payload_file.cc',
        'payload_generator/payload_generation_config.cc',
        'payload_generator/payload_signer.cc',
        'payload_generator/raw_filesystem.cc',
        'payload_generator/tarjan.cc',
        'payload_generator/topological_sort.cc',
        'payload_generator/verity_utils.cc',
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
          'includes': ['../../../platform2/common-mk/openssl_pem.gypi'],
        },
        # Unpacks sample images used for testing.
        {
          'target_name': 'update_engine-test_images',
          'type': 'none',
          'variables': {
            'image_out_dir': '.',
          },
          'sources': [
            'sample_images/sample_images.tar.bz2',
          ],
          'includes': ['tar_bunzip2.gypi'],
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
          'includes': ['../../../platform2/common-mk/common_test.gypi'],
          'variables': {
            'deps': [
              'libchromeos-test-<(libbase_ver)',
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'dependencies': [
            'libupdate_engine',
            'libpayload_generator',
          ],
          'includes': ['../../../platform2/common-mk/common_test.gypi'],
          'sources': [
            'action_pipe_unittest.cc',
            'action_processor_unittest.cc',
            'action_unittest.cc',
            'boot_control_chromeos_unittest.cc',
            'bzip_extent_writer_unittest.cc',
            'certificate_checker_unittest.cc',
            'chrome_browser_proxy_resolver_unittest.cc',
            'connection_manager_unittest.cc',
            'dbus_service_unittest.cc',
            'delta_performer_unittest.cc',
            'download_action_unittest.cc',
            'extent_writer_unittest.cc',
            'fake_prefs.cc',
            'fake_shill_proxy.cc',
            'fake_system_state.cc',
            'file_writer_unittest.cc',
            'filesystem_verifier_action_unittest.cc',
            'http_fetcher_unittest.cc',
            'hwid_override_unittest.cc',
            'mock_http_fetcher.cc',
            'omaha_hash_calculator_unittest.cc',
            'omaha_request_action_unittest.cc',
            'omaha_request_params_unittest.cc',
            'omaha_response_handler_action_unittest.cc',
            'p2p_manager_unittest.cc',
            'payload_generator/ab_generator_unittest.cc',
            'payload_generator/blob_file_writer_unittest.cc',
            'payload_generator/block_mapping_unittest.cc',
            'payload_generator/cycle_breaker_unittest.cc',
            'payload_generator/delta_diff_utils_unittest.cc',
            'payload_generator/ext2_filesystem_unittest.cc',
            'payload_generator/extent_ranges_unittest.cc',
            'payload_generator/extent_utils_unittest.cc',
            'payload_generator/fake_filesystem.cc',
            'payload_generator/full_update_generator_unittest.cc',
            'payload_generator/graph_utils_unittest.cc',
            'payload_generator/inplace_generator_unittest.cc',
            'payload_generator/payload_signer_unittest.cc',
            'payload_generator/payload_file_unittest.cc',
            'payload_generator/tarjan_unittest.cc',
            'payload_generator/topological_sort_unittest.cc',
            'payload_generator/verity_utils_unittest.cc',
            'payload_state_unittest.cc',
            'postinstall_runner_action_unittest.cc',
            'prefs_unittest.cc',
            'subprocess_unittest.cc',
            'terminator_unittest.cc',
            'test_utils.cc',
            'test_utils_unittest.cc',
            'update_attempter_unittest.cc',
            'update_manager/boxed_value_unittest.cc',
            'update_manager/chromeos_policy_unittest.cc',
            'update_manager/evaluation_context_unittest.cc',
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
