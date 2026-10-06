<?php
/**
 * @file
 * Contains the CRM contact name field handler.
 */

/**
 * Field handler for a contact's name that can link to the contact page.
 *
 * The generic handler Entity Plus assigns ignores the "link_to_entity" option
 * the shipped CRM views set, so names rendered as plain text. Modelled on
 * core's views_handler_field_node.
 *
 * @ingroup views_field_handlers
 */
class drop_crm_handler_field_contact_name extends views_handler_field {

  /**
   * {@inheritdoc}
   */
  function init(&$view, &$options) {
    parent::init($view, $options);
    if (!empty($this->options['link_to_entity'])) {
      $this->additional_fields['cid'] = array('table' => 'crm_contact', 'field' => 'cid');
    }
  }

  /**
   * {@inheritdoc}
   */
  function option_definition() {
    $options = parent::option_definition();
    $options['link_to_entity'] = array('default' => FALSE, 'bool' => TRUE);
    return $options;
  }

  /**
   * {@inheritdoc}
   */
  function options_form(&$form, &$form_state) {
    $form['link_to_entity'] = array(
      '#title' => t('Link this field to the contact page'),
      '#description' => t("Enable to override this field's links."),
      '#type' => 'checkbox',
      '#default_value' => !empty($this->options['link_to_entity']),
    );
    parent::options_form($form, $form_state);
  }

  /**
   * Render the name as a link to drop/crm/contact/{cid}.
   *
   * Data should be made XSS safe prior to calling this function.
   */
  function render_link($data, $values) {
    if (!empty($this->options['link_to_entity']) && !empty($this->additional_fields['cid'])) {
      if ($data !== NULL && $data !== '') {
        $this->options['alter']['make_link'] = TRUE;
        $this->options['alter']['path'] = 'drop/crm/contact/' . $this->get_value($values, 'cid');
      }
      else {
        $this->options['alter']['make_link'] = FALSE;
      }
    }
    return $data;
  }

  /**
   * {@inheritdoc}
   */
  function render($values) {
    $value = $this->get_value($values);
    return $this->render_link($this->sanitize_value($value), $values);
  }

}
