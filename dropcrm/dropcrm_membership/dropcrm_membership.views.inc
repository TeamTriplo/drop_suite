<?php
/**
 * @file
 * Views integration for DropCRM Membership.
 *
 * Forward relationships are generated automatically by
 * EntityPlusDefaultViewsController because hook_entity_property_info() types
 * FK columns as their target entity. Reverse relationships between entities
 * within this module are also auto-generated.
 *
 * This file only supplements the one case that can't be generated
 * automatically: a reverse relationship from crm_contact (owned by dropcrm)
 * to crm_membership, so a contact-based view can list a contact's memberships.
 */

/**
 * Implements hook_views_data_alter().
 */
function dropcrm_membership_views_data_alter(&$data) {
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
}
