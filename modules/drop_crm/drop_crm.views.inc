<?php
/**
 * @file
 * Views integration for Drop CRM.
 *
 * The three entity types (crm_contact, crm_contact_method, crm_activity) are
 * exposed to Views via EntityPlusDefaultViewsController, which reads
 * hook_entity_property_info() to generate field, filter, and sort handlers
 * automatically, including relationship handlers for entity-typed properties.
 *
 * This file supplements that automatic integration with:
 *  - A relationship from crm_contact back to crm_contact_method records
 *    (the cid column in the contact_method table creates the forward direction
 *    automatically; this adds the reverse "contact methods for a contact").
 *  - A relationship from crm_contact back to crm_activity via the
 *    field_crm_contacts entity reference field (handled by Field API Views
 *    integration automatically, but documented here for clarity).
 */

/**
 * Implements hook_views_data_alter().
 *
 * Gives the contact name field a handler that links to the contact page, and
 * adds a reverse relationship from crm_contact to crm_contact_method so that
 * a contacts-based view can include the contact's email addresses, phone
 * numbers, and addresses.
 */
function drop_crm_views_data_alter(&$data) {
  // Reverse relationship: crm_contact → crm_contact_method records.
  // The forward relationship (contact_method → contact) is handled
  // automatically because the 'cid' property on crm_contact_method has type
  // 'crm_contact' in hook_entity_property_info().
  if (isset($data['crm_contact'])) {
    // Contact names link to the contact page. The generic handler ignored
    // the views' "link_to_entity" option, so lists showed plain-text names.
    if (isset($data['crm_contact']['name']['field'])) {
      $data['crm_contact']['name']['field']['handler'] = 'drop_crm_handler_field_contact_name';
    }

    // "Last updated": the changed time as an age, with a staleness flag.
    $data['crm_contact']['last_updated'] = array(
      'title' => t('Last updated'),
      'help' => t('How long ago the contact was saved, flagged when past the Clients report staleness threshold.'),
      'real field' => 'changed',
      'field' => array(
        'handler' => 'drop_crm_handler_field_last_updated',
        'click sortable' => TRUE,
      ),
    );

    $data['crm_contact']['contact_methods'] = array(
      'title'  => t('Contact Methods'),
      'help'   => t('Relate a contact to its email, telephone, and address records.'),
      'relationship' => array(
        'base'          => 'crm_contact_method',
        'base field'    => 'cid',
        'field'         => 'cid',
        'id'            => 'standard',
        'label'         => t('Contact Methods'),
        'relationship field' => 'cid',
      ),
    );
  }

  // Warmth sorts Hot, Warm, Cold, unset rather than alphabetically.
  if (isset($data['field_data_field_crm_warmth']['field_crm_warmth']['field'])) {
    $data['field_data_field_crm_warmth']['field_crm_warmth']['field']['handler'] = 'drop_crm_handler_field_warmth';
    $data['field_data_field_crm_warmth']['field_crm_warmth_rank'] = array(
      'title' => t('Warmth rank'),
      'help' => t('Sort by warmth: Hot, Warm, Cold, then contacts with no warmth.'),
      'real field' => 'field_crm_warmth_value',
      'sort' => array('handler' => 'drop_crm_handler_sort_warmth'),
    );
  }

  // Summary line for the Clients report.
  $data['views']['drop_crm_client_summary'] = array(
    'title' => t('Clients summary'),
    'help' => t('Client count, plus how many need an update.'),
    'area' => array('handler' => 'drop_crm_handler_area_client_summary'),
  );
}
