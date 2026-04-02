<?php
/**
 * @file
 * Inline Entity Form controller for crm_contact entities.
 *
 * Allows a crm_organization to be created inline while editing a crm_person,
 * via the field_crm_employer Inline Entity Form widget.
 */

/**
 * IEF controller for crm_contact (used for the crm_organization bundle).
 *
 * The crm_contact entity stores its human-readable name in a base-table column
 * ('name'), not as a Field API field. The default EntityInlineEntityFormController
 * only calls field_attach_form(), so we override entityForm() to add the name
 * textfield and entityFormSubmit() to copy it onto the entity.
 */
class CrmContactInlineEntityFormController extends EntityInlineEntityFormController {

  /**
   * {@inheritdoc}
   *
   * Returns display columns for the IEF reference table. We show the
   * organisation name only.
   */
  public function tableFields($bundles) {
    return array(
      'name' => array(
        'type'   => 'property',
        'label'  => t('Name'),
        'weight' => 1,
      ),
    );
  }

  /**
   * {@inheritdoc}
   *
   * Adds the organisation name field, which is a base-table property and
   * therefore not rendered by field_attach_form(). Also adds a helper note
   * explaining that the org is saved when the person form is submitted.
   */
  public function entityForm($entity_form, &$form_state) {
    $entity = $entity_form['#entity'];

    $entity_form['name'] = array(
      '#type'          => 'textfield',
      '#title'         => t('Organization Name'),
      '#default_value' => isset($entity->name) ? $entity->name : '',
      '#required'      => TRUE,
      '#maxlength'     => 255,
      '#weight'        => -10,
    );

    // Helper note: the org is held in memory until the person form is saved.
    $entity_form['ief_note'] = array(
      '#type'   => 'item',
      '#markup' => '<p class="description">' . t('Enter the organization name and click <em>Create organization</em>. It will appear in the table above and be saved when you save this person.') . '</p>',
      '#weight' => 99,
    );

    // Let the base class attach any Field API fields on this bundle.
    $entity_form = parent::entityForm($entity_form, $form_state);

    return $entity_form;
  }

  /**
   * {@inheritdoc}
   *
   * The base class copies non-field values to entity properties automatically,
   * so 'name' is handled there. We just need to ensure status defaults to
   * active (1) for newly created organisations.
   */
  public function entityFormSubmit(&$entity_form, &$form_state) {
    parent::entityFormSubmit($entity_form, $form_state);

    $entity = $entity_form['#entity'];
    // Default new organisations to active status.
    if (empty($entity->cid)) {
      $entity->status = 1;
    }
  }

  /**
   * {@inheritdoc}
   *
   * Called when the parent person form is submitted and the organisation is
   * actually written to the database. Show a status message so the user
   * knows the new org was created.
   */
  public function save($entity, $context) {
    $is_new = empty($entity->cid);
    $entity->save();
    if ($is_new) {
      backdrop_set_message(t('Organization %name has been created.', array('%name' => $entity->name)));
    }
  }

}
