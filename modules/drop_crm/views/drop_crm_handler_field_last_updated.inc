<?php
/**
 * @file
 * Contains the CRM "last updated" field handler.
 */

/**
 * Shows how long ago a contact was saved, flagging stale contacts.
 *
 * A contact is stale when it has not been saved within the Clients report
 * threshold (drop_crm.settings:client_stale_days). The exact date shows on
 * hover.
 *
 * @ingroup views_field_handlers
 */
class drop_crm_handler_field_last_updated extends views_handler_field {

  /**
   * {@inheritdoc}
   */
  function option_definition() {
    $options = parent::option_definition();
    $options['stale_flag'] = array('default' => TRUE, 'bool' => TRUE);
    return $options;
  }

  /**
   * {@inheritdoc}
   */
  function options_form(&$form, &$form_state) {
    $form['stale_flag'] = array(
      '#type' => 'checkbox',
      '#title' => t('Flag contacts that need an update'),
      '#description' => t('Adds a "Needs update" badge when the contact has not been saved within the threshold set in the CRM settings.'),
      '#default_value' => !empty($this->options['stale_flag']),
    );
    parent::options_form($form, $form_state);
  }

  /**
   * Whether a result row is past the staleness threshold.
   */
  function is_stale($values) {
    $changed = (int) $this->get_value($values);
    return !$changed || $changed < REQUEST_TIME - (drop_crm_client_stale_days() * 86400);
  }

  /**
   * {@inheritdoc}
   */
  function render($values) {
    $changed = (int) $this->get_value($values);
    if ($changed) {
      $text = t('@interval ago', array('@interval' => format_interval(REQUEST_TIME - $changed, 1)));
      $title = format_date($changed, 'short');
    }
    else {
      $text = t('Never');
      $title = t('No recorded edit');
    }
    $stale = $this->is_stale($values);
    $output = '<span class="drop_crm-updated' . ($stale ? ' drop_crm-updated-stale' : '') . '" title="' . check_plain($title) . '">' . check_plain($text) . '</span>';
    if ($stale && !empty($this->options['stale_flag'])) {
      $output .= ' <span class="drop_crm-stale-flag" title="'
        . check_plain(t('Not updated in the last @count days.', array('@count' => drop_crm_client_stale_days())))
        . '">' . t('Needs update') . '</span>';
    }
    return $output;
  }

}
