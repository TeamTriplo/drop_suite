<?php
/**
 * @file
 * Views field handler for the project name with optional link.
 */

/**
 * Displays the project name, optionally linked to the project page.
 */
class drop_base_handler_field_project_name extends views_handler_field {

  /**
   * {@inheritdoc}
   */
  function option_definition() {
    $options = parent::option_definition();
    $options['link_to_project'] = array('default' => TRUE, 'bool' => TRUE);
    return $options;
  }

  /**
   * {@inheritdoc}
   */
  function options_form(&$form, &$form_state) {
    $form['link_to_project'] = array(
      '#type' => 'checkbox',
      '#title' => t('Link to project'),
      '#default_value' => $this->options['link_to_project'],
    );
    parent::options_form($form, $form_state);
  }

  /**
   * {@inheritdoc}
   */
  function render($values) {
    $value = $this->get_value($values);
    $text = $this->sanitize_value($value);
    if ($this->options['link_to_project'] && !empty($values->{$this->aliases['pid']})) {
      $pid = $values->{$this->aliases['pid']};
      return l($text, 'drop/projects/' . $pid);
    }
    return $text;
  }

  /**
   * {@inheritdoc}
   */
  function query() {
    $this->ensure_my_table();
    $this->add_additional_fields(array('pid'));
    parent::query();
  }
}
