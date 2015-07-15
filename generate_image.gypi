{
  'variables': {
    'out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(image_out_dir)',
    'generator': 'sample_images/generate_image.sh',
  },
  'rules': [
    {
      'rule_name': 'generate_image',
      'extension': 'txt',
      'inputs': [
        '<(generator)',
        '<(RULE_INPUT_PATH)',
      ],
      'outputs': [
        '<(out_dir)/<(RULE_INPUT_ROOT).img',
      ],
      'action': [
        '<(generator)',
        '<(RULE_INPUT_PATH)',
        '<(out_dir)',
      ],
      'msvs_cygwin_shell': 0,
      'message': 'Generating image from <(RULE_INPUT_PATH)',
      'process_outputs_as_sources': 1,
    },
  ],
}
