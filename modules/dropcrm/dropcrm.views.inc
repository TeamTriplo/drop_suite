<?php
/**
 * @file
 * Views integration for DropCRM.
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
 * Adds a reverse relationship from crm_contact to crm_contact_method so that
 * a contacts-based view can include the contact's email addresses, phone
 * numbers, and addresses.
 */
function dropcrm_views_data_alter(&$data) {
  // Reverse relationship: crm_contact → crm_contact_method records.
  // The forward relationship (contact_method → contact) is handled
  // automatically because the 'cid' property on crm_contact_method has type
  // 'crm_contact' in hook_entity_property_info().
  if (isset($data['crm_contact'])) {
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
}
