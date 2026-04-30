<?php
/**
 * @file
 * Views integration for Drop CRM Membership.
 *
 * Forward relationships are generated automatically by
 * EntityPlusDefaultViewsController because hook_entity_property_info() types
 * FK columns as their target entity. Reverse relationships between entities
 * within this module are also auto-generated.
 *
 * This file only supplements the one case that can't be generated
 * automatically: a reverse relationship from crm_contact (owned by drop_crm)
 * to crm_membership, so a contact-based view can list a contact's memberships.
 */

/**
 * Implements hook_views_data_alter().
 */
function drop_crm_membership_views_data_alter(&$data) {
  if (isset($data['crm_contact'])) {
    $data['crm_contact']['memberships'] = array(
      'title'  => t('Memberships'),
      'help'   => t('Relate a contact to their membership records.'),
      'relationship' => array(
        'base'              => 'crm_membership',
        'base field'        => 'cid',
        'field'             => 'cid',
        'id'                => 'standard',
        'label'             => t('Memberships'),
        'relationship field' => 'cid',
      ),
    );
  }

  // Swap the Views date-field handler on columns that are intentionally
  // nullable. The stock handler calls strtotime($value) without a NULL
  // guard, emitting a PHP 8.1+ deprecation when these rows have no date.
  $nullable_dates = array(
    'crm_membership_term'    => array('end_date'),
    'crm_membership'         => array('last_payment_date'),
  );
  foreach ($nullable_dates as $table => $columns) {
    foreach ($columns as $col) {
      if (isset($data[$table][$col]['field']['handler'])) {
        $data[$table][$col]['field']['handler'] = 'DropcrmMembershipHandlerFieldDateSafe';
      }
    }
  }

}
