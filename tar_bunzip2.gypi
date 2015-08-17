{
  'variables': {
    'out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(image_out_dir)',
  },
  'rules': [
    {
      'rule_name': 'tar-bunzip2',
      'extension': 'bz2',
      'inputs': [
        '<(RULE_INPUT_PATH)',
      ],
      'outputs': [
        # The .flag file is used to mark the timestamp of the file extraction
        # and re-run this action if a new .bz2 file is generated.
        '<(out_dir)/<(RULE_INPUT_ROOT).flag',
      ],
      'action': [
        'sh',
        '-c',
        'tar -xvf "<(RULE_INPUT_PATH)" -C "<(out_dir)" && touch <(out_dir)/<(RULE_INPUT_ROOT).flag',
      ],
      'msvs_cygwin_shell': 0,
      'process_outputs_as_sources': 1,
      'message': 'Unpacking file <(RULE_INPUT_PATH)',
    },
  ],
}
